#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <fstream>
#include <filesystem>
#include "beamline/worker/core.hpp"

using namespace beamline::worker;

// Mock BlockExecutor for testing
class MockBlockExecutor : public BlockExecutor {
public:
    MockBlockExecutor(std::string type, ResourceClass resource_class) 
        : type_(type), resource_class_(resource_class), should_fail_(false),
          fail_with_network_error_(false), fail_with_file_error_(false),
          fail_with_timeout_(false), latency_ms_(10) {}
    
    std::string block_type() const override { return type_; }
    ResourceClass resource_class() const override { return resource_class_; }
    
    caf::expected<void> init(const BlockContext& ctx) override {
        context_ = ctx;
        return caf::unit;
    }
    
    caf::expected<StepResult> execute(const StepRequest& req, const BlockContext& ctx) override {
        auto start = std::chrono::steady_clock::now();
        
        // Simulate network error
        if (fail_with_network_error_) {
            auto meta = metadata_from_context(ctx);
            auto result = StepResult::error_result(
                ErrorCode::network_error,
                "Network connection failed: connection timeout",
                meta,
                latency_ms_
            );
            return result;
        }
        
        // Simulate file error
        if (fail_with_file_error_) {
            auto meta = metadata_from_context(ctx);
            auto result = StepResult::error_result(
                ErrorCode::execution_failed,
                "File operation failed: file not found",
                meta,
                latency_ms_
            );
            return result;
        }
        
        // Simulate timeout
        if (fail_with_timeout_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(req.timeout_ms + 100));
            auto meta = metadata_from_context(ctx);
            return StepResult::timeout_result(meta, req.timeout_ms + 100);
        }
        
        // Simulate general error
        if (should_fail_) {
            auto meta = metadata_from_context(ctx);
            auto result = StepResult::error_result(
                ErrorCode::execution_failed,
                "Execution failed: mock error",
                meta,
                latency_ms_
            );
            return result;
        }
        
        // Simulate work with configurable latency
        std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));
        
        auto end = std::chrono::steady_clock::now();
        auto actual_latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        // CONTRACT: Worker always returns StepResult with complete metadata
        auto meta = metadata_from_context(ctx);
        StepResult result = StepResult::success(meta);
        result.outputs["mock_result"] = "success";
        result.outputs["block_type"] = type_;
        result.latency_ms = actual_latency;
        result.retries_used = 0;
        
        return result;
    }
    
    caf::expected<StepResult> execute(const StepRequest& req) override {
        return execute(req, context_);
    }
    
    caf::expected<void> cancel(const std::string& step_id) override {
        return caf::unit;
    }
    
    BlockMetrics metrics() const override {
        BlockMetrics m;
        m.latency_ms = latency_ms_;
        m.success_count = should_fail_ ? 0 : 1;
        m.error_count = should_fail_ ? 1 : 0;
        return m;
    }
    
    // Test control methods
    void set_should_fail(bool fail) { should_fail_ = fail; }
    void set_network_error(bool fail) { fail_with_network_error_ = fail; }
    void set_file_error(bool fail) { fail_with_file_error_ = fail; }
    void set_timeout_error(bool fail) { fail_with_timeout_ = fail; }
    void set_latency_ms(int64_t latency) { latency_ms_ = latency; }
    
private:
    std::string type_;
    ResourceClass resource_class_;
    BlockContext context_;
    bool should_fail_;
    bool fail_with_network_error_;
    bool fail_with_file_error_;
    bool fail_with_timeout_;
    int64_t latency_ms_;
};

