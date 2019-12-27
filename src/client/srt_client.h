#ifndef AIR_SRT_CLIENT_H_
#define AIR_SRT_CLIENT_H_

#include <cstddef>
#include "common/common.h"

void SrtClientInit();
void SrtClientShutdown();

int SrtClientSock();
int SrtClientClose(int sockfd);

size_t SrtClientSend(int sockfd, const void *data, unsigned int length);

int SrtClientConnect(int sockfd, Ip server_ip, uint16_t server_port);
int SrtClientDisconnect(int sockfd);

#endif
