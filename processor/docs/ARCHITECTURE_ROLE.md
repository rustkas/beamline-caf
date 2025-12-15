# CAF Worker: Role in System Architecture

**Component**: `apps/caf/processor/`  
**Technology**: C++/CAF (C++ Actor Framework)  
**Checkpoint**: CP1-LC (Baseline Worker)  
**Status**: Core Component

---

## Purpose

This document defines the **architectural role** of CAF Worker in the Beamline system, focusing on its responsibilities, integration points, and CP1 requirements. It serves as the foundation for understanding how Worker fits into the overall system architecture.

**Related Documents**:
- `README.md` - Technical implementation details
- `docs/CAF_WORKER_SPEC.md` - Detailed specification
- `docs/API_CONTRACTS.md` - Router ↔ Worker contracts
- `docs/CORE_COMPONENTS.md` - System overview

---

## 1. Role in Overall Architecture

### 1.1. High-Performance Compute Node

**CAF Worker** is the **high-performance compute engine** of the Beamline system. It serves as a dedicated execution runtime for compute-intensive tasks that require:

- **Predictable resource control** (CPU, GPU, I/O)
- **Parallel execution** with strong isolation guarantees
- **External resource access** (file system, HTTP APIs, databases)
- **Stable status reporting** (success/error, retryable, timeouts)

### 1.2. Position in System Flow

```
┌─────────────┐
│   Client    │
│  (HTTP/REST)│
└──────┬───────┘
       │
       ▼
┌─────────────┐      NATS      ┌─────────────┐
│ C-Gateway   │ ──────────────▶│   Router    │
│  (C11)      │                 │ (Erlang/OTP)│
└─────────────┘                 └──────┬──────┘
                                       │
                                       │ ExecAssignment
                                       │ (caf.exec.assign.v1)
                                       ▼
                              ┌─────────────────┐
                              │   CAF Worker    │
                              │  (C++/CAF)      │
                              │                 │
                              │ • Flow Steps    │
                              │ • Block Exec    │
                              │ • Resource Pools│
                              └────────┬────────┘
                                       │
                                       │ ExecResult
                                       │ (caf.exec.result.v1)
                                       ▼
                              ┌─────────────────┐
                              │     Router      │
                              │  (continues)    │
                              └─────────────────┘
```

**Key Integration Points**:
- **Input**: Receives `ExecAssignment` from Router via NATS (`caf.exec.assign.v1`)
- **Output**: Publishes `ExecResult` back to Router via NATS (`caf.exec.result.v1`)
- **Communication**: Loosely coupled via NATS (no direct dependencies)

---

## 2. Core Responsibilities

### 2.1. Execute Flow Steps (Blocks)

**Primary Task**: Execute Flow DSL steps (blocks) that arrive from Router.

**Flow Steps** are atomic units of work defined in the Flow DSL:
- Each step represents a single operation (HTTP request, file operation, SQL query, etc.)
- Steps are executed in isolation with their own context
- Steps can be retried independently
- Steps have configurable timeouts

**Block Types** (CP1):
- **HTTP Block**: Generic HTTP requests (REST API calls)
- **FS Block**: File system operations (blob put/get)
- **SQL Block**: Database queries (parameterized, safe execution)
- **Human Block**: Approval workflows (timeout handling)

**Example Flow Step**:
```json
{
  "type": "http.request",
  "inputs": {
    "method": "POST",
    "url": "https://api.example.com/process",
    "body": "{...}"
  },
  "timeout_ms": 5000,
  "retry": {
    "max_retries": 3,
    "backoff_ms": 1000
  }
}
```

### 2.2. Actor Runtime (CAF)

**Actor-Based Architecture**: Worker uses CAF (C++ Actor Framework) to provide:

- **Parallelism**: Multiple actors execute steps concurrently
- **Isolation**: Each actor has isolated state and resources
- **Resource Pools**: Actors organized by resource class (CPU/GPU/I/O)
- **Message Passing**: Asynchronous communication between actors

**Actor Hierarchy**:
```
Worker Actor (Main Coordinator)
├── Pool Actors (Resource-Specific)
│   ├── CPU Pool Actor
│   ├── GPU Pool Actor
│   └── I/O Pool Actor
└── Executor Actors (Individual Block Execution)
    ├── HTTP Executor
    ├── FS Executor
    ├── SQL Executor
    └── Human Executor
```

