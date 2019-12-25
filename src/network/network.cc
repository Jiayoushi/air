#include "network.h"

#include <thread>

int transport_conn = -1;
int overlay_conn = -1;

std::atomic<bool> running = false;

static int ConnectOverlay() {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("[NETWORK]: cannot create socket");
    exit(-1);
  }

  struct hostent *server;
  server = gethostbyname(hostname);
  if (server == NULL) {
    perror("[NETWORK]: gethostbyname failed");
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
    std::cerr << "[NETWORK]: connect failed" << std::endl;
    exit(-1);
  }

  return sockfd;
}

static int AcceptTranposrtLayer() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("[NETWORK]: cannot create socket");
    exit(-1);
  }

  int optval = 1;
  setsockopt(listen-fd, SOL_SOCKET, SO_REUSEADDR,
         (const void *)&optval , sizeof(int));

  struct sockaddr_in server_addr;
  bzero((char *) &server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(nt.GetLocalNetwork());
  server_addr.sin_port = htons(kOverlayPort);

  if (bind(listen_fd, (const struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("[NETWORK]: bind failed");
    exit(-1);
  }

  if (listen(listen_fd, kMaxHostNum) < 0) {
    perror("[NETWORK]: listen failed");
    exit(-1);
  }

  socklen_t size = sizeof(server_addr);
  int conn_fd = 0;
  if ((conn_fd = accept(listen_fd, (struct sockaddr *)&server_addr,
                        &size)) < 0) {
    perror("[NETWORK]: accept");
    exit(-1);
  }

  return conn_fd;
}

int NetworkStop() {
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
      fprintf(stderr, "[NETWORK] index [%d] received null packet");
      return 0;
    }

    // TODO: forward to transport layer
    std::cout << "[NETWORK] received packet" << pkt << std::endl;
  }
}

int main() {
  std::cout << "[NETWORK] network layer starting ..." << std::endl;

  /* Connect to overlay */
  if ((overlay_conn = ConnectOverlay()) < 0) {
    std::cerr << "[NETWORK] connect overlay failed" << std::endl;
    exit(-1);
  }
  std::cout << "[NETWORK] overlay connected" << std::endl;

  /* Accept connection from transport layer above */
  if ((transport_conn = AcceptTransport()) < 0) {
    std::cerr << "[NETWORK] accept transport failed" << std::endl;
    exit(-1);
  }
  std::cout << "[NETWORK] transport connected" << std::endl;

  std::thread input = std::thread(Input);
  std::thread update = std::thread(RouteUpdate);

  std::cout << "[NETWORK] network layer started" << std::endl;

  while (running)
    std::this_thread::sleep_for(60);

  update.join();
  input.join();

  OverlayStop();

  return 0;
}
