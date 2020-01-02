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
  uint32_t retry_;

  std::list<SegBufPtr> unacked_;
  std::list<SegBufPtr> unsent_;
 public:
  SendBuffer():
    retry_(0),
    unacked_(),
    unsent_() {}

  int PushBack(SegBufPtr seg_buf);
  void ResendUnacked();
  void SendTopUnsent();
  uint32_t SendUnsent();
  size_t Ack(uint32_t);
  void Clear();
  void PopUnsentFront();

  size_t Unacked() { return unacked_.size(); }
  size_t Unsent() { return unsent_.size(); }
  bool Full() const;
  bool Timeout() const;
  bool MaxRetryReached() const;
};

inline void SendBuffer::Clear() {
  retry_ = 0;
  unacked_.clear();
  unsent_.clear();
}

inline bool SendBuffer::Full() const {
  return unacked_.size() + unsent_.size() == kGbnWindowSize;
}

inline bool SendBuffer::MaxRetryReached() const {
  return retry_ == kMaxSynRetry;
}

#endif
