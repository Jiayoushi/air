#ifndef AIR_IP_H_
#define AIR_IP_H_

#include "common/pkt.h"
#include "common/seg.h"

int IpSend(SegBufPtr seg_buf);
int IpStop();
int IpMain();

int IpInputQueuePush(PktBufPtr pkt_buf);
PktBufPtr IpInputQueuePop();

Ip GetLocalIp();
int HostnameToIp(const char *hostname);

#endif
