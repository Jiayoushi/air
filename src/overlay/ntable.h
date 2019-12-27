#ifndef AIR_NTABLE_H_
#define AIR_NTABLE_H_

#include <arpa/inet.h>
#include <unordered_map>
#include <string>

#include "common/common.h"


struct NetInfo {
  Ip ip;
  int conn;
};

class NeighborTable {
 private:
  std::unordered_map<Ip, NetInfo> neighbors_;
  NetInfo local_;

  std::unordered_map<Ip, std::unordered_map<Ip, Cost>> costs_;

 public:
  NeighborTable();

  void Init();
  int ReadCostTable(const std::string &filename);
  int AddConnection(Ip ip, int conn);
 
  const NetInfo &operator[](int index) const;
  size_t Size() const;
  Cost GetCost(Ip from_ip, Ip to_ip) const;
};

inline size_t NeighborTable::Size() const {
  return neighbors_.size();
}

inline Cost NeighborTable::GetCost(Ip from_ip, Ip to_ip) const {
  return costs_.at(from_ip).at(to_ip);
}

inline const NetInfo &NeighborTable::operator[](int index) const {
  return neighbors_.at(index);
}

#endif
