#include "srt_client.h"

#include <stdlib.h>
#include <thread>
#include <queue>
#include <iterator>
#include <mutex>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <list>
#include <condition_variable>

#include "../common/seg.h"
#include "../common/timer.h"
#include "../common/blocking_queue.h"

// Client States
#define kClosed          0
#define kSynSent         1
#define kConnected       2
#define kFinWait         3

#define kMaxSynRetry     3
#define kMaxConnection   1024


// The connection to network below
static int overlay_conn;



// Store segments in send buffer linked list
struct SegmentBuffer {
  std::shared_ptr<Segment> segment;
  std::chrono::milliseconds send_time;

  SegmentBuffer() = default;
  SegmentBuffer(std::shared_ptr<Segment> s):
   segment(s), send_time() {}
};

typedef std::list<SegmentBuffer> SendBuffer;

/*
 * Client transport control block, the client side of a SRT connection
 * uses this data structure to keep track of connection information
 *
 * The server_id serves as the purpose of 'ip address' when demultiplexing.
 * Current implementation supports data segments flow from client to server.
 *
 */
struct ClientTcb {
  unsigned int server_id;
  unsigned int server_port;
  
  unsigned int client_id;
  unsigned int client_port;

  unsigned int state;

  unsigned int initial_seq_num;
  
  unsigned int next_client_seq_num;
  unsigned int next_server_seq_num;
  

  std::mutex lock;

  /*
   * A condition variable used to wait for message either from timeouts or from a new incoming segment
   */
  std::condition_variable waiting;

  // Connection
  unsigned int syn_retry;



  /*
   * [unacked][unsent]
   */
  SendBuffer send_buffer; 

  // First unsent segment
  SendBuffer::iterator unsent;  


  //BlockingQueue<Segment> recv_buffer;

  ClientTcb(): 
    server_id(0), server_port(0), client_id(0), client_port(0), 
    state(kClosed), initial_seq_num(rand() % std::numeric_limits<unsigned int>::max()),
    next_client_seq_num(0), next_server_seq_num(0),
    lock(),  waiting(),
    syn_retry(0), send_buffer(), unsent(send_buffer.end()) {
    //recv_buffer() {
  }
};



// A global variable control the running of all threads.
std::atomic<bool> running;

std::mutex tcb_table_lock;
static std::vector<std::shared_ptr<ClientTcb>> tcb_table(kMaxConnection, nullptr);

/*
 * A thread responsible for sending timeout
 */
static std::shared_ptr<std::thread> timeout_thread;

/*
 * A thread responsible for receiving segments from network stack
 */
static std::shared_ptr<std::thread> input_thread;


// Time interval between checking timeouts.
const static std::chrono::duration<double, std::milli> kTimeoutSleepTime(500);

// If a Syn Ack is not receved after 'kSynTimeout', resend Syn
const static std::chrono::milliseconds kSynTimeout(6000);   // 6 seconds



int SrtClientSock(unsigned int client_port) {
  tcb_table_lock.lock();
  for (int i = 0; i < kMaxConnection; ++i) {
    if (tcb_table[i] != nullptr) {
      tcb_table[i] = std::make_shared<ClientTcb>();
      tcb_table[i]->client_port = client_port;
      tcb_table_lock.unlock();
      return i;
    }
  }

  return -1;
}

static std::shared_ptr<Segment> CreateSynSegment(std::shared_ptr<ClientTcb> tcb) {
  std::shared_ptr<Segment> seg = std::make_shared<Segment>();

  seg->header.src_port = tcb->client_port;
  seg->header.dest_port = tcb->server_port;
  seg->header.seq_num = tcb->initial_seq_num; 
  seg->header.ack_num = 0;
  seg->header.length = sizeof(Segment);
  seg->header.type = kSyn;
  seg->header.rcv_win = 0;
  seg->header.checksum = Checksum(seg);

  return seg;
}


