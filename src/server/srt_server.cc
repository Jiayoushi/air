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

  unsigned int initial_seq;
  unsigned int client_seq;
  unsigned int server_seq;

  std::mutex lock;

  std::condition_variable waiting;

  char *recv_buffer;         // Reciving buffer
  unsigned int buffer_size;  // Size of the received data in received buffer

  ServerTcb(): server_id(0), server_port(0), client_id(0),
    client_port(0), state(kClosed),
    initial_seq(rand() % std::numeric_limits<unsigned int>::max()),
    client_seq(0), server_seq(initial_seq),
    lock(), recv_buffer(nullptr), buffer_size(0) {}
};

std::mutex table_lock;
static std::vector<std::shared_ptr<ServerTcb>> tcb_table(kMaxConnection, nullptr);


// A global variable control the running of all threads.
static std::atomic<bool> running;

std::shared_ptr<std::thread> input_thread;


// TODO
std::shared_ptr<ServerTcb> Demultiplex(std::shared_ptr<Segment> seg) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

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
  seg->header.seq = tcb->server_seq;
  seg->header.ack = input_seg->header.seq + 1;
  seg->header.length = sizeof(Segment);
  seg->header.type = kSynAck;
  seg->header.rcv_win = 0;
  seg->header.checksum = Checksum(seg, 0);

  return seg;
}

static std::shared_ptr<Segment> CreateFinAck(std::shared_ptr<ServerTcb> tcb, std::shared_ptr<Segment> input_seg) {
  std::shared_ptr<Segment> seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->server_port;
  seg->header.dest_port = tcb->client_port;
  seg->header.seq = tcb->server_seq;
  seg->header.ack = input_seg->header.seq + 1;
  seg->header.length = sizeof(Segment);
  seg->header.type = kFinAck;
  seg->header.rcv_win = 0;
  seg->header.checksum = Checksum(seg, 0);

  return seg;
}

static void SendSegment(std::shared_ptr<ServerTcb> tcb, enum SegmentType type, 
                        std::shared_ptr<Segment> input_seg) {
  std::shared_ptr<Segment> seg;

  switch (type) {
    case kSynAck:
      seg = CreateSynAck(tcb, input_seg);
      break;
    case kFinAck:
      seg = CreateFinAck(tcb, input_seg);
      break;
    default:
      SDEBUG << "SendSegment: unmatched segment type" << std::endl;
      assert(false);
  }
  
  SnpSendSegment(overlay_conn, seg);
  SDEBUG << "SENT: " << SegToString(seg) << std::endl;
}

static int Input(std::shared_ptr<Segment> seg) {
  if (!ValidChecksum(seg, 0)) {
    return -1;
  }

  std::shared_ptr<ServerTcb> tcb = Demultiplex(seg);
  if (tcb == nullptr)
    return -1;

  std::lock_guard<std::mutex> lck(tcb->lock);
  switch (tcb->state) {
    case kClosed:
      std::cerr << "Input: segment received when state is closed" << std::endl;
      return -1;

    case kListening:
      if (seg->header.type != kSyn)
        return -1;

      tcb->client_seq = seg->header.seq + 1;
      tcb->client_port = seg->header.src_port;
      tcb->state = kConnected;

      SendSegment(tcb, kSynAck, seg);
      tcb->waiting.notify_one();

      return 0;

    case kConnected:
      if (seg->header.type == kFin) {
        tcb->state = kCloseWait;

        tcb->server_seq += 1;
        tcb->client_seq = seg->header.seq + 1;
        //TODO: do we need to handle ack here?

        SendSegment(tcb, kFinAck, seg);

        return 0;
      } else if (seg->header.type == kSyn) {
        SendSegment(tcb, kSynAck, seg);
        return 0;
      }

      //if (seg->header.seq != tcb->client_seq)
      //  return -1;

      break;
 
    case kCloseWait:
    default:
     std::cerr << "Error: sockfd already connected" << std::endl;
     return -1;
  }

  return 0;
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

  return 0;
}

static void InputFromIp() {
  while (running) {
    std::shared_ptr<Segment> seg = SnpRecvSegment(overlay_conn);

    if (seg != nullptr) {
      SDEBUG << "RECV: " << SegToString(seg) << std::endl;
      Input(seg);
    }
  }
}

int SrtServerSock(unsigned int server_port) {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<ServerTcb>();
      tcb_table[i]->server_port = server_port;
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

int SrtServerRecv(int sockfd, void *buffer, unsigned int length) {
  return 0;
}
