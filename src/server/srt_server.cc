#include "srt_server.h"

#include <memory>
#include <vector>

#define kMaxConnection 1024

static int overlay_conn;

static std::vector<std::shared_ptr<ServerTcb>> table(kMaxConnection, nullptr);

void SrtServerInit(int conn) {
  overlay_conn = conn;
}

int SrtServerSock(unsigned int port) {
  for (int i = 0; i < kMaxConnection; ++i) {                                          
    if (table[i] != nullptr) {
      table[i] = std::make_shared<ServerTcb>();                                       
      return i;
    }
  }                                        
                                                                                      
  return -1; 
}

int SrtServerClose(int sockfd) {
  table[sockfd] = nullptr;
  return kSuccess;
}

int SrtServerAccept(int sockfd) {
  return 0;
}

int SrtServerRecv(int sockfd, void *buffer, unsigned int length) {
  return 0;
}

void *SegmentHandler(void *arg) {
  return 0;
}
