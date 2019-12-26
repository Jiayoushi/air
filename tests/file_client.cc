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

void ConnectNetwork() {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error: cannot create socket");
    return -1;
  }

  struct sockaddr_in network_addr;
  bzero((char *)&network_addr, sizeof(network_addr));
  network_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&network_addr.sin_addr.s_addr, server->h_length);
  network_addr.sin_port = htons(hostport);

  return connect(sockfd, (const struct sockaddr *)&network_addr, sizeof(network_addr));
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

  SrtClientShutdown();
  std::cerr << "Client shutdown" << std::endl;

  OverlayClientStop(conn);
  std::cerr << "Overlay stopped" << std::endl;

  return 0;
}
