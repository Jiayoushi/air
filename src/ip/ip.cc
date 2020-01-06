#include "ip.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <cstring>
#include <sstream>
#include <atomic>
#include <thread>

#include <common.h>
#include <seg.h>
#include <pkt.h>
#include <blocking_queue.h>
#include <tcp.h>
#include <ip_dvt.h>
#include <overlay.h>

std::atomic<bool> ip_running;
static std::atomic<bool> running;

BlockingQueue<PktBufPtr> ip_input;
Dvt dvt;

std::mutex rt_lock;
std::unordered_map<Ip, Ip> rtable;

int IpSend(SegBufPtr seg_buf) {
  PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();

  pkt_buf->packet = std::make_shared<Packet>();
  pkt_buf->packet->header.type = kSnp;
  pkt_buf->packet->header.src_ip = seg_buf->src_ip;
  pkt_buf->packet->header.dest_ip = seg_buf->dest_ip;
  pkt_buf->packet->header.length = sizeof(PacketHeader) + 
                                   sizeof(SegmentHeader) +
                                   seg_buf->data_size;

  memcpy(pkt_buf->packet->data, seg_buf->segment.get(), seg_buf->data_size + sizeof(SegmentHeader));

  std::lock_guard<std::mutex> lock(rt_lock);
  auto p = rtable.find(seg_buf->dest_ip);
  if (p == rtable.end()) {
    std::cout << "[IP] route to " << seg_buf->dest_ip << " not found " << std::endl;
    return -1;
  }

  pkt_buf->next_hop = p->second;

  //std::cout << "[IP] Send packet " << pkt_buf << std::endl;
  OverlaySend(pkt_buf);

  return 0;
}

int IpRelay(PktBufPtr pkt_buf) {
  std::lock_guard<std::mutex> lock(rt_lock);
  auto p = rtable.find(pkt_buf->packet->header.dest_ip);
  if (p == rtable.end())
    return -1;

  pkt_buf->next_hop = p->second;

  OverlaySend(pkt_buf);
  std::cout << "[IP] Relay packet " << pkt_buf << std::endl;

  return 0;
}

static void BroadcastOnce(PktBufPtr pkt_buf) {
  std::unordered_set<Ip> next_hop;

  for (auto p = rtable.begin(); p != rtable.end(); ++p) {
    if (next_hop.find(p->second) != next_hop.end() || p->second == kInvalidIp)
      continue;

    next_hop.insert(p->second);
    pkt_buf->next_hop = p->second;
    OverlaySend(pkt_buf);
  }
}

static void  BroadcastInitalRouteInfo() {
  PktBufPtr dv_pkt = dvt.CreatePacket();
  BroadcastOnce(dv_pkt);
}

int IpInputQueuePush(PktBufPtr pkt_buf) {
  ip_input.Push(pkt_buf);
  return 0;
}

PktBufPtr IpInputQueuePop() {
  auto p = ip_input.Pop(std::chrono::duration<double, std::deci>(1));
  if (!p)
    return nullptr;

  return *p.get();
}

static int Forward(PktBufPtr pkt_buf) {
  SegBufPtr seg_buf = std::make_shared<SegmentBuffer>();
  seg_buf->src_ip = pkt_buf->packet->header.src_ip;
  seg_buf->dest_ip = pkt_buf->packet->header.dest_ip;
  seg_buf->data_size = pkt_buf->packet->header.length - 
                       sizeof(PacketHeader) - sizeof(SegmentHeader);

  seg_buf->segment = std::make_shared<Segment>();
  memcpy(seg_buf->segment.get(), 
         pkt_buf->packet->data, 
         pkt_buf->packet->header.length - sizeof(PacketHeader));

  TcpInputQueuePush(seg_buf);
  return 0;
}

static void UpdateRouteTable(const HopMap &map) {
  std::lock_guard<std::mutex> lock(rt_lock);
  for (auto p = map.cbegin(); p != map.cend(); ++p)
    rtable[p->first] = p->second;

  if (!map.empty()) {
    PktBufPtr dv_pkt = dvt.CreatePacket();
    BroadcastOnce(dv_pkt);
  }

#ifdef DEBUG_ROUTING
  std::cout << "[IP] Route Table possibly updated ";
  for (auto p = rtable.begin(); p != rtable.end(); ++p)
    std::cout << IpStr(p->first) << "->" << IpStr(p->second) << " ";
  std::cout << std::endl;
#endif
}

static void Input() {
  while (running) {
    PktBufPtr pkt_buf = IpInputQueuePop();

    if (!running)
      return;

    if (!pkt_buf)
      continue;

#ifdef DEBUG_ROUTING
    //std::cout << "[IP] received packet " << pkt_buf << std::endl;
#else
    //if (pkt_buf->packet->header.type != kRouteUpdate)
    //  std::cout << "[IP] received packet " << pkt_buf << std::endl;
#endif

    if (pkt_buf->packet->header.type == kRouteUpdate) {
      HopMap map = dvt.Update(pkt_buf->packet);
      UpdateRouteTable(map);
    } else if (pkt_buf->packet->header.dest_ip == GetLocalIp()) {
      Forward(pkt_buf);
    } else {
      IpRelay(pkt_buf);
    }
  }
}

int IpStop() {
  running = false;
  return 0;
}

static void InitRouteTable() {
  std::cout << "[IP] " << "Initial Route Table " << std::endl;
  for (Ip ip: dvt.Neighbors()) {
    std::cout << "[IP] " << IpStr(ip) << " " << IpStr(ip) << std::endl;
    rtable[ip] = ip;
  }
}

Ip GetLocalIp() {
  static Ip local_ip = 0;

  if (local_ip != 0)
    return local_ip;

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
	local_ip = addr->sin_addr.s_addr;
	freeifaddrs(ifaddr);
        return local_ip;
      }
    }
  }

  freeifaddrs(ifaddr);
  return 0;
}

int HostnameToIp(const char *hostname) {
  struct hostent *he;
  struct in_addr **addr_list;
  int i;
  if ((he = gethostbyname(hostname)) == nullptr) {
    perror("gethostbyname");
    return -1;
  }

  addr_list = (struct in_addr **)he->h_addr_list;

  for (i = 0; addr_list[i] != nullptr; i++) {
    return addr_list[i]->s_addr;
  }

  return -1;
}

int IpMain() {
  std::cout << "[IP] network layer starting ..." << std::endl;

  dvt.Init(GetLocalIp());
  InitRouteTable();
  running = true;

  std::thread input = std::thread(Input);
  BroadcastInitalRouteInfo();

  std::cout << "[IP] network layer started" << std::endl;
  ip_running = true;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(2));

  input.join();
  std::cout << "[IP] exited" << std::endl;
  return 0;
}
