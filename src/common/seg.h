#ifndef AIR_SEG_H_
#define AIR_SEG_H_

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
  char *data;

  Segment(): data(nullptr) {}
  ~Segment() { delete [] data; }
};

typedef std::chrono::milliseconds Timepoint;

/*
 * Segment Buffer
 */
struct SegmentBuffer {
  std::shared_ptr<Segment> segment;
  
  uint32_t data_size; /* Size of the payload, not including header */

  Timepoint send_time; /* Last time this segment was sent */

  SegmentBuffer(std::shared_ptr<Segment> s=nullptr, uint32_t size=0):
   segment(s), data_size(size), send_time() {}
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






#endif
