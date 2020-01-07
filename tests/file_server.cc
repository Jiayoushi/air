#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fstream>
#include <string>
#include <iostream>

#include <air.h>

#define server_port 8000

const char *server_hostname = "turtle.zoo.cs.yale.edu";

typedef in_addr_t Ip;

int test();

int main(int argc, char *argv[]) {
  return test();
}

int test() {
  Init();

  // Create socket
  int sockfd = Sock();
  if (sockfd < 0) {
    std::cerr << "Sock failed" << std::endl;
    exit(-1);
  }
  
  struct hostent *he = gethostbyname(server_hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  if (Bind(sockfd, server_addr.sin_addr.s_addr, server_port) < 0) {
    std::cerr << "Bind failed" << std::endl;
    exit(-1);
  }

  // Connect to client
  if (Accept(sockfd) < 0) {
    std::cerr << "Accept failed" << std::endl;
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
  size_t recved = Recv(sockfd, &len, sizeof(size_t));
  if (recved != sizeof(size_t)) {
    std::cerr << "Recv #1 failed, unexpected size, expected " << sizeof(size_t) << " got " << recved << std::endl;
    exit(-1);
  }
  if (len != expected_len) {
    std::cerr << "Recv #1 failed, unexpected content received, expected " << expected_len << " got " << len << std::endl;
    exit(-1);
  }

  char buf[5000];
  size_t recved2 = Recv(sockfd, buf, expected_len);
  if (recved2 != expected_len) {
    std::cerr << "Recv #2 failed, unexpected length received, expected " << expected_len << " got " << recved2 << std::endl;
    exit(-1);
  } else {
    std::cout << "Recvd " << recved2 << " bytes" << std::endl;
  }

  buf[recved2] = 0;
  if (strcmp(buf, str.c_str()) != 0) {
    std::cerr << "Recv #2 failed, unexpected content received, expected " << std::endl;
  } else {
    std::cerr << "Recv #2 succeed. correct file content." << std::endl;
  }

  sleep(5);

  std::cout << "Closing..." << std::endl;
  if(Close(sockfd) < 0) {
    std::cerr << "fail to close srt server" << std::endl;
    exit(-1);
  }
  std::cout << "Closed" << std::endl;

  Stop();
  std::cout << "Stopped" << std::endl;

  std::cerr << "Test success" << std::endl;
  return 0;
}