void test_mock_executor() {
    std::cout << "Testing MockBlockExecutor..." << std::endl;
    
    MockBlockExecutor executor("test.block", ResourceClass::cpu);
    
    assert(executor.block_type() == "test.block");
    assert(executor.resource_class() == ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "test_tenant";
    ctx.trace_id = "test_trace";
    
    auto init_result = executor.init(ctx);
    assert(init_result.has_value());
    
    StepRequest req;
    req.type = "test.block";
    req.inputs["test_input"] = "test_value";
    
    auto execute_result = executor.execute(req);
    assert(execute_result.has_value());
    assert(execute_result->status == StepStatus::ok);
    assert(execute_result->outputs["mock_result"] == "success");
    assert(execute_result->outputs["block_type"] == "test.block");
    
    auto metrics = executor.metrics();
    assert(metrics.latency_ms == 10);
    assert(metrics.success_count == 1);
    
    std::cout << "✓ MockBlockExecutor test passed" << std::endl;
}

void test_step_execution_with_retry() {
    std::cout << "Testing step execution with retry..." << std::endl;
    
    MockBlockExecutor executor("retry.test", ResourceClass::cpu);
    
    StepRequest req;
    req.type = "retry.test";
    req.retry_count = 3;
    req.timeout_ms = 1000;
    
    auto result = executor.execute(req);
    assert(result.has_value());
    assert(result->status == StepStatus::ok);
    assert(result->retries_used == 0); // No retries needed for mock executor
    
    std::cout << "✓ Step execution with retry test passed" << std::endl;
}

void test_resource_class_determination() {
    std::cout << "Testing resource class determination..." << std::endl;
    
    // Test CPU resource class
    MockBlockExecutor cpu_executor("cpu.block", ResourceClass::cpu);
    assert(cpu_executor.resource_class() == ResourceClass::cpu);
    
    // Test GPU resource class
    MockBlockExecutor gpu_executor("gpu.block", ResourceClass::gpu);
    assert(gpu_executor.resource_class() == ResourceClass::gpu);
    
    // Test I/O resource class
    MockBlockExecutor io_executor("io.block", ResourceClass::io);
    assert(io_executor.resource_class() == ResourceClass::io);
    
    std::cout << "✓ Resource class determination test passed" << std::endl;
}

void test_block_context_isolation() {
    std::cout << "Testing block context isolation..." << std::endl;
    
    MockBlockExecutor executor1("block1", ResourceClass::cpu);
    MockBlockExecutor executor2("block2", ResourceClass::cpu);
    
    BlockContext ctx1;
    ctx1.tenant_id = "tenant1";
    ctx1.flow_id = "flow1";
    
    BlockContext ctx2;
    ctx2.tenant_id = "tenant2";
    ctx2.flow_id = "flow2";
    
    auto init1 = executor1.init(ctx1);
    auto init2 = executor2.init(ctx2);
    
    assert(init1.has_value());
    assert(init2.has_value());
    
    // Both executors should have different contexts
    // (This is a basic test - in a real implementation we'd verify isolation more thoroughly)
    
    std::cout << "✓ Block context isolation test passed" << std::endl;
}

// ============================================================================
// CONTRACT TESTS: Worker ↔ Router Interaction Contract
// ============================================================================

/**
 * CONTRACT: Worker always returns StepResult with complete metadata
 * - metadata.trace_id, metadata.flow_id, metadata.step_id, metadata.tenant_id
 * - status (ok|error|timeout|cancelled)
 * - latency_ms (always present, >= 0)
 * - retries_used (always present, >= 0)
 */
void test_contract_metadata_always_present() {
    std::cout << "Testing CONTRACT: metadata always present in StepResult..." << std::endl;
    
    MockBlockExecutor executor("contract.test", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    ctx.flow_id = "flow_789";
    ctx.step_id = "step_abc";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "contract.test";
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Metadata must always be present
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.flow_id.empty());
    assert(!result->metadata.step_id.empty());
    assert(!result->metadata.tenant_id.empty());
    
    // CONTRACT: Metadata must match context
    assert(result->metadata.trace_id == ctx.trace_id);
    assert(result->metadata.flow_id == ctx.flow_id);
    assert(result->metadata.step_id == ctx.step_id);
    assert(result->metadata.tenant_id == ctx.tenant_id);
    
    // CONTRACT: latency_ms always present and >= 0
    assert(result->latency_ms >= 0);
    
    // CONTRACT: retries_used always present and >= 0
    assert(result->retries_used >= 0);
    
    std::cout << "✓ CONTRACT: metadata always present test passed" << std::endl;
}

/**
 * CONTRACT: Worker semantics - idempotency
 * - Same request with same inputs should produce same outputs (if successful)
 * - Retry-safe: multiple executions with same step_id should be safe
 */
void test_contract_idempotency() {
    std::cout << "Testing CONTRACT: idempotency semantics..." << std::endl;
    
    MockBlockExecutor executor("idempotent.test", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    ctx.flow_id = "flow_789";
    ctx.step_id = "step_idempotent";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "idempotent.test";
    req.inputs["key"] = "value";
    
    // Execute same request multiple times
    auto result1 = executor.execute(req, ctx);
    auto result2 = executor.execute(req, ctx);
    auto result3 = executor.execute(req, ctx);
    
    assert(result1.has_value());
    assert(result2.has_value());
    assert(result3.has_value());
    
    // CONTRACT: All executions should succeed (idempotent)
    assert(result1->status == StepStatus::ok);
    assert(result2->status == StepStatus::ok);
    assert(result3->status == StepStatus::ok);
    
    // CONTRACT: Same inputs should produce same outputs
    assert(result1->outputs["mock_result"] == result2->outputs["mock_result"]);
    assert(result2->outputs["mock_result"] == result3->outputs["mock_result"]);
    
    std::cout << "✓ CONTRACT: idempotency semantics test passed" << std::endl;
}

/**
 * CONTRACT: Worker timing guarantees
 * - latency_ms reflects actual execution time
 * - timeout_ms is respected (execution should not exceed timeout)
 */
void test_contract_timing_guarantees() {
    std::cout << "Testing CONTRACT: timing guarantees..." << std::endl;
    
    MockBlockExecutor executor("timing.test", ResourceClass::cpu);
    executor.set_latency_ms(50);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "timing.test";
    req.timeout_ms = 1000; // 1 second timeout
    
    auto start = std::chrono::steady_clock::now();
    auto result = executor.execute(req, ctx);
    auto end = std::chrono::steady_clock::now();
    
    assert(result.has_value());
    
    auto actual_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // CONTRACT: latency_ms should be close to actual execution time (within 20ms tolerance)
    assert(std::abs(result->latency_ms - actual_time) < 20);
    
    // CONTRACT: Execution should not exceed timeout
    assert(actual_time < req.timeout_ms);
    
    std::cout << "✓ CONTRACT: timing guarantees test passed" << std::endl;
}

// ============================================================================
// HAPPY PATH TESTS
// ============================================================================

void test_happy_path_basic_execution() {
    std::cout << "Testing happy path: basic execution..." << std::endl;
    
    MockBlockExecutor executor("happy.basic", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    ctx.flow_id = "flow_789";
    ctx.step_id = "step_001";
    
    auto init_result = executor.init(ctx);
    assert(init_result.has_value());
    
    StepRequest req;
    req.type = "happy.basic";
    req.inputs["url"] = "https://api.example.com";
    req.inputs["method"] = "GET";
    req.timeout_ms = 5000;
    req.retry_count = 3;
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    assert(result->status == StepStatus::ok);
    assert(result->error_code == ErrorCode::none);
    assert(result->outputs.find("mock_result") != result->outputs.end());
    assert(result->latency_ms >= 0);
    
    std::cout << "✓ Happy path: basic execution test passed" << std::endl;
}

void test_happy_path_with_large_payload() {
    std::cout << "Testing happy path: large payload..." << std::endl;
    
    MockBlockExecutor executor("happy.large", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "happy.large";
    
    // Simulate large payload (1MB)
    std::string large_payload(1024 * 1024, 'A');
    req.inputs["payload"] = large_payload;
    req.inputs["size"] = std::to_string(large_payload.size());
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    assert(result->status == StepStatus::ok);
    
    std::cout << "✓ Happy path: large payload test passed" << std::endl;
}

void test_happy_path_with_retries() {
    std::cout << "Testing happy path: execution with retries..." << std::endl;
    
    MockBlockExecutor executor("happy.retry", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "happy.retry";
    req.retry_count = 5;
    req.timeout_ms = 1000;
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    assert(result->status == StepStatus::ok);
    assert(result->retries_used >= 0);
    assert(result->retries_used <= req.retry_count);
    
    std::cout << "✓ Happy path: execution with retries test passed" << std::endl;
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

void test_error_network_failure() {
    std::cout << "Testing error: network failure..." << std::endl;
    
    MockBlockExecutor executor("error.network", ResourceClass::cpu);
    executor.set_network_error(true);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    ctx.flow_id = "flow_789";
    ctx.step_id = "step_error";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "error.network";
    req.inputs["url"] = "https://unreachable.example.com";
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Error result must have complete metadata
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.tenant_id.empty());
    
    // CONTRACT: Error status and error_code must be set
    assert(result->status == StepStatus::error);
    assert(result->error_code == ErrorCode::network_error);
    assert(!result->error_message.empty());
    
    // CONTRACT: latency_ms still present even on error
    assert(result->latency_ms >= 0);
    
    std::cout << "✓ Error: network failure test passed" << std::endl;
}

void test_error_file_operation_failure() {
    std::cout << "Testing error: file operation failure..." << std::endl;
    
    MockBlockExecutor executor("error.file", ResourceClass::io);
    executor.set_file_error(true);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "error.file";
    req.inputs["file_path"] = "/nonexistent/file.txt";
    req.inputs["operation"] = "read";
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Error result must have complete metadata
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.tenant_id.empty());
    
    // CONTRACT: Error status and error_code must be set
    assert(result->status == StepStatus::error);
    assert(result->error_code == ErrorCode::execution_failed);
    assert(!result->error_message.empty());
    
    std::cout << "✓ Error: file operation failure test passed" << std::endl;
}

void test_error_timeout() {
    std::cout << "Testing error: timeout..." << std::endl;
    
    MockBlockExecutor executor("error.timeout", ResourceClass::cpu);
    executor.set_timeout_error(true);
    executor.set_latency_ms(2000);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    StepRequest req;
    req.type = "error.timeout";
    req.timeout_ms = 500; // Short timeout
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Timeout result must have complete metadata
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.tenant_id.empty());
    
    // CONTRACT: Timeout status and error_code must be set
    assert(result->status == StepStatus::timeout);
    assert(result->error_code == ErrorCode::cancelled_by_timeout);
    
    // CONTRACT: latency_ms should reflect timeout
    assert(result->latency_ms >= req.timeout_ms);
    
    std::cout << "✓ Error: timeout test passed" << std::endl;
}

void test_error_invalid_parameters() {
    std::cout << "Testing error: invalid parameters..." << std::endl;
    
    MockBlockExecutor executor("error.invalid", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    // Test with missing required field
    StepRequest req;
    req.type = "error.invalid";
    // Missing required inputs
    
    // In a real implementation, this would validate and return error
    // For mock, we simulate validation error
    executor.set_should_fail(true);
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Error result must have complete metadata
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.tenant_id.empty());
    
    // CONTRACT: Error status must be set
    assert(result->status == StepStatus::error);
    assert(result->error_code != ErrorCode::none);
    
    std::cout << "✓ Error: invalid parameters test passed" << std::endl;
}

// ============================================================================
// BOUNDARY/LOAD TESTS
// ============================================================================

void test_boundary_many_small_tasks() {
    std::cout << "Testing boundary: many small tasks..." << std::endl;
    
    MockBlockExecutor executor("boundary.many", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    const int num_tasks = 1000;
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < num_tasks; ++i) {
        StepRequest req;
        req.type = "boundary.many";
        req.inputs["task_id"] = std::to_string(i);
        req.timeout_ms = 100;
        
        auto result = executor.execute(req, ctx);
        if (result.has_value() && result->status == StepStatus::ok) {
            success_count++;
            
            // CONTRACT: Every successful result must have complete metadata
            assert(!result->metadata.trace_id.empty());
            assert(!result->metadata.tenant_id.empty());
            assert(result->latency_ms >= 0);
        } else {
            error_count++;
        }
    }
    
    // All tasks should succeed (mock executor)
    assert(success_count == num_tasks);
    assert(error_count == 0);
    
    std::cout << "✓ Boundary: many small tasks test passed (" << success_count << " tasks)" << std::endl;
}

void test_boundary_large_payload() {
    std::cout << "Testing boundary: very large payload..." << std::endl;
    
    MockBlockExecutor executor("boundary.large", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    // Test with 10MB payload
    const size_t payload_size = 10 * 1024 * 1024; // 10MB
    std::string large_payload(payload_size, 'B');
    
    StepRequest req;
    req.type = "boundary.large";
    req.inputs["payload"] = large_payload;
    req.inputs["size"] = std::to_string(payload_size);
    req.timeout_ms = 30000; // 30 seconds
    
    auto result = executor.execute(req, ctx);
    assert(result.has_value());
    
    // CONTRACT: Even with large payload, result must have complete metadata
    assert(!result->metadata.trace_id.empty());
    assert(!result->metadata.tenant_id.empty());
    assert(result->latency_ms >= 0);
    
    // Should succeed (mock executor handles large payload)
    assert(result->status == StepStatus::ok);
    
    std::cout << "✓ Boundary: very large payload test passed (" << (payload_size / 1024 / 1024) << "MB)" << std::endl;
}

void test_boundary_concurrent_executions() {
    std::cout << "Testing boundary: concurrent executions..." << std::endl;
    
    MockBlockExecutor executor("boundary.concurrent", ResourceClass::cpu);
    
    BlockContext ctx;
    ctx.tenant_id = "tenant_123";
    ctx.trace_id = "trace_456";
    
    executor.init(ctx);
    
    const int num_concurrent = 50;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_concurrent, false);
    
    // Launch concurrent executions
    for (int i = 0; i < num_concurrent; ++i) {
        threads.emplace_back([&executor, &ctx, i, &results]() {
            StepRequest req;
            req.type = "boundary.concurrent";
            req.inputs["task_id"] = std::to_string(i);
            req.timeout_ms = 1000;
            
            auto result = executor.execute(req, ctx);
            results[i] = (result.has_value() && result->status == StepStatus::ok);
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all succeeded
    int success_count = 0;
    for (bool success : results) {
        if (success) success_count++;
    }
    
    assert(success_count == num_concurrent);
    
    std::cout << "✓ Boundary: concurrent executions test passed (" << success_count << " concurrent)" << std::endl;
}

int main() {
    std::cout << "Running Block Executor Tests..." << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        // Basic tests
        std::cout << "\n[Basic Tests]" << std::endl;
        test_mock_executor();
        test_step_execution_with_retry();
        test_resource_class_determination();
        test_block_context_isolation();
        
        // CONTRACT tests: Worker ↔ Router interaction contract
        std::cout << "\n[CONTRACT Tests: Worker ↔ Router]" << std::endl;
        test_contract_metadata_always_present();
        test_contract_idempotency();
        test_contract_timing_guarantees();
        
        // Happy path tests
        std::cout << "\n[Happy Path Tests]" << std::endl;
        test_happy_path_basic_execution();
        test_happy_path_with_large_payload();
        test_happy_path_with_retries();
        
        // Error handling tests
        std::cout << "\n[Error Handling Tests]" << std::endl;
        test_error_network_failure();
        test_error_file_operation_failure();
        test_error_timeout();
        test_error_invalid_parameters();
        
        // Boundary/load tests
        std::cout << "\n[Boundary/Load Tests]" << std::endl;
        test_boundary_many_small_tasks();
        test_boundary_large_payload();
        test_boundary_concurrent_executions();
        
        std::cout << "\n===========================================" << std::endl;
        std::cout << "✅ All block executor tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}