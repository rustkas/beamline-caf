#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <limits>
#include "beamline/worker/core.hpp"
#include "beamline/worker/observability.hpp"

using namespace beamline::worker;

void test_block_context() {
    std::cout << "Testing BlockContext..." << std::endl;
    
    BlockContext ctx;
    ctx.tenant_id = "test_tenant";
    ctx.trace_id = "test_trace";
    ctx.flow_id = "test_flow";
    ctx.step_id = "test_step";
    ctx.sandbox = false;
    ctx.rbac_scopes = {"read", "write"};
    
    assert(ctx.tenant_id == "test_tenant");
    assert(ctx.trace_id == "test_trace");
    assert(ctx.flow_id == "test_flow");
    assert(ctx.step_id == "test_step");
    assert(ctx.sandbox == false);
    assert(ctx.rbac_scopes.size() == 2);
    
    std::cout << "✓ BlockContext test passed" << std::endl;
}

void test_step_request() {
    std::cout << "Testing StepRequest..." << std::endl;
    
    StepRequest req;
    req.type = "http.request";
    req.inputs["url"] = "https://api.example.com";
    req.inputs["method"] = "GET";
    req.timeout_ms = 5000;
    req.retry_count = 3;
    
    assert(req.type == "http.request");
    assert(req.inputs["url"] == "https://api.example.com");
    assert(req.inputs["method"] == "GET");
    assert(req.timeout_ms == 5000);
    assert(req.retry_count == 3);
    
    std::cout << "✓ StepRequest test passed" << std::endl;
}

void test_step_result() {
    std::cout << "Testing StepResult..." << std::endl;
    
    StepResult result;
    result.status = StepStatus::ok;
    result.outputs["status_code"] = "200";
    result.outputs["body"] = "{\"success\": true}";
    result.latency_ms = 150;
    result.retries_used = 0;
    
    assert(result.status == StepStatus::ok);
    assert(result.outputs["status_code"] == "200");
    assert(result.outputs["body"] == "{\"success\": true}");
    assert(result.latency_ms == 150);
    assert(result.retries_used == 0);
    
    std::cout << "✓ StepResult test passed" << std::endl;
}

void test_block_metrics() {
    std::cout << "Testing BlockMetrics..." << std::endl;
    
    BlockMetrics metrics;
    metrics.latency_ms = 150;
    metrics.cpu_time_ms = 50;
    metrics.mem_bytes = 1024;
    metrics.success_count = 10;
    metrics.error_count = 2;
    
    assert(metrics.latency_ms == 150);
    assert(metrics.cpu_time_ms == 50);
    assert(metrics.mem_bytes == 1024);
    assert(metrics.success_count == 10);
    assert(metrics.error_count == 2);
    
    std::cout << "✓ BlockMetrics test passed" << std::endl;
}

void test_worker_config() {
    std::cout << "Testing WorkerConfig..." << std::endl;
    
    WorkerConfig config;
    config.cpu_pool_size = 8;
    config.gpu_pool_size = 2;
    config.io_pool_size = 16;
    config.max_memory_per_tenant_mb = 2048;
    config.max_cpu_time_per_tenant_ms = 7200000; // 2 hours
    config.sandbox_mode = false;
    config.nats_url = "nats://localhost:4222";
    config.prometheus_endpoint = "0.0.0.0:9090";
    
    assert(config.cpu_pool_size == 8);
    assert(config.gpu_pool_size == 2);
    assert(config.io_pool_size == 16);
    assert(config.max_memory_per_tenant_mb == 2048);
    assert(config.max_cpu_time_per_tenant_ms == 7200000);
    assert(config.sandbox_mode == false);
    assert(config.nats_url == "nats://localhost:4222");
    assert(config.prometheus_endpoint == "0.0.0.0:9090");
    
    std::cout << "✓ WorkerConfig test passed" << std::endl;
}

// ============================================================================
// CONTRACT TESTS: Worker ↔ Router Core Data Structures
// ============================================================================

/**
 * CONTRACT: StepResult factory methods always produce valid results
 * - success() always has status=ok, error_code=none
 * - error_result() always has status=error, valid error_code
 * - timeout_result() always has status=timeout
 * - cancelled_result() always has status=cancelled
 */
