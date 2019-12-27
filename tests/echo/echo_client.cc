#include <string.h>
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
  int sockfd, new_fd;
  struct sockaddr_in server_addr;
  socklen_t size;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    exit(-1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect failed");
    exit(-1);
  }

  std::vector<std::string> msgs = {"Hello!", "My name is", " Jack !"};
  
  for (const std::string &msg: msgs) {
    if (send(sockfd, msg.c_str(), msg.size(), 0) < 0) {
      perror("send failed");
    }

    char buf[100];
    int recved = recv(sockfd, buf, 100, 0);
    if (recved < 0) {
      perror("recved failed");
    }
    buf[recved] = 0;
    assert(strcmp(buf, msg.c_str()) == 0);
    std::cout << "echo back: [" << buf << "]" << std::endl;
  }

  return 0;
}
