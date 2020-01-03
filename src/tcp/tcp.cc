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
#define kListening            3
#define kSynRcvd              4
#define kFinWait1             5
#define kFinWait2             6
#define kCoseWait             7
#define kCloseWait            8
#define kClosing              9
#define kLastAck              10

#define kMaxConnection        1024

#define kSegmentLost          0
#define kSegmentError         1
#define kSegmentIntact        2
#define kPacketLossRate       0
#define kPacketErrorRate      0

#define kConnectingTimer      0
#define kRetransmitTimer      1
#define kDelayedAckTimer      2
#define kPersistTimer         3
#define kKeepaliveTimer       4
#define kFinWait2Timer        5
#define kTimeWaitTimer        6

#define kTimerNum             7
#define kTimeoutInterval      500      /* 500ms */
#define kTimeWait             10       /* 10 intervals */

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
  uint32_t iss;                       /* Initial send sequence number */
  uint32_t snd_una;                   /* The oldest unacked segment's sequence number */
  uint32_t snd_nxt;                   /* The next sequence number to be used to send segments */

  uint32_t rcv_nxt;
  uint32_t rcv_win;
  uint32_t irs;                       /* Initial receive sequence number */

  std::mutex lock;
  std::condition_variable waiting;    /* A condition variable used to wait for message either from timeouts or from a new incoming segment */
  SendBuffer send_buffer; 
  RecvBuffer recv_buffer;

  uint8_t timers[kTimerNum];

  Tcb(): 
    src_ip(0),
    src_port(0), 
    dest_ip(0),
    dest_port(0),
    state(kClosed),
    iss(0),
    snd_una(0),
    snd_nxt(iss),
    rcv_nxt(0),
    rcv_win(0),
    irs(0),
    lock(),
    waiting(),
    send_buffer(),
    recv_buffer() {
      memset(timers, 0, sizeof(uint8_t) * kTimerNum);
    }
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

static int PassiveClose(TcbPtr tcb) {
  std::lock_guard<std::mutex> lck(tcb->lock);

  /* Create Fin Segment and push it back to the sender buffer */
  uint8_t flags = kFin;
  SegBufPtr syn_buf = Output(tcb, flags);
  tcb->send_buffer.PushBack(syn_buf);

  /* Update */
  tcb->state = kLastAck;
  tcb->snd_nxt++;

  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kClosed;
    });

  return 0;
}

/* 
 * If the state is close wait, continue with passive close,
 * otherwise, initiate active close. 
 */
int Close(int sockfd) {
  TcbPtr tcb = tcb_table[sockfd];

  if (tcb->state == kCloseWait)
    PassiveClose(tcb);
  else if (tcb->state != kClosed)
    Teardown(sockfd);

  tcb_table[sockfd] = nullptr;
  return 0;
}

static SegBufPtr Output(TcbPtr tcb, uint8_t flags, const void *data=nullptr, uint32_t len=0) {
  SegPtr seg = std::make_shared<Segment>();
  SegmentHeader &hdr = seg->header;

  hdr.src_port = tcb->src_port;
  hdr.dest_port = tcb->dest_port;
  hdr.ack = (flags & kAck) ? tcb->rcv_nxt : 0;
  hdr.length = sizeof(SegmentHeader);
  hdr.flags = flags;
  hdr.rcv_win = tcb->rcv_win;
  hdr.checksum = 0;

  if (flags & kSyn)
    hdr.seq = tcb->iss;
  else if (flags & kAck)
    hdr.seq = 0;
  else
    hdr.seq = tcb->snd_nxt;

  if (data)
    memcpy(seg->data, data, len);

  hdr.checksum = Checksum(seg, len + sizeof(SegmentHeader));

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
    SegBufPtr seg_buf = Output(tcb, flags, p, size);

    bufs.push_back(seg_buf);
    tcb->send_buffer.PushBack(seg_buf);
 
    tcb->snd_nxt += size;
    p += size;
    seg_len -= size;
  }

  return length;
}

size_t Recv(int sockfd, void *buffer, unsigned int length) {
  std::shared_ptr<ServerTcb> tcb = tcb_table2[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  size_t recved = 0;
  while (recved < length) {
    std::cout << "[TCP] " << "Recv Current " << recved << " expect " << length << std::endl;
    tcb->waiting.wait(lk,
      [&tcb] {
        return !tcb->recv_buffer.Empty();
      });

    while (!tcb->recv_buffer.Empty()) {
      SegBufPtr seg_buf = tcb->recv_buffer.Front();
      tcb->recv_buffer.Pop();

      memcpy((char *)buffer + recved, seg_buf->segment->data, seg_buf->data_size);
      recved += seg_buf->data_size;
    }
  }

  return recved;
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
  SegBufPtr syn_buf = Output(tcb, flags);
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
  SegBufPtr seg_buf = Output(tcb, flags);
  tcb->send_buffer.PushBack(seg_buf);

  /* Update */
  tcb->state = kFinWait1;
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

int Bind(int sockfd, Ip src_ip, uint16_t src_port) {
  if (tcb_table[sockfd] == nullptr)
    return -1;

  tcb_table[sockfd]->src_ip = src_ip;
  tcb_table[sockfd]->src_port = src_port;

  return 0;
}

int Accept(int sockfd) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  tcb->state = kListening;

  // Blocked until input notify that the state is connected
  std::unique_lock<std::mutex> lk(tcb->lock);
  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kConnected;
    });

  return 0;
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

    TcbPtr tcb = tcb_table[tcb_id];
    const SegmentHeader &hdr = seg_buf->segment->header;

    /* Accept new connection */
    if (tcb->state == kListening && 
	(hdr.flags & kSyn) &&
        hdr.dest_port == tcb->src_port &&
	seg_buf->dest_ip == tcb->src_ip)
      return tcb_table2[tcb_id];

    /* Exact match */
    if (hdr.src_port == tcb->dest_port && 
	hdr.dest_port == tcb->src_port &&
        seg_buf->src_ip == tcb->dest_ip &&
	seg_buf->dest_ip == tcb->src_ip)
    return tcb_table[tcb_id];
  }

  return nullptr;
}

