#include "network.h"

#include <thread>

std::atomic<bool> running = false;

static int overlay_conn = -1
static int transport_conn = -1;
Ip local_ip = 0;

BlockingQueue<PacketBuffer> ip_input;

int IpSend(SegBufPtr seg_buf) {
  PakBufPtr pkt_buf = std::make_shared<PacketBuffer>();

  pkt_buf->pkt.header.src_ip = local_ip;
  pkt_buf->pkt.header.dest_ip = seg_buf.ip;
  pkt_buf->pkt.length = sizeof(PacketHeader) + sizeof(SegmentHeader)
                        + seg_buf->data_size;

  // TODO:
  pkt_buf->next_hop = 0;

  OverlaySend(pkt_buf);
}

int IpStop() {
  running = false;
}

static void RouteUpdate() {
  while (running) {
    PktPtr pkt = std::make_shared<Packet>();
    pkt.dest_ip = kBroadcast;

    OverlaySendPacket(pkt);

    std::this_thread::sleep_for(kRouteUpdateInterval);
  }
}

int IpInputQueuePush(PktBufPtr pkt_buf) {
  ip_input.Push(pkt_buf);
  return 0;
}

PktBufPtr IpInputQueuePop() {
  return ip_input.Pop();
}

static int Forward(PktBufPtr pkt_buf) {
  SegBufPtr seg_buf = std::make_shared<SegmentBuffer>();
  seg_buf->src_ip = pkt_buf->packet->src_ip;
  seg_buf->dest_ip = pkt_buf->packet->dest_ip;
  seg_buf->data_size = pkt_buf->length - sizeof(PacketHeader)
                       - sizeof(SegmentHeader);
  seg_buf->segment = std::make_shared<Segment>();
  memcpy(seg_buf->segment.get(), pkt_buf->data + sizeof(PacketHeader), 
         pkt_buf->length - sizeof(PacketHeader));

  TcpInputQueuePush(seg_buf);
}

static void Input() {
  while (running) {
    PktBufPtr pkt_buf = RecvPacket();

    if (!running)
      return;

    if (!pkt) {
      fprintf(stderr, "[IP] index [%d] received null packet");
      return 0;
    }

    ForwardPacket(pkt_buf);
    std::cout << "[IP] received packet" << pkt << std::endl;
  }
}

Ip GetLocalIp() {
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) < 0) {
    perror("getifaddres failed");
    return -1;
  }

  for (struct ifaddrs *ifa = ifaddr; ifa != nullptr;
       ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr)
      continue;

    int family = ifa->ifa_addr->sa_family;

    if (family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
      if (addr->sin_addr.s_addr != 16777343) {
        local_.ip = addr->sin_addr.s_addr;
        return local_.ip;
      }
    }
  }

  local_ip = 0;
  return 0;
}

int IpInit() {
  std::cout << "[IP] network layer starting ..." << std::endl;

  std::thread input = std::thread(Input);
  std::thread update = std::thread(RouteUpdate);

  std::cout << "[IP] network layer started" << std::endl;

  while (running)
    std::this_thread::sleep_for(60);

  update.join();
  input.join();

  OverlayStop();

  return 0;
}
