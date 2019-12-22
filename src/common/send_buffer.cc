#include "send_buffer.h"

#include "common.h"

int SendBuffer::PushBack(SegBufPtr seg_buf) {
  if (Full())
    return -1;

  unsent_.push_back(seg_buf);
  SendUnsent();

  return 0;
}

// Go-Back-N: resend all unacked
void SendBuffer::ResendUnacked() {
  for (auto p = unacked_.begin(); p != unacked_.end(); ++p) {
    SnpSendSegment(kOverlayConn, (*p));

    CDEBUG << "RESEND: " << *p << std::endl;
    (*p)->send_time = GetCurrentTime();
  }

  ++retry_;
}

bool SendBuffer::Timeout() const {
  if (unacked_.empty())
    return false;

  return GetCurrentTime() - unacked_.front()->send_time >
         std::chrono::milliseconds(kTimeoutIntervalInMs);
}

void SendBuffer::PopUnsentFront() {
  unacked_.pop_front();
}

void SendBuffer::SendTopUnsent() {
  if (unsent_.empty())
    return;

  SegBufPtr seg_buf = unsent_.front();
  SnpSendSegment(kOverlayConn, seg_buf);
  CDEBUG << "Send: " << seg_buf << std::endl;

  unacked_.push_back(seg_buf);
  unsent_.pop_front();
}

uint32_t SendBuffer::SendUnsent() {
  uint32_t count = 0;
  while (!unsent_.empty()) {
    SendTopUnsent();
    ++count;
  }
  return count;
}

size_t SendBuffer::Ack(uint32_t ack) {
  if (unacked_.empty()) {
    CDEBUG << "unacked empty" << std::endl;
    return -1;
  }

  size_t count = 0;
  while (!unacked_.empty()
      && unacked_.front()->segment->header.seq < ack) {

    unacked_.front()->acked_time = GetCurrentTime();
    unacked_.pop_front();
    ++count;
  }

  if (count != 0)
    retry_ = 0;

  return count;
}
