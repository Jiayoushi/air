#ifndef AIR_TCP_H_
#define AIR_TCP_H_

#include "common/seg.h"

int TcpInit();
int TcpStop();

int TcpInputQueuePush(SegBufPtr seg_buf);
SegBufPtr TcpInputQueuePop();

#endif
