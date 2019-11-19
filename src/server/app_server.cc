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

  if (listen(sockfd, 5) < 0) {
    perror("Error: listen failed");
    exit(kFailure);
  }

  return sockfd;
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
