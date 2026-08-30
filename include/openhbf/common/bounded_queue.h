#pragma once

#include <cstddef>
#include <deque>
#include <stdexcept>
#include <utility>

namespace openhbf {

// A capacity-limited FIFO used at architectural backpressure boundaries.
// try_push() is deliberately transactional: a full queue leaves both the
// queue and the caller-owned value unchanged.
template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
      throw std::invalid_argument("BoundedQueue capacity must be greater than zero");
    }
  }

  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
  [[nodiscard]] bool full() const noexcept { return entries_.size() == capacity_; }

  [[nodiscard]] bool try_push(const T& value) {
    if (full()) {
      return false;
    }
    entries_.push_back(value);
    return true;
  }

  [[nodiscard]] bool try_push(T&& value) {
    if (full()) {
      return false;
    }
    entries_.push_back(std::move(value));
    return true;
  }

  template <typename... Args>
  [[nodiscard]] bool try_emplace(Args&&... args) {
    if (full()) {
      return false;
    }
    entries_.emplace_back(std::forward<Args>(args)...);
    return true;
  }

  T& front() {
    require_nonempty();
    return entries_.front();
  }

  const T& front() const {
    require_nonempty();
    return entries_.front();
  }

  void pop() {
    require_nonempty();
    entries_.pop_front();
  }

  void clear() noexcept { entries_.clear(); }

 private:
  void require_nonempty() const {
    if (entries_.empty()) {
      throw std::underflow_error("BoundedQueue operation requires a non-empty queue");
    }
  }

  std::size_t capacity_;
  std::deque<T> entries_;
};

}  // namespace openhbf
