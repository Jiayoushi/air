#include <iostream>
#include <fstream>

#include "server/srt_server.h"
#include "server/srt_server.h"
#include "server/srt_server_overlay.h"
#include "server/srt_server_overlay.h"

int test();

#define kClientPort  8000
#define kServerPort  8001

int main() {
  return test();
}


int test() {
  srand(time(nullptr));

  Init();

  /* Connect to network layer */
  int conn = OverlayClientStart("127.0.0.1", kServerPort);
  if (conn < 0) {
    std::cerr << "overlay_start failed" << std::endl;
    exit(-1);
  }

  SrtServerInit(conn);

  // Create socket
  int sockfd = SrtServerSock(kServerPort);
  if (sockfd < 0) {
    std::cerr << "srt_server_sock failed" << std::endl;
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

  Stop();

  std::cerr << "Server exits" << std::endl;

  return 0;
}
