#ifndef AIR_DISTANCE_VECTOR_TABLE_H_
#define AIR_DISTANCE_VECTOR_TABLE_H_

#include <unordered_map>
#include <sstream>
#include <mutex>

#include "common/pkt.h"
#include <cereal/archives/binary.hpp>
#include "cereal/types/concepts/pair_associative_container.hpp"

class Dvt;

typedef std::unordered_map<Ip, Ip> HopMap;
typedef std::unordered_map<Ip, Cost> Dv;  /* Distance Vector */
typedef std::shared_ptr<Dv> DvPtr;
typedef std::shared_ptr<Dvt> DvtPtr;

class Dvt {
 private:
  std::unordered_map<Ip, Dv> costs_;
  Ip local_ip_;

  mutable std::mutex mtx_;
 public:
  int Init(Ip local_ip);
  HopMap Update(PktPtr pkt);

  /* Deserialize a distance vector received from other hosts */
  static DvPtr Deserialize(PktPtr pkt) {
    DvPtr dv = std::make_shared<Dv>();

    std::string s(pkt->data,
                  pkt->header.length - sizeof(PacketHeader));
    std::stringstream ss(s);
    cereal::BinaryInputArchive i_archive(ss);

    i_archive(*dv.get());

    return dv;
  }


  PktBufPtr CreatePacket() const;
  std::vector<Ip> Neighbors() const;
  void Print(int option) const;

  template <typename Archive>
  void serialize(Archive &archive) {
    archive(costs_[local_ip_]);
  }
};

#endif
