#include <tcp_send_buffer.h>

#include <cstring>
#include <common.h>
#include <ip.h>

int SendBuffer::PushBack(const void *data, uint32_t len) {
  if (Full())
    return -1;

  SegBufPtr seg_buf = std::make_shared<SegmentBuffer>();
  memcpy(seg_buf->segment->data, data, len);

  unsent_.push_back(seg_buf);
  return 0;
}

int SendBuffer::PushBackUnacked(SegBufPtr seg_buf) {
  if (Full())
    return -1;

  unacked_.push_back(seg_buf);
  return 0;
}

// Go-Back-N: resend all unacked
void SendBuffer::ResendUnacked() {
  for (auto p = unacked_.begin(); p != unacked_.end(); ++p) {
    std::cout << "[TCP] [RESEND] " << *p << std::endl;
    IpSend(*p);
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

SegBufPtr SendBuffer::PopUnsent() {
  if (unsent_.empty())
    return nullptr;

  SegBufPtr seg_buf = unsent_.front();
  unsent_.pop_front();
  return seg_buf;
}

SegBufPtr SendBuffer::PopUnacked() {
  if (unacked_.empty())
    return nullptr;

  SegBufPtr seg_buf = unacked_.front();
  unacked_.pop_front();
  return seg_buf;
}

uint32_t SendBuffer::Ack(uint32_t ack) {
  if (unacked_.empty())
    return 0;

  uint32_t last_acked = 0;
  while (!unacked_.empty() && unacked_.front()->segment->header.seq < ack) {
    last_acked = unacked_.front()->segment->header.seq;
    unacked_.front()->acked_time = GetCurrentTime();
    unacked_.pop_front();
  }

  if (last_acked != 0)
    retry_ = 0;

  return last_acked;
}
