#include "pkt.h"

#include <vector>
#include <string>
#include <sstream>

static std::vector<std::string> type_strings =
{"ROUTE_UPDATE", "SNP"};

static std::string GetTypeString(uint16_t type) {
  if (type < 0 || type >= type_strings.size()) {
    return "UNMATCHED_TYPE";
  } else {
    return type_strings[type];
  }
}



std::string PktToString(PktPtr pkt) {
  std::stringstream ss;

  PacketHeader &h = pkt->header;

  ss << "src_ip=" << IpStr(h.src_ip) 
     << ", dest_ip=" << IpStr(h.dest_ip)
     << ", length=" << h.length   
     << ", type=" << GetTypeString(h.type);

  return ss.str();
}
