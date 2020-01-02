#include "srt_server.h"

#include <iostream>
#include <cstring>
#include <memory>
#include <vector>
#include <atomic>
#include <condition_variable>

#include "common/recv_buffer.h"
#include "common/common.h"
#include "ip/ip.h"
#include "tcp/tcp.h"

#define kServerClosed        0
#define kServerListening     1
#define kServerRcvd          2
#define kServerConnected     3
#define kSeverCloseWait      4

#define kMaxConnection       1024
#define kSeverCloseWaitTimeout2  10          /* 10 times 500ms */
#define kTimersNum           1

enum TimerType {
  kSeverCloseWaitTimer = 0,
};

// Server transport control block.
struct ServerTcb {
  Ip src_ip;
  unsigned int src_port;

  Ip dest_ip;
  unsigned int dest_port;

  unsigned int state;

  unsigned int iss;
  unsigned int snd_nxt;

  unsigned int rcv_nxt;
  unsigned int rcv_win;

  std::mutex lock;

  std::condition_variable waiting;

  RecvBuffer recv_buffer;

  uint16_t timers[kTimersNum];

  ServerTcb(): src_ip(0), src_port(0), dest_ip(0),
    dest_port(0), state(kServerClosed),
    iss(100),
    snd_nxt(iss + 1),
    rcv_nxt(0),
    rcv_win(1),
    lock(),
    recv_buffer() {
    memset(timers, 0, sizeof(uint16_t) * kTimersNum);
  }
};

static std::mutex table_lock;
static std::vector<std::shared_ptr<ServerTcb>> tcb_table2;

typedef std::chrono::duration<double, std::milli> TimeInterval;
typedef std::shared_ptr<ServerTcb> TcbPtr;

// A global variable control the running of all threads.
static std::atomic<bool> running;

static std::shared_ptr<std::thread> input_thread;
static std::shared_ptr<std::thread> timeout_thread;  /* Timeout2 loop */
const static TimeInterval kTimeout2LoopInterval(500); /* 500ms */

std::shared_ptr<ServerTcb> Dem2(SegBufPtr seg_buf) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table2[tcb_id] == nullptr)
      continue;

    /* Accept new connection */
    if (tcb_table2[tcb_id]->state == kServerListening &&
        (seg_buf->segment->header.flags & kSyn) &&
        seg_buf->segment->header.dest_port == tcb_table2[tcb_id]->src_port)
      return tcb_table2[tcb_id];

    /* Normal case */
    if (seg_buf->segment->header.src_port == tcb_table2[tcb_id]->dest_port &&
        seg_buf->segment->header.dest_port == tcb_table2[tcb_id]->src_port &&
        seg_buf->src_ip == tcb_table2[tcb_id]->dest_ip)
    return tcb_table2[tcb_id];
  }

  return nullptr;
}

static SegBufPtr Create(TcbPtr tcb, uint8_t flags, const void *data=nullptr, uint32_t len=0) {
  SegPtr seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->src_port;
  seg->header.dest_port = tcb->dest_port;
  seg->header.seq = tcb->snd_nxt;
  seg->header.length = sizeof(SegmentHeader);
  seg->header.flags = flags;
  seg->header.rcv_win = tcb->rcv_win;
  seg->header.ack = (flags & kAck) ? tcb->rcv_nxt : 0;

  if (data)
    memcpy(seg->data, data, len);

  seg->header.checksum = Checksum(seg, len + sizeof(SegmentHeader));

  return std::make_shared<SegmentBuffer>(seg, len, tcb->src_ip, tcb->dest_ip);
}

