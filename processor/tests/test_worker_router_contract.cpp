#include <iostream>
#include <cassert>
#include <string>
#include <unordered_map>
#include "beamline/worker/core.hpp"
#include "beamline/worker/result_converter.hpp"

using namespace beamline::worker;

/**
 * Integration tests for Worker ↔ Router contract
 * 
 * These tests verify the StepResult → ExecResult conversion contract
 * as defined in apps/caf/processor/docs/ARCHITECTURE_ROLE.md#43-stepresult-contract-cp1-invariant
 * 
 * Contract Requirements:
 * - All StepResult values must be convertible to ExecResult JSON format
 * - Status mapping: StepStatus → ExecResult.status (ok→success, error→error, timeout→timeout, cancelled→cancelled)
 * - ErrorCode (1xxx-5xxx) → ExecResult.error_code (string format)
 * - ResultMetadata (trace_id, run_id, flow_id, step_id, tenant_id) → ExecResult correlation fields
 * - run_id: CP1 observability invariant for execution flow tracking
 */

void test_stepresult_to_execresult_success() {
    std::cout << "Testing StepResult → ExecResult: success status..." << std::endl;
    
    // Create StepResult with success status
    ResultMetadata meta;
    meta.trace_id = "trace_abc123";
    meta.run_id = "run_123456";        // CP1 observability invariant
    meta.flow_id = "flow_xyz789";
    meta.step_id = "step_001";
    meta.tenant_id = "tenant_123";
    
    std::unordered_map<std::string, std::string> outputs;
    outputs["status_code"] = "200";
    outputs["body"] = "{\"result\": \"success\"}";
    
    StepResult step_result = StepResult::success(meta, outputs, 150);
    
    // Convert to ExecResult format
    std::string assignment_id = "assign_123456";
    std::string request_id = "req_789012";
    std::string provider_id = "openai:gpt-4o";
    std::string job_type = "text.generate";
    
    auto exec_result = ResultConverter::to_exec_result_json(
        step_result, assignment_id, request_id, provider_id, job_type
    );
    
    // Verify required fields
    assert(exec_result["version"] == "1");
    assert(exec_result["assignment_id"] == assignment_id);
    assert(exec_result["request_id"] == request_id);
    assert(exec_result["status"] == "success");
    assert(exec_result["provider_id"] == provider_id);
    assert(exec_result["latency_ms"] == "150");
    
    // Verify correlation fields from metadata (CP1 observability invariants)
    assert(exec_result["trace_id"] == meta.trace_id);
    assert(exec_result["run_id"] == meta.run_id);
    assert(exec_result["tenant_id"] == meta.tenant_id);
    
    std::cout << "✓ StepResult → ExecResult: success status test passed" << std::endl;
}

void test_stepresult_to_execresult_error() {
    std::cout << "Testing StepResult → ExecResult: error status..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_error_123";
    meta.run_id = "run_error_456";
    meta.flow_id = "flow_error_456";
    meta.step_id = "step_error_001";
    meta.tenant_id = "tenant_error";
    
    StepResult step_result = StepResult::error_result(
        ErrorCode::network_error,
        "Connection timeout",
        meta,
        5000
    );
    
    std::string assignment_id = "assign_error_123";
    std::string request_id = "req_error_456";
    std::string provider_id = "openai:gpt-4o";
    std::string job_type = "text.generate";
    
    auto exec_result = ResultConverter::to_exec_result_json(
        step_result, assignment_id, request_id, provider_id, job_type
    );
    
    // Verify error status
    assert(exec_result["status"] == "error");
    assert(exec_result["error_code"] == "NETWORK_ERROR");
    assert(exec_result["error_message"] == "Connection timeout");
    assert(exec_result["latency_ms"] == "5000");
    
    // Verify correlation fields
    assert(exec_result["trace_id"] == meta.trace_id);
    assert(exec_result["tenant_id"] == meta.tenant_id);
    
    std::cout << "✓ StepResult → ExecResult: error status test passed" << std::endl;
}

