#ifndef AIR_PKT_H_
#define AIR_PKT_H_

#include <netinet/in.h>
#include <memory>

#include "constants.h"
#include "common.h"

/*
 * Packet
 */
struct Packet;
struct PacketBuffer;
typedef std::shared_ptr<Packet> PktPtr;
typedef std::shared_ptr<PacketBuffer> PktBufPtr;


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

struct PacketBuffer {
  PktPtr packet;

  Ip next_hop;
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

std::string PktToString(PktPtr pkt);

char *IpStr(Ip ip);

inline std::ostream &operator<<(std::ostream &out, PktBufPtr pkt_buf) {
  out << PktToString(pkt_buf->packet);

  return out;
}


#endif
