#include "srt_server.h"

#include <iostream>
#include <memory>
#include <vector>
#include <condition_variable>

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
  
  std::mutex lock;

  std::condition_variable waiting;

  char *recv_buffer;         // Reciving buffer
  unsigned int buffer_size;  // Size of the received data in received buffer

  ServerTcb(): server_id(0), server_port(0), client_id(0),
    client_port(0), state(kClosed), expect_seq_num(0), lock(),
    recv_buffer(nullptr), buffer_size(0) {}
};


static std::vector<std::shared_ptr<ServerTcb>> tcb_table(kMaxConnection, nullptr);


// TODO
std::shared_ptr<ServerTcb> Demultiplex(std::shared_ptr<Segment> seg) {
  return tcb_table[0];
}

static int input(std::shared_ptr<Segment> seg) {
    std::shared_ptr<ServerTcb> tcb = Demultiplex(seg);

    switch (tcb->state) {
      case kClosed:
        std::cerr << "Input: segment received when state is closed" << std::endl;
        return kFailure;

      case kListening:
        return kSuccess;

      case kConnected:
      case kCloseWait:
      default:
        std::cerr << "Error: sockfd already connected" << std::endl;
        return kFailure;
    }
}

// TODO
static std::shared_ptr<Segment> CreateSynAck(std::shared_ptr<ServerTcb> tcb) {
  return nullptr;
}

int SrtServerAccept(int sockfd) {
  std::shared_ptr<ServerTcb> tcb = tcb_table[sockfd];

  std::shared_ptr<Segment> syn_ack = CreateSynAck(tcb);

  SnpSendSegment(overlay_conn, syn_ack);
  tcb->state = kListening;

  // Blocked until input notify that the state is k
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

void *SegmentHandler(void *arg) {
  return 0;
}

int SrtServerSock(unsigned int server_port) {
  for (int i = 0; i < kMaxConnection; ++i) {                                          
    if (tcb_table[i] != nullptr) {
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
}
