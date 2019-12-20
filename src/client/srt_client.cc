#include "srt_client.h"

#include <stdlib.h>
#include <cstring>
#include <thread>
#include <queue>
#include <iterator>
#include <mutex>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <list>
#include <condition_variable>

#include "../common/seg.h"
#include "../common/timer.h"
#include "../common/blocking_queue.h"
#include "../common/common.h"
#include "../common/send_buffer.h"

#define kClosed          0
#define kSynSent         1
#define kConnected       2
#define kFinWait         3

#define kMaxFinRetry     3
#define kMaxSynRetry     3
#define kMaxConnection   1024


/*
 * Client transport control block, the client side of a SRT connection
 * uses this data structure to keep track of connection information
 *
 * The dest_id serves as the purpose of 'ip address' when demultiplexing.
 * Current implementation supports data segments flow from client to server.
 *
 */
struct ClientTcb {
  uint32_t src_id;
  uint32_t src_port;
  uint32_t dest_id;
  uint32_t dest_port;

  uint32_t state;
  uint32_t iss;                   /* Initial send sequence number */
  uint32_t snd_una;               /* The oldest unacked segment's sequence number */
  uint32_t snd_nxt;               /* The next sequence number to be used to send segments */

  uint32_t rcv_nxt;
  uint32_t rcv_win;
  uint32_t irs;                   /* Initial receive sequence number */
  

  std::mutex lock;
  std::condition_variable waiting;    /* A condition variable used to wait for message either from timeouts or from a new incoming segment */
  SendBuffer send_buffer; 

  ClientTcb(uint32_t overlay_conn): 
    src_id(0),
    src_port(0), 
    dest_id(0),
    dest_port(0),
    state(kClosed),
    iss(rand() % std::numeric_limits<uint32_t>::max()),
    snd_una(0), 
    snd_nxt(iss),
    rcv_nxt(0),
    rcv_win(1),
    irs(0),
    lock(),
    waiting(),
    send_buffer(overlay_conn) {}
};

typedef std::shared_ptr<ClientTcb> TcbPtr;
typedef std::chrono::duration<double, std::milli> TimeInterval;

static uint32_t overlay_conn;                                      /* Connector to the ip stack */
static std::atomic<bool> running;                                  /* Control the running of all threads. */
std::mutex tcb_table_lock;
static std::vector<std::shared_ptr<ClientTcb>> tcb_table;
static std::shared_ptr<std::thread> timeout_thread;                /* Timeout loop */
static std::shared_ptr<std::thread> input_thread;                  /* Receiving segments from network stack */
const static TimeInterval kTimeoutLoopInterval(500);               /* Time interval between checking timeouts. */



int SrtClientSock(uint32_t src_port) {
  tcb_table_lock.lock();

  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<ClientTcb>(overlay_conn);
      tcb_table[i]->src_port = src_port;
      tcb_table_lock.unlock();
      return i;
    }
  }

  tcb_table_lock.unlock();
  return -1;
}

static SegBufPtr CreateSegmentBuffer(TcbPtr tcb, enum SegmentType type, void *data=nullptr, uint32_t data_size=0) {
  SegPtr seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->src_port;
  seg->header.dest_port = tcb->dest_port;
  seg->header.seq = tcb->snd_nxt;
  seg->header.length = sizeof(SegmentHeader);
  seg->header.type = type;
  seg->header.rcv_win = tcb->rcv_win;
  seg->header.ack = (type == kSyn) ? 0 : tcb->rcv_nxt;
  seg->header.checksum = Checksum(seg, data_size + sizeof(SegmentHeader));

  if (data) {
    seg->data = new char[data_size];
    memcpy(seg->data, data, data_size);
  }

  return std::make_shared<SegmentBuffer>(seg, data_size);
}

/*
 * Establish connection by completing two way handshake.
 *
 * Return value: -1 on error, 0 on success
 */
int SrtClientConnect(int sockfd, uint32_t dest_port) {
  std::shared_ptr<ClientTcb> tcb = tcb_table[sockfd];
  if (tcb->state != kClosed)
    return -1;

  /* Lock to ensure the atomicity of send_buffer */
  std::unique_lock<std::mutex> lk(tcb->lock);

  /* Create Syn Segment and push it back to the sender buffer */
  if (tcb->send_buffer.Full())
    return -1;

  tcb->dest_port = dest_port;
  SegBufPtr syn_buf = CreateSegmentBuffer(tcb, kSyn);
  tcb->send_buffer.PushBack(syn_buf);

  /* Update */
  tcb->dest_port = dest_port;
  tcb->state = kSynSent;
  tcb->snd_nxt++;

  /* Wait until connection timeout or success */
  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kConnected || tcb->send_buffer.MaxRetryReached();
    });

  /* Success */
  if (tcb->state == kConnected)
    return 0;

  /* Connection timed out */
  tcb->state = kClosed;
  tcb->snd_nxt = tcb->iss;
  tcb->send_buffer.Clear();

  return -1;
}

