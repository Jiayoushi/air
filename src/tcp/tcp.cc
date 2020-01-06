#include <tcp.h>

#include <stdlib.h>
#include <cstring>
#include <thread>
#include <algorithm>
#include <queue>
#include <iterator>
#include <mutex>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <list>
#include <condition_variable>

#include <seg.h>
#include <timer.h>
#include <blocking_queue.h>
#include <common.h>
#include <send_buffer.h>
#include <ip.h>
#include <overlay.h>

#define kSegmentLost          0
#define kSegmentError         1
#define kSegmentIntact        2
#define kPacketLossRate       0
#define kPacketErrorRate      0

std::mutex tcb_table_lock;
std::vector<TcbPtr> tcb_table;

std::atomic<bool> tcp_running;
static std::atomic<bool> running;                                  /* Control the running of all threads. */
static std::shared_ptr<std::thread> timeout_thread;                /* Timeout loop */
static std::shared_ptr<std::thread> input_thread;                  /* Receiving segments from network stack */
static BlockingQueue<SegBufPtr> tcp_input;
static uint16_t random_port = 10000;

int Sock() {
  tcb_table_lock.lock();

  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] == nullptr) {
      tcb_table[i] = std::make_shared<Tcb>();
      tcb_table_lock.unlock();
      return i;
    }
  }

  tcb_table_lock.unlock();
  return -1;
}

static int PassiveClose(TcbPtr tcb) {
  std::unique_lock<std::mutex> lk(tcb->lock);

  TcpOutput(tcb);

  /* Update */
  tcb->state = kLastAck;

  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kClosed;
    });

  return 0;
}

/* 
 * If the state is close wait, continue with passive close,
 * otherwise, initiate active close. 
 */
int Close(int sockfd) {
  TcbPtr tcb = tcb_table[sockfd];

  if (tcb->state == kCloseWait)
    PassiveClose(tcb);
  else if (tcb->state != kClosed)
    Teardown(sockfd);

  tcb_table[sockfd] = nullptr;
  return 0;
}

size_t Send(int sockfd, const void *data, uint32_t length) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  const char *p = (const char *)data;
  size_t seg_len = length;

  while (seg_len > 0) {
    size_t size = std::min(seg_len, kMss);

    tcb->send_buffer.PushBack(p, size);
 
    p += size;
    seg_len -= size;
  }

  return length;
}

size_t Recv(int sockfd, void *buffer, unsigned int length) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  size_t recved = 0;
  while (recved < length) {
    std::cout << "[TCP] " << "Recv Current " << recved << " expect " << length << std::endl;
    tcb->waiting.wait(lk,
      [&tcb] {
        return !tcb->recv_buffer.Empty();
      });

    while (!tcb->recv_buffer.Empty()) {
      SegBufPtr seg_buf = tcb->recv_buffer.Front();
      tcb->recv_buffer.Pop();

      memcpy((char *)buffer + recved, seg_buf->segment->data, seg_buf->data_size);
      recved += seg_buf->data_size;
    }
  }

  return recved;
}

/*
 * Establish connection by completing two way handshake.
 *
 * Return value: -1 on error, 0 on success
 */
int Connect(int sockfd, Ip dest_ip, uint16_t dest_port) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;
  if (tcb->state != kClosed)
    return -1;

  /* Lock to ensure the atomicity of send_buffer */
  std::unique_lock<std::mutex> lk(tcb->lock);

  /* Assign addresses */
  tcb->src_ip = GetLocalIp();
  tcb->src_port = random_port++;
  tcb->dest_ip = dest_ip;
  tcb->dest_port = dest_port;

  tcb->iss = 10; // TODO: testing purpose

  TcpOutput(tcb);

  tcb->state = kSynSent;

  /* Blocked until connection timeout or success */
  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kConnected || tcb->send_buffer.MaxRetryReached();
    });

  /* Success */
  if (tcb->state == kConnected)
    return 0;

  /* Connection timed out */
  return -1;
}

/*
 * Teardown connection
 *
 * Return value: -1 on failure, 0 on success
 */
int Teardown(int sockfd) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  std::unique_lock<std::mutex> lk(tcb->lock);

  TcpOutput(tcb);

  /* Update */
  tcb->state = kFinWait1;

  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kClosed || tcb->send_buffer.MaxRetryReached();
    });

  if (tcb->state == kClosed)
    return 0;

  /* Timed out */
  std::cout << "[TCP] " << "Disconnect max retry reached" << std::endl;

  return -1; 
}

int Bind(int sockfd, Ip src_ip, uint16_t src_port) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  tcb->src_ip = src_ip;
  tcb->src_port = src_port;

  return 0;
}

int Accept(int sockfd) {
  std::shared_ptr<Tcb> tcb = tcb_table[sockfd];
  if (tcb == nullptr)
    return -1;

  tcb->state = kListening;
  tcb->iss = 100; // TODO: testing purpose

  // Blocked until input notify that the state is connected
  std::unique_lock<std::mutex> lk(tcb->lock);
  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kConnected;
    });

  return 0;
}

/*
  Artificial segment lost and invalid checksum
*/
static int SegmentLost(SegPtr seg) {
  if ((rand() % 100) < kPacketLossRate * 100) {
    std::cerr << "[LOST]" << std::endl;
    return kSegmentLost;
  }

  if ((rand() % 100) < kPacketErrorRate * 100) {
    int len = sizeof(SegmentHeader);

    // Random error bit
    int error_bit = rand() % (len * 8);

    // Flip
    char *p = (char *)seg.get() + error_bit / 8;
    *p ^= 1 << (error_bit % 8);

    return kSegmentError;
  }

  return kSegmentIntact;
}

int TcpInputQueuePush(SegBufPtr seg_buf) {
  if (SegmentLost(seg_buf->segment) == kSegmentLost)
    return 0;

  tcp_input.Push(seg_buf);
  return 0;
}

SegBufPtr TcpInputQueuePop() {
  auto p = tcp_input.Pop(std::chrono::duration<double, std::deci>(1));
  if (!p)
    return nullptr;

  return *p.get();
}

int TcpDemultiplex(SegBufPtr seg_buf) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (tcb_table[tcb_id] == nullptr)
      continue;

    TcbPtr tcb = tcb_table[tcb_id];
    const SegmentHeader &hdr = seg_buf->segment->header;

    /* Accept new connection */
    if (tcb->state == kListening && 
	(hdr.flags & kSyn) &&
        hdr.dest_port == tcb->src_port &&
	seg_buf->dest_ip == tcb->src_ip)
      return tcb_id;

    /* Exact match */
    if (hdr.src_port == tcb->dest_port && 
	hdr.dest_port == tcb->src_port &&
        seg_buf->src_ip == tcb->dest_ip &&
	seg_buf->dest_ip == tcb->src_ip)
    return tcb_id;
  }

  return -1;
}

int TcpMain() {
  std::cout << "[TCP] transport layer starting ..." << std::endl;
  tcb_table = std::vector<TcbPtr>(kMaxConnection, nullptr);

  running = true;
  input_thread = std::make_shared<std::thread>(TcpInputFromIp);
  timeout_thread = std::make_shared<std::thread>(TcpTimeout);
  tcp_running = true;

  std::cout << "[TCP] transport layer started" << std::endl;

  return 0;
}

bool TcpRunning() {
  return running;
}

int TcpStop() {
  running = false;

  timeout_thread->join();
  input_thread->join();

  for (int i = 0; i < kMaxConnection; ++i)
    tcb_table[i] = nullptr;

  std::cout << "[TCP] exited" << std::endl;
  return 0;
}
