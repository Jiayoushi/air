#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
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

#include "server/srt_server.h"
#include "air/air.h"

#define kClientPort1 8000
#define kServerPort1 8001
#define kClientPort2 8002
#define kServerPort2 8003

#define kWaitTime    10

const char *hostname = "turtle.zoo.cs.yale.edu";

int main() {
  Init();

  SrtServerInit();
  std::cerr << "Server Init success" << std::endl;

  // Open 1
  int sockfd = SrtServerSock();
  if (sockfd < 0) {
    std::cerr << "can't create srt socket1" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "socket created: " << sockfd << std::endl;

  struct hostent *he = gethostbyname(hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  uint32_t server_ip = server_addr.sin_addr.s_addr;

  if (SrtServerBind(sockfd, server_ip, kServerPort1) < 0) {
    std::cerr << "srt_server_bind #1 failed" << std::endl;
    exit(-1);
  }

  if (SrtServerAccept(sockfd) < 0) {
    std::cerr << "srt_server accept #1 failed" << std::endl;
    exit(-1);
  }
  std::cerr << "Accept #1 succeed" << std::endl;

  // Open 2
  int sockfd2 = SrtServerSock();
  if (sockfd2 < 0) {
    std::cerr << "can't create srt socket2" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (SrtServerBind(sockfd2, server_ip, kServerPort2) < 0) {
    std::cerr << "srt_server_bind #2 failed" << std::endl;
    exit(-1);
  }

  if (SrtServerAccept(sockfd2) < 0) {
    std::cerr << "srt_server accept #2 failed" << std::endl;
    exit(-1);
  }
  std::cerr << "Accept #2 succeed" << std::endl;

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

  Stop();
  std::cout << "Stopped" << std::endl;

  return 0;
}
