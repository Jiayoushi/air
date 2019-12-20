#ifndef AIR_SRT_SERVER_OVERLAY_H_
#define AIR_SRT_SERVER_OVERLAY_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <iostream>


/*       
 *      my protocol
 * ------- Overlay ------
 *      tcp/ip
 */

int OverlayServerStart(const char *hostname, unsigned int hostport) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error: cannot create socket");
    exit(-1);
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
    exit(-1);
  }

  if (listen(sockfd, 5000) < 0) {
    perror("Error: listen failed");
    exit(-1);
  }

  socklen_t size = sizeof(server_addr);
  int overlay_fd = 0;
  if ((overlay_fd = accept(sockfd, (struct sockaddr *)&server_addr, &size)) < 0) {
    perror("Error: conncet");
    exit(-1);
  }

  return overlay_fd;
}

void OverlayServerStop(int conn) {
  close(conn);
}

#endif
