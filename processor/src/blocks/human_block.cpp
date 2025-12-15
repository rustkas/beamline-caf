#include "beamline/worker/core.hpp"
#include <chrono>
#include <thread>

namespace beamline {
namespace worker {

class HumanBlockExecutor : public BaseBlockExecutor {
public:
    HumanBlockExecutor() : BaseBlockExecutor("human.approval", ResourceClass::cpu) {}
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override {
        auto start_time = std::chrono::steady_clock::now();
        auto metadata = BlockExecutor::metadata_from_context(ctx);
        
        // Validate required inputs
        std::vector<std::string> required_inputs = {"approval_type", "description"};
        if (!validate_required_inputs(req, required_inputs)) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::missing_required_field,
                "Missing required inputs: approval_type, description",
                metadata,
                latency_ms
            );
        }
        
        std::string approval_type = req.inputs.at("approval_type");
        std::string description = req.inputs.at("description");
        std::string approvers = get_input_or_default(req, "approvers");
        int64_t timeout_seconds = std::stoll(get_input_or_default(req, "timeout_seconds", "3600")); // 1 hour default
        
        try {
            // Generate unique approval request ID
            std::string approval_id = generate_approval_id();
            
            // Create approval event
            std::unordered_map<std::string, std::string> approval_event = {
                {"approval_id", approval_id},
                {"approval_type", approval_type},
                {"description", description},
                {"approvers", approvers},
                {"timeout_seconds", std::to_string(timeout_seconds)},
                {"tenant_id", metadata.tenant_id},
                {"flow_id", metadata.flow_id},
                {"step_id", metadata.step_id},
                {"trace_id", metadata.trace_id},
                {"requested_at", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
                {"status", "pending"}
            };
            
            // In a real implementation, this would:
            // 1. Publish approval event to NATS
            // 2. Store approval request in database
            // 3. Wait for approval response (with timeout)
            // 4. Return result based on approval decision
            
            // For now, we'll simulate a pending approval that times out
            // In sandbox mode, we'll simulate an immediate approval
            if (ctx.sandbox) {
                auto end_time = std::chrono::steady_clock::now();
                auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                
                std::unordered_map<std::string, std::string> outputs;
                outputs["approval_id"] = approval_id;
                outputs["decision"] = "approved";
                outputs["approved_by"] = "sandbox_user";
                outputs["approved_at"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                outputs["reason"] = "Sandbox approval";
                
                record_success(latency_ms);
                return StepResult::success(metadata, outputs, latency_ms);
            }
            
            // Simulate waiting for approval (would be async in real implementation)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            // Simulate timeout (in real implementation, this would be handled by timeout mechanism)
            if (latency_ms > timeout_seconds * 1000) {
                std::unordered_map<std::string, std::string> outputs;
                outputs["approval_id"] = approval_id;
                
                record_error(latency_ms);
                return StepResult::timeout_result(metadata, latency_ms);
            }
            
            // For demonstration, simulate a pending approval
            std::unordered_map<std::string, std::string> outputs;
            outputs["approval_id"] = approval_id;
            outputs["status"] = "pending";
            outputs["message"] = "Approval request submitted. Waiting for human approval.";
            outputs["timeout_seconds"] = std::to_string(timeout_seconds);
            
            record_success(latency_ms);
            return StepResult::success(metadata, outputs, latency_ms);
            
        } catch (const std::exception& e) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::execution_failed,
                "Human approval error: " + std::string(e.what()),
                metadata,
                latency_ms
            );
        }
    }
    
private:
    std::string generate_approval_id() {
        // Generate a unique approval ID
        static int counter = 0;
        return "approval_" + std::to_string(++counter) + "_" + 
               std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
};

} // namespace worker
} // namespace beamline