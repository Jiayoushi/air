#ifndef AIR_SEG_H_
#define AIR_SEG_H_

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

#include "constants.h"

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

typedef std::chrono::milliseconds Timepoint;

/*
 * Segment Buffer
 */
struct SegmentBuffer {
  std::shared_ptr<Segment> segment;
  
  uint32_t data_size;                 /* Size of the payload, not including header */

  Timepoint send_time;                /* Last time this segment was sent */
  Timepoint acked_time;               /* The first time this segment was acked */

  SegmentBuffer(std::shared_ptr<Segment> s=nullptr, uint32_t size=0):
   segment(s), data_size(size), send_time(), acked_time() {}
};

typedef std::shared_ptr<Segment> SegPtr;
typedef std::shared_ptr<SegmentBuffer> SegBufPtr;



/*
  First send "!&" to indicate the start of a segment
  Then send the segment.
  Finally, send end of packet markers "!#" to indicate the end of a segment.

  Return -1 on error, 0 on success.
*/
int SnpSendSegment(int connection, SegBufPtr seg_buf);

SegBufPtr SnpRecvSegment(int connection);

int SegmentLost(std::shared_ptr<Segment> seg);

uint16_t Checksum(std::shared_ptr<Segment> seg, uint32_t size);

bool ValidChecksum(std::shared_ptr<Segment> seg, uint32_t size);

std::string SegToString(std::shared_ptr<Segment> seg);

inline std::ostream &operator<<(std::ostream &out, SegBufPtr seg_buf) {
  out << SegToString(seg_buf->segment);

  out << ", data_size=" << seg_buf->data_size;

  return out;
}

#endif
