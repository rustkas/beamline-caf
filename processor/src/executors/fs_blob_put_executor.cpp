#include "worker/block_executor.hpp"
#include <chrono>

namespace worker {

class FsBlobPutExecutor : public BlockExecutor {
 public:
  StatusCode init([[maybe_unused]] const BlockContext& ctx) override { return StatusCode::ok; }
  StepResult execute(const StepRequest& req) override {
    auto start = std::chrono::steady_clock::now();
    StepResult res{};
    // Stub: write req.inputs["path"], req.inputs["data"] to filesystem
    res.status = StatusCode::ok;
    res.outputs = { {"path", req.inputs.at("path")}, {"bytes", std::to_string(req.inputs.at("data").size())} };
    res.retries_used = 0;
    res.error = "";
    res.latency_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    return res;
  }
  StatusCode cancel([[maybe_unused]] const BlockContext& ctx) override { return StatusCode::ok; }
  BlockMetrics metrics() const override { return BlockMetrics{}; }
};

}