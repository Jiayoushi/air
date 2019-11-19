#include "srt_client.h"

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
      return i;
    }
  }

  return -1;
}

int SrtClientClose(int sockfd) {
  table[sockfd] = nullptr;
  return kSuccess;
}

int SrtClientConnect(int sockfd, unsigned int server_port) {
  

  return 0;
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
