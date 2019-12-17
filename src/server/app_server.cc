#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <iostream>

#include "../common/constants.h"
#include "srt_server.h"
#include "srt_server_overlay.h"

#define kClientPort1 8000
#define kServerPort1 8001
#define kClientPort2 8002
#define kServerPort2 8003

#define kWaitTime    3

const char *hostname = "127.0.0.1";
unsigned int hostport = 8080;

int main() {
  srand(time(nullptr));

  int overlay_conn = OverlayServerStart(hostname, hostport);
  if (overlay_conn < 0) {
    std::cerr << "can not start overlay" << std::endl;
    exit(EXIT_FAILURE);
  }

  SrtServerInit(overlay_conn);
  std::cerr << "Server Init success" << std::endl;

  // Open 1
  int sockfd = SrtServerSock(kServerPort1);
  if (sockfd < 0) {
    std::cerr << "can't create srt socket1" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "socket created: " << sockfd << std::endl;

  SrtServerAccept(sockfd);
  std::cerr << "Accept #1 succeed" << std::endl;

  // Open 2
  int sockfd2 = SrtServerSock(kServerPort2);
  if (sockfd2 < 0) {
    std::cerr << "can't create srt socket2" << std::endl;
    exit(EXIT_FAILURE);
  }
  SrtServerAccept(sockfd2);
  std::cerr << "Accept #1 succeed" << std::endl;

  // Sleep
  sleep(kWaitTime);

  std::cerr << "Attempt to close connecion #1" << std::endl;
  // Close 1
  if (SrtServerClose(sockfd) < 0) {
    std::cerr << "can't destroy srt socket" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "Connection #1 closed" << std::endl;

  // Close 2
  if (SrtServerClose(sockfd2) < 0) {
    std::cerr << "can't destroy srt socket" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "Connection #2 closed" << std::endl;

  SrtServerShutdown();
  std::cerr << "shutdown " << std::endl;

  OverlayServerStop(overlay_conn);
  std::cerr << "overlay stop" << std::endl;

  return 0;
}
