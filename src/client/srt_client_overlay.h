#ifndef AIR_CLIENT_OVERLAY_H_
#define AIR_CLIENT_OVERLAY_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <iostream>

int OverlayClientStart(const char *hostname, unsigned int hostport) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error: cannot create socket");
    exit(-1);
  }
  
  struct hostent *server;
  server = gethostbyname(hostname);
  if (server == NULL) {
    perror("Error: gethostbyname failed");
    exit(-1);
  }

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&server_addr.sin_addr.s_addr, server->h_length);
  server_addr.sin_port = htons(hostport);
  
  if (connect(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Error: connect failed" << std::endl;
    exit(-1);
  }
  
  return sockfd;
}

void OverlayClientStop(int conn) {
  close(conn);
}



#endif
