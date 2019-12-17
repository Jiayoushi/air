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
#define kSegmentNotLost       1

int SnpSendSegment(int connection, std::shared_ptr<Segment> seg) {
  assert(seg != nullptr);

  const char *seg_start = "!&";
  const char *seg_end = "!#";

  if (send(connection, seg_start, 2, 0) < 0) {
    perror("SnpSendSegment failed to send start");
    return kFailure;
  }

  if (send(connection, seg.get(), sizeof(Segment), 0) < 0) {
    perror("SnpSendSegment failed to send segment");
    return kFailure;
  }

  if (send(connection, seg_end, 2, 0) < 0) {
    perror("SnpSendSegment failed to send segment end");
    return kFailure;
  }

  return kSuccess;
}

std::shared_ptr<Segment> SnpRecvSegment(int connection) {
  char buf[sizeof(Segment) + 2];
  int idx = 0;

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
          std::shared_ptr<Segment> seg = std::make_shared<Segment>();
          memcpy(seg.get(), buf, sizeof(Segment));

          if (SegmentLost(seg) == kSegmentLost) {
            state = kWaitFirstStart;
            idx = 0;
            continue;
          }

          return seg;
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
  int random = rand() % 100;
  if (random < kPacketLossRate * 100) {
    // 50% chance of losing a segment
    if (rand() % 2 == 0) {
      return kSegmentLost;

    // 50% chance of invalid checksum
    } else {
      // Start of the data
      int data_len = sizeof(Segment) + seg->header.length;

      // Random error bit
      int error_bit = rand() % (data_len * 8);

      // Flip the error bit
      char *p = seg->data + error_bit / 8;
      *p ^= 1 << (error_bit % 8);

	  return kSegmentNotLost;
    }
  }

  return kSegmentNotLost;
}

// TODO
unsigned short Checksum(std::shared_ptr<Segment> seg) {
  return 0;
}

// TODO
bool ValidChecksum(std::shared_ptr<Segment> seg) {
  return true;
}

std::vector<std::string> type_strings = 
{"SYN", "SYN_ACK", "FIN", "FIN_ACK", "DATA", "DATA_ACK"};

std::string SegToString(std::shared_ptr<Segment> seg) {
  std::stringstream ss;

  SrtHeader &h = seg->header;

  ss << "src_port=" << h.src_port << ", dest_port=" << h.dest_port
     << ", seq_num=" << h.seq_num << ", ack_num=" << h.ack_num
     << ", length=" << h.length << ", type=" << type_strings[h.type]
     << ", recv_win=" << h.rcv_win << ", checksum=" << h.checksum;

  return ss.str();
}
