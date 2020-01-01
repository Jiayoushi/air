#include "dvt.h"

#include <cstring>
#include <vector>

#include "common/pkt.h"
#include "overlay/overlay.h"

int Dvt::Init(Ip local_ip) {
  local_ip_ = local_ip;

  std::vector<std::pair<Ip, Cost>> costs = GetCost();

  for (const std::pair<Ip, Cost> p1: costs)
    for (const std::pair<Ip, Cost> p2: costs)
      costs_[p1.first][p2.first] = p1.first == p2.first ?
        0 : std::numeric_limits<Cost>::max();

  for (std::pair<Ip, Cost> cost: costs)
    costs_[local_ip_][cost.first] = cost.second;



  return 0;
}

PktBufPtr Dvt::CreatePacket() const {
  std::lock_guard<std::mutex> lck(mtx_);

  PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();
  pkt_buf->packet = std::make_shared<Packet>();
  pkt_buf->packet->header.type = kRouteUpdate;
  pkt_buf->packet->header.src_ip = local_ip_;

  /* Serialize route table and copy into packet's data */
  std::stringstream ss;
  cereal::BinaryOutputArchive o_archive(ss);
  o_archive(*this);
  std::string s = ss.str();
  memcpy(pkt_buf->packet->data, s.c_str(), s.size());
  pkt_buf->packet->header.length = sizeof(PacketHeader) + s.size();

  return pkt_buf;
}

void Dvt::Print() const {
  for (auto p = costs_.begin(); p != costs_.end(); ++p) {
    std::cout << IpStr(p->first) << " ";

    for (auto x = p->second.begin(); x != p->second.end(); ++x) {
      std::cout << IpStr(x->first) << "=" << x->second << " ";
    }

    std::cout << std::endl;
  }
}

void Dvt::Update(DvPtr in_dv) {
  std::lock_guard<std::mutex> lck(mtx_);

  Print();
}

std::vector<Ip> Dvt::Neighbors() const {
  std::vector<Ip> n;

  for (auto p = costs_.at(local_ip_).cbegin();
       p != costs_.at(local_ip_).cend(); ++p) {
    if (p->first != local_ip_)
      n.push_back(p->first);
  }

  return n;
}
