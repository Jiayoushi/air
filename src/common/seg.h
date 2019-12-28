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

const size_t kMss                     = 512;


enum SegmentType {
  kSyn,
  kSynAck,
  kFin,
  kFinAck,
  kData,
  kDataAck
};

struct SegmentHeader {
  uint32_t src_port;
  uint32_t dest_port;
  uint32_t seq;
  uint32_t ack;
  uint16_t length;       /* Header length */
  uint16_t type;
  uint16_t rcv_win;
  uint16_t checksum;
};

struct Segment {
  SegmentHeader header;
  char data[kMss];
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

  out << ", data_size=" << seg_buf->data_size;

  return out;
}

#endif
