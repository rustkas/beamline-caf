#include "worker/types.hpp"
#include "runtime/actor_pools.hpp"
#include <unordered_map>

namespace worker {

class Scheduler {
 public:
  explicit Scheduler(Pools pools) : pools_(std::move(pools)) {}

  void schedule(const StepRequest& req, std::function<void(StepResult)> cb, BlockExecutor* executor) {
    ActorPool* target = nullptr;
    switch (req.resources.cls) {
      case ResourceClass::cpu: target = pools_.cpu_pool.get(); break;
      case ResourceClass::gpu: target = pools_.gpu_pool.get(); break;
      case ResourceClass::io: target = pools_.io_pool.get(); break;
      default:
        StepResult r{}; r.status = StatusCode::error; r.error = "unknown resource class"; cb(r); return;
    }
    if (!target) {
      StepResult r{}; r.status = StatusCode::error; r.error = "no pool"; cb(r); return;
    }
    target->submit([executor, req, cb]{
      auto res = executor->execute(req);
      cb(res);
    });
  }

  size_t queue_depth(ResourceClass cls) const {
    switch (cls) {
      case ResourceClass::cpu: return pools_.cpu_pool ? pools_.cpu_pool->queue_depth() : 0;
      case ResourceClass::gpu: return pools_.gpu_pool ? pools_.gpu_pool->queue_depth() : 0;
      case ResourceClass::io: return pools_.io_pool ? pools_.io_pool->queue_depth() : 0;
      default: return 0;
    }
  }

 private:
  Pools pools_;
};

}