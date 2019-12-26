#ifndef AIR_IP_H_
#define AIR_IP_H_

extern Ip local_ip;

int IpSend(SegBufPtr);

// TODO
int IpForward();

void IpStop();

Ip GetLocalIp();


int IpInputQueuePush(PktBufPtr pkt_buf);
PktBufPtr IpInputQueuePop();


#endif
