#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <caf/actor_system.hpp>
#include <caf/actor.hpp>
#include <caf/behavior.hpp>

namespace beamline {
namespace worker {

// Forward declarations
class BlockExecutor;
class Scheduler;
class Observability;

// Core data structures
struct BlockContext {
    std::string tenant_id;
    std::string trace_id;
    std::string run_id;      // Run identifier for tracking execution flow (CP1 observability invariant)
    std::string flow_id;
    std::string step_id;
    bool sandbox = false;
    std::vector<std::string> rbac_scopes;
    
    template <class Inspector>
    friend bool inspect(Inspector& f, BlockContext& ctx) {
        return f.object(ctx).fields(
            f.field("tenant_id", ctx.tenant_id),
            f.field("trace_id", ctx.trace_id),
            f.field("run_id", ctx.run_id),
            f.field("flow_id", ctx.flow_id),
            f.field("step_id", ctx.step_id),
            f.field("sandbox", ctx.sandbox),
            f.field("rbac_scopes", ctx.rbac_scopes)
        );
    }
};

struct StepRequest {
    std::string type;
    std::unordered_map<std::string, std::string> inputs;
    std::unordered_map<std::string, std::string> resources;
    int64_t timeout_ms = 30000; // 30s default
    int32_t retry_count = 3;
    std::unordered_map<std::string, std::string> guardrails;
    
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, StepRequest& req) {
        return f(req.type, req.inputs, req.resources, req.timeout_ms, req.retry_count, req.guardrails);
    }
};

// Step execution status aligned with ExecResult contract (success|error|timeout|cancelled)
enum class StepStatus {
    ok,        // Maps to "success" in ExecResult
    error,     // Maps to "error" in ExecResult
    timeout,   // Maps to "timeout" in ExecResult
    cancelled  // Maps to "cancelled" in ExecResult
};

// Machine-readable error codes for programmatic error handling
enum class ErrorCode {
    none = 0,
    // Validation errors (1xxx)
    invalid_input = 1001,
    missing_required_field = 1002,
    invalid_format = 1003,
    // Execution errors (2xxx)
    execution_failed = 2001,
    resource_unavailable = 2002,
    permission_denied = 2003,
    quota_exceeded = 2004,
    // Network errors (3xxx)
    network_error = 3001,
    connection_timeout = 3002,
    http_error = 3003,
    // System errors (4xxx)
    internal_error = 4001,
    system_overload = 4002,
    // Cancellation (5xxx)
    cancelled_by_user = 5001,
    cancelled_by_timeout = 5002
};

// Metadata for tracing and correlation
struct ResultMetadata {
    std::string trace_id;    // Distributed tracing identifier (required when trace context available)
    std::string run_id;      // Run identifier for tracking execution flow (required when run context available)
    std::string flow_id;     // Flow identifier for workflow tracking (required when flow context available)
    std::string step_id;     // Step identifier within a flow (required when step context available)
    std::string tenant_id;   // Tenant identifier (required when tenant context available)
    
    template <class Inspector>
    friend bool inspect(Inspector& f, ResultMetadata& meta) {
        return f.object(meta).fields(
            f.field("trace_id", meta.trace_id),
            f.field("run_id", meta.run_id),
            f.field("flow_id", meta.flow_id),
            f.field("step_id", meta.step_id),
            f.field("tenant_id", meta.tenant_id)
        );
    }
};

// Unified result type for all block executions
// This type is validated and can be safely converted to ExecResult format
struct StepResult {
    StepStatus status = StepStatus::ok;
    ErrorCode error_code = ErrorCode::none;
    std::unordered_map<std::string, std::string> outputs;
    std::string error_message;  // Human-readable error message
    ResultMetadata metadata;     // Trace, flow, step, tenant IDs
    int64_t latency_ms = 0;
    int32_t retries_used = 0;
    
    // Helper methods for status checks
    bool is_success() const { return status == StepStatus::ok; }
    bool is_error() const { return status == StepStatus::error; }
    bool is_timeout() const { return status == StepStatus::timeout; }
    bool is_cancelled() const { return status == StepStatus::cancelled; }
    
    // Factory methods for common result types
    static StepResult success(const ResultMetadata& meta, 
                             const std::unordered_map<std::string, std::string>& outputs = {},
                             int64_t latency_ms = 0) {
        StepResult result;
        result.status = StepStatus::ok;
        result.error_code = ErrorCode::none;
        result.metadata = meta;
        result.outputs = outputs;
        result.latency_ms = latency_ms;
        return result;
    }
    
