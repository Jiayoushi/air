#include "srt_client.h"

#include <stdlib.h>
#include <thread>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>

#define kMaxConnection 1024

const static std::chrono::duration<double, std::milli> kTimeoutsSleepTime(500);

static int overlay_conn;

static std::vector<std::shared_ptr<ClientTcb>> table(kMaxConnection, nullptr);

std::atomic<bool> running;
static std::shared_ptr<std::thread> main_thread;
static std::shared_ptr<std::thread> timer_thread;

int SrtClientSock(unsigned int client_port) {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (table[i] != nullptr) {
      table[i] = std::make_shared<ClientTcb>();
      table[i]->client_port_num = client_port;
      return i;
    }
  }

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
  syn->header.rcv_win = 0;  // Not used
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
  while (true) {
    switch (tcb->state) {
      case kClosed:
        tcb->next_seq_num = syn->header.seq_num + 1;
        tcb->unacked = 1;
        tcb->state = kSynSent;

        SnpSendSegment(sockfd, syn);
        break;
      case kSynSent:
        if (tcb->unacked == kSynMaxRetry) {
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

static void HandleSegment(std::shared_ptr<Segment> seg) {

}

// Handles all incoming segments
static void MainThread() {
  while (running) {
    std::shared_ptr<Segment> seg = SnpRecvSegment(overlay_conn);

    HandleSegment(seg);
  }
}

// Periodically called by Timer thread to check timeouts
static void Timeouts() {

}

static void TimerThread() {
  while (running) {
    Timeouts();

    // Go to sleep
    std::this_thread::sleep_for(kTimeoutsSleepTime);
  }
}

void SrtClientInit(int conn) {
  overlay_conn = conn;
  running = true;
  main_thread = std::make_shared<std::thread>(MainThread);
  timer_thread = std::make_shared<std::thread>(Timeouts);
}

int SrtClientClose(int sockfd) {
  NotifyShutdown();

  timer_thread->join();
  main_thread->join();
  table[sockfd] = nullptr;

  return kSuccess;
}


