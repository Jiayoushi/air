#include "tcp.h"

BlockingQueue<SegmentBuffer> tcp_input;

int TcpInputQueuePush(SegBufPtr seg_buf) {
  tcp_input.Push(seg_buf);
  return 0;
}

SegBufPtr TcpInputQueuePop() {
  return tcp_input.Pop();
}
