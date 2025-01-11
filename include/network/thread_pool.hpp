#pragma once

#include "task_queue.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace network {

class ThreadPool {
  using writelock = std::unique_lock<std::shared_mutex>;
  using readlock = std::shared_lock<std::shared_mutex>;

public:
  ThreadPool() = default;
  ~ThreadPool() { terminate(); }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  void init(int num);
  void terminate();
  void cancel();
  bool isInitialised() const;
  bool isRunning() const;
  size_t getSize() const;

  template <class F, class... Args>
  auto async(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
    using ReturnType = decltype(f(args...));
    using PackagedTask = std::packaged_task<ReturnType()>;

    {
      readlock lock(_mutex);
      if (_hasStopped || _isCancelled) {
        throw std::runtime_error(
            "Thread pool has been terminated or cancelled");
      }
    }

    auto bindFunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = std::make_shared<PackagedTask>(std::move(bindFunc));
    auto future = task->get_future();

    _tasks.emplace([task]() { (*task)(); });
    _condition.notify_one();
    return future;
  }

private:
  void spawn();
  bool isRunningImpl() const;

  std::atomic<bool> _isInitialised{false};
  std::atomic<bool> _isCancelled{false};
  std::atomic<bool> _hasStopped{false};
  std::vector<std::thread> _workers;
  mutable std::shared_mutex _mutex;
  mutable std::once_flag _once;
  mutable std::condition_variable_any _condition;
  TaskQueue<std::function<void()>> _tasks;
};

} // namespace network
