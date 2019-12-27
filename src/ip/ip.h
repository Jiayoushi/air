#ifndef AIR_IP_H_
#define AIR_IP_H_

#include "common/pkt.h"
#include "common/seg.h"

extern Ip local_ip;

int IpSend(SegBufPtr seg_buf);
int IpStop();
Ip GetLocalIp();
int IpInit();

int IpInputQueuePush(PktBufPtr pkt_buf);
PktBufPtr IpInputQueuePop();


#endif
