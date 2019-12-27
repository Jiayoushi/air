#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
  int sockfd, new_fd;
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  socklen_t size = sizeof(client_addr);

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    exit(-1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  
  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind failed");
    exit(-1);
  }

  if (listen(sockfd, 5)) {
    perror("listen failed");
    exit(-1);
  }

  if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &size)) < 0) {
    perror("accept failed");
    exit(-1);
  }

  while (1) {
    char buf[100];
    int recved = recv(new_fd, buf, 100, 0);
    if (recved < 0) {
      perror("recved failed");
    }

    if (send(new_fd, buf, recved, 0) < 0) {
      perror("send failed");
    }
  }

  return 0;
}
