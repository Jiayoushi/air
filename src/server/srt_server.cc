#include "srt_server.h"

#include <iostream>
#include <memory>
#include <vector>
#include <atomic>
#include <condition_variable>

#include "../common/common.h"

#define kClosed        1
#define kListening     2
#define kConnected     3
#define kCloseWait     4

#define kMaxConnection 1024

static int overlay_conn;


// Server transport control block.
struct ServerTcb {
  unsigned int server_id;
  unsigned int server_port;

  unsigned int client_id;
  unsigned int client_port;

  unsigned int state;

  unsigned int expect_seq_num;
  unsigned int initial_seq_num;

  std::mutex lock;

  std::condition_variable waiting;

  char *recv_buffer;         // Reciving buffer
  unsigned int buffer_size;  // Size of the received data in received buffer

  ServerTcb(): server_id(0), server_port(0), client_id(0),
    client_port(0), state(kClosed), expect_seq_num(0),
    initial_seq_num(rand() % std::numeric_limits<unsigned int>::max()),
    lock(), recv_buffer(nullptr), buffer_size(0) {}
};

std::mutex table_lock;
static std::vector<std::shared_ptr<ServerTcb>> tcb_table(kMaxConnection, nullptr);


// A global variable control the running of all threads.
std::atomic<bool> running;

std::shared_ptr<std::thread> input_thread;


// TODO
std::shared_ptr<ServerTcb> Demultiplex(std::shared_ptr<Segment> seg) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

    SDEBUG << "CHECK id " << tcb_id << std::endl;
    if ((seg->header.type == kSyn && seg->header.dest_port == tcb_table[tcb_id]->server_port) ||
        (seg->header.src_port == tcb_table[tcb_id]->client_port && seg->header.dest_port == tcb_table[tcb_id]->server_port))
    return tcb_table[tcb_id];
  }

  return nullptr;
}


static std::shared_ptr<Segment> CreateSynAck(std::shared_ptr<ServerTcb> tcb, std::shared_ptr<Segment> input_seg) {
  std::shared_ptr<Segment> seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->server_port;
  seg->header.dest_port = tcb->client_port;
  seg->header.seq_num = tcb->initial_seq_num;
  seg->header.ack_num = input_seg->header.seq_num + 1;
  seg->header.length = sizeof(Segment);
  seg->header.type = kSynAck;
  seg->header.rcv_win = 0;
  seg->header.checksum = Checksum(seg);

  return seg;
}

static void SendSegment(std::shared_ptr<ServerTcb> tcb, enum SegmentType type, std::shared_ptr<Segment> input_seg) {
  std::shared_ptr<Segment> seg;

  switch (type) {
    case kSynAck:
      seg = CreateSynAck(tcb, input_seg);
      break;
    default:
      SDEBUG << "SendSegment: unmatched segment type" << std::endl;
      assert(false);
  }
  
  SnpSendSegment(overlay_conn, seg);
}

static int Input(std::shared_ptr<Segment> seg) {
    std::shared_ptr<ServerTcb> tcb = Demultiplex(seg);

    switch (tcb->state) {
      case kClosed:
        std::cerr << "Input: segment received when state is closed" << std::endl;
        return kFailure;

      case kListening:
        if (seg->header.type != kSyn)
          return kFailure;

        tcb->state = kConnected;
        tcb->expect_seq_num = seg->header.seq_num + 1;
        tcb->client_port = seg->header.src_port;
        SendSegment(tcb, kSynAck, seg);
        tcb->waiting.notify_one();

        return kSuccess;

      case kConnected:
      case kCloseWait:
      default:
        std::cerr << "Error: sockfd already connected" << std::endl;
        return kFailure;
    }
}


int SrtServerAccept(int sockfd) {
  std::shared_ptr<ServerTcb> tcb = tcb_table[sockfd];
  tcb->state = kListening;

  // Blocked until input notify that the state is connected
  std::unique_lock<std::mutex> lk(tcb->lock);
  tcb->waiting.wait(lk, 
    [&tcb] {
      return tcb->state == kConnected; 
    });

  return kSuccess;
}

int SrtServerRecv(int sockfd, void *buffer, unsigned int length) {
  return 0;
}


static void InputFromIp() {
  while (running) {
    //SDEBUG << "waiting for an incoming segment" << std::endl;
    std::shared_ptr<Segment> seg = SnpRecvSegment(overlay_conn);

    if (seg != nullptr) {
      Input(seg);
    } else {
      // The whole IP stack is down
      exit(kSuccess);
    }
  }
}

int SrtServerSock(unsigned int server_port) {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<ServerTcb>();
      tcb_table[i]->server_port = server_port;
      tcb_table[i]->state = kListening;    // Combine the 'bind' and 'sockfd' into one
      return i;
    }
  }

  return -1; 
}

int SrtServerClose(int sockfd) {
  tcb_table[sockfd] = nullptr;
  return kSuccess;
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
