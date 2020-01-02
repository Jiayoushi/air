#include "tcp.h"

#include <stdlib.h>
#include <cstring>
#include <thread>
#include <algorithm>
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

#include "common/seg.h"
#include "common/timer.h"
#include "common/blocking_queue.h"
#include "common/common.h"
#include "common/send_buffer.h"
#include "tcp/tcp.h"
#include "ip/ip.h"
#include "overlay/overlay.h"
#include "air/air.h"

#define kClosed               0
#define kSynSent              1
#define kConnected            2
#define kFinWait              3
#define kMaxConnection        1024

#define kSegmentLost          0
#define kSegmentError         1
#define kSegmentIntact        2
#define kPacketLossRate       0
#define kPacketErrorRate      0

#define kTimeoutInterval      500      /* 500ms */

struct Tcb;
typedef std::shared_ptr<Tcb> TcbPtr;

/*
 * Transport control block.
 *
 */
struct Tcb {
  uint32_t src_ip;
  uint32_t src_port;
  uint32_t dest_ip;
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

  Tcb(): 
    src_ip(0),
    src_port(0), 
    dest_ip(0),
    dest_port(0),
    state(kClosed),
    iss(0),
    snd_una(0), 
    snd_nxt(iss + 1),
    rcv_nxt(0),
    rcv_win(1),
    irs(0),
    lock(),
    waiting(),
    send_buffer() {}
};

static std::mutex tcb_table_lock;
static std::vector<TcbPtr> tcb_table;

static std::atomic<bool> running;                                  /* Control the running of all threads. */
static std::shared_ptr<std::thread> timeout_thread;                /* Timeout loop */
static std::shared_ptr<std::thread> input_thread;                  /* Receiving segments from network stack */

static BlockingQueue<SegBufPtr> tcp_input;

static uint16_t random_port = 10000;

int Sock() {
  tcb_table_lock.lock();

  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<Tcb>();
      tcb_table_lock.unlock();
      return i;
    }
  }

  tcb_table_lock.unlock();
  return -1;
}

int Close(int sockfd) {
  Teardown(sockfd);
  tcb_table[sockfd] = nullptr;
  return 0;
}



static SegBufPtr CreateSegmentBuffer(TcbPtr tcb, uint8_t flags, const void *data=nullptr, uint32_t len=0) {
  SegPtr seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->src_port;
  seg->header.dest_port = tcb->dest_port;
  seg->header.seq = tcb->snd_nxt;
  seg->header.ack = (flags & kAck) ? tcb->rcv_nxt : 0;
  seg->header.length = sizeof(SegmentHeader);
  seg->header.flags = flags;
  seg->header.rcv_win = tcb->rcv_win;
  seg->header.checksum = 0;

  if (data != nullptr)
    memcpy(seg->data, data, len);

  seg->header.checksum = Checksum(seg, len + sizeof(SegmentHeader));

  return std::make_shared<SegmentBuffer>(seg, len, tcb->src_ip, tcb->dest_ip);
}

size_t Send(int sockfd, const void *data, uint32_t length) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;


  std::unique_lock<std::mutex> lk(tcb->lock);

  std::vector<SegBufPtr> bufs;

  const char *p = (const char *)data;
  size_t seg_len = length;

  while (seg_len > 0) {
    size_t size = std::min(seg_len, kMss);

    uint8_t flags = kAck;
    SegBufPtr seg_buf = CreateSegmentBuffer(tcb, flags, p, size);

    bufs.push_back(seg_buf);
    tcb->send_buffer.PushBack(seg_buf);
 
    tcb->snd_nxt += size;
    p += size;
    seg_len -= size;
  }

  return length;
}

/*
 * Establish connection by completing two way handshake.
 *
 * Return value: -1 on error, 0 on success
 */
int Connect(int sockfd, Ip dest_ip, uint16_t dest_port) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb->state != kClosed)
    return -1;

  /* Lock to ensure the atomicity of send_buffer */
  std::unique_lock<std::mutex> lk(tcb->lock);

  /* Assign addresses */
  tcb->src_ip = GetLocalIp();
  tcb->src_port = random_port++;
  tcb->dest_ip = dest_ip;
  tcb->dest_port = dest_port;

  /* Create Syn Segment and push it back to the sender buffer */
  uint8_t flags = kSyn;
  SegBufPtr syn_buf = CreateSegmentBuffer(tcb, flags);
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

  return -1;
}

/*
 * Teardown connection
 *
 * Return value: -1 on failure, 0 on success
 */
int Teardown(int sockfd) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  uint8_t flags = kFin | kAck;
  SegBufPtr seg_buf = CreateSegmentBuffer(tcb, flags);
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
  
  std::cout << "[TCP] " << "Disconnect max retry reached" << std::endl;

  return -1; 
}

