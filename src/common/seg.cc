#include "seg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <iostream>

#define kWaitForFirstStartControl    0
#define kFirstStartControlReceived   1
#define kSecondStartControlReceived  2
#define kFirstEndControlReceived     3
#define kSecondEndControlReceived    4

int SnpSendSegment(int connection, const Segment &seg) {
  const char *seg_start = "!&";
  const char *seg_end = "!#";

  if (send(connection, seg_start, 2, 0) < 0) {
    perror("SnpSendSegment failed to send start");
    return kFailure;
  }

  if (send(connection, seg, sizeof(Segment), 0) < 0) {
    perror("SnpSendSegment failed to send segment");
    return kFailure;
  }

  if (send(connection, seg_end, 2, 0) < 0) {
    perror("SnpSendSegment failed to send segment end");
    return kFailure;
  }

  return kSuccess;
}

int SnpRecvSegment(int connection, Segment &seg) {
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
          if (SegmentLost() == kSegmentLost) {
            state = kWaitFirstStart;
            idx = 0;
            continue;
          }
          memcpy(&seg, buf, sizeof(Segment));
          return kSuccess;
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

  return kFailure;
}

/*
  Artificial segment lost and invalid checksum
*/
int SegmentLost(const Segment &seg) {
  int random = rand() % 100;
  if (random < kPacketLossRate * 100) {
    // 50% chance of losing a segment
    if (rand() % 2 == 0) {
      return kSegmentLost;
    // 50% chance of invalid checksum
    } else {
      // Start of the data
      int data_len = sizeof(Segment) + seg.header.length;

      // Random error bit
      int error_bit = rand() % (data_len * 8);

      // Flip the error bit
      char *p = seg.data + error_bit / 8;
      *p ^= 1 << (error_bit % 8);

	  return kSegmentNotLost;
    }
  }

  return kSegmentNotLost;
}

// TODO
unsigned short Checksum(const Segment &seg) {
  return 0;
}

// TODO
int CheckCheckSum(const Segment &seg) {
  return 0;
}
