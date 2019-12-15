#ifndef AIR_SRT_SERVER_H_
#define AIR_SRT_SERVER_H_

#include <mutex>
#include <thread>
#include "../common/seg.h"
#include "../common/constants.h"


void SrtServerInit(int conn);

int SrtServerSock(unsigned int server_port);

int SrtServerAccept(int sockfd);

int SrtServerRecv(int sockfd, void *buffer, unsigned int length);

int SrtServerClose(int sockfd);

void *SegmentHandler(void *arg);

#endif
