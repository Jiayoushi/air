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

#include <air.h>

#define kServerPort1  8000
#define kServerPort2  8001

const char *hostname = "turtle.zoo.cs.yale.edu";

// After the connections are created, wait for 5 seconds and then close it
#define kWaitTime      1


int main() {
  Init();

  std::cerr << "Client init success" << std::endl;

  // First connection
  int sockfd = Sock();
  if (sockfd < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(-1);
  }
  std::cerr << "socket created: " << sockfd << std::endl;

  struct hostent *he = gethostbyname(hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  uint32_t server_ip = server_addr.sin_addr.s_addr;

  if (Connect(sockfd, server_ip, kServerPort1) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(-1);
  }
  printf("Client port #1 connects to server [%d]\n", kServerPort1);

  // Second connection
  int sockfd2 = Sock();
  if (sockfd2 < 0) {
    std::cerr << "Failed to create srt client sock" << std::endl;
    exit(-1);
  }

  if (Connect(sockfd2, server_ip, kServerPort2) < 0) {
    std::cerr << "Failed to connect to srt server" << std::endl;
    exit(-1);
  }
  printf("Client port #2 connects to server [%d]\n", kServerPort2);

  // Sleep
  sleep(kWaitTime);

  // Close second connection
  if (Close(sockfd) < 0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #1 closed " << std::endl;

  if (Close(sockfd2) < 0) {
    std::cerr << "Failed to close srt client" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cerr << "connection #2 closed" << std::endl;

  Stop();
  std::cout << "Stopped" << std::endl;

  return 0;
}
