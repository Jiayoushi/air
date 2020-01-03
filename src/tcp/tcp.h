#ifndef AIR_TCP_H_
#define AIR_TCP_H_

#include <common/seg.h>

int TcpMain();
int TcpStop();
int Teardown(int sockfd);
int TcpInputQueuePush(SegBufPtr seg_buf);
SegBufPtr TcpInputQueuePop();


/* User API */
int Sock();
int Connect(int sockfd, Ip dest_ip, uint16_t dest_port);
int Bind(int sockfd, Ip src_ip, uint16_t src_port);
int Accept(int sockfd);
size_t Send(int sockfd, const void *data, uint32_t length);
size_t Recv(int sockfd, void *buffer, uint32_t, length);
int Close(int sockfd);

#endif
