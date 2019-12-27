#ifndef AIR_NTABLE_H_
#define AIR_NTABLE_H_

#include <arpa/inet.h>
#include <map>
#include <string>

#include "common/common.h"


struct NetInfo {
  int conn;

  std::map<Ip, Cost> costs;
};

class NeighborTable {
 private:
  std::map<Ip, NetInfo> neighbors_;
  Ip local_ip_;

 public:
  NeighborTable();

  void Init();
  int ReadCostTable(const std::string &filename);
  int AddConnection(Ip ip, int conn);
  Cost AddCost(Ip from_ip, Ip to_ip, Cost cost); 

  std::map<Ip, NetInfo>::iterator Begin();
  std::map<Ip, NetInfo>::iterator End();

  Ip MinIp() const;
  const NetInfo &operator[](Ip ip) const;
  size_t Size() const;
  Cost GetCost(Ip from_ip, Ip to_ip) const;
  Ip GetLocalIp() const;
};

inline Ip NeighborTable::GetLocalIp() const {
  return local_ip_;
}

inline Ip NeighborTable::MinIp() const {
  return neighbors_.begin()->first;
}

inline const NetInfo &NeighborTable::operator[](Ip ip) const {
  return neighbors_.at(ip);
}

inline std::map<Ip, NetInfo>::iterator NeighborTable::Begin() {
  return neighbors_.begin();
}

inline std::map<Ip, NetInfo>::iterator NeighborTable::End() {
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
