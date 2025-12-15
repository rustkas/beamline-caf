#pragma once
#include "worker/block_executor.hpp"
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>

namespace worker {

class ActorPool {
 public:
  explicit ActorPool(int concurrency) : concurrency_(concurrency), stop_(false) {
    for (int i = 0; i < concurrency_; ++i) {
      threads_.emplace_back([this]() { this->run(); });
    }
  }
  ~ActorPool() {
    {
      std::unique_lock<std::mutex> lk(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto &t : threads_) t.join();
  }

  using Task = std::function<void()>;
  void submit(Task t) {
    {
      std::unique_lock<std::mutex> lk(mu_);
      q_.push(std::move(t));
    }
    cv_.notify_one();
  }

  size_t queue_depth() const {
    std::unique_lock<std::mutex> lk(mu_);
    return q_.size();
  }

 private:
  void run() {
    while (true) {
      Task task;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [this]{ return stop_ || !q_.empty(); });
        if (stop_ && q_.empty()) return;
        task = std::move(q_.front());
        q_.pop();
      }
      try {
        task();
      } catch (...) {
        // Swallow exceptions in worker threads to prevent thread termination
        // In production, exceptions should be logged and handled appropriately
      }
    }
  }

  int concurrency_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::queue<Task> q_;
  bool stop_;
  std::vector<std::thread> threads_;
};

struct Pools {
  std::unique_ptr<ActorPool> cpu_pool;
  std::unique_ptr<ActorPool> gpu_pool;
  std::unique_ptr<ActorPool> io_pool;
};

}