#include "overlay.h"


struct OverlayPacket {
  Ip next_hop; 
  Packet pkt;
};

unsigned short kOverlayPort = 8080;

NeighborTable nt;

int listen_fd = -1;
int network_conn = -1;

static std::atomic<bool> running = false;

static void AcceptNeighbors() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("Error: cannot create socket");
    exit(-1);
  }

  int optval = 1;
  setsockopt(listen-fd, SOL_SOCKET, SO_REUSEADDR,
         (const void *)&optval , sizeof(int));

  struct sockaddr_in server_addr;
  bzero((char *) &server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(nt.GetLocalIp());
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

  for (int i = 0; i < nt.Size(); ++i) {
    if (nt[i].ip < nt.GetLocalIp())
      continue;

    socklen_t size = sizeof(server_addr);
    int conn_fd = 0;
    if ((conn_fd = accept(listen_fd, (struct sockaddr *)&server_addr,
                             &size)) < 0) {
      perror("Error: accept");
      exit(-1);
    }

    nt.AddConnection(nt[i]->ip, connfd);
  }

  return 0;
}

static int ConnectNeighbors() {
  for (int i = 0; i < nt.Size(); ++i) {
    if (nt[i].ip > nt.GetLocalIp())
      continue;

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

    nt.AddConnection(nt[i]->ip, sockfd);
  }
 
  return 0; 
}

int OverlaySend(PktPtr pkt) {
  const char *pkt_start = "!&";
  const char *pkt_end = "!#";

  int connection = 


  if (send(connection, pkt_start, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_start");
    return -1;
  }

  if (send(connection, pkt.get(), pkt->header.length, 0) < 0) {
    perror("[PKT] SendPacket failed to send packet");
    return -1;
  }

  if (send(connection, pkt_end, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_end");
    return -1;
  }

  return 0;
}

PktPtr OverlayRecvPacket(int conn) {
  return RecvPacket(conn);
}

/*
 * Forward packets to the network layer
 */
static int Forward(PktPtr pkt) {
  return SendPacket(pkt, network_conn);
}

/*
 * Receive packet from the other hosts and send them to the network
 * layer.
 */
static void Input(int index) {
  int conn = nt[index].ip;

  while (running) {
    PktPtr pkt = OverlayRecvPacket(conn);

    if (!running)
      return;

    if (!pkt) {
      fprintf(stderr, "[Overlay] index [%d] received null packet");
      return 0;
    }

    if (Forward(pkt) < 0) {
      fprintf(stderr, "[Overlay] failed to deliver packet to network layer");
    }
  }
}

int OverlayStop() {
  running = false;
  if (close(sockfd) < 0)
    return -1;

  return 0;
}

int OverlayInit() {
  std::cout << "Overlay layer starting ..." << std::endl;

  nt.Init();
  std::cout << "Printing neighbors' ip addresses" << std::endl;
  for (Ip ip: nt.GetNeighbors()) {
    struct in_addr a;
    a.s_addr = ip;
    std::cout << inet_ntoa(a) << std::endl;
  }
  
  std::cout << "[OVERLAY]: connecting to other hosts" << std::endl;
  // Accept connections from neighbors with larger ip
  std::thread accept_neighbors(AcceptNeighbors);
  std::thread connect_neighbors(ConnectNeighbors);
  accept_neighbors.join();
  connect_neighbors.join();

  /* Connection to other hosts are established */
  std::cout << "[OVERLAY]: connections established" << std::endl;

  /* Keep receiving packets from neighbors */
  std::vector<std::thread> input_threads;
  for (int i = 0; i < nt.Size(); ++i) {
    input_threads.emplace_back(Input, i);
  }

  std::cout << "[OVERLAY]: overlay started" << std::endl;
  std::cout << "[OVERLAY]: waiting for connections from network layer"
            << std::endl;

  while (running)
    std::this_thread::sleep_for(60); 

  for (int i = 0; i < nt.Size(); ++i)
    input_threads[i].join();

  std::cout << "[OVERLAY]: exited." << std::endl;

  return 0;
}
