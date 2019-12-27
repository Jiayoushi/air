#include "seg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>

#include "../common/common.h"

unsigned short Checksum(std::shared_ptr<Segment> seg, uint32_t count) {
  unsigned short old_checksum = seg->header.checksum;
  seg->header.checksum = 0;

  unsigned short *addr = (unsigned short *)seg.get();
  
  register long sum = 0;
  
  while (count > 1) {
    sum += *addr++;
    count -= 2;
  }

  if (count > 0)
    sum += *addr++;


  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);

  seg->header.checksum = old_checksum;
  return ~sum;
}

bool ValidChecksum(std::shared_ptr<Segment> seg, unsigned int size) {
  uint16_t got = Checksum(seg, size);

  if (got != seg->header.checksum)
    std::cerr << "cheksum: got " << got << " expected " << seg->header.checksum << std::endl;
  return got  == seg->header.checksum;
}


std::vector<std::string> type_strings = 
{"SYN", "SYN_ACK", "FIN", "FIN_ACK", "DATA", "DATA_ACK"};

std::string GetTypeString(uint16_t type) {
  if (type < 0 || type >= type_strings.size()) {
    return "UNMATCHED_TYPE";
  } else {
    return type_strings[type];
  }
}

std::string SegToString(std::shared_ptr<Segment> seg) {
  std::stringstream ss;

  SegmentHeader &h = seg->header;

  ss << "src_port="   << h.src_port << ", dest_port=" << h.dest_port
     << ", seq="      << h.seq      << ", ack="       << h.ack
     << ", length="   << h.length   << ", type="      << GetTypeString(h.type)
     << ", recv_win=" << h.rcv_win  << ", checksum="  << h.checksum;

  return ss.str();
}
