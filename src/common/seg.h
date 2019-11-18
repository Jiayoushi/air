#ifndef AIR_SEG_H_
#define AIR_SEG_H_

#include "constants.h"

enum SegmentType {
  kSyn,
  kSynAck,
  kFin,
  kFinAck,
  kData,
  kDataAck
};

struct SrtHeader {
  unsigned int src_port;
  unsigned int dest_port;
  unsigned int seq_num;
  unsigned int ack_num;
  unsigned short int length;
  unsigned short int type;
  unsigned short int rcv_win;
  unsigned short int checksum;
};

struct Segment {
  SrtHeader header;
  char data[MAX_SEG_LEN];
};

/*
  First send "!&" to indicate the start of a segment
  Then send the segment.
  Finally, send end of packet markers "!#" to indicate the end of a segment.

  Return -1 on error, 0 on success.
*/
int SnpSendSegment(int connection, const Segment &seg);

int SnpRecvSegment(int connection, const Segment &seg);

int SegmentLost(const Segment &seg);

unsigned short Checksum(const Segment &seg);

int CheckCheckSum(const Segment &seg);

#endif