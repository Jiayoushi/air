#ifndef AIR_NTABLE_H_
#define AIR_NTABLE_H_

#include <arpa/inet.h>
#include <unordered_map>
#include <string>

#include "common/common.h"


struct NetInfo {
  int conn;

  std::unordered_map<Ip, Cost> costs;
};

class NeighborTable {
 private:
  std::unordered_map<Ip, NetInfo> neighbors_;
  Ip local_ip_;

 public:
  NeighborTable();

  void Init();
  int ReadCostTable(const std::string &filename);
  int AddConnection(Ip ip, int conn);
  Cost AddCost(Ip from_ip, Ip to_ip, Cost cost); 

  std::unordered_map<Ip, NetInfo>::iterator Begin();
  std::unordered_map<Ip, NetInfo>::iterator End();

  const NetInfo &operator[](Ip ip) const;
  size_t Size() const;
  Cost GetCost(Ip from_ip, Ip to_ip) const;
  Ip GetLocalIp() const;
};

inline Ip NeighborTable::GetLocalIp() const {
  return local_ip_;
}

inline const NetInfo &NeighborTable::operator[](Ip ip) const {
  return neighbors_.at(ip);
}

inline std::unordered_map<Ip, NetInfo>::iterator NeighborTable::Begin() {
  return neighbors_.begin();
}

inline std::unordered_map<Ip, NetInfo>::iterator NeighborTable::End() {
  return neighbors_.end();
}

inline size_t NeighborTable::Size() const {
  return neighbors_.size();
}

inline Cost NeighborTable::GetCost(Ip from_ip, Ip to_ip) const {
  return neighbors_.at(from_ip).costs.at(to_ip);
}

inline Cost NeighborTable::AddCost(Ip from_ip, Ip to_ip, Cost cost) {
  return neighbors_[from_ip].costs[to_ip] = cost;
}

#endif
