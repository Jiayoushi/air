#ifndef AIR_SEND_BUFFER_H_
#define AIR_SEND_BUFFER_H_

#include <unordered_set>
#include <mutex>
#include <list>
#include <memory>

#include "timer.h"
#include "seg.h"
#include "constants.h"

/* 
 * [unacked] ---- [unsent]
 * Two seperate linkedlist.
 */
class SendBuffer {
 private:
  const uint32_t kOverlayConn;
  uint32_t retry;

  std::unordered_set<uint32_t> seq_nums_;
  std::list<SegBufPtr> unacked_;
  std::list<SegBufPtr> unsent_;


 public:
  SendBuffer(uint32_t overlay_conn):
    kOverlayConn(overlay_conn),
    seq_nums_(),
    unacked_(),
    unsent_() {}

  int PushBack(SegBufPtr seg_buf);
  void ResendUnacked();
  void SendTopUnsent();
  int Ack(uint32_t);
  void Clear();

  bool Full() const;
  bool Timeout() const;
  bool MaxRetryReached() const;
  bool ValidAck(uint32_t ack) const;
};



inline void SendBuffer::Clear() {
  retry = 0;
  seq_nums_.clear();
  unacked_.clear();
  unsent_.clear();
}

inline bool SendBuffer::Full() const {
  return unsent_.size() == kUnsentCapacity;
}

inline bool SendBuffer::ValidAck(uint32_t ack) const {
  return seq_nums_.find(ack) != seq_nums_.end();
}

inline bool SendBuffer::MaxRetryReached() const {
  return retry == kMaxSynRetry;
}

#endif