/*
 * Teardown connection
 *
 * Return value: -1 on failure, 0 on success
 */
int SrtClientDisconnect(int sockfd) {
  std::shared_ptr<ClientTcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  if (tcb->send_buffer.Full())
    return -1;
  SegBufPtr seg_buf = CreateSegmentBuffer(tcb, kFin);
  tcb->send_buffer.PushBack(seg_buf);

  /* Update */
  tcb->state = kFinWait;
  tcb->snd_nxt++;

  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kClosed || tcb->send_buffer.MaxRetryReached();
    });

  if (tcb->state == kClosed)
    return 0;

  /* Timed out */
  tcb->state = kConnected;
  tcb->snd_nxt--;
  tcb->send_buffer.Clear();

  return -1; 
}

// Destination port number, the source IP address, and the source port number.

// TODO: right now id is not used to demultiplex, in theory SrtConnect should
// also have a server node id field
static std::shared_ptr<ClientTcb> Demultiplex(std::shared_ptr<Segment> seg) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

    if (seg->header.src_port == tcb_table[tcb_id]->dest_port
     && seg->header.dest_port == tcb_table[tcb_id]->src_port)
    return tcb_table[tcb_id];
  }

  return nullptr;
}

static int Input(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment; 

  if (!ValidChecksum(seg, seg_buf->data_size + sizeof(SegmentHeader))) {
    CDEBUG << "Invalid checksum" << std::endl;
    return -1;
  }

  std::shared_ptr<ClientTcb> tcb = Demultiplex(seg);
  if (tcb == nullptr) {
    CDEBUG << "No matching tcb" << std::endl;
    return -1;
  }

  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kClosed: {
      break;
    }
    case kSynSent: {
	  if (seg->header.type != kSynAck) {
	    break;
      }

      size_t acked = tcb->send_buffer.Ack(seg->header.ack);
      CDEBUG << "ACKED: " << acked << std::endl;
      tcb->send_buffer.SendUnsent();

	  tcb->state = kConnected;
      tcb->irs = seg->header.seq;
	  tcb->rcv_nxt = tcb->irs + 1;

      tcb->waiting.notify_one();
      return 0;
    }
    case kConnected: {
      break;
    }
    case kFinWait: {
      if (seg->header.type != kFinAck)
        break;

      uint32_t snd_nxt = 0;
      if ((snd_nxt = tcb->send_buffer.Ack(seg->header.ack)) < 0)
        break;

      tcb->state = kClosed;
      tcb->waiting.notify_one();

      return 0;

    }
    default:
      break;
  }

  return -1;
}

static void InputFromIp() {
  while (1) {
    SegBufPtr seg_buf = SnpRecvSegment(overlay_conn);

    if (!running)
      break;

    if (seg_buf != nullptr) {
      CDEBUG << "RECV: " << SegToString(seg_buf->segment) << std::endl;
      Input(seg_buf);
    }
  }
}

static void Timeout() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      TcbPtr tcb = tcb_table[tcb_id];
      if (tcb_table[tcb_id] == nullptr)
        continue;

      if (tcb->send_buffer.Timeout()) {
        if (tcb->send_buffer.MaxRetryReached()) {
          tcb->waiting.notify_one();
          continue;
        }

        tcb->send_buffer.ResendUnacked(); 
      }
    }

    std::this_thread::sleep_for(kTimeoutLoopInterval);
  }
}

void SrtClientInit(int conn) {
  overlay_conn = conn;
  tcb_table = std::vector<TcbPtr>(kMaxConnection, nullptr);

  running = true;
  input_thread = std::make_shared<std::thread>(InputFromIp);
  timeout_thread = std::make_shared<std::thread>(Timeout);
}



int SrtClientClose(int sockfd) {
  tcb_table_lock.lock();
  tcb_table[sockfd] = nullptr;
  tcb_table_lock.unlock();

  return 0;
}



static void NotifyShutdown() {
  running = false;
}

void SrtClientShutdown() {
  NotifyShutdown();

  timeout_thread->join();
  input_thread->join();
}

int SrtClientSend(int sockfd, void *data, uint32_t length) {
  return 0;
}
