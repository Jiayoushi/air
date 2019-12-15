#ifndef AIR_SRT_SERVER_H_
#define AIR_SRT_SERVER_H_

#include <mutex>
#include <thread>
#include "../common/seg.h"
#include "../common/constants.h"

#define kClosed     0
#define kListening  1
#define kConnected  2
#define kCloseWait  4

// Server transport control block.
struct ServerTcb {
  unsigned int server_id;
  unsigned int server_port_num;

  unsigned int client_id;
  unsigned int client_port_num;

  unsigned int state;
  unsigned int expect_seq_num;
  
  std::mutex buffer_lock;
  char *recv_buffer;  // Reciving buffer
  unsigned int buffer_size;  // Size of the received data in received buffer

  ServerTcb(): server_id(0), server_port_num(0), client_id(0),
    client_port_num(0), state(kClosed), expect_seq_num(0),
    recv_buffer(nullptr), buffer_size(0) {}
};

void SrtServerInit(int conn);
int SrtServerSock(unsigned int port);
int SrtServerAccept(int sockfd);
int SrtServerRecv(int sockfd, void *buffer, unsigned int length);

int SrtServerClose(int sockfd);

void *SegmentHandler(void *arg);

#endif