void test_stepresult_to_execresult_timeout() {
    std::cout << "Testing StepResult → ExecResult: timeout status..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_timeout_123";
    meta.run_id = "run_timeout_456";
    meta.flow_id = "flow_timeout_456";
    meta.step_id = "step_timeout_001";
    meta.tenant_id = "tenant_timeout";
    
    StepResult step_result = StepResult::timeout_result(meta, 10000);
    
    std::string assignment_id = "assign_timeout_123";
    std::string request_id = "req_timeout_456";
    std::string provider_id = "openai:gpt-4o";
    std::string job_type = "text.generate";
    
    auto exec_result = ResultConverter::to_exec_result_json(
        step_result, assignment_id, request_id, provider_id, job_type
    );
    
    // Verify timeout status
    assert(exec_result["status"] == "timeout");
    assert(exec_result["latency_ms"] == "10000");
    
    // Verify correlation fields
    assert(exec_result["trace_id"] == meta.trace_id);
    assert(exec_result["tenant_id"] == meta.tenant_id);
    
    std::cout << "✓ StepResult → ExecResult: timeout status test passed" << std::endl;
}

void test_stepresult_to_execresult_cancelled() {
    std::cout << "Testing StepResult → ExecResult: cancelled status..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_cancelled_123";
    meta.run_id = "run_cancelled_456";
    meta.flow_id = "flow_cancelled_456";
    meta.step_id = "step_cancelled_001";
    meta.tenant_id = "tenant_cancelled";
    
    StepResult step_result = StepResult::cancelled_result(meta, 500);
    
    std::string assignment_id = "assign_cancelled_123";
    std::string request_id = "req_cancelled_456";
    std::string provider_id = "openai:gpt-4o";
    std::string job_type = "text.generate";
    
    auto exec_result = ResultConverter::to_exec_result_json(
        step_result, assignment_id, request_id, provider_id, job_type
    );
    
    // Verify cancelled status
    assert(exec_result["status"] == "cancelled");
    assert(exec_result["latency_ms"] == "500");
    
    // Verify correlation fields
    assert(exec_result["trace_id"] == meta.trace_id);
    assert(exec_result["tenant_id"] == meta.tenant_id);
    
    std::cout << "✓ StepResult → ExecResult: cancelled status test passed" << std::endl;
}

void test_stepresult_metadata_preservation() {
    std::cout << "Testing StepResult metadata preservation in ExecResult..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_full_123";
    meta.run_id = "run_full_456";
    meta.flow_id = "flow_full_456";
    meta.step_id = "step_full_001";
    meta.tenant_id = "tenant_full";
    
    StepResult step_result = StepResult::success(meta, {}, 200);
    
    std::string assignment_id = "assign_full_123";
    std::string request_id = "req_full_456";
    std::string provider_id = "openai:gpt-4o";
    std::string job_type = "text.generate";
    
    auto exec_result = ResultConverter::to_exec_result_json(
        step_result, assignment_id, request_id, provider_id, job_type
    );
    
    // Verify all metadata fields are preserved (CP1 observability invariants)
    assert(exec_result["trace_id"] == meta.trace_id);
    assert(exec_result["run_id"] == meta.run_id);
    assert(exec_result["tenant_id"] == meta.tenant_id);
    // Note: flow_id and step_id are not in ExecResult contract, but trace_id, run_id, and tenant_id are
    
    std::cout << "✓ StepResult metadata preservation test passed" << std::endl;
}

