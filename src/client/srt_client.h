#ifndef AIR_SRT_CLIENT_H_
#define AIR_SRT_CLIENT_H_

#include <thread>
#include <memory>
#include <mutex>
#include "../common/seg.h"

// Client States
#define kClosed     0
#define kSynSent    1
#define kConnected  2
#define kFinWait    3

#define kSynMaxRetry 3

// Store segments in send buffer linked list
struct SegmentBuffer {
  Segment segment;
  unsigned int send_time;
  std::shared_ptr<SegmentBuffer> next;
};

// Client transport control block, the client side of a SRT connection
// uses this data structure to keep track of connection information
struct ClientTcb {
  unsigned int server_node_id;
  unsigned int server_port_num;
  
  unsigned int client_node_id;
  unsigned int client_port_num;

  unsigned int state;
  unsigned int next_seq_num;               // Next sequence number to be used by
                                           // new segment
  
  std::mutex buffer_lock;
  std::shared_ptr<SegmentBuffer> head;     // Head of send buffer
  std::shared_ptr<SegmentBuffer> unsent;   // First unsent segment
  std::shared_ptr<SegmentBuffer> tail;     // Tail of send buffer

  unsigned int unacked;  // Number of sent-but-not-Acked segments

  ClientTcb(): 
    server_node_id(0), server_port_num(0), client_node_id(0),
    client_port_num(0), state(kClosed), next_seq_num(0), head(nullptr),
    unsent(nullptr), tail(nullptr) {}
};

void SrtClientInit(int conn);
int SrtClientSock(unsigned int client_port);
int SrtClientConnect(int sockfd, unsigned int server_port);

int SrtClientSend(int sockfd, void *data, unsigned int length);

int SrtClientDisconnect(int sockfd);
int SrtClientClose(int sockfd);

#endif
