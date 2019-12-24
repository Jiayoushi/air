#include "network.h"

#include <thread>

int overlay_conn = -1;

std::atomic<bool> running = false;

static int ConnectOverlay() {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error: cannot create socket");
    exit(-1);
  }

  struct hostent *server;
  server = gethostbyname(hostname);
  if (server == NULL) {
    perror("Error: gethostbyname failed");
    exit(-1);
  }

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&server_addr.sin_addr.s_addr, server->h_length);
  server_addr.sin_port = htons(hostport);

  if (connect(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Error: connect failed" << std::endl;
    exit(-1);
  }

  return sockfd;
}

int IpStop() {
  running = false;
}

static void RouteUpdate() {
  while (running) {
    PktPtr pkt = std::make_shared<Packet>();
    pkt.dest_ip = kBroadcast;

    OverlaySendPacket(0, pkt);

    std::this_thread::sleep_for(kRouteUpdateInterval);
  }
}

static void Input() {
  while (running) {
    PktPtr pkt = OverlayRecvPacket(conn);

    if (!running)
      return;

    if (!pkt) {
      fprintf(stderr, "[IP] index [%d] received null packet");
      return 0;
    }

    // TODO: forward to transport layer
    std::cout << "[IP] received packet" << pkt << std::endl;
  }
}

int main() {
  std::cout << "[IP] network layer starting ..." << std::endl;

  overlay_conn = ConnectOverlay();
  if (overlay_conn < 0) {
    std::cerr << "[IP] connect overlay failed" << std::endl;
    exit(-1);
  }

  std::thread input = std::thread(Input);
  std::thread update = std::thread(RouteUpdate);

  std::cout << "[IP] network layer started" << std::endl;

  while (running)
    std::this_thread::sleep_for(60);

  update.join();
  input.join();

  return 0;
}
