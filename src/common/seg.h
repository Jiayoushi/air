#ifndef AIR_SEG_H_
#define AIR_SEG_H_

#include <memory>
#include <vector>

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
  unsigned int src_port;
  unsigned int dest_port;
  unsigned int seq_num;
  unsigned int ack_num;
  unsigned short length;   // Header length
  unsigned short type;
  unsigned short rcv_win;
  unsigned short checksum;
};

struct Segment {
  SegmentHeader header;

  char *data = nullptr;
};

/*
  First send "!&" to indicate the start of a segment
  Then send the segment.
  Finally, send end of packet markers "!#" to indicate the end of a segment.

  Return -1 on error, 0 on success.
*/
int SnpSendSegment(int connection, std::shared_ptr<Segment> seg);

std::shared_ptr<Segment> SnpRecvSegment(int connection);

int SegmentLost(std::shared_ptr<Segment> seg);

unsigned short Checksum(std::shared_ptr<Segment> seg, unsigned int data_size);

bool ValidChecksum(std::shared_ptr<Segment> seg, unsigned int data_size);

std::string SegToString(std::shared_ptr<Segment> seg);

#endif
