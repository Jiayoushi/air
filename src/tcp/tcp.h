#ifndef AIR_TCP_H_
#define AIR_TCP_H_

#include "common/Seg.h"



int TcpInputQueuePush(SegBufPtr seg_buf);
SegBufPtr TcpInputQueuePop();

#endif
