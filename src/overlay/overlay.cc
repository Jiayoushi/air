#include "overlay.h"

#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <unordered_map>

#include "ntable.h"
#include "ip/ip.h"

#define kWaitFirstStart       0
#define kWaitSecondStart      1
#define kWaitFirstEnd         2
#define kWaitSecondEnd        3


const uint16_t kOverlayPort = 65531;    /* Same for all hosts */

NeighborTable nt;

int listen_fd = -1;
int network_conn = -1;

static std::atomic<bool> running;

Ip kLocalIp;

static void AcceptNeighbors() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("Error: cannot create socket");
    exit(-1);
  }

  int optval = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
         (const void *)&optval , sizeof(int));

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(kLocalIp);
  server_addr.sin_port = htons(kOverlayPort);

  if (bind(listen_fd, (const struct sockaddr *)&server_addr, 
           sizeof(server_addr)) < 0) {
    perror("Error: bind failed");
    exit(-1);
  }

  if (listen(listen_fd, kMaxHostNum) < 0) {
    perror("Error: listen failed");
    exit(-1);
  }

  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    if (p->first < kLocalIp)
      continue;

    socklen_t size = sizeof(server_addr);
    int connfd = 0;
    if ((connfd = accept(listen_fd, (struct sockaddr *)&server_addr,
                             &size)) < 0) {
      perror("Error: accept");
      exit(-1);
    }

    nt.AddConnection(p->first, connfd);
  }
}

static int ConnectNeighbors() {
  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    if (p->first > kLocalIp)
      continue;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      perror("Error: cannot create socket");
      exit(-1);
    }
  
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    struct sockaddr_in neighbor_addr;
    memset(&neighbor_addr, 0, sizeof(neighbor_addr));
    neighbor_addr.sin_family = AF_INET;
    neighbor_addr.sin_port = htons(kOverlayPort);
    neighbor_addr.sin_addr.s_addr = p->first;
  
    if (connect(sockfd, (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) < 0) {
      std::cerr << "Error: connect failed" << std::endl;
      exit(-1);
    }

    nt.AddConnection(p->first, sockfd);
  }
 
  return 0; 
}

int OverlaySend(PktBufPtr pkt_buf) {
  const char *pkt_start = "!&";
  const char *pkt_end = "!#";

  int connection = nt[pkt_buf->next_hop].conn;

  if (send(connection, pkt_start, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_start");
    return -1;
  }

  if (send(connection, pkt_buf->packet.get(), 
           pkt_buf->packet->header.length, 0) < 0) {
    perror("[PKT] SendPacket failed to send packet");
    return -1;
  }

  if (send(connection, pkt_end, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_end");
    return -1;
  }

  return 0;
}

static PktPtr OverlayRecv(int conn) {
  char buf[65535];
  memset(buf, 0, 65535);

  uint32_t idx = 0;

  int state = kWaitFirstStart;
  char c;
  int recved = 0;
  while ((recved = recv(conn, &c, 1, 0)) > 0) {
    switch (state) {
      case kWaitFirstStart:   // Wait !
        if (c == '!')
          state = kWaitSecondStart;
        break;
      case kWaitSecondStart:  // Wait &
        if (c == '&') {
          state = kWaitFirstEnd;
        } else {
          state = kWaitFirstStart;
        }
        break;
      case kWaitFirstEnd:     // Wait !
        buf[idx++] = c;
        if (c == '!') {
          state = kWaitSecondEnd;
        }
        break;
      case kWaitSecondEnd:    // Wait #
        buf[idx++] = c;
        // "!#" is received, the segment receiving is complete
        if (c == '#') {
          PktPtr pkt = std::make_shared<Packet>();
 
          uint16_t pkt_len = idx - 2;
          memcpy(pkt.get(), buf, pkt_len);
          pkt->header.length = pkt_len;

          return pkt;
        } else if (c == '!') {
          // Previous '!' is not end control character
          // This one might be, so still wait for '#'
        } else {
          // Previous '!' is definitely not end control character
          // Go back to previous state to wait for first end control character
          state = kWaitFirstEnd;
        }
        break;
      default:
        break;
    }
  }

  return nullptr;
}

/*
 * Forward packets to the network layer
 */
static int Forward(PktBufPtr pkt_buf) {
  return IpInputQueuePush(pkt_buf);
}

/*
 * Receive packet from the other hosts and send them to the network
 * layer.
 */
static void Input(Ip ip) {
  int conn = nt[ip].conn;

  while (running) {
    PktPtr pkt = OverlayRecv(conn);

    if (!running)
      return;

    if (!pkt) {
      fprintf(stderr, "[Overlay] received null packet");
      continue;
    }

    PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();
    pkt_buf->packet = pkt;
    if (Forward(pkt_buf) < 0) {
      fprintf(stderr, "[Overlay] failed to deliver packet to network layer");
    }
  }
}

int OverlayStop() {
  running = false;

  for (int i = 0; i < nt.Size(); ++i) {
    if (close(nt[i].conn) < 0)
      return -1;
  }

  return 0;
}

int OverlayInit() {
  std::cout << "[OVERLAY]: Overlay layer starting ..." << std::endl;

  kLocalIp = GetLocalIp();
  struct in_addr ia;
  ia.s_addr = kLocalIp;
  std::cout << "[OVERLAY]: Local ip " << inet_ntoa(ia) << std::endl;

  nt.Init();
  std::cout << "Printing neighbors' ip addresses" << std::endl;
  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    struct in_addr a;
    a.s_addr = p->first;

    std::cout << inet_ntoa(a) << std::endl;
  }
 
  sleep(3);
 
  std::cout << "[OVERLAY]: connecting to other hosts" << std::endl;
  // Accept connections from neighbors with larger ip
  std::thread accept_neighbors(AcceptNeighbors);
  std::thread connect_neighbors(ConnectNeighbors);
  accept_neighbors.join();
  connect_neighbors.join();

  /* Connection to other hosts are established */
  std::cout << "[OVERLAY]: connections established" << std::endl;


  running = true;
  /* Keep receiving packets from neighbors */
  std::vector<std::thread> input_threads;
  for (int i = 0; i < nt.Size(); ++i) {
    input_threads.emplace_back(Input, i);
  }

  std::cout << "[OVERLAY]: overlay started" << std::endl;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(60)); 

  for (int i = 0; i < nt.Size(); ++i)
    input_threads[i].join();

  std::cout << "[OVERLAY]: exited." << std::endl;

  return 0;
}
