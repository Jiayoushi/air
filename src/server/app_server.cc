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

#define kClientPort1 87
#define kServerPort1 88
#define kClientPort2 90
#define kServerPort2 90

#define kWaitTime    10


int OverlayStart() {
  return 0;
}

void OverlayStop(int conn) {

}

int main() {
  srand(time(nullptr));

  int overlay_conn = OverlayStart();
  if (overlay_conn < 0) {
    std::cerr << "can not start overlay" << std::endl;
    exit(EXIT_FAILURE);
  }

  SrtServerInit(overlay_conn);

  // Open 1
  int sockfd = SrtServerSock(kServerPort1);
  if (sockfd < 0) {
    std::cerr << "can't create srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  SrtServerAccept(sockfd);

  // Open 2
  int sockfd2 = SrtServerSock(kServerPort2);
  if (sockfd2 < 0) {
    std::cerr << "can't create srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  SrtServerAccept(sockfd2);

  // Sleep
  sleep(kWaitTime);

  // Close 1
  if (SrtServerClose(sockfd) < 0) {
    std::cerr << "can't destroy srt server" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Close 2
  if (SrtServerClose(sockfd2) < 0) {
    std::cerr << "can't destroy srt server" << std::endl;
    exit(EXIT_FAILURE);
  }

  OverlayStop(overlay_conn);

  return 0;
}
