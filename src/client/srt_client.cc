#include "srt_client.h"

#include <stdlib.h>
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

#include "../common/seg.h"
#include "../common/timer.h"

// Client States
#define kClosed     0
#define kSynSent    1
#define kConnected  2
#define kFinWait    3

#define kSynMaxRetry 3
#define kMaxConnection 1024

// Store segments in send buffer linked list
struct SegmentBuffer {
  std::shared_ptr<Segment> segment;
  std::chrono::milliseconds send_time;
};

typedef std::list<std::shared_ptr<SegmentBuffer>> SendBuffer;
typedef SendBuffer::iterator SendBufferIter;

// Client transport control block, the client side of a SRT connection
// uses this data structure to keep track of connection information
struct ClientTcb {
  unsigned int server_node_id;
  unsigned int server_port_num;
  
  unsigned int client_node_id;
  unsigned int client_port_num;

  unsigned int state;
  unsigned int next_seq_num;               // Next sequence number to be used by
                                           // new segment
  
  // [unacked][unsent]
  std::mutex send_lock;
  SendBufferIter unsent;  // First unsent segment
  SendBuffer send_buffer;

  std::mutex recv_lock;
  std::list<std::shared_ptr<Segment>> recv_buffer;

  ClientTcb(): 
    server_node_id(0), server_port_num(0), client_node_id(0),
    client_port_num(0), state(kClosed), next_seq_num(0), send_buffer(),
    unsent(send_buffer.end()) {}
};


static int overlay_conn;
std::atomic<bool> running;

std::mutex table_lock;
static std::vector<std::shared_ptr<ClientTcb>> table(kMaxConnection, nullptr);

static std::queue<std::shared_ptr<Segment>> send_queue;
static std::shared_ptr<std::thread> send_thread;

static std::shared_ptr<std::thread> recv_thread;

//const static std::chrono::duration<double, std::milli> kTimeoutsSleepTime(500);
std::chrono::milliseconds kTimeout(6000);   // 6 seconds

int SrtClientSock(unsigned int client_port) {
  table_lock.lock();
  for (int i = 0; i < kMaxConnection; ++i) {
    if (table[i] != nullptr) {
      table[i] = std::make_shared<ClientTcb>();
      table[i]->client_port_num = client_port;
      table_lock.unlock();
      return i;
    }
  }

  table_lock.unlock();
  return kFailure;
}

static std::shared_ptr<Segment> CreateSynSegment(
  std::shared_ptr<ClientTcb> tcb, unsigned int server_port) {
  std::shared_ptr<Segment> syn = std::make_shared<Segment>();

  syn->header.src_port = tcb->client_port_num;
  syn->header.dest_port = server_port;
  syn->header.seq_num = rand() % std::numeric_limits<unsigned int>::max();
  syn->header.ack_num = syn->header.seq_num + 1;
  syn->header.length = sizeof(Segment);
  syn->header.type = kSyn;
  syn->header.rcv_win = 0;  // TODO: Not used yet
  syn->header.checksum = Checksum(syn);

  return syn;
}

// Send SYN and waits for SYN_ACK
int SrtClientConnect(int sockfd, unsigned int server_port) {
  std::shared_ptr<ClientTcb> tcb = table[sockfd];
  if (tcb->state != kClosed) {
    return kFailure;
  }

  std::shared_ptr<Segment> syn = CreateSynSegment(tcb, server_port);
  unsigned int attempts = 0;
  while (true) {
    switch (tcb->state) {
      case kClosed:
        tcb->next_seq_num = syn->header.seq_num + 1;
        tcb->state = kSynSent;
        attempts = 1;

        SnpSendSegment(sockfd, syn);
        break;
      case kSynSent:
        if (attempts == kSynMaxRetry) {
          tcb->state = kClosed;
          return kFailure;
        }
      case kConnected:
          std::cerr << "Error: sockfd already connected" << std::endl;
          return kFailure;
      case kFinWait:
          std::cerr << "Error: sockfd already connected" << std::endl;
          return kFailure;
      default:
        break;
    }

    // Wait here for a segment
  }

  return kFailure;
}

int SrtClientDisconnect(int sockfd) {
  return 0;
}

int SrtClientSend(int sockfd, void *data, unsigned int length) {
  return 0;
}

static void NotifyShutdown() {
  running = false;
}

static void DispatchSegment(std::shared_ptr<Segment> seg) {

}

static void ProcessTimeouts(std::shared_ptr<ClientTcb> tcb) {
  for (SendBufferIter it = tcb->send_buffer.begin(); it != tcb->unsent; ++it) {
    std::shared_ptr<SegmentBuffer> seg_buf = *tcb->unsent;

    if (GetCurrentTime() - seg_buf->send_time > kTimeout) {
      SnpSendSegment(overlay_conn, seg_buf->segment);
      seg_buf->send_time = GetCurrentTime();
    }
  }
}

static void ProcessUnsent(std::shared_ptr<ClientTcb> tcb) {
  if (tcb->unsent != tcb->send_buffer.end()) {
    std::shared_ptr<SegmentBuffer> seg_buf = *tcb->unsent;

    SnpSendSegment(overlay_conn, seg_buf->segment);

    seg_buf->send_time = GetCurrentTime();
    ++tcb->unsent;
  }
}

static void SendThread() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      if (table[tcb_id] == nullptr) {
        continue;
      }
      std::shared_ptr<ClientTcb> tcb = table[tcb_id];
      std::lock_guard<std::mutex> lock(tcb->send_lock);

      ProcessTimeouts(tcb);
      ProcessUnsent(tcb);
    }
  }
}

static void Dispatch(std::shared_ptr<Segment> seg) {


}

static void RecvThread() {
  while (running) {
    std::shared_ptr<Segment> seg = SnpRecvSegment(overlay_conn);

    Dispatch(seg);
  }
}

void SrtClientInit(int conn) {
  overlay_conn = conn;
  running = true;

  recv_thread = std::make_shared<std::thread>(RecvThread);
  send_thread = std::make_shared<std::thread>(SendThread);
}

int SrtClientClose(int sockfd) {
  table[sockfd] = nullptr;
  return kSuccess;
}

void SrtClientShutdown() {
  NotifyShutdown();

  send_thread->join();
  recv_thread->join();
}