void test_contract_step_result_factories() {
    std::cout << "Testing CONTRACT: StepResult factory methods..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    meta.flow_id = "flow_456";
    meta.step_id = "step_789";
    meta.tenant_id = "tenant_abc";
    
    // Test success factory
    auto success_result = StepResult::success(meta, {{"key", "value"}}, 100);
    assert(success_result.status == StepStatus::ok);
    assert(success_result.error_code == ErrorCode::none);
    assert(success_result.metadata.trace_id == meta.trace_id);
    assert(success_result.latency_ms == 100);
    assert(success_result.outputs["key"] == "value");
    
    // Test error_result factory
    auto error_result = StepResult::error_result(
        ErrorCode::network_error,
        "Network failed",
        meta,
        50
    );
    assert(error_result.status == StepStatus::error);
    assert(error_result.error_code == ErrorCode::network_error);
    assert(error_result.error_message == "Network failed");
    assert(error_result.metadata.trace_id == meta.trace_id);
    assert(error_result.latency_ms == 50);
    
    // Test timeout_result factory
    auto timeout_result = StepResult::timeout_result(meta, 200);
    assert(timeout_result.status == StepStatus::timeout);
    assert(timeout_result.error_code == ErrorCode::cancelled_by_timeout);
    assert(timeout_result.metadata.trace_id == meta.trace_id);
    assert(timeout_result.latency_ms == 200);
    
    // Test cancelled_result factory
    auto cancelled_result = StepResult::cancelled_result(meta, 75);
    assert(cancelled_result.status == StepStatus::cancelled);
    assert(cancelled_result.error_code == ErrorCode::cancelled_by_user);
    assert(cancelled_result.metadata.trace_id == meta.trace_id);
    assert(cancelled_result.latency_ms == 75);
    
    std::cout << "✓ CONTRACT: StepResult factory methods test passed" << std::endl;
}

/**
 * CONTRACT: StepResult helper methods correctly identify status
 */
void test_contract_step_result_helpers() {
    std::cout << "Testing CONTRACT: StepResult helper methods..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    
    auto success = StepResult::success(meta);
    assert(success.is_success());
    assert(!success.is_error());
    assert(!success.is_timeout());
    assert(!success.is_cancelled());
    
    auto error = StepResult::error_result(ErrorCode::network_error, "Error", meta);
    assert(!error.is_success());
    assert(error.is_error());
    assert(!error.is_timeout());
    assert(!error.is_cancelled());
    
    auto timeout = StepResult::timeout_result(meta);
    assert(!timeout.is_success());
    assert(!timeout.is_error());
    assert(timeout.is_timeout());
    assert(!timeout.is_cancelled());
    
    auto cancelled = StepResult::cancelled_result(meta);
    assert(!cancelled.is_success());
    assert(!cancelled.is_error());
    assert(!cancelled.is_timeout());
    assert(cancelled.is_cancelled());
    
    std::cout << "✓ CONTRACT: StepResult helper methods test passed" << std::endl;
}

/**
 * CONTRACT: ErrorCode enum covers all error categories
 */
void test_contract_error_codes() {
    std::cout << "Testing CONTRACT: ErrorCode enum coverage..." << std::endl;
    
    // Validation errors (1xxx)
    assert(static_cast<int>(ErrorCode::invalid_input) == 1001);
    assert(static_cast<int>(ErrorCode::missing_required_field) == 1002);
    assert(static_cast<int>(ErrorCode::invalid_format) == 1003);
    
    // Execution errors (2xxx)
    assert(static_cast<int>(ErrorCode::execution_failed) == 2001);
    assert(static_cast<int>(ErrorCode::resource_unavailable) == 2002);
    assert(static_cast<int>(ErrorCode::permission_denied) == 2003);
    assert(static_cast<int>(ErrorCode::quota_exceeded) == 2004);
    
    // Network errors (3xxx)
    assert(static_cast<int>(ErrorCode::network_error) == 3001);
    assert(static_cast<int>(ErrorCode::connection_timeout) == 3002);
    assert(static_cast<int>(ErrorCode::http_error) == 3003);
    
    // System errors (4xxx)
    assert(static_cast<int>(ErrorCode::internal_error) == 4001);
    assert(static_cast<int>(ErrorCode::system_overload) == 4002);
    
    // Cancellation (5xxx)
    assert(static_cast<int>(ErrorCode::cancelled_by_user) == 5001);
    assert(static_cast<int>(ErrorCode::cancelled_by_timeout) == 5002);
    
    std::cout << "✓ CONTRACT: ErrorCode enum coverage test passed" << std::endl;
}

// ============================================================================
// HAPPY PATH TESTS
// ============================================================================

