#pragma once

#include <mutex>
#include <queue>
#include <shared_mutex>

namespace network {

template <typename T> class TaskQueue : protected std::queue<T> {
public:
  using writelock = std::unique_lock<std::shared_mutex>;
  using readlock = std::shared_lock<std::shared_mutex>;

  TaskQueue() = default;
  ~TaskQueue() { clear(); }

  TaskQueue(const TaskQueue &) = delete;
  TaskQueue(TaskQueue &&) = delete;
  TaskQueue &operator=(const TaskQueue &) = delete;
  TaskQueue &operator=(TaskQueue &&) = delete;

  bool isEmpty() const {
    readlock lock(_mutex);
    return std::queue<T>::empty();
  }

  size_t getSize() const {
    readlock lock(_mutex);
    return std::queue<T>::size();
  }

  void clear() {
    writelock lock(_mutex);
    while (!std::queue<T>::empty()) {
      std::queue<T>::pop();
    }
  }

  void push(const T &object) {
    writelock lock(_mutex);
    std::queue<T>::push(object);
  }

  template <typename... Args> void emplace(Args &&...args) {
    writelock lock(_mutex);
    std::queue<T>::emplace(std::forward<Args>(args)...);
  }

  bool tryPop(T &holder) {
    writelock lock(_mutex);
    if (std::queue<T>::empty()) {
      return false;
    }
    holder = std::move(std::queue<T>::front());
    std::queue<T>::pop();
    return true;
  }

private:
  mutable std::shared_mutex _mutex;
};

} // namespace network
