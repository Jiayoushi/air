#include <tcp.h>

#include <tcp_timer.h>
#include <ip.h>

#define kOutputAck  (1 << 0)
#define kOutputSyn  (1 << 1)
#define kOutputSeq  (1 << 2)  /* Consumes sequence number */
#define kOutputDat  (1 << 3)  /* Can carray data */
#define kOutputFin  (1 << 4)

int output_flags[] =
{
 /* kClosed */     kOutputSyn | kOutputSeq,
 /* kSynSent */    0,
 /* kConnected */  kOutputAck | kOutputDat,
 /* kListening */  kOutputSyn | kOutputAck,
 /* kSynRcvd */    kOutputSyn | kOutputSeq | kOutputAck,
 /* kFinWait1 */   kOutputFin | kOutputSeq
};

int TcpOutput(TcbPtr tcb) {
  SegBufPtr seg_buf;
  int flags = output_flags[tcb->state];

  /* Use the segment buffer that came along with the data if any */
  if (flags & kOutputDat)
    seg_buf = tcb->send_buffer.PopUnsent();

  /* No pre-created segment buffer (which contains data) available, create a segment buffer */
  if (seg_buf == nullptr)
    seg_buf = std::make_shared<SegmentBuffer>();

  /* Fill in headers */
  SegmentHeader &hdr = seg_buf->segment->header;
  hdr.src_port = tcb->src_port;
  hdr.dest_port = tcb->dest_port;
  hdr.flags = 0;
  hdr.length = sizeof(SegmentHeader);
  hdr.rcv_win = tcb->rcv_win;

  /* Ack */
  if (flags & kOutputAck)
    hdr.ack = tcb->rcv_nxt;

  /* Sequence number */
  if (flags & kOutputSyn)
    hdr.seq = tcb->iss;
  else if (flags & kOutputSeq)
    hdr.seq = tcb->snd_nxt;

  /* Flags */
  if (flags & kOutputSyn)
    hdr.flags |= kSyn;
  if (flags & kOutputAck)
    hdr.flags |= kAck;

  hdr.checksum = Checksum(seg_buf->segment, seg_buf->data_size + sizeof(SegmentHeader));

  /* Fill the buffer's information */
  seg_buf->dest_ip = tcb->dest_ip;
  seg_buf->src_ip = tcb->src_ip;
  seg_buf->send_time = GetCurrentTime();

  if (flags & kOutputSeq)
    tcb->send_buffer.PushBackUnacked(seg_buf);

  /* Turn on timer */
  if (flags & kOutputSeq) {
    tcb->timer_flags |= kRetransmitTimer;
    tcb->timers[kRetransmitTimer] = kRetransmitTimeout;
  }

  std::cout << "[TCP] [SEND] " << seg_buf << std::endl;
  IpSend(seg_buf);
  return 0;
}