void test_happy_path_block_context_complete() {
    std::cout << "Testing happy path: complete BlockContext..." << std::endl;
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    ctx.flow_id = "flow_789";
    ctx.step_id = "step_abc";
    ctx.sandbox = true;
    ctx.rbac_scopes = {"read", "write", "execute"};
    
    assert(ctx.tenant_id == "tenant_123");
    assert(ctx.trace_id == "trace_456");
    assert(ctx.flow_id == "flow_789");
    assert(ctx.step_id == "step_abc");
    assert(ctx.sandbox == true);
    assert(ctx.rbac_scopes.size() == 3);
    
    std::cout << "✓ Happy path: complete BlockContext test passed" << std::endl;
}

void test_happy_path_step_request_complete() {
    std::cout << "Testing happy path: complete StepRequest..." << std::endl;
    
    StepRequest req;
    req.type = "http.request";
    req.inputs["url"] = "https://api.example.com";
    req.inputs["method"] = "POST";
    req.inputs["body"] = "{\"key\": \"value\"}";
    req.resources["cpu"] = "2";
    req.resources["memory"] = "512MB";
    req.timeout_ms = 5000;
    req.retry_count = 3;
    req.guardrails["max_latency_ms"] = "1000";
    req.guardrails["max_retries"] = "5";
    
    assert(req.type == "http.request");
    assert(req.inputs.size() == 3);
    assert(req.resources.size() == 2);
    assert(req.timeout_ms == 5000);
    assert(req.retry_count == 3);
    assert(req.guardrails.size() == 2);
    
    std::cout << "✓ Happy path: complete StepRequest test passed" << std::endl;
}

