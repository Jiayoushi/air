#include "overlay.h"

#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <unordered_map>

#include "air/air.h"
#include "ntable.h"
#include "ip/ip.h"

#define kWaitFirstStart       0
#define kWaitSecondStart      1
#define kWaitFirstEnd         2
#define kWaitSecondEnd        3

const uint16_t kOverlayPort = 6553;    /* Same for all hosts */

NeighborTable nt;

static std::atomic<bool> running;

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
  server_addr.sin_addr.s_addr = INADDR_ANY;
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
    if (p->first <= GetLocalIp())
      continue;

    int connfd = 0;
    struct sockaddr_in client_addr;
    socklen_t size = sizeof(client_addr);
    if ((connfd = accept(listen_fd, (struct sockaddr *)&client_addr,
                             &size)) < 0) {
      perror("Error: accept");
      exit(-1);
    }

    std::cout << "[OVERLAY] accepted " << inet_ntoa(client_addr.sin_addr) << std::endl;

    nt.AddConnection(client_addr.sin_addr.s_addr, connfd);
  }

  std::cout << "[OVERLAY] accepting terminated" << std::endl;
}

static int ConnectNeighbors() {
  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    if (p->first >= GetLocalIp())
      continue;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      perror("[OVERLAY] cannot create socket");
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
 
    struct in_addr a;
    a.s_addr = p->first;

    if (connect(sockfd, (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) < 0) {
      perror("[OVERLAY] connect failed");
      exit(-1);
    }
    std::cout << "[OVERLAY] connected to " << inet_ntoa(a) << std::endl;

    nt.AddConnection(p->first, sockfd);
  }
 
  std::cout << "[OVERLAY] connection terminated" << std::endl;
  return 0; 
}

int OverlaySend(PktBufPtr pkt_buf) {
  const char *pkt_start = "!&";
  const char *pkt_end = "!#";

  int connection = nt[pkt_buf->next_hop].conn;

  if (send(connection, pkt_start, 2, 0) < 0) {
    perror("[OVERLAY] SendPacket failed to send pkt_start");
    return -1;
  }

  if (send(connection, pkt_buf->packet.get(), 
           pkt_buf->packet->header.length, 0) < 0) {
    perror("[OVERLAY] SendPacket failed to send packet");
    return -1;
  }

  if (send(connection, pkt_end, 2, 0) < 0) {
    perror("[OVERLAY] SendPacket failed to send pkt_end");
    return -1;
  }

  return 0;
}

static PktPtr OverlayRecv(int conn) {
  char buf[65535];
  memset(buf, 0, 65535);

  uint32_t idx = 0;

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

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
      sleep(1);
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

  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    if (close(p->second.conn) < 0)
      return -1;
  }

  return 0;
}

static void SigpipeHandler(int sig, siginfo_t *siginfo, void *context) {
  OverlayStop();
}

static int RegisterSigpipeHandler() {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = SigpipeHandler;
  act.sa_flags = SA_SIGINFO;

  if (sigaction(SIGPIPE, &act, nullptr) < 0) {
    perror("[OVERLAY] sigaction");
    return -1;
  }
  return 0;
}

std::vector<std::pair<Ip, Cost>> GetAllCost() {
  while (!running)
    sleep(1);

  std::vector<std::pair<Ip, Cost>> costs;

  for (auto p = nt.Begin(); p != nt.End(); ++p)
    costs.push_back({p->first, nt.GetCost(p->first, GetLocalIp())});

  return costs;
}

Cost GetCost(Ip from, Ip to) {
  return nt.GetCost(from, to);
}

int OverlayInit() {
  std::cout << "[OVERLAY] Overlay layer starting ..." << std::endl;

  std::cout << "[OVERLAY] local ip " << IpStr(GetLocalIp()) << std::endl;

  if (RegisterSigpipeHandler() < 0)
    exit(-1);

  nt.Init();


  std::cout << "[OVERLAY] accepting connections from other hosts" << std::endl;
  std::thread accept_neighbors(AcceptNeighbors);

  sleep(7);

  std::cout << "[OVERLAY] connecting to other hosts" << std::endl;
  std::thread connect_neighbors(ConnectNeighbors);

  accept_neighbors.join();
  connect_neighbors.join();

  /* Connection to other hosts are established */
  std::cout << "[OVERLAY] topology established" << std::endl;

  running = true;
  /* Keep receiving packets from neighbors */
  std::vector<std::thread> input_threads;
  for (auto p = nt.Begin(); p != nt.End(); ++p) {
    input_threads.emplace_back(Input, p->first);
  }

  RegisterInitSuccess();

  std::cout << "[OVERLAY] overlay started" << std::endl;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(2)); 

  for (int i = 0; i < nt.Size(); ++i)
    input_threads[i].join();

  std::cout << "[OVERLAY] exited." << std::endl;

  return 0;
}