**Benefits**:
- **Scalability**: Add more actors to handle increased load
- **Fault Isolation**: Failure in one actor doesn't affect others
- **Resource Control**: Pools enforce quotas and limits per tenant

### 2.3. External Resource Access

**Worker must access external resources** safely and reliably:

- **File System**: Read/write operations with path restrictions
- **HTTP APIs**: REST API calls with retry/timeout handling
- **Databases**: SQL queries with parameterized statements
- **Future**: Media processing, AI inference, crypto operations

**Safety Guarantees**:
- **Path Restrictions**: File operations limited to safe directories
- **SQL Injection Prevention**: Parameterized queries only
- **Timeout Enforcement**: All operations have configurable timeouts
- **Retry Logic**: Configurable retry attempts with exponential backoff

### 2.4. Status Reporting

**Worker must provide clear, actionable status information**:

**Status Types**:
- **Success**: Step completed successfully with output data
- **Error**: Step failed with error details (retryable vs non-retryable)
- **Timeout**: Step exceeded configured timeout
- **Canceled**: Step was canceled (e.g., by user or system)

**Status Format** (`ExecResult`):
```json
{
  "status": "success|error|timeout|canceled",
  "assignment_id": "uuid",
  "request_id": "uuid",
  "tenant_id": "tenant-123",
  "outputs": {
    "result": "..."
  },
  "error": {
    "code": "timeout",
    "message": "Step exceeded timeout of 5000ms",
    "retryable": true
  },
  "latency_ms": 150,
  "retries_used": 1
}
```

**Key Fields**:
- `status`: Clear outcome (success/error/timeout/canceled)
- `retryable`: Indicates if error can be retried
- `latency_ms`: Execution time for observability
- `retries_used`: Number of retries attempted

---

## 3. CP1 Requirements: Stability and Predictability

### 3.1. Core Principle

**In CP1, Worker must be stable and predictable**, even if functionality is partially stubbed.

**What This Means**:
- ✅ **Stable**: Worker doesn't crash, handles errors gracefully
- ✅ **Predictable**: Behavior is consistent, status codes are reliable
- ✅ **Observable**: Logs, metrics, and health checks work correctly
- ✅ **Contract-Compliant**: Follows NATS contracts exactly

**What Can Be Stubbed** (CP1):
- ❌ Advanced block types (AI inference, media processing) → Stub with clear error
- ❌ Complex resource scheduling → Basic round-robin is acceptable
- ❌ Advanced observability → Basic metrics and logs are sufficient

### 3.2. CP1 Minimum Viable Features

**Must Work**:
1. **NATS Integration**: Subscribe to `caf.exec.assign.v1`, publish `ExecResult`
2. **Assignment Validation**: Validate `ExecAssignment` structure
3. **ACK Publication**: Publish `ExecAssignmentAck` (accepted/rejected)
4. **Basic Block Execution**: HTTP, FS, SQL blocks (can be simplified)
5. **Status Reporting**: Clear status codes (success/error/timeout)
6. **Health Endpoint**: `/_health` returns `200 OK` with JSON
7. **Structured Logging**: JSON logs with correlation IDs

**Can Be Stubbed**:
- Advanced resource pools (basic CPU pool is enough)
- Complex retry logic (simple retry is acceptable)
- Advanced observability (basic metrics are sufficient)
- Sandbox mode (can be minimal)

### 3.3. Error Handling

**Worker must handle errors gracefully**:

**Error Categories**:
- **Validation Errors**: Invalid `ExecAssignment` → Reject with reason
- **Execution Errors**: Block execution failed → Report error with `retryable` flag
- **Timeout Errors**: Step exceeded timeout → Report timeout status
- **System Errors**: Worker internal errors → Report error, don't crash

**Error Response Format**:
```json
{
  "status": "error",
  "error": {
    "code": "execution_failed",
    "message": "HTTP request failed: connection timeout",
    "retryable": true,
    "details": {
      "block_type": "http.request",
      "url": "https://api.example.com/process"
    }
  }
}
```

### 3.4. Observability Baseline

**CP1 Observability Requirements**:

