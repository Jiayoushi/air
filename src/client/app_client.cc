#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "../common/constants.h"
#include "srt_client.h"

#define kClientPort1  87
#define kServerPort1  88
#define kClientPort2  89
#define kServerPort2  90

// After the connections are created, wait for 5 seconds and then close it
#define kWaitTime      5

int OverlayStart() {
  return 0;
}

void OverlayStop(int conn) {

}

int main() {
  srand(time(nullptr));

  int overlay_conn = OverlayStart();
  if (overlay_conn < 0) {
    std::cerr << "Failed to start overlay" << std::endl;
    exit(kFailure);
  }

  SrtClientInit(overlay_conn);

  // First connection
  int sockfd = SrtClientSock(kClientPort1);
  if (sockfd < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(kFailure);
  }
  if (SrtClientConnect(sockfd, kServerPort1) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(kFailure);
  }
  printf("Client port [%d] connects to server [%d]\n", kClientPort1, kServerPort1);

  // Second connection
  int sockfd2 = SrtClientSock(kClientPort2);
  if (sockfd2 < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(kFailure);
  }
  if (SrtClientConnect(sockfd2, kServerPort2) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(kFailure);
  }
  printf("Client port [%d] connects to server [%d]\n", kClientPort2, kServerPort2);

  // Sleep
  sleep(kWaitTime);

  // Close first connection
  if (SrtClientDisconnect(sockfd)<0) {
    std::cerr << "Failed to disconnect from srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  if (SrtClientClose(sockfd)<0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Close second connection
  if (SrtClientDisconnect(sockfd2)<0) {
    std::cerr << "Failed to disconnect from srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  if (SrtClientClose(sockfd2) < 0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }

  OverlayStop(overlay_conn);

  return 0;
}