/*
  Artificial segment lost and invalid checksum
*/
static int SegmentLost(SegPtr seg) {
  if ((rand() % 100) < kPacketLossRate * 100) {
    std::cerr << "[LOST]" << std::endl;
    return kSegmentLost;
  }

  if ((rand() % 100) < kPacketErrorRate * 100) {
    int len = sizeof(SegmentHeader);

    // Random error bit
    int error_bit = rand() % (len * 8);

    // Flip
    char *p = (char *)seg.get() + error_bit / 8;
    *p ^= 1 << (error_bit % 8);

    return kSegmentError;
  }

  return kSegmentIntact;
}

int TcpInputQueuePush(SegBufPtr seg_buf) {
  if (SegmentLost(seg_buf->segment) == kSegmentLost)
    return 0;

  tcp_input.Push(seg_buf);
  return 0;
}

SegBufPtr TcpInputQueuePop() {
  auto p = tcp_input.Pop(std::chrono::duration<double, std::deci>(1));
  if (!p)
    return nullptr;

  return *p.get();
}

static std::shared_ptr<Tcb> Demultiplex(SegBufPtr seg_buf) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

    if (seg_buf->segment->header.src_port == tcb_table[tcb_id]->dest_port &&
        seg_buf->segment->header.dest_port == tcb_table[tcb_id]->src_port &&
        seg_buf->src_ip == tcb_table[tcb_id]->dest_ip)
    return tcb_table[tcb_id];
  }

  return nullptr;
}

static int Input(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment; 

  if (!ValidChecksum(seg, seg_buf->data_size + sizeof(SegmentHeader))) {
    std::cout << "[TCP] " << "Invalid checksum" << std::endl;
    return -1;
  }

  std::shared_ptr<Tcb> tcb = Demultiplex(seg_buf);
  if (tcb == nullptr) {
    std::cout << "[TCP] " << "No matching tcb" << std::endl;
    return -1;
  }

  /* Handle Ack */
  size_t acked = 0;
  if (seg->header.flags & kAck) {
    acked = tcb->send_buffer.Ack(seg->header.ack);
    std::cout << "[TCP] " << "ACKED: " << acked << " UNACKED: " << tcb->send_buffer.Unacked() << " UNSENT: " << tcb->send_buffer.Unsent() << std::endl;
    if (acked != 0)
      tcb->send_buffer.SendUnsent();
  }

  /* Change states */
  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kClosed: {
      break;
    }
    case kSynSent: {
      if (acked <= 0)
        break;

      tcb->state = kConnected;
      if (seg->header.flags & kSyn) {
        tcb->irs = seg->header.seq;
        tcb->rcv_nxt = tcb->irs + 1;

        uint8_t flags = kAck;
        SegBufPtr ack = CreateSegmentBuffer(tcb, flags);
	IpSend(ack);
	std::cout << "[TCP] sent " << ack << std::endl;
      }

      tcb->waiting.notify_one();
      return 0;
    }
    case kConnected: {
      if (seg->header.flags & kSyn) {
        uint8_t flags = kAck;
        SegBufPtr ack = CreateSegmentBuffer(tcb, flags);
	IpSend(ack);
	std::cout << "[TCP] sent " << ack << std::endl;
      }
      return 0;
    }
    case kFinWait: {
      if ((seg->header.flags & kFin) != kFin || acked <= 0)
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

static void Timeout() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      TcbPtr tcb = tcb_table[tcb_id];
      if (tcb_table[tcb_id] == nullptr)
        continue;

      if (tcb->send_buffer.Timeout()) {
        if (tcb->send_buffer.MaxRetryReached()) {
          tcb->send_buffer.PopUnsentFront();
          tcb->waiting.notify_one();
          continue;
        }

        tcb->send_buffer.ResendUnacked(); 
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kTimeoutInterval));
  }
}

static void InputFromIp() {
  while (1) {
    SegBufPtr seg_buf = TcpInputQueuePop();

    if (!running)
      break;

    if (seg_buf != nullptr) {
      std::cout << "[TCP] " << "RECV: " << seg_buf << std::endl;
      Input(seg_buf);
    }
  }
}

int TcpMain() {
  std::cout << "[TCP] transport layer starting ..." << std::endl;
  tcb_table = std::vector<TcbPtr>(kMaxConnection, nullptr);

  running = true;
  input_thread = std::make_shared<std::thread>(InputFromIp);
  timeout_thread = std::make_shared<std::thread>(Timeout);

  RegisterInitSuccess();
  std::cout << "[TCP] transport layer started" << std::endl;

  return 0;
}

int TcpStop() {
  running = false;

  timeout_thread->join();
  input_thread->join();

  for (int i = 0; i < kMaxConnection; ++i)
    tcb_table[i] = nullptr;

  std::cout << "[TCP] exited" << std::endl;
  return 0;
}
