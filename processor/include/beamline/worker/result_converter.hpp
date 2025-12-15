#pragma once

#include "beamline/worker/core.hpp"
#include <string>
#include <unordered_map>

namespace beamline {
namespace worker {

// Converter utilities for StepResult <-> ExecResult (NATS/Proto format)
// Ensures compatibility with ExecResult contract without breaking changes

class ResultConverter {
public:
    // Convert StepStatus to ExecResult status string
    // Contract: "success" | "error" | "timeout" | "cancelled"
    static std::string status_to_string(StepStatus status) {
        switch (status) {
            case StepStatus::ok:
                return "success";
            case StepStatus::error:
                return "error";
            case StepStatus::timeout:
                return "timeout";
            case StepStatus::cancelled:
                return "cancelled";
            default:
                return "error";
        }
    }
    
    // Convert ExecResult status string to StepStatus
    static StepStatus string_to_status(const std::string& status_str) {
        if (status_str == "success") {
            return StepStatus::ok;
        } else if (status_str == "error") {
            return StepStatus::error;
        } else if (status_str == "timeout") {
            return StepStatus::timeout;
        } else if (status_str == "cancelled") {
            return StepStatus::cancelled;
        }
        return StepStatus::error;  // Default to error for unknown status
    }
    
    // Convert ErrorCode to machine-readable string code
    static std::string error_code_to_string(ErrorCode code) {
        switch (code) {
            case ErrorCode::none:
                return "NONE";
            case ErrorCode::invalid_input:
                return "INVALID_INPUT";
            case ErrorCode::missing_required_field:
                return "MISSING_REQUIRED_FIELD";
            case ErrorCode::invalid_format:
                return "INVALID_FORMAT";
            case ErrorCode::execution_failed:
                return "EXECUTION_FAILED";
            case ErrorCode::resource_unavailable:
                return "RESOURCE_UNAVAILABLE";
            case ErrorCode::permission_denied:
                return "PERMISSION_DENIED";
            case ErrorCode::quota_exceeded:
                return "QUOTA_EXCEEDED";
            case ErrorCode::network_error:
                return "NETWORK_ERROR";
            case ErrorCode::connection_timeout:
                return "CONNECTION_TIMEOUT";
            case ErrorCode::http_error:
                return "HTTP_ERROR";
            case ErrorCode::internal_error:
                return "INTERNAL_ERROR";
            case ErrorCode::system_overload:
                return "SYSTEM_OVERLOAD";
            case ErrorCode::cancelled_by_user:
                return "CANCELLED_BY_USER";
            case ErrorCode::cancelled_by_timeout:
                return "CANCELLED_BY_TIMEOUT";
            default:
                return "UNKNOWN_ERROR";
        }
    }
    
    // Convert StepResult to ExecResult JSON format (for NATS publishing)
    // Returns JSON object as string (can be used with any JSON library)
    // Format matches ExecResult contract from API_CONTRACTS.md
    static std::unordered_map<std::string, std::string> to_exec_result_json(
        const StepResult& result,
        const std::string& assignment_id,
        const std::string& request_id,
        const std::string& provider_id,
        const std::string& job_type) {
        
        std::unordered_map<std::string, std::string> exec_result;
        
        // Required fields per ExecResult contract
        exec_result["version"] = "1";
        exec_result["assignment_id"] = assignment_id;
        exec_result["request_id"] = request_id;
        exec_result["status"] = status_to_string(result.status);
        exec_result["provider_id"] = provider_id;
        exec_result["job"] = "{\"type\":\"" + job_type + "\"}";
        exec_result["latency_ms"] = std::to_string(result.latency_ms);
        // Cost calculation is not implemented in CP1
        // In CP2, this would calculate actual cost based on resource usage and provider pricing
        exec_result["cost"] = "0.0";
        
        // Optional fields from metadata (CP1 correlation fields)
        if (!result.metadata.trace_id.empty()) {
            exec_result["trace_id"] = result.metadata.trace_id;
        }
        if (!result.metadata.run_id.empty()) {
            exec_result["run_id"] = result.metadata.run_id;
        }
        if (!result.metadata.tenant_id.empty()) {
            exec_result["tenant_id"] = result.metadata.tenant_id;
        }
        
        // Add error information if present
        if (result.status == StepStatus::error) {
            exec_result["error_code"] = error_code_to_string(result.error_code);
            if (!result.error_message.empty()) {
                exec_result["error_message"] = result.error_message;
            }
        }
        
        // Add outputs if present (for success cases)
        if (result.status == StepStatus::ok && !result.outputs.empty()) {
            // Outputs can be included in a nested "result" field if needed
            // For now, we keep them separate for compatibility
        }
        
        return exec_result;
    }
    
    // Validate StepResult before conversion
    // Ensures all required fields are present and valid
    static bool validate_result(const StepResult& result) {
        // Check that metadata is populated for tracing
        if (result.metadata.trace_id.empty() && result.metadata.flow_id.empty()) {
            // Warning: missing trace/flow IDs, but not blocking
        }
        
        // Check that error code matches status
        if (result.status == StepStatus::ok && result.error_code != ErrorCode::none) {
            return false;  // Invalid: success status with error code
        }
        
        if (result.status == StepStatus::error && result.error_code == ErrorCode::none) {
            return false;  // Invalid: error status without error code
        }
        
        // Check latency is non-negative
        if (result.latency_ms < 0) {
            return false;
        }
        
        return true;
    }
};

} // namespace worker
} // namespace beamline

