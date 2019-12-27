#ifndef AIR_SRT_SERVER_H_
#define AIR_SRT_SERVER_H_

#include <mutex>
#include <thread>
#include "../common/seg.h"
#include "../common/constants.h"


int SrtServerSock();

int SrtServerBind(int sockfd, Ip server_ip, uint16_t server_port);

int SrtServerAccept(int sockfd);

size_t SrtServerRecv(int sockfd, void *buffer, unsigned int length);

int SrtServerClose(int sockfd);

#endif
