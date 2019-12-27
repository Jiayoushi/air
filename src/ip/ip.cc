#include "ip.h"

#include <sys/types.h>
#include <ifaddrs.h>
#include <cstring>
#include <atomic>
#include <thread>

#include "common/common.h"
#include "common/seg.h"
#include "common/pkt.h"
#include "common/blocking_queue.h"
#include "tcp/tcp.h"
#include "overlay/overlay.h"

std::atomic<bool> running;

static int overlay_conn = -1;
static int transport_conn = -1;

BlockingQueue<PktBufPtr> ip_input;

int IpSend(SegBufPtr seg_buf) {
  PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();

  pkt_buf->packet->header.src_ip = seg_buf->src_ip;
  pkt_buf->packet->header.dest_ip = seg_buf->dest_ip;
  pkt_buf->packet->header.length = sizeof(PacketHeader) + 
                                   sizeof(SegmentHeader) +
                                   seg_buf->data_size;

  // TODO:
  pkt_buf->next_hop = seg_buf->dest_ip;

  OverlaySend(pkt_buf);

  return 0;
}

int IpStop() {
  running = false;
  return 0;
}

// TODO
static void RouteUpdate() {
  while (running) {
    PktBufPtr pkt_buf = std::make_shared<PacketBuffer>();
    pkt_buf->packet = std::make_shared<Packet>();
    pkt_buf->packet->header.dest_ip = kBroadcastIpAddr;

    OverlaySend(pkt_buf);

    std::this_thread::sleep_for(kRouteUpdateIntervalInSecs);
  }
}

int IpInputQueuePush(PktBufPtr pkt_buf) {
  ip_input.Push(pkt_buf);
  return 0;
}

PktBufPtr IpInputQueuePop() {
  return *ip_input.Pop().get();
}

static int Forward(PktBufPtr pkt_buf) {
  SegBufPtr seg_buf = std::make_shared<SegmentBuffer>();
  seg_buf->src_ip = pkt_buf->packet->header.src_ip;
  seg_buf->dest_ip = pkt_buf->packet->header.dest_ip;
  seg_buf->data_size = pkt_buf->packet->header.length - 
                       sizeof(PacketHeader) - sizeof(SegmentHeader);

  seg_buf->segment = std::make_shared<Segment>();
  memcpy(seg_buf->segment.get(), 
         pkt_buf->packet->data + sizeof(PacketHeader), 
         pkt_buf->packet->header.length - sizeof(PacketHeader));

  TcpInputQueuePush(seg_buf);
  return 0;
}

static void Input() {
  while (running) {
    PktBufPtr pkt_buf = IpInputQueuePop();

    if (!running)
      return;

    if (!pkt_buf) {
      fprintf(stderr, "[IP] received null packet");
      continue;
    }

    Forward(pkt_buf);
    std::cout << "[IP] received packet" << pkt_buf << std::endl;
  }
}

int IpInit() {
  std::cout << "[IP] network layer starting ..." << std::endl;

  running = true;

  std::thread input = std::thread(Input);
  std::thread update = std::thread(RouteUpdate);

  std::cout << "[IP] network layer started" << std::endl;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(60));

  update.join();
  input.join();

  OverlayStop();

  return 0;
}
