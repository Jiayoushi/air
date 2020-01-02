#ifndef AIR_SEG_H_
#define AIR_SEG_H_

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

#include "common.h"

struct Segment;
struct SegmentBuffer;
typedef std::chrono::milliseconds Timepoint;
typedef std::shared_ptr<Segment> SegPtr;
typedef std::shared_ptr<SegmentBuffer> SegBufPtr;

const size_t kMss = 512;


enum FlagMask {
  kAck  = 0b100000,
  kFin  = 0b010000,
  kPush = 0b001000,
  kRst  = 0b000100,
  kSyn  = 0b000010,
  kUrg  = 0b000001
};

struct SegmentHeader {
  uint16_t src_port;
  uint16_t dest_port;
  uint32_t seq;
  uint32_t ack;
  uint16_t length;       /* Header length */
  uint8_t flags;         /* ACK, FIN, PUSH, RST, SYN, URG */
  uint16_t rcv_win;
  uint16_t checksum;

  SegmentHeader():
    src_port(0), dest_port(0), seq(0), ack(0), length(0),
    flags(0), rcv_win(0), checksum(0) {}
};

struct Segment {
  SegmentHeader header;
  char data[kMss];

  Segment():
    header() {}
};

/*
 * Segment Buffer
 */
struct SegmentBuffer {
  SegPtr segment;
  uint32_t data_size;                 /* Size of the payload, not including header */

  Ip src_ip;
  Ip dest_ip;

  Timepoint send_time;                /* Last time this segment was sent */
  Timepoint acked_time;               /* The first time this segment was acked */

  SegmentBuffer(std::shared_ptr<Segment> s=nullptr, uint32_t size=0, Ip sip=0, Ip dip=0):
   segment(s), data_size(size), src_ip(sip), dest_ip(dip), send_time(), acked_time() {}
};

uint16_t Checksum(std::shared_ptr<Segment> seg, uint32_t size);

bool ValidChecksum(std::shared_ptr<Segment> seg, uint32_t size);

std::string SegToString(std::shared_ptr<Segment> seg);

inline std::ostream &operator<<(std::ostream &out, SegBufPtr seg_buf) {
  out << SegToString(seg_buf->segment);

  out << ", len=" << seg_buf->data_size;

  return out;
}

#endif
