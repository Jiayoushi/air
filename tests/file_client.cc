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

const char *hostname = "turtle.zoo.cs.yale.edu";

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
 
  uint16_t port = server_port;
  struct hostent *he = gethostbyname(hostname);
  struct sockaddr_in server_addr;
  memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  // Connect
  if (Connect(sockfd, server_addr.sin_addr.s_addr, port) < 0) {
    std::cerr << "Connect failed" << std::endl;
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

  size_t sent = Send(sockfd, &len, k);
  std::cerr << "Data 'len' sent: " << sent << std::endl;

  sent = Send(sockfd, str.c_str(), len);
  std::cerr << "Data 'file' sent: " << sent << std::endl;

  sleep(7);

  std::cout << "Closing..." << std::endl;
  if(Close(sockfd) < 0) {
    std::cerr << "fail to close srt client" << std::endl;
    exit(-1);
  }
  std::cerr << "Closed" << std::endl;

  Stop();
  std::cout << "Stopped" << std::endl;

  std::cerr << "Test success" << std::endl;
  return 0;
}
