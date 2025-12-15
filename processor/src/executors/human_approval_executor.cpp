#include "worker/block_executor.hpp"
#include <chrono>

namespace worker {

class HumanApprovalExecutor : public BlockExecutor {
 public:
  StatusCode init([[maybe_unused]] const BlockContext& ctx) override { return StatusCode::ok; }
  StepResult execute(const StepRequest& req) override {
    auto start = std::chrono::steady_clock::now();
    StepResult res{};
    // Stub: emit event and return pending status as ok with info
    res.status = StatusCode::ok;
    res.outputs = { {"approval", "requested"}, {"actor", req.ctx.tenant_id} };
    res.retries_used = 0;
    res.error = "";
    res.latency_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    return res;
  }
  StatusCode cancel([[maybe_unused]] const BlockContext& ctx) override { return StatusCode::ok; }
  BlockMetrics metrics() const override { return BlockMetrics{}; }
};

}