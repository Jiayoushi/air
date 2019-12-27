#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fstream>
#include <string>
#include <iostream>

#include "client/srt_client.h"
#include "air/air.h"
#include "ip/ip.h"

int test(const char *, const char *);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: ./a.out <server hostname> <server port>" << std::endl;
    return -1;
  }

  return test(argv[1], argv[2]);
}

int test(const char *server_hostname, const char *server_port) {
  Init();

  // Create socket
  int sockfd = SrtClientSock();
  if (sockfd < 0) {
    std::cerr << "srt_client_sock failed" << std::endl;
    exit(-1);
  }
 
  uint16_t port = atoi(server_port);
  struct hostent *he = gethostbyname(server_hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  // Connect
  if (SrtClientConnect(sockfd, server_addr.sin_addr.s_addr, port) < 0) {
    std::cerr << "srt_client_connect failed" << std::endl;
    exit(-1);
  }

  std::cerr << "Connected" << std::endl;


  std::ifstream ifs("tests/send_this.txt");
  if (!ifs.is_open()) {
    std::cerr << "ifstream failed to open" << std::endl;
    exit(-1);
  }
  
  std::string str((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());

  size_t len = strlen(str.c_str());
  uint32_t k = sizeof(size_t);

  SrtClientSend(sockfd, &len, k);

  std::cerr << "len sent: " << len << std::endl;
  SrtClientSend(sockfd, str.c_str(), len);
  std::cerr << "file sent." << std::endl;

  sleep(3);

  if(SrtClientDisconnect(sockfd) < 0) {
    std::cerr << "fail to disconnect from srt server" << std::endl;
    exit(1);
  }
  std::cerr << "Disconnected" << std::endl;

  if(SrtClientClose(sockfd) < 0) {
    std::cerr << "fail to close srt client" << std::endl;
    exit(-1);
  }
  std::cerr << "Client closed" << std::endl;

  Stop();
  std::cerr << "Client stopped" << std::endl;

  return 0;
}
