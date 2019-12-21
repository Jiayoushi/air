#ifndef AIR_SRT_CLIENT_H_
#define AIR_SRT_CLIENT_H_

#include <cstddef>

void SrtClientInit(int conn);
void SrtClientShutdown();

int SrtClientSock(unsigned int client_port);
int SrtClientClose(int sockfd);

size_t SrtClientSend(int sockfd, const void *data, unsigned int length);

int SrtClientConnect(int sockfd, unsigned int server_port);
int SrtClientDisconnect(int sockfd);

#endif