static int Input(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment; 
  uint8_t flags = seg->headers.flags;

  /* Checksum */
  if (!ValidChecksum(seg, seg_buf->data_size + sizeof(SegmentHeader))) {
    std::cout << "[TCP] " << "Invalid checksum" << std::endl;
    return -1;
  }

  /* Locate tcb */
  std::shared_ptr<Tcb> tcb = Demultiplex(seg_buf);
  if (tcb == nullptr) {
    std::cout << "[TCP] " << "No matching tcb" << std::endl;
    return -1;
  }

  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kClosed: {
      return 0;
    }
    case kListen: {
      if (flags & kAck)
        return 0;
      if ((flags & kSyn) == 0)
        return 0;

      tcb->state = kSynRcvd;
      tcb->dest_ip = seg_buf->src_ip;
      tcb->dest_port = seg->header.src_port;
      tcb->irs = seg->header.seq;
      tcb->rcv_nxt = seg->header.seq + 1;
      break;
    }
    /* If syn ack not reached to client, send it again.
     * If not contain ack, return.
     */
    case kSynRcvd: {
      if (flags & kSyn)
        break;
      if ((flags & kAck) == 0)
        return 0;
      if (seg->header.ack != tcb->iss + 1)
        return 0;

      tcb->state = kConnected;
      tcb->waiting.notify_one();
      return 0;
    }
    /* If ACK of our SYN, connection completed */
    case kSynSent: {
      if ((flags & kAck) == 0)
        return 0;
      if (seg->header.ack != tcb->iss + 1)
        return 0;
      if ((flags & kSyn) == 0)
        return 0;

      tcb->state = kConnected;
      tcb->waiting.notify_one();
      break;
    }
    case kConnected: {
      if (flags & kSyn)
	return 0;
      if ((flags & kAck) != 0)
	return 0;
      if (flags & kFin) {
        if (seg->header.ack != tcb->snd_nxt)
	  return 0;

	tcb->state = kCloseWait;
      }
      break;
    }
    case kFinWait1: {
      if ((flags & kAck) == 0)
        return 0;
      if (seg->header.ack != tcb->snd_nxt)
	return 0;
      
      tcb->state = kFinWait2;
      return 0;
    }
    case kFinWait2: {
      if ((flags & kFin) == 0)
        return 0;

      break;
    }
    case kTimeWait: {
      break;
    }
    case kCloseWait: {
      break;
    }
    case kLastAck: {
      tcb->state = kClosed;
      tcb->waiting.notify_one();
      return 0;
    }
    default:
      return -1;
  }

  /* Ack sent packets */
  if (seg->header.flags & kAck) {
    size_t acked = tcb->send_buffer.Ack(seg->header.ack);
    if (acked > 0)
      tcb->send_buffer.SendUnsent();

    std::cout << "[TCP] " 
              << "ACKED: " << acked  << " "
              << "UNACKED: " << tcb->send_buffer.Unacked() << " "
              << "UNSENT: " << tcb->send_buffer.Unsent() << std::endl;
  }

  /* Send ack */
  if (flags & kSyn) {
    SegBufPtr syn_ack = Output(tcb, flags);
    IpSend(syn_ack);
    SDEBUG << "SENT: " << syn_ack << std::endl;
    tcb->snd_nxt += 1;
  }

  return 0;
}

static void Timeout() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      TcbPtr tcb = tcb_table[tcb_id];
      if (!tcb)
        continue;

      /* Restransmit timeout */
      if (tcb->send_buffer.Timeout()) {
        if (tcb->send_buffer.MaxRetryReached()) {
          tcb->send_buffer.PopUnsentFront();
          tcb->waiting.notify_one();
          continue;
        }
        tcb->send_buffer.ResendUnacked(); 
      }

      /* Close wait timeout */
      if (tcb->state == kCloseWait) {
	if (++tcb->timers[kTimeWaitTimer] == kTimeWaitTimeout) {
          tcb->state = kClosed;
          std::cout << "[TCP] time wait timed out, " << tcb_id << " is now closed" << std::endl;
	}
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
