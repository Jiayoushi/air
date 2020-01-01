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

#include <cereal/archives/binary.hpp>
#include "air/air.h"
#include "common/common.h"
#include "common/seg.h"
#include "common/pkt.h"
#include "common/blocking_queue.h"
#include "tcp/tcp.h"
#include "ip/dv.h"
#include "overlay/overlay.h"

std::atomic<bool> running;

BlockingQueue<PktBufPtr> ip_input;
DistanceVector dv;
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

  // TODO:
  pkt_buf->next_hop = seg_buf->dest_ip;

  OverlaySend(pkt_buf);
  std::cout << "[IP] Sent packet " << pkt_buf << std::endl;

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

static PktBufPtr CreateDistanceVectorPacket() {
  PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();
  pkt_buf->packet = std::make_shared<Packet>();
  pkt_buf->packet->header.type = kRouteUpdate;
  pkt_buf->packet->header.src_ip = GetLocalIp();

  /* Serialize route table and copy into packet's data */
  std::stringstream ss;
  cereal::BinaryOutputArchive o_archive(ss);
  o_archive(rtable);
  std::string s = ss.str();
  memcpy(pkt_buf->packet->data, s.c_str(), s.size());
  pkt_buf->packet->header.length = sizeof(PacketHeader) + s.size();

  return pkt_buf;
}

static void Broadcast() {
  while (running) {
    PktBufPtr dv_pkt = CreateDistanceVectorPacket();

    BroadcastOnce(dv_pkt);

    std::this_thread::sleep_for(kRouteUpdateIntervalInSecs);
  }
}

static void UpdateDistanceVector(DvPtr dv) {

}

static void UpdateRouteTable() {
}

static void Update(DvPtr dv) {
  UpdateDistanceVector(dv);
  UpdateRouteTable();
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

static void Input() {
  while (running) {
    PktBufPtr pkt_buf = IpInputQueuePop();

    if (!running)
      return;

    if (!pkt_buf)
      continue;

    if (pkt_buf->packet->header.type == kRouteUpdate) {
      Update(DistanceVector::Deserialize(pkt_buf));

      // TODO: relay to other hosts
    } else {
      Forward(pkt_buf);
    }

    std::cout << "[IP] received packet " << pkt_buf << std::endl;
  }
}


int IpStop() {
  running = false;
  return 0;
}

static void InitDistanceVector() {
  std::vector<std::pair<Ip, Cost>> costs = GetCost();

  std::cout << "[IP] " << "Initial Distance Vector " << std::endl;
  for (std::pair<Ip, Cost> cost: costs) {
    dv[cost.first] = cost.second;
    std::cout << "[IP] " << IpStr(cost.first) << " " << cost.second << std::endl;
  }
}

static void InitRouteTable() {
  std::cout << "[IP] " << "Initial Route Table " << std::endl;
  for (auto p = dv.Begin(); p != dv.End(); ++p) {
    if (p->second != std::numeric_limits<Cost>::max()) {
      std::cout << "[IP] " << IpStr(p->first) << " " << IpStr(p->first) << std::endl;
      rtable[p->first] = p->first;
    }
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

int IpInit() {
  std::cout << "[IP] network layer starting ..." << std::endl;

  InitDistanceVector();
  InitRouteTable();

  running = true;

  std::thread input = std::thread(Input);
  std::thread broadcast = std::thread(Broadcast);

  RegisterInitSuccess();

  std::cout << "[IP] network layer started" << std::endl;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(2));

  broadcast.join();
  input.join();

  std::cout << "[IP] exited" << std::endl;

  return 0;
}
