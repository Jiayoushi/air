#ifndef AIR_BLOCKING_QUEUE_H_
#define AIR_BLOCKING_QUEUE_H_

#include <memory>
#include <cassert>
#include <deque>
#include <condition_variable>
#include <mutex>

template <typename T>
class BlockingQueue {
 public:
  BlockingQueue():
    mutex_(),
    queue_() {

  }

  void Push(const T &item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(item);
    not_empty_.notify_all();
  }

  void Push(T &&item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.emplace_back(item);
    not_empty_.notify_all();   
  }

  template <typename Rep, typename Period = std::ratio<1>>
  std::shared_ptr<T> Pop(const std::chrono::duration<Rep, Period> &rel_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait_for(lock, rel_time, [&]{return !queue_.empty();});

    if (queue_.empty()) {
      return nullptr;
    }

    std::shared_ptr<T> front = std::make_shared<T>(queue_.front());
    queue_.pop_back();

    return front;
  }

  std::shared_ptr<T> Pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [&]{return !queue_.empty();});

    assert(!queue_.empty());

    std::shared_ptr<T> front = std::make_shared<T>(queue_.front());
    queue_.pop_back();

    return front;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::deque<T> queue_;
};

#endif
