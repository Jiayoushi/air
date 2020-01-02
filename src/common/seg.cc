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
  return Checksum(seg, size) == seg->header.checksum;
}

std::string SegToString(std::shared_ptr<Segment> seg) {
  SegmentHeader &h = seg->header;

  std::stringstream ss;
  ss << "src_port="   << h.src_port << ", dest_port=" << h.dest_port
     << ", seq="      << h.seq      << ", ack="       << h.ack
     << ", length="   << h.length   
     << ", ACK="      << bool(h.flags & kAck)
     << ", FIN="      << bool(h.flags & kFin)
     << ", PUSH="     << bool(h.flags & kPush)
     << ", RST="      << bool(h.flags & kRst)
     << ", SYN="      << bool(h.flags & kSyn)
     << ", URG="      << bool(h.flags & kUrg)
     << ", win=" << h.rcv_win  << ", checksum="  << h.checksum;

  return ss.str();
}
