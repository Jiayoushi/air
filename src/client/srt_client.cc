#include "srt_client.h"

#include <stdlib.h>
#include <memory>
#include <vector>

#define kMaxConnection 1024

static int overlay_conn;

static std::vector<std::shared_ptr<ClientTcb>> table(kMaxConnection, nullptr);

void SrtClientInit(int conn) {
  overlay_conn = conn;
}

int SrtClientSock(unsigned int client_port) {
  for (int i = 0; i < kMaxConnection; ++i) {
    if (table[i] != nullptr) {
      table[i] = std::make_shared<ClientTcb>();
      table[i]->client_port_num = client_port;
      return i;
    }
  }

  return -1;
}

int SrtClientClose(int sockfd) {
  table[sockfd] = nullptr;
  return kSuccess;
}

static void CreateSynSegment(std::shared_ptr<ClientTcb> tcb, 
  Segment &syn, unsigned int server_port) {
  syn.header.src_port = tcb->client_port_num;
  syn.header.dest_port = server_port;
  syn.header.seq_num = rand() % std::numeric_limits<unsigned int>::max();
  syn.header.ack_num = syn.header.seq_num + 1;
  syn.header.length = sizeof(Segment);
  syn.header.type = kSyn;
  syn.header.rcv_win = 0;  // Not used
  syn.header.checksum = Checksum(syn);
}

// Send SYN and waits for SYN_ACK
int SrtClientConnect(int sockfd, unsigned int server_port) {
  std::shared_ptr<ClientTcb> tcb = table[sockfd];
  if (tcb->state != kClosed) {
    return -1;
  }

  Segment syn;
  CreateSynSegment(tcb, syn, server_port);
  while (tcb->state != kConnected) {
    switch (tcb->state) {
      case kClosed:
        tcb->next_seq_num = syn.header.seq_num + 1;
        tcb->unacked = 1;
        tcb->state = kSynSent;
        SnpSendSegment(sockfd, syn);
      case kSynSent:

      default:
        break;
    }
  }

  return kSuccess;
}

int SrtClientDisconnect(int sockfd) {
  return 0;
}

int SrtClientSend(int sockfd, void *data, unsigned int length) {
  return 0;
}

void *SegmentHandler(void *arg) {
  return 0;
}
void *SendBufferTimer(void *client_tcb) {
  return 0;
}
