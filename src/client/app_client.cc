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
#include "srt_client_overlay.h"

#define kClientPort1  8000
#define kServerPort1  8001
#define kClientPort2  8002
#define kServerPort2  8003

const char *hostname = "127.0.0.1";
unsigned int hostport = 8080;

// After the connections are created, wait for 5 seconds and then close it
#define kWaitTime      1



int main() {
  srand(time(nullptr));

  int overlay_conn = OverlayClientStart(hostname, hostport);
  if (overlay_conn < 0) {
    std::cerr << "Failed to start overlay" << std::endl;
    exit(kFailure);
  }

  SrtClientInit(overlay_conn);
  std::cerr << "Client init success" << std::endl;

  // First connection
  int sockfd = SrtClientSock(kClientPort1);
  if (sockfd < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(kFailure);
  }
  std::cerr << "socket created: " << sockfd << std::endl;

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

  std::cerr << "Attempt to close sockfd #1 " << std::endl;
  // Close first connection
  if (SrtClientDisconnect(sockfd)<0) {
    std::cerr << "Failed to disconnect from srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #1 closed " << std::endl;

  // Close second connection
  if (SrtClientClose(sockfd)<0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #1 socket closed " << std::endl;

  // Close second connection
  if (SrtClientDisconnect(sockfd2)<0) {
    std::cerr << "Failed to disconnect from srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #2 closed" << std::endl;

  if (SrtClientClose(sockfd2) < 0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #2 socket closed" << std::endl;

  SrtClientShutdown();
  std::cerr << "shutdown " << std::endl;

  OverlayClientStop(overlay_conn);
  std::cerr << "overlay stop" << std::endl;

  return 0;
}
