#include "srt_client.h"

void SrtClientInit(int conn) {}
int SrtClientSock(unsigned int client_port) {
  return 0;
}
int SrtClientConnect(int sockfd, unsigned int server_port) {
  return 0;
}

int SrtClientSend(int sockfd, void *data, unsigned int length) {
  return 0;
}

int SrtClientDisconnect(int sockfd) {
  return 0;
}
int SrtClientClose(int sockfd) {
  return 0;
}

void *SegmentHandler(void *arg) {
  return 0;
}
void *SendBufferTimer(void *client_tcb) {
  return 0;
}