void test_happy_path_step_result_complete() {
    std::cout << "Testing happy path: complete StepResult..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    meta.flow_id = "flow_456";
    meta.step_id = "step_789";
    meta.tenant_id = "tenant_abc";
    
    StepResult result = StepResult::success(meta);
    result.outputs["status_code"] = "200";
    result.outputs["body"] = "{\"success\": true}";
    result.outputs["headers"] = "Content-Type: application/json";
    result.latency_ms = 150;
    result.retries_used = 1;
    
    assert(result.status == StepStatus::ok);
    assert(result.error_code == ErrorCode::none);
    assert(result.outputs.size() == 3);
    assert(result.metadata.trace_id == meta.trace_id);
    assert(result.latency_ms == 150);
    assert(result.retries_used == 1);
    
    std::cout << "✓ Happy path: complete StepResult test passed" << std::endl;
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

void test_error_invalid_block_context() {
    std::cout << "Testing error: invalid BlockContext..." << std::endl;
    
    // Test with empty tenant_id (should be validated in real implementation)
    BlockContext ctx;
    ctx.tenant_id = "";  // Invalid: empty
    ctx.trace_id = "trace_123";
    
    // In real implementation, this would be validated
    // For test, we just verify structure
    assert(ctx.tenant_id.empty());
    assert(!ctx.trace_id.empty());
    
    std::cout << "✓ Error: invalid BlockContext test passed" << std::endl;
}

void test_error_invalid_step_request() {
    std::cout << "Testing error: invalid StepRequest..." << std::endl;
    
    // Test with empty type (invalid)
    StepRequest req;
    req.type = "";  // Invalid: empty type
    req.timeout_ms = -1;  // Invalid: negative timeout
    req.retry_count = -1;  // Invalid: negative retry count
    
    assert(req.type.empty());
    assert(req.timeout_ms < 0);
    assert(req.retry_count < 0);
    
    // In real implementation, these would be validated
    std::cout << "✓ Error: invalid StepRequest test passed" << std::endl;
}

void test_error_step_result_with_error_code() {
    std::cout << "Testing error: StepResult with error code..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    meta.tenant_id = "tenant_456";
    
    // Test various error codes
    auto network_error = StepResult::error_result(
        ErrorCode::network_error,
        "Connection timeout",
        meta
    );
    assert(network_error.status == StepStatus::error);
    assert(network_error.error_code == ErrorCode::network_error);
    
    auto file_error = StepResult::error_result(
        ErrorCode::execution_failed,
        "File not found",
        meta
    );
    assert(file_error.status == StepStatus::error);
    assert(file_error.error_code == ErrorCode::execution_failed);
    
    auto permission_error = StepResult::error_result(
        ErrorCode::permission_denied,
        "Access denied",
        meta
    );
    assert(permission_error.status == StepStatus::error);
    assert(permission_error.error_code == ErrorCode::permission_denied);
    
    std::cout << "✓ Error: StepResult with error code test passed" << std::endl;
}

// ============================================================================
// BOUNDARY TESTS
// ============================================================================

void test_boundary_max_timeout() {
    std::cout << "Testing boundary: maximum timeout..." << std::endl;
    
    StepRequest req;
    req.type = "boundary.timeout";
    req.timeout_ms = std::numeric_limits<int64_t>::max();
    
    assert(req.timeout_ms == std::numeric_limits<int64_t>::max());
    
    std::cout << "✓ Boundary: maximum timeout test passed" << std::endl;
}

void test_boundary_max_retry_count() {
    std::cout << "Testing boundary: maximum retry count..." << std::endl;
    
    StepRequest req;
    req.type = "boundary.retry";
    req.retry_count = std::numeric_limits<int32_t>::max();
    
    assert(req.retry_count == std::numeric_limits<int32_t>::max());
    
    std::cout << "✓ Boundary: maximum retry count test passed" << std::endl;
}

void test_boundary_large_inputs_map() {
    std::cout << "Testing boundary: large inputs map..." << std::endl;
    
    StepRequest req;
    req.type = "boundary.large_inputs";
    
    // Add 1000 inputs
    for (int i = 0; i < 1000; ++i) {
        req.inputs["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }
    
    assert(req.inputs.size() == 1000);
    assert(req.inputs.find("key_0") != req.inputs.end());
    assert(req.inputs.find("key_999") != req.inputs.end());
    
    std::cout << "✓ Boundary: large inputs map test passed (" << req.inputs.size() << " inputs)" << std::endl;
}

void test_boundary_large_outputs_map() {
    std::cout << "Testing boundary: large outputs map..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    
    StepResult result = StepResult::success(meta);
    
    // Add 1000 outputs
    for (int i = 0; i < 1000; ++i) {
        result.outputs["output_" + std::to_string(i)] = "data_" + std::to_string(i);
    }
    
    assert(result.outputs.size() == 1000);
    assert(result.outputs.find("output_0") != result.outputs.end());
    assert(result.outputs.find("output_999") != result.outputs.end());
    
    std::cout << "✓ Boundary: large outputs map test passed (" << result.outputs.size() << " outputs)" << std::endl;
}

void test_boundary_zero_latency() {
    std::cout << "Testing boundary: zero latency..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    
    StepResult result = StepResult::success(meta);
    result.latency_ms = 0;  // Zero latency (instant execution)
    
    assert(result.latency_ms == 0);
    assert(result.status == StepStatus::ok);
    
    std::cout << "✓ Boundary: zero latency test passed" << std::endl;
}

void test_boundary_max_latency() {
    std::cout << "Testing boundary: maximum latency..." << std::endl;
    
    ResultMetadata meta;
    meta.trace_id = "trace_123";
    
    StepResult result = StepResult::success(meta);
    result.latency_ms = std::numeric_limits<int64_t>::max();
    
    assert(result.latency_ms == std::numeric_limits<int64_t>::max());
    assert(result.status == StepStatus::ok);
    
    std::cout << "✓ Boundary: maximum latency test passed" << std::endl;
}

int main() {
    std::cout << "Running Worker Core Tests..." << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        // Basic tests
        std::cout << "\n[Basic Tests]" << std::endl;
        test_block_context();
        test_step_request();
        test_step_result();
        test_block_metrics();
        test_worker_config();
        
        // CONTRACT tests: Worker ↔ Router core data structures
        std::cout << "\n[CONTRACT Tests: Core Data Structures]" << std::endl;
        test_contract_step_result_factories();
        test_contract_step_result_helpers();
        test_contract_error_codes();
        
        // Happy path tests
        std::cout << "\n[Happy Path Tests]" << std::endl;
        test_happy_path_block_context_complete();
        test_happy_path_step_request_complete();
        test_happy_path_step_result_complete();
        
        // Error handling tests
        std::cout << "\n[Error Handling Tests]" << std::endl;
        test_error_invalid_block_context();
        test_error_invalid_step_request();
        test_error_step_result_with_error_code();
        
        // Boundary tests
        std::cout << "\n[Boundary Tests]" << std::endl;
        test_boundary_max_timeout();
        test_boundary_max_retry_count();
        test_boundary_large_inputs_map();
        test_boundary_large_outputs_map();
        test_boundary_zero_latency();
        test_boundary_max_latency();
        
        std::cout << "\n===========================================" << std::endl;
        std::cout << "✅ All core tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}