#include "send_buffer.h"

int SendBuffer::PushBack(SegBufPtr seg_buf) {
  if (unsent_.size() == kUnsentCapacity)
    return -1;

  unsent_.push_back(seg_buf);

  while (unacked_.size() < kGbnWindowSize)
    SendTopUnsent();

  return 0;
}

// Go-Back-N: resend all unacked
void SendBuffer::ResendUnacked() {
  for (auto p = unacked_.begin(); p != unacked_.end(); ++p) {
    SnpSendSegment(kOverlayConn, (*p)->segment);

    (*p)->send_time = GetCurrentTime();
  }

  ++retry;
}

bool SendBuffer::Timeout() const {
  if (unacked_.empty())
    return false;

  return GetCurrentTime() - unacked_.front()->send_time >
         std::chrono::milliseconds(kTimeoutIntervalInMs);
}

void SendBuffer::SendTopUnsent() {
  if (unsent_.empty())
    return;

  SegBufPtr seg_buf = unsent_.front();
  SnpSendSegment(kOverlayConn, seg_buf->segment); 

  seq_nums_.insert(seg_buf->segment->header.seq);
  unacked_.push_back(seg_buf);
  unsent_.pop_front();
}

/*
 * Given a received ack,
 *   If this ack corresponds to a segment's sequence number,
 *     then all segments whose sequence numbers <= this ack is acked.
 *
 * Return Value: 
 *   Success: the current oldest unacked segment's sequence number after ack
 *   Failure: -1
 */
int SendBuffer::Ack(uint32_t ack) {
  if (!ValidAck(ack))
    return -1;

  SegBufPtr last_seg_buf = nullptr;

  while (!unacked_.empty()
      && unacked_.front()->segment->header.seq <= ack) {
    seq_nums_.erase(unacked_.front()->segment->header.seq);
    last_seg_buf = unacked_.front();
    unacked_.pop_front();
  }

  while (unacked_.size() < kGbnWindowSize && !unsent_.empty())
    SendTopUnsent();

  if (!unacked_.empty())
    return unacked_.front()->segment->header.seq;

  return -1;
}