void test_stepresult_error_code_mapping() {
    std::cout << "Testing StepResult error code mapping..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_error_code_123";
    meta.run_id = "run_error_code_456";
    meta.tenant_id = "tenant_error_code";
    
    // Test various error codes
    std::vector<std::pair<ErrorCode, std::string>> error_codes = {
        {ErrorCode::invalid_input, "INVALID_INPUT"},
        {ErrorCode::missing_required_field, "MISSING_REQUIRED_FIELD"},
        {ErrorCode::execution_failed, "EXECUTION_FAILED"},
        {ErrorCode::network_error, "NETWORK_ERROR"},
        {ErrorCode::connection_timeout, "CONNECTION_TIMEOUT"},
        {ErrorCode::internal_error, "INTERNAL_ERROR"},
        {ErrorCode::system_overload, "SYSTEM_OVERLOAD"},
        {ErrorCode::cancelled_by_user, "CANCELLED_BY_USER"},
        {ErrorCode::cancelled_by_timeout, "CANCELLED_BY_TIMEOUT"}
    };
    
    for (const auto& [error_code, expected_string] : error_codes) {
        StepResult step_result = StepResult::error_result(
            error_code,
            "Test error message",
            meta,
            100
        );
        
        auto exec_result = ResultConverter::to_exec_result_json(
            step_result, "assign_123", "req_456", "provider", "job"
        );
        
        assert(exec_result["error_code"] == expected_string);
    }
    
    std::cout << "✓ StepResult error code mapping test passed" << std::endl;
}

void test_stepresult_validation() {
    std::cout << "Testing StepResult validation before conversion..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_valid_123";
    meta.run_id = "run_valid_456";
    meta.tenant_id = "tenant_valid";
    
    // Valid StepResult (success with no error code)
    StepResult valid_success = StepResult::success(meta, {}, 100);
    assert(ResultConverter::validate_result(valid_success) == true);
    
    // Valid StepResult (error with error code)
    StepResult valid_error = StepResult::error_result(
        ErrorCode::network_error,
        "Network error",
        meta,
        200
    );
    assert(ResultConverter::validate_result(valid_error) == true);
    
    // Invalid StepResult (success with error code)
    StepResult invalid_success = StepResult::success(meta, {}, 100);
    invalid_success.error_code = ErrorCode::network_error;  // Should not have error code for success
    assert(ResultConverter::validate_result(invalid_success) == false);
    
    // Invalid StepResult (error without error code)
    StepResult invalid_error;
    invalid_error.status = StepStatus::error;
    invalid_error.error_code = ErrorCode::none;  // Should have error code for error
    invalid_error.metadata = meta;
    assert(ResultConverter::validate_result(invalid_error) == false);
    
    // Invalid StepResult (negative latency)
    StepResult invalid_latency = StepResult::success(meta, {}, -1);
    assert(ResultConverter::validate_result(invalid_latency) == false);
    
    std::cout << "✓ StepResult validation test passed" << std::endl;
}

void test_stepresult_status_mapping() {
    std::cout << "Testing StepResult status mapping to ExecResult..." << std::endl;
    
    // Test all status mappings
    assert(ResultConverter::status_to_string(StepStatus::ok) == "success");
    assert(ResultConverter::status_to_string(StepStatus::error) == "error");
    assert(ResultConverter::status_to_string(StepStatus::timeout) == "timeout");
    assert(ResultConverter::status_to_string(StepStatus::cancelled) == "cancelled");
    
    // Test reverse mapping
    assert(ResultConverter::string_to_status("success") == StepStatus::ok);
    assert(ResultConverter::string_to_status("error") == StepStatus::error);
    assert(ResultConverter::string_to_status("timeout") == StepStatus::timeout);
    assert(ResultConverter::string_to_status("cancelled") == StepStatus::cancelled);
    
    // Test unknown status (should default to error)
    assert(ResultConverter::string_to_status("unknown") == StepStatus::error);
    
    std::cout << "✓ StepResult status mapping test passed" << std::endl;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "Worker ↔ Router Contract Integration Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_stepresult_status_mapping();
        test_stepresult_to_execresult_success();
        test_stepresult_to_execresult_error();
        test_stepresult_to_execresult_timeout();
        test_stepresult_to_execresult_cancelled();
        test_stepresult_metadata_preservation();
        test_stepresult_error_code_mapping();
        test_stepresult_validation();
        
        std::cout << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << "✓ All Worker ↔ Router contract tests passed!" << std::endl;
        std::cout << "=========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}