static int Input2(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment;

  if (!ValidChecksum(seg, sizeof(SegmentHeader) + seg_buf->data_size)) {
    SDEBUG << "INVALID CHECKSUM" << std::endl;
    return -1;
  }

  std::shared_ptr<ServerTcb> tcb = Dem2(seg_buf);
  if (tcb == nullptr) {
    SDEBUG << "No matching tcb" << std::endl;
    return -1;
  }

  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kServerClosed: {
      std::cerr << "[TCP] Input2: segment received when state is closed" << std::endl;
      return -1;
    }
    case kServerListening: {
      if ((seg->header.flags & kSyn) != kSyn)
        return -1;

      tcb->dest_ip = seg_buf->src_ip;
      tcb->dest_port = seg->header.src_port;
      tcb->rcv_nxt = seg->header.seq + 1;
      tcb->state = kServerRcvd;

      uint8_t flags = kAck | kSyn;
      SegBufPtr syn_ack = Create(tcb, flags);
      IpSend(syn_ack);
      SDEBUG << "SENT: " << syn_ack << std::endl;
      tcb->snd_nxt += 1;

      return 0;
    }
    case kServerRcvd: {
      if ((seg->header.flags & kAck) != kAck)
        break;
      if (seg->header.ack != tcb->snd_nxt)
	break;

      tcb->state = kServerConnected;
      tcb->waiting.notify_one();
      return 0;
    }
    case kServerConnected: {
      if (seg->header.flags & kFin) {
        tcb->state = kSeverCloseWait;
        tcb->rcv_nxt += 1;

        uint8_t flags = kAck | kFin;
        SegBufPtr fin_ack = Create(tcb, flags);
        IpSend(fin_ack);
        SDEBUG << "SENT: " << fin_ack << std::endl;
	tcb->snd_nxt += 1;

        return 0;
      } else if (seg->header.flags & kSyn) {
        uint8_t flags = kAck;
        SegBufPtr syn_ack = Create(tcb, flags);
        IpSend(syn_ack);
        SDEBUG << "SENT: " << syn_ack << std::endl;

        return 0;
      } else if (seg->header.length != 0) {
        if (seg->header.seq != tcb->rcv_nxt)
          break;

        tcb->rcv_nxt += seg_buf->data_size;

	uint8_t flags = kAck;
        SegBufPtr data_ack = Create(tcb, flags);
        IpSend(data_ack);
        SDEBUG << "SENT: " << data_ack << std::endl;

        tcb->recv_buffer.PushBack(seg_buf);
        tcb->waiting.notify_one();
        return 0;
      }
      break;
   }
   case kSeverCloseWait: {
      if ((seg->header.flags & kFin) != kFin)
        break;

      uint8_t flags = kAck | kFin;
      SegBufPtr fin_ack = Create(tcb, flags);
      IpSend(fin_ack);
      SDEBUG << "SENT: " << fin_ack << std::endl;

      // Wait for 30s before really close
      // tcb->state = kServerClosed;
      return 0;
   }
   default:
     std::cerr << "Error: sockfd already connected" << std::endl;
     return -1;
  }

  return 0;
}


int SrtServerAccept(int sockfd) {
  std::shared_ptr<ServerTcb> tcb = tcb_table2[sockfd];
  if (tcb == nullptr)
    return -1;

  tcb->state = kServerListening;
  std::cout << "[DEBUG] " << sockfd << " " << tcb->state << std::endl;

  // Blocked until input notify that the state is connected
  std::unique_lock<std::mutex> lk(tcb->lock);
  tcb->waiting.wait(lk, 
    [&tcb] {
      return tcb->state == kServerConnected; 
    });

  return 0;
}


size_t SrtServerRecv(int sockfd, void *buffer, unsigned int length) {
  std::shared_ptr<ServerTcb> tcb = tcb_table2[sockfd]; 
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  size_t recved = 0;
  while (recved < length) {
    SDEBUG << "TOTAL " << recved << " expect " << length << std::endl;
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

static void Input2FromIp() {
  while (running) {
    SegBufPtr seg_buf = TcpInputQueuePop();

    if (seg_buf != nullptr) {
      SDEBUG << "RECV: " << seg_buf << std::endl;
      Input2(seg_buf);
    }
  }
}

static void Timeout2() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      TcbPtr tcb = tcb_table2[tcb_id];
      if (tcb_table2[tcb_id] == nullptr)
        continue;

      if (tcb_table2[tcb_id]->state == kSeverCloseWait &&
	  ++tcb_table2[tcb_id]->timers[kSeverCloseWaitTimer] == kSeverCloseWaitTimeout2) {
	std::cout << "[TCP] close wait timed out, " << tcb_id << " is now closed" << std::endl;
	tcb_table2[tcb_id]->state = kServerClosed;
      }
    }

    std::this_thread::sleep_for(kTimeout2LoopInterval);
  }
}

int SrtServerSock() {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table2[i] == nullptr) {
      tcb_table2[i] = std::make_shared<ServerTcb>();
      tcb_table2[i]->state = kServerListening;
      return i;
    }
  }

  return -1; 
}

int SrtServerBind(int sockfd, Ip src_ip, uint16_t src_port) {
  if (tcb_table2[sockfd] == nullptr)
    return -1;

  tcb_table2[sockfd]->src_ip = src_ip;
  tcb_table2[sockfd]->src_port = src_port;

  return 0;
}

int SrtServerClose(int sockfd) {
  tcb_table2[sockfd] = nullptr;
  return 0;
}

void SrtServerInit() {
  tcb_table2 = std::vector<TcbPtr>(kMaxConnection, nullptr);

  running = true;
  input_thread = std::make_shared<std::thread>(Input2FromIp);
  timeout_thread = std::make_shared<std::thread>(Timeout2);
}

static void NotifyShutdown() {
  running = false;
}

void SrtServerShutdown() {
  NotifyShutdown();

  input_thread->join();
  timeout_thread->join();

  std::cout << "Exited" << std::endl;
}

