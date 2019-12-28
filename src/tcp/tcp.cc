#include "tcp.h"

#include "common/blocking_queue.h"
#include "common/seg.h"
#include "air/air.h"

BlockingQueue<SegBufPtr> tcp_input;

#define kSegmentLost          0
#define kSegmentError         1
#define kSegmentIntact        2

#define kPacketLossRate                 0
#define kPacketErrorRate                0

/*
  Artificial segment lost and invalid checksum
*/
static int SegmentLost(SegPtr seg) {
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

int TcpInputQueuePush(SegBufPtr seg_buf) {
  if (SegmentLost(seg_buf->segment) == kSegmentLost)
    return 0;

  tcp_input.Push(seg_buf);
  return 0;
}

SegBufPtr TcpInputQueuePop() {
  auto p = tcp_input.Pop(std::chrono::duration<double, std::deci>(1));
  if (!p)
    return nullptr;

  return *p.get();
}

// TODO
int TcpInit() {
  RegisterInitSuccess();

  return 0;
}

// TODO
int TcpStop() {

  return 0;
}
