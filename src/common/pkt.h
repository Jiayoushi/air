#ifndef AIR_PKT_H_
#define AIR_PKT_H_

#include <memory>

#include "constants.h"

/*
 * Packet
 */
struct PacketHeader;
typedef std::shared_ptr<Packet> PktPtr;
typedef in_addr_t Ip;
typedef uint32_t cost;


enum PacketType {
  kRouteType,           /* It's a packet for updating route table */
  kSnp                  /* A general application packet sending */
};

struct PacketHeader {
  Ip src_ip;
  Ip dest_ip;
  
  uint16_t length;      /* The total length of header + datagram */
  uint16_t type;
};

struct Packet {
  PacketHeader header;
  char data[kMaxPacketData];
};


/*
 * Routing Update
 */
struct RouteTableEntry {
  Ip ip;
  Cost cost;
};

struct RouteUpdate {
  uint32_t entry_count;
  RouteTableEntry entry[kMaxHostNum];
};



int SendPacket(PktPtr pkt, int conn);
int RecvPacket(PktPtr pkt, int conn);


#endif