**Logs** (Structured JSON):
```json
{
  "timestamp": "2025-01-27T12:00:00Z",
  "level": "INFO",
  "component": "worker",
  "message": "Step execution completed",
  "fields": {
    "assignment_id": "uuid",
    "block_type": "http.request",
    "status": "success",
    "latency_ms": 150
  },
  "trace_id": "trace-123",
  "tenant_id": "tenant-123"
}
```

**Health Endpoint**:
- **Path**: `GET /_health`
- **Status**: `200 OK`
- **Response**: `{"status": "healthy", "timestamp": "..."}`

**Metrics** (Basic):
- Task execution count (by status)
- Task latency (histogram)
- Error rate

---

## 4. Integration with Router

### 4.1. Message Flow

**Request Flow** (Router → Worker):
1. Router publishes `ExecAssignment` to `caf.exec.assign.v1`
2. Worker subscribes to `caf.exec.assign.v1` (durable queue group)
3. Worker validates `ExecAssignment`
4. Worker publishes `ExecAssignmentAck` to `caf.exec.assign.v1.ack`
5. Worker executes block (if accepted)
6. Worker publishes `ExecResult` to `caf.exec.result.v1`

**Response Flow** (Worker → Router):
1. Worker publishes `ExecResult` to `caf.exec.result.v1`
2. Router subscribes to `caf.exec.result.v1`
3. Router processes result and continues flow

### 4.2. Contracts

**Source of Truth**: `docs/API_CONTRACTS.md`

**Key Contracts**:
- `ExecAssignment`: Router → Worker assignment
- `ExecAssignmentAck`: Worker → Router acknowledgment
- `ExecResult`: Worker → Router execution result
- `WorkerHeartbeat`: Worker → Router health status

**Required Fields**:
- `ExecAssignment.version == "1"`
- `assignment_id`, `request_id`, `tenant_id` (non-empty strings)
- `executor.provider_id` and `job.type` (supported)

### 4.3. StepResult Contract (CP1 Invariant)

**CRITICAL**: All block executions in CP1 must return a unified `StepResult` type with complete metadata. This contract ensures predictable and reliable integration with Router.

**Contract Definition**:

```cpp
struct StepResult {
    StepStatus status;                    // Required: ok | error | timeout | cancelled
    ErrorCode error_code;                  // Required: Machine-readable error code
    ResultMetadata metadata;               // Required: Complete correlation metadata
    std::unordered_map<std::string, std::string> outputs;  // Optional: Execution outputs
    std::string error_message;            // Optional: Human-readable error message
    int64_t latency_ms;                   // Optional: Execution latency
    int32_t retries_used;                 // Optional: Number of retries attempted
};
```

**Required Fields**:
- `status`: Must be one of `ok`, `error`, `timeout`, `cancelled`
- `error_code`: Must be set (use `ErrorCode::none` for success)
- `metadata`: Must contain complete correlation IDs (trace_id, flow_id, step_id, tenant_id)

**Status Mapping** (StepResult → ExecResult):
- `StepStatus::ok` → `ExecResult.status = "success"`
- `StepStatus::error` → `ExecResult.status = "error"`
- `StepStatus::timeout` → `ExecResult.status = "timeout"`
- `StepStatus::cancelled` → `ExecResult.status = "cancelled"`

**ErrorCode Categories**:
- **1xxx**: Validation errors (invalid_input, missing_required_field, invalid_format)
- **2xxx**: Execution errors (execution_failed, resource_unavailable, permission_denied, quota_exceeded)
- **3xxx**: Network errors (network_error, connection_timeout, http_error)
- **4xxx**: System errors (internal_error, system_overload)
- **5xxx**: Cancellation (cancelled_by_user, cancelled_by_timeout)

**Metadata Requirements**:
- `trace_id`: Distributed tracing identifier (required when trace context available)
- `run_id`: Run identifier for tracking execution flow (required when run context available) - **CP1 observability invariant**
- `flow_id`: Flow identifier for workflow tracking (required when flow context available)
- `step_id`: Step identifier within a flow (required when step context available)
- `tenant_id`: Tenant identifier (required when tenant context available)

