#ifndef AIR_ROUTE_TABLE_H_
#define AIR_ROUTE_TABLE_H_

#include <unordered_map>

#include "common/pkt.h"
#include "cereal/types/concepts/pair_associative_container.hpp"

class RouteTable;

typedef std::shared_ptr<RouteTable> RtPtr;

class RouteTable {
 private:
  std::unordered_map<Ip, Ip> route_;

 public:
  std::unordered_map<Ip, Cost>::iterator Begin() {
    return route_.begin();
  }

  std::unordered_map<Ip, Cost>::iterator End() {
    return route_.end();
  }

  void SetNextHop(Ip dest_ip, Ip next_hop) {
    route_[dest_ip] = next_hop;
  }

  template <typename Archive>
  void serialize(Archive &archive) {
    archive(route_);
  }
};


#endif
