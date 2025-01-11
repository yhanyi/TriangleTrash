#include "../../include/network/thread_pool.hpp"

namespace network {

void ThreadPool::init(int num) {
  std::call_once(_once, [this, num]() {
    writelock lock(_mutex);
    _hasStopped.store(false);
    _isCancelled.store(false);
    _workers.reserve(num);

    for (int i = 0; i < num; ++i) {
      _workers.emplace_back(std::bind(&ThreadPool::spawn, this));
    }
    _isInitialised.store(true);
  });
}

void ThreadPool::spawn() {
  while (true) {
    std::function<void()> task;
    bool hasTask = false;

    {
      writelock lock(_mutex);
      _condition.wait(lock, [this, &hasTask, &task] {
        hasTask = _tasks.tryPop(task);
        return _isCancelled.load() || _hasStopped.load() || hasTask;
      });
    }

    if (_isCancelled.load() || (_hasStopped.load() && !hasTask)) {
      return;
    }

    if (hasTask) {
      task();
    }
  }
}

void ThreadPool::terminate() {
  {
    writelock lock(_mutex);
    if (!isRunningImpl())
      return;
    _hasStopped.store(true);
  }
  _condition.notify_all();
  for (auto &worker : _workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::cancel() {
  {
    writelock lock(_mutex);
    if (!isRunningImpl())
      return;
    _isCancelled.store(true);
  }
  _tasks.clear();
  _condition.notify_all();
  for (auto &worker : _workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

bool ThreadPool::isInitialised() const {
  readlock lock(_mutex);
  return _isInitialised.load();
}

bool ThreadPool::isRunningImpl() const {
  return _isInitialised.load() && !(_hasStopped.load()) &&
         !(_isCancelled.load());
}

bool ThreadPool::isRunning() const {
  readlock lock(_mutex);
  return isRunningImpl();
}

size_t ThreadPool::getSize() const {
  readlock lock(_mutex);
  return _workers.size();
}

} // namespace network
