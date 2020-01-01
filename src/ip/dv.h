#ifndef AIR_DISTANCE_VECTOR_H_
#define AIR_DISTANCE_VECTOR_H_

#include <unordered_map>

#include "common/pkt.h"
#include "cereal/types/concepts/pair_associative_container.hpp"

class DistanceVector;

typedef std::shared_ptr<DistanceVector> DvPtr;

class DistanceVector {
 private:
  std::unordered_map<Ip, Cost> costs_;

 public:
  std::unordered_map<Ip, Cost>::iterator Begin() {
    return costs_.begin();
  }

  std::unordered_map<Ip, Cost>::iterator End() {
    return costs_.end();
  }

  Cost &operator[](Ip ip) {
    return costs_[ip];
  }

  static DvPtr Deserialize(PktBufPtr pkt_buf) {
    DvPtr dv = std::make_shared<DistanceVector>();

    std::string s(pkt_buf->packet->data,
                  pkt_buf->packet->header.length - sizeof(PacketHeader));
    std::stringstream ss(s);
    cereal::BinaryInputArchive i_archive(ss);

    i_archive(*dv.get());

    return dv;
  }

  template <typename Archive>
  void serialize(Archive &archive) {
    archive(costs_);
  }
};

#endif
