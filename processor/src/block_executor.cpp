#include "beamline/worker/core.hpp"
#include <chrono>

namespace beamline {
namespace worker {

// Base BlockExecutor implementation with common functionality
class BaseBlockExecutor : public BlockExecutor {
public:
    BaseBlockExecutor(std::string block_type, ResourceClass resource_class)
        : block_type_(std::move(block_type)), resource_class_(resource_class) {}
    
    std::string block_type() const override { return block_type_; }
    ResourceClass resource_class() const override { return resource_class_; }
    
    caf::expected<void> init(const BlockContext& ctx) override {
        context_ = ctx;
        return caf::unit;
    }
    
    // Default implementation uses stored context_
    caf::expected<StepResult> execute(const StepRequest& req, const BlockContext& ctx) override {
        // Use provided context (may override stored context_)
        return execute_impl(req, ctx);
    }
    
    // Legacy execute without context - uses stored context_
    caf::expected<StepResult> execute(const StepRequest& req) override {
        return execute_impl(req, context_);
    }
    
    caf::expected<void> cancel(const std::string& step_id) override {
        // Default implementation - can be overridden by specific executors
        return caf::unit;
    }
    
    BlockMetrics metrics() const override {
        return metrics_;
    }
    
protected:
    std::string block_type_;
    ResourceClass resource_class_;
    BlockContext context_;
    mutable BlockMetrics metrics_;
    
    // Subclasses should override this method
    virtual caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) = 0;
    
    void record_success(int64_t latency_ms, int64_t cpu_time_ms = 0, int64_t mem_bytes = 0) {
        metrics_.latency_ms = latency_ms;
        metrics_.cpu_time_ms = cpu_time_ms;
        metrics_.mem_bytes = mem_bytes;
        metrics_.success_count++;
    }
    
    void record_error(int64_t latency_ms) {
        metrics_.latency_ms = latency_ms;
        metrics_.error_count++;
    }
    
    bool validate_required_inputs(const StepRequest& req, const std::vector<std::string>& required_inputs) {
        for (const auto& input : required_inputs) {
            if (req.inputs.find(input) == req.inputs.end()) {
                return false;
            }
        }
        return true;
    }
    
    std::string get_input_or_default(const StepRequest& req, const std::string& key, const std::string& default_value = "") {
        auto it = req.inputs.find(key);
        return it != req.inputs.end() ? it->second : default_value;
    }
};

} // namespace worker
} // namespace beamline