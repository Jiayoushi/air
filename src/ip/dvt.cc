#include "dvt.h"

#include <cstring>
#include <vector>

#include "common/pkt.h"
#include "overlay/overlay.h"

int Dvt::Init(Ip local_ip) {
  local_ip_ = local_ip;

  std::vector<std::pair<Ip, Cost>> costs = GetAllCost();

  for (const std::pair<Ip, Cost> p1: costs)
    for (const std::pair<Ip, Cost> p2: costs)
      costs_[p1.first][p2.first] = p1.first == p2.first ?
        0 : std::numeric_limits<Cost>::max();

  for (std::pair<Ip, Cost> cost: costs)
    costs_[local_ip_][cost.first] = cost.second;

  costs_[local_ip_][local_ip_] = 0;
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

void Dvt::Print(int option) const {
  if (option == 0) {
    for (auto p = costs_.begin(); p != costs_.end(); ++p) {
      std::cout << "[IP] " << IpStr(p->first) << " ";
      for (auto x = p->second.begin(); x != p->second.end(); ++x) {
        std::cout << IpStr(x->first) << "=" << x->second << " ";
      }
      std::cout << std::endl;
    }
  } else {
    std::cout << "[IP] DV ";
    for (auto x = costs_.at(local_ip_).cbegin(); x != costs_.at(local_ip_).end(); ++x) {
      std::cout << IpStr(x->first) << "=" << x->second << " ";
    }
    std::cout << std::endl;
  }
}

HopMap Dvt::Update(PktPtr pkt) {
  std::lock_guard<std::mutex> lck(mtx_);

  DvPtr in_dv = Dvt::Deserialize(pkt);
  
  if (costs_.find(pkt->header.src_ip) == costs_.end())
    return HopMap();

  /* Update */
  costs_[pkt->header.src_ip] = *in_dv.get();
  
  HopMap map;

  /* Calculate shortest path */
  for (auto p = costs_.begin(); p != costs_.end(); ++p) {
    for (auto x = costs_[local_ip_].begin(); x != costs_[local_ip_].end(); ++x) {
      Ip dest = p->first;
      Ip nbr = x->first;

      if (dest == local_ip_ ||
          nbr == local_ip_ ||
          costs_[nbr][dest] == kInvalidCost ||
          GetCost(local_ip_, nbr) == kInvalidCost)
        continue;

      Cost new_cost = GetCost(local_ip_, nbr) + costs_[nbr][dest];

      if (new_cost < costs_[local_ip_][dest]) {
        costs_[local_ip_][dest] = new_cost;
	map[dest] = nbr;
      }
    }
  }

  return map;
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
