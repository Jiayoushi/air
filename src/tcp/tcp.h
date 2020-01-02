#ifndef AIR_TCP_H_
#define AIR_TCP_H_

#include <common/seg.h>

int TcpMain();
int TcpStop();
int Teardown(int sockfd);

int TcpInputQueuePush(SegBufPtr seg_buf);
SegBufPtr TcpInputQueuePop();

/* User API */
int Connect(int sockfd, Ip dest_ip, uint16_t dest_port);
int Sock();
int Close(int sockfd);

#endif