    static StepResult error_result(ErrorCode code, const std::string& message,
                                  const ResultMetadata& meta,
                                  int64_t latency_ms = 0) {
        StepResult result;
        result.status = StepStatus::error;
        result.error_code = code;
        result.error_message = message;
        result.metadata = meta;
        result.latency_ms = latency_ms;
        return result;
    }
    
    static StepResult timeout_result(const ResultMetadata& meta, int64_t latency_ms = 0) {
        StepResult result;
        result.status = StepStatus::timeout;
        result.error_code = ErrorCode::cancelled_by_timeout;
        result.metadata = meta;
        result.latency_ms = latency_ms;
        return result;
    }
    
    static StepResult cancelled_result(const ResultMetadata& meta, int64_t latency_ms = 0) {
        StepResult result;
        result.status = StepStatus::cancelled;
        result.error_code = ErrorCode::cancelled_by_user;
        result.metadata = meta;
        result.latency_ms = latency_ms;
        return result;
    }
    
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, StepResult& result) {
        return f(result.status, result.error_code, result.outputs, result.error_message, result.metadata, result.latency_ms, result.retries_used);
    }
};

struct BlockMetrics {
    int64_t latency_ms = 0;
    int64_t cpu_time_ms = 0;
    int64_t mem_bytes = 0;
    int64_t success_count = 0;
    int64_t error_count = 0;
    
    template <class Inspector>
    friend bool inspect(Inspector& f, BlockMetrics& metrics) {
        return f.object(metrics).fields(
            f.field("latency_ms", metrics.latency_ms),
            f.field("cpu_time_ms", metrics.cpu_time_ms),
            f.field("mem_bytes", metrics.mem_bytes),
            f.field("success_count", metrics.success_count),
            f.field("error_count", metrics.error_count)
        );
    }
};

// Resource classification
enum class ResourceClass {
    cpu,
    gpu,
    io
};

// BlockExecutor interface
// All execute() methods must return StepResult with complete metadata
class BlockExecutor {
public:
    virtual ~BlockExecutor() = default;
    
    virtual std::string block_type() const = 0;
    virtual caf::expected<void> init(const BlockContext& ctx) = 0;
    
    // Execute with context - metadata will be automatically populated from context
    virtual caf::expected<StepResult> execute(const StepRequest& req, const BlockContext& ctx) = 0;
    
    // Legacy execute without context (deprecated, will be removed)
    // Implementations should extract context from request if available
    virtual caf::expected<StepResult> execute(const StepRequest& req) {
        // Default implementation creates empty context
        // Subclasses should override execute(req, ctx) instead
        BlockContext empty_ctx;
        return execute(req, empty_ctx);
    }
    
    virtual caf::expected<void> cancel(const std::string& step_id) = 0;
    virtual BlockMetrics metrics() const = 0;
    virtual ResourceClass resource_class() const = 0;
    
protected:
    // Helper to create ResultMetadata from BlockContext
    static ResultMetadata metadata_from_context(const BlockContext& ctx) {
        ResultMetadata meta;
        meta.trace_id = ctx.trace_id;
        meta.run_id = ctx.run_id;      // CP1 observability invariant: run_id for execution flow tracking
        meta.flow_id = ctx.flow_id;
        meta.step_id = ctx.step_id;
        meta.tenant_id = ctx.tenant_id;
        return meta;
    }
};

// Worker configuration
struct WorkerConfig {
    int cpu_pool_size = 4;
    int gpu_pool_size = 1;
    int io_pool_size = 8;
    int64_t max_memory_per_tenant_mb = 1024;
    int64_t max_cpu_time_per_tenant_ms = 3600000; // 1 hour
    bool sandbox_mode = false;
    std::string nats_url = "nats://localhost:4222";
    std::string prometheus_endpoint = "0.0.0.0:9090";
    
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, WorkerConfig& config) {
        return f(config.cpu_pool_size, config.gpu_pool_size, config.io_pool_size, config.max_memory_per_tenant_mb, config.max_cpu_time_per_tenant_ms, config.sandbox_mode, config.nats_url, config.prometheus_endpoint);
    }
};

} // namespace worker
} // namespace beamline