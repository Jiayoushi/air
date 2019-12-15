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

#define kClientPort1 8000
#define kServerPort1 8001
#define kClientPort2 8002
#define kServerPort2 8003

#define kWaitTime    3

const char *hostname = "127.0.0.1";
unsigned int hostport = 8080;

int OverlayStart() {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error: cannot create socket");
    exit(kFailure);
  }

   /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  int optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  struct sockaddr_in server_addr;
  bzero((char *) &server_addr, sizeof(server_addr));
 
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  server_addr.sin_port = htons((unsigned short)hostport);

  if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Error: bind failed");
    exit(kFailure);
  }

  if (listen(sockfd, 5000) < 0) {
    perror("Error: listen failed");
    exit(kFailure);
  }

  socklen_t size = sizeof(server_addr);
  int overlay_fd = 0;
  if ((overlay_fd = accept(sockfd, (struct sockaddr *)&server_addr, &size)) < 0) {
    perror("Error: conncet");
    exit(kFailure);
  }

  return overlay_fd;
}

void OverlayStop(int conn) {
  close(conn);
}


int main() {
  srand(time(nullptr));

  int overlay_conn = OverlayStart();
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

  OverlayStop(overlay_conn);
  std::cerr << "overlay stop" << std::endl;

  return 0;
}
