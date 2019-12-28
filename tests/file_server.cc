#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fstream>
#include <string>
#include <iostream>

#include "server/srt_server.h"
#include "air/air.h"
#include "ip/ip.h"

int test(const char *, const char *);

typedef in_addr_t Ip;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: ./a.out <server hostname> <server port>" << std::endl;
    return -1;
  }

  return test(argv[1], argv[2]);
}

int test(const char *server_hostname, const char *server_port) {
  Init();

  SrtServerInit();

  // Create socket
  int sockfd = SrtServerSock();
  if (sockfd < 0) {
    std::cerr << "srt_server_sock failed" << std::endl;
    exit(-1);
  }
  
  struct hostent *he = gethostbyname(server_hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  if (SrtServerBind(sockfd, server_addr.sin_addr.s_addr, std::stoi(server_port)) < 0) {
    std::cerr << "srt_server_bind failed" << std::endl;
    exit(-1);
  }

  // Connect to client
  if (SrtServerAccept(sockfd) < 0) {
    std::cerr << "srt_server_accept failed" << std::endl;
    exit(-1);
  }

  std::cerr << "Connected" << std::endl;

  std::ifstream ifs("tests/send_this.txt");
  if (!ifs.is_open()) {
    std::cerr << "ifstream failed to open" << std::endl;
    exit(-1);
  }

  if (!ifs.is_open()) {
    std::cerr << "ifstream failed to open" << std::endl;
    exit(-1);
  }
 
  std::string str((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());


  size_t expected_len = strlen(str.c_str());
  size_t len = 0;
  size_t recved = SrtServerRecv(sockfd, &len, sizeof(size_t));
  if (recved != sizeof(size_t)) {
    std::cerr << "Recv #1 failed, unexpected size, expected " << sizeof(size_t) << " got " << recved << std::endl;
    exit(-1);
  }
  if (len != expected_len) {
    std::cerr << "Recv #1 failed, unexpected content received, expected " << expected_len << " got " << len << std::endl;
    exit(-1);
  }

  char buf[5000];
  size_t recved2 = SrtServerRecv(sockfd, buf, expected_len);
  if (recved2 != expected_len) {
    std::cerr << "Recv #2 failed, unexpected length received, expected " << expected_len << " got " << recved2 << std::endl;
    exit(-1);
  }

  buf[recved2] = 0;
  if (strcmp(buf, str.c_str()) != 0) {
    std::cerr << "Recv #2 failed, unexpected content received, expected " << std::endl;
  } else {
    std::cerr << "Recv #2 succeed. correct file content." << std::endl;
  }

  sleep(10);

  if(SrtServerClose(sockfd) < 0) {
    std::cerr << "fail to close srt server" << std::endl;
    exit(-1);
  }
  std::cout << "[APP] closed" << std::endl;

  SrtServerShutdown();
  std::cout << "[APP] shutdown" << std::endl;

  Stop();
  std::cerr << "Server stopped" << std::endl;

  return 0;
}
