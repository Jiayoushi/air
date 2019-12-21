#include <iostream>
#include <fstream>

#include "server/srt_server.h"
#include "client/srt_client.h"
#include "server/srt_server_overlay.h"
#include "client/srt_client_overlay.h"

int test();

#define kClientPort  8000
#define kServerPort  8001

int main() {
  return test();
}

int test() {
  srand(time(nullptr));

  int conn = OverlayClientStart("127.0.0.1", kServerPort);
  if (conn < 0) {
    std::cerr << "overlay_start failed" << std::endl;
    exit(-1);
  }

  SrtClientInit(conn);

  // Create socket
  int sockfd = SrtClientSock(kClientPort);
  if (sockfd < 0) {
    std::cerr << "srt_client_sock failed" << std::endl;
    exit(-1);
  }
  
  // Connect
  if (SrtClientConnect(sockfd, kServerPort) < 0) {
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
  SrtClientSend(sockfd, &len, sizeof(size_t));
  std::cerr << "len sent: " << len << std::endl;
  SrtClientSend(sockfd, str.c_str(), len);
  std::cerr << "file sent." << std::endl;

  sleep(3);

  if(SrtClientDisconnect(sockfd) < 0) {
    std::cerr << "fail to disconnect from srt server" << std::endl;
    exit(1);
  }

  if(SrtClientClose(sockfd) < 0) {
    std::cerr << "fail to close srt client" << std::endl;
    exit(-1);
  }

  OverlayClientStop(conn);

  return 0;
}