**Conversion to ExecResult**:
- All `StepResult` values must be convertible to `ExecResult` JSON format via `ResultConverter::to_exec_result_json()`
- Conversion preserves all required fields and correlation IDs
- Error codes are converted to string format for ExecResult contract

**Contract Enforcement**:
- All `BaseBlockExecutor::execute()` methods must return `caf::expected<StepResult>`
- All factory methods (`StepResult::success()`, `StepResult::error_result()`, etc.) ensure complete metadata
- Validation via `ResultConverter::validate_result()` before publishing to NATS

**Implementation Reference**:
- Type definition: `include/beamline/worker/core.hpp` (StepResult, StepStatus, ErrorCode, ResultMetadata)
- Converter: `include/beamline/worker/result_converter.hpp` (ResultConverter)
- Tests: `tests/test_core.cpp` (StepResult contract tests)

### 4.4. NATS Subjects

**Input**:
- `caf.exec.assign.v1`: Router → Worker assignments

**Output**:
- `caf.exec.assign.v1.ack`: Worker → Router ACK
- `caf.exec.result.v1`: Worker → Router execution results
- `caf.worker.heartbeat.v1`: Worker → Router heartbeat
- `caf.exec.dlq.v1`: Worker → DLQ for non-retriable failures

---

## 5. CP1 Acceptance Criteria

### 5.1. Functional Requirements

- ✅ Worker subscribes to `caf.exec.assign.v1` and receives assignments
- ✅ Worker validates `ExecAssignment` and publishes ACK
- ✅ Worker executes basic blocks (HTTP, FS, SQL) with correct status
- ✅ Worker publishes `ExecResult` with accurate status codes
- ✅ Worker handles errors gracefully (no crashes)
- ✅ Worker health endpoint returns `200 OK`

### 5.2. Non-Functional Requirements

- ✅ **Stability**: Worker runs without crashes for extended periods
- ✅ **Predictability**: Status codes are consistent and reliable
- ✅ **Observability**: Structured JSON logs with correlation IDs
- ✅ **Contract Compliance**: Follows NATS contracts exactly

### 5.3. Test Coverage

**Required Tests**:
- Assignment validation tests
- Block execution tests (HTTP, FS, SQL)
- Error handling tests
- Status reporting tests
- Health endpoint tests
- NATS integration tests

**Test Files**:
- `tests/worker_assignment_SUITE.cpp` - Assignment handling
- `tests/worker_blocks_SUITE.cpp` - Block execution
- `tests/worker_status_SUITE.cpp` - Status reporting
- `tests/worker_health_SUITE.cpp` - Health endpoint

---

## 6. Future Enhancements (CP2+)

**Not in CP1 Scope**:
- Advanced resource pools (GPU, specialized I/O)
- Complex scheduling algorithms
- Advanced observability (distributed tracing, advanced metrics)
- Sandbox mode with full isolation
- Plugin system for custom blocks
- Advanced retry policies (circuit breakers, etc.)

**CP2+ Roadmap**:
- CP2: Advanced observability, JetStream integration
- CP3: Full block suite (AI, media, crypto), ≥500 tasks/s throughput
- CP4+: Plugin system, distributed execution

---

## 7. Summary

**CAF Worker** is the **high-performance compute engine** of the Beamline system:

1. **Executes Flow Steps**: Runs blocks (HTTP, FS, SQL) from Router
2. **Actor Runtime**: CAF-based parallelism with resource pools
3. **External Resources**: Safely accesses FS, HTTP, databases
4. **Status Reporting**: Clear, actionable status (success/error/timeout)
5. **CP1 Stability**: Stable and predictable, even with stubbed features

**Key Principle**: In CP1, **stability and predictability** are more important than feature completeness. Worker must work reliably, even if some features are stubbed.

---

## References

- `README.md` - Technical implementation details
- `docs/CAF_WORKER_SPEC.md` - Detailed specification
- `docs/API_CONTRACTS.md` - Router ↔ Worker contracts
- `docs/CORE_COMPONENTS.md` - System overview
- `docs/ARCHITECTURE/` - Architecture documentation
- `docs/CP1_WORKER_CORE_PROFILE.md` - **CP1 minimal core profile** (CP1 required vs CP2+ optional)

---

**Last Updated**: 2025-01-27  
**Checkpoint**: CP1-LC  
**Status**: Core Component

