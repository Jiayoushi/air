#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "client/srt_client.h"
#include "air/air.h"

#define kClientPort1  8000
#define kServerPort1  8001
#define kClientPort2  8002
#define kServerPort2  8003

const char *hostname = "turtle.zoo.cs.yale.edu";

// After the connections are created, wait for 5 seconds and then close it
#define kWaitTime      1


int main() {
  Init();

  SrtClientInit();
  std::cerr << "Client init success" << std::endl;

  // First connection
  int sockfd = SrtClientSock();
  if (sockfd < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(-1);
  }
  std::cerr << "socket created: " << sockfd << std::endl;

  struct hostent *he = gethostbyname(hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  uint32_t server_ip = server_addr.sin_addr.s_addr;

  if (SrtClientConnect(sockfd, server_ip, kServerPort1) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(-1);
  }
  printf("Client port [%d] connects to server [%d]\n", kClientPort1, kServerPort1);

  // Second connection
  int sockfd2 = SrtClientSock();
  if (sockfd2 < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(-1);
  }

  if (SrtClientConnect(sockfd2, server_ip, kServerPort2) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(-1);
  }
  printf("Client port [%d] connects to server [%d]\n", kClientPort2, kServerPort2);

  // Sleep
  sleep(kWaitTime);

  std::cerr << "Attempt to close sockfd #1 " << std::endl;
  // Close first connection
  if (SrtClientDisconnect(sockfd) < 0) {
    std::cerr << "Failed to disconnect from srt server" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #1 closed " << std::endl;

  // Close second connection
  if (SrtClientClose(sockfd) < 0) {
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

  Stop();
  std::cout << "Stopped" << std::endl;

  return 0;
}
