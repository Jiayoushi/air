#ifndef AIR_RECV_BUFFER_H_
#define AIR_RECV_BUFFER_H_

#include <queue>

#include "timer.h"
#include "seg.h"
#include "constants.h"

class RecvBuffer {
 private:
  std::queue<SegBufPtr> bufs_;

 public:
  RecvBuffer():
    bufs_() {}

  void PushBack(SegBufPtr seg_buf);
  void Pop();

  SegBufPtr Front();
  bool Empty() const;
};

#endif


inline void RecvBuffer::PushBack(SegBufPtr seg_buf) {
  bufs_.push(seg_buf);
}

inline bool RecvBuffer::Empty() const {
  return bufs_.empty();
}

inline SegBufPtr RecvBuffer::Front() {
  return bufs_.front();
}

inline void RecvBuffer::Pop() {
  bufs_.pop();
}
