#ifndef AIR_TCP_H_
#define AIR_TCP_H_

#include <mutex>
#include <condition_variable>
#include <cstring>
#include <atomic>

#include <tcp_fsm.h>
#include <tcp_timer.h>
#include <send_buffer.h>
#include <recv_buffer.h>
#include <seg.h>

#define kMaxConnection        1024

struct Tcb;
typedef std::shared_ptr<Tcb> TcbPtr;

/*
 * Transport control block.
 *
 */
struct Tcb {
  uint32_t src_ip;
  uint32_t src_port;
  uint32_t dest_ip;
  uint32_t dest_port;

  uint32_t state;
  uint32_t iss;                       /* Initial send sequence number */
  uint32_t snd_una;                   /* The oldest unacked segment's sequence number */
  uint32_t snd_nxt;                   /* The next sequence number to be used to send segments */

  uint32_t rcv_nxt;
  uint32_t rcv_win;
  uint32_t irs;                       /* Initial receive sequence number */

  std::mutex lock;
  std::condition_variable waiting;    /* A condition variable used to wait for message either from timeouts or from a new incoming segment */
  SendBuffer send_buffer; 
  RecvBuffer recv_buffer;

  uint8_t timer_flags;
  uint8_t timers[kTimerNum];

  Tcb(): 
    src_ip(0),
    src_port(0), 
    dest_ip(0),
    dest_port(0),
    state(kClosed),
    iss(0),
    snd_una(0),
    snd_nxt(iss),
    rcv_nxt(0),
    rcv_win(0),
    irs(0),
    lock(),
    waiting(),
    send_buffer(),
    recv_buffer(),
    timer_flags(0) {
      memset(timers, 0, sizeof(uint8_t) * kTimerNum);
    }
};

// TODO: better to provide API
extern std::mutex tcb_table_lock;
extern std::vector<TcbPtr> tcb_table;
extern std::atomic<bool> tcp_running;

int TcpMain();
int TcpStop();
int TcpInput(SegBufPtr seg_buf);
void TcpInputFromIp();
int TcpOutput(TcbPtr tcb);
int TcpInputQueuePush(SegBufPtr seg_buf);
int Teardown(int sockfd);
bool TcpRunning();
void TcpTimeout();
int TcpDemultiplex(SegBufPtr seg_buf);
SegBufPtr TcpInputQueuePop();


/* User API */
int Sock();
int Connect(int sockfd, Ip dest_ip, uint16_t dest_port);
int Bind(int sockfd, Ip src_ip, uint16_t src_port);
int Accept(int sockfd);
size_t Send(int sockfd, const void *data, uint32_t length);
size_t Recv(int sockfd, void *buffer, uint32_t length);
int Close(int sockfd);

#endif