// Establish connection
int SrtClientConnect(int sockfd, unsigned int server_port) {
  std::shared_ptr<ClientTcb> tcb = tcb_table[sockfd];
  tcb->server_port = server_port;

  if (tcb->state != kClosed)
    return kFailure;

  std::shared_ptr<Segment> syn = CreateSynSegment(tcb);

  tcb->next_client_seq_num = syn->header.seq_num + 1;
  tcb->state = kSynSent;
  tcb->send_buffer.emplace_back(syn);


  // Wait until connection timeout or success
  std::unique_lock<std::mutex> lk(tcb->lock);
  tcb->waiting.wait(lk,
    [&tcb] {
      return tcb->state == kConnected || tcb->syn_retry == kMaxSynRetry;
    });

  if (tcb->state == kConnected)
    return kSuccess;

  if (tcb->syn_retry == kMaxSynRetry)
    return kFailure;

  return kFailure;
}



static void ProcessTimeouts(std::shared_ptr<ClientTcb> tcb) {
  for (SendBuffer::iterator it = tcb->send_buffer.begin(); it != tcb->unsent; ++it) {
    SegmentBuffer &seg_buf = *it;

    // TODO: or we can set it as 1, 2, 3, 4... units of time, measured by timeout unit
    if (GetCurrentTime() - seg_buf.send_time > kSynTimeout) {
      if (tcb->syn_retry == kMaxSynRetry) {
        tcb->waiting.notify_one();
        continue;
      }

      SnpSendSegment(overlay_conn, seg_buf.segment);
      seg_buf.send_time = GetCurrentTime();
    }
  }
}

static void ProcessUnsent(std::shared_ptr<ClientTcb> tcb) {
  if (tcb->unsent != tcb->send_buffer.end()) {
    SegmentBuffer &seg_buf = *tcb->unsent;

    SnpSendSegment(overlay_conn, seg_buf.segment);

    seg_buf.send_time = GetCurrentTime();
    ++tcb->unsent;
  }
}

static void Timeout() {
  while (running) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      if (tcb_table[tcb_id] == nullptr)
        continue;

      std::shared_ptr<ClientTcb> tcb = tcb_table[tcb_id];
      std::lock_guard<std::mutex> lock(tcb->lock);

      ProcessTimeouts(tcb);
      ProcessUnsent(tcb);
    }

    std::this_thread::sleep_for(kTimeoutSleepTime);
  }
}


// Destination port number, the source IP address, and the source port number.

// TODO: right now id is not used to demultiplex, in theory SrtConnect should
// also have a server node id field
static std::shared_ptr<ClientTcb> Demultiplex(std::shared_ptr<Segment> seg) {
  for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
    if (seg->header.src_port == tcb_table[tcb_id]->server_port &&
        seg->header.dest_port == tcb_table[tcb_id]->client_port)
    return tcb_table[tcb_id];
  }

  return nullptr;
}

static void input(std::shared_ptr<Segment> seg) {
  std::shared_ptr<ClientTcb> tcb = Demultiplex(seg);
  if (tcb == nullptr)
    return;

  switch (tcb->state) {
    case kClosed:
      std::cerr << "input: segment received for closed connection." << std::endl;
      break;

    case kSynSent:
	  if (tcb->syn_retry == kMaxSynRetry) {
	    tcb->state = kClosed;
        tcb->waiting.notify_one();
        break;
	  }

	  // Check Type
	  if (seg->header.type != kSynAck)
	    break;

	  // Check ack number
	  if (seg->header.ack_num != tcb->next_client_seq_num)
	    break;

	  tcb->state = kConnected;
	  tcb->next_server_seq_num = seg->header.seq_num + 1;

	  tcb->send_buffer.pop_back();
	  assert(tcb->send_buffer.empty());

	  // TODO: send an ack back

      tcb->waiting.notify_one();
      break;

    // TODO:
    case kConnected:
    case kFinWait:
    default:
      break;
  }
}

static void InputFromIp() {
  while (running) {
    std::shared_ptr<Segment> seg = SnpRecvSegment(overlay_conn);

    input(seg);
  }
}



void SrtClientInit(int conn) {
  overlay_conn = conn;
  running = true;

  input_thread = std::make_shared<std::thread>(InputFromIp);
  timeout_thread = std::make_shared<std::thread>(Timeout);
}



int SrtClientClose(int sockfd) {
  tcb_table[sockfd] = nullptr;
  return kSuccess;
}



static void NotifyShutdown() {
  running = false;
}

void SrtClientShutdown() {
  NotifyShutdown();

  timeout_thread->join();
  input_thread->join();
}

int SrtClientDisconnect(int sockfd) {
  return 0;
}

int SrtClientSend(int sockfd, void *data, unsigned int length) {
  return 0;
}
