#include "seg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>

#include "../common/common.h"

#define kWaitFirstStart       0
#define kWaitSecondStart      1
#define kWaitFirstEnd         2
#define kWaitSecondEnd        3

#define kSegmentLost          0
#define kSegmentError         1
#define kSegmentIntact        2

int SnpSendSegment(int connection, SegBufPtr seg_buf) {
  assert(seg_buf != nullptr);
  assert(seg_buf->segment != nullptr);

  const char *seg_start = "!&";
  const char *seg_end = "!#";

  if (send(connection, seg_start, 2, 0) < 0) {
    perror("SnpSendSegment failed to send start");
    return -1;
  }

  if (send(connection, seg_buf->segment.get(),
           sizeof(SegmentHeader) + seg_buf->data_size, 0) < 0) {
    perror("SnpSendSegment failed to send segment");
    return -1;
  }

  if (send(connection, seg_end, 2, 0) < 0) {
    perror("SnpSendSegment failed to send segment end");
    return -1;
  }

  return 0;
}


char buf[65535];
SegBufPtr SnpRecvSegment(int connection) {
  memset(buf, 0, 65535);

  uint32_t idx = 0;

  int state = kWaitFirstStart;
  char c;
  int recved = 0;
  while ((recved = recv(connection, &c, 1, 0)) > 0) {
    switch (state) {
      case kWaitFirstStart:   // Wait !
        if (c == '!')
          state = kWaitSecondStart;
        break;
      case kWaitSecondStart:  // Wait &
        if (c == '&') {
          state = kWaitFirstEnd;
        } else {
          state = kWaitFirstStart;
        }
        break;
      case kWaitFirstEnd:     // Wait !
        buf[idx++] = c;
        if (c == '!') {
          state = kWaitSecondEnd;
        }
        break;
      case kWaitSecondEnd:    // Wait #
        buf[idx++] = c;
        // "!#" is received, the segment receiving is complete
        if (c == '#') {
          SegBufPtr seg_buf = std::make_shared<SegmentBuffer>();
          SegPtr seg = std::make_shared<Segment>();
          seg_buf->segment = seg;

          /*
           *                 i
           * [Header][Data]!#
           */

          uint32_t data_size = idx - 2 - sizeof(SegmentHeader);
          seg_buf->data_size = data_size;
          memcpy(seg.get(), buf, sizeof(SegmentHeader) + data_size);

          /* Artificial packet loss and packet error */
          if (SegmentLost(seg_buf->segment) == kSegmentLost) {
            state = kWaitFirstStart;
            idx = 0;
            continue;
          }

          return seg_buf;
        } else if (c == '!') {
          // Previous '!' is not end control character
          // This one might be, so still wait for '#'
        } else {
          // Previous '!' is definitely not end control character
          // Go back to previous state to wait for first end control character
          state = kWaitFirstEnd;
        }
        break;
      default:
        break;
    }
  }

  return nullptr;
}

/*
  Artificial segment lost and invalid checksum
*/
int SegmentLost(std::shared_ptr<Segment> seg) {
  if ((rand() % 100) < kPacketLossRate * 100) {
    std::cerr << "[LOST]" << std::endl;
    return kSegmentLost;
  }

  if ((rand() % 100) < kPacketErrorRate * 100) {
    int len = sizeof(SegmentHeader);

    // Random error bit
    int error_bit = rand() % (len * 8);

    // Flip
    char *p = (char *)seg.get() + error_bit / 8;
    *p ^= 1 << (error_bit % 8);

	return kSegmentError;
  }

  return kSegmentIntact;
}


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
