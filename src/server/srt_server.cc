#include "srt_server.h"

#include <iostream>
#include <cstring>
#include <memory>
#include <vector>
#include <atomic>
#include <condition_variable>

#include "common/recv_buffer.h"
#include "common/common.h"

#define kClosed        1
#define kListening     2
#define kConnected     3
#define kCloseWait     4

#define kMaxConnection 1024

static uint32_t overlay_conn;


// Server transport control block.
struct ServerTcb {
  ip src_ip;
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

  ServerTcb(): src_ip(0), src_port(0), dest_ip(0),
    dest_port(0), state(kClosed),
    iss(100), //rand() % std::numeric_limits<unsigned int>::max()),
    snd_nxt(iss + 1),
    rcv_nxt(0),
    rcv_win(1),
    lock(),
    recv_buffer() {}
};

std::mutex table_lock;
static std::vector<std::shared_ptr<ServerTcb>> tcb_table(kMaxConnection, nullptr);


typedef std::shared_ptr<ServerTcb> TcbPtr;

// A global variable control the running of all threads.
static std::atomic<bool> running;

std::shared_ptr<std::thread> input_thread;

std::shared_ptr<ServerTcb> Demultiplex(SegBufPtr seg_buf) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

    /* Accept new connection */
    if (tcb_table[tcb_id]->state == kListening &&
        seg_buf->segment->header.type == kSyn &&
        seg->header.dest_port == tcb_table[tcb_id]->src_port)
      return tcb_table[tcb_id];

    /* Normal case */
    if (seg_buf->segment->header.src_port == tcb_table[tcb_id]->dest_port &&
        seg_buf->segment->header.dest_port == tcb_table[tcb_id]->src_port &&
        seg_buf->ip == tcb_table[tcb_id]->dest_ip)
    return tcb_table[tcb_id];
  }

  return nullptr;
}

static SegBufPtr CreateSegmentBuffer(TcbPtr tcb, enum SegmentType type, const void *data=nullptr, uint32_t len=0) {
  SegPtr seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->src_port;
  seg->header.dest_port = tcb->dest_port;

  if (type == kSynAck)
    seg->header.seq = tcb->iss + 1;
  else
    seg->header.seq = 0;

  seg->header.length = sizeof(SegmentHeader);
  seg->header.type = type;
  seg->header.rcv_win = tcb->rcv_win;
  seg->header.ack = (type == kSyn) ? 0 : tcb->rcv_nxt;

  if (data) {
    memcpy(seg->data, data, len);
  }

  seg->header.checksum = Checksum(seg, len + sizeof(SegmentHeader));

  return std::make_shared<SegmentBuffer>(seg, len, tcb->src_ip, tcb->dest_ip);
}

static int Input(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment;

  if (!ValidChecksum(seg, sizeof(SegmentHeader) + seg_buf->data_size)) {
    SDEBUG << "INVALID CHECKSUM" << std::endl;
    return -1;
  }

  std::shared_ptr<ServerTcb> tcb = Demultiplex(seg_buf);
  if (tcb == nullptr) {
    SDEBUG << "No matching tcb" << std::endl;
    return -1;
  }

  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kClosed:
      std::cerr << "Input: segment received when state is closed" << std::endl;
      return -1;

    case kListening: {
      if (seg->header.type != kSyn)
        return -1;

      tcb->dest_ip = seg_buf->ip;
      tcb->dest_port = seg->header.src_port;
      tcb->rcv_nxt = seg->header.seq + 1;

      SegBufPtr syn_ack = CreateSegmentBuffer(tcb, kSynAck);
      SnpSendSegment(overlay_conn, syn_ack);
 
      tcb->state = kConnected;
      tcb->snd_nxt += 1;
     
      SDEBUG << "SENT: " << syn_ack << std::endl;
      tcb->waiting.notify_one();

      return 0;
    }
    case kConnected: {
      if (seg->header.type == kFin) {
        SegBufPtr fin_ack = CreateSegmentBuffer(tcb, kFinAck);
        SnpSendSegment(overlay_conn, fin_ack);
        SDEBUG << "SENT: " << fin_ack << std::endl;

        tcb->state = kCloseWait;
        tcb->rcv_nxt += 1;

        return 0;
      } else if (seg->header.type == kSyn) {
        SegBufPtr syn_ack = CreateSegmentBuffer(tcb, kSynAck);
        SnpSendSegment(overlay_conn, syn_ack);
        SDEBUG << "SENT: " << syn_ack << std::endl;

        return 0;
      } else if (seg->header.type == kData) {
        if (seg->header.seq != tcb->rcv_nxt)
          break;

        tcb->rcv_nxt += seg_buf->data_size;

        SegBufPtr data_ack = CreateSegmentBuffer(tcb, kDataAck);
        SnpSendSegment(overlay_conn, data_ack);
        SDEBUG << "SENT: " << data_ack << std::endl;

        tcb->recv_buffer.PushBack(seg_buf);
        tcb->waiting.notify_one();

        return 0;
      }

      break;
   }
   case kCloseWait: {
     if (seg->header.type != kFin)
       break;
   
     SegBufPtr fin_ack = CreateSegmentBuffer(tcb, kFinAck);
     SnpSendSegment(overlay_conn, fin_ack);
     SDEBUG << "SENT: " << fin_ack << std::endl;
     return 0;           
   }
   default:
     std::cerr << "Error: sockfd already connected" << std::endl;
     return -1;
  }

  return 0;
}


int SrtServerAccept(int sockfd) {
  std::shared_ptr<ServerTcb> tcb = tcb_table[sockfd];
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


size_t SrtServerRecv(int sockfd, void *buffer, unsigned int length) {
  std::shared_ptr<ServerTcb> tcb = tcb_table[sockfd]; 
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  size_t recved = 0;
  while (recved < length) {
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

static void InputFromIp() {
  while (running) {
    SegBufPtr seg_buf = SnpRecvSegment(overlay_conn);

    if (seg_buf != nullptr) {
      SDEBUG << "RECV: " << seg_buf << std::endl;
      Input(seg_buf);
    }
  }
}

int SrtServerSock(unsigned int src_port) {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<ServerTcb>();
      tcb_table[i]->src_port = src_port;
      tcb_table[i]->state = kListening;
      return i;
    }
  }

  return -1; 
}

int SrtServerClose(int sockfd) {
  tcb_table[sockfd] = nullptr;
  return 0;
}

void SrtServerInit(int conn) {
  overlay_conn = conn;
  running = true;

  input_thread = std::make_shared<std::thread>(InputFromIp);
}

static void NotifyShutdown() {
  running = false;
}

void SrtServerShutdown() {
  NotifyShutdown();

  input_thread->join();
}

