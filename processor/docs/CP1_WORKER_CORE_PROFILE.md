# CP1 Worker Core Profile

**Version**: 1.0  
**Last Updated**: 2025-01-27  
**Purpose**: Define minimal CP1 core profile for Worker, separating required CP1 features from CP2+ enhancements

## Overview

This document defines the **minimal CP1 core profile** for CAF Worker, clearly separating:
- **CP1 Required**: Mandatory features, contracts, and tests for CP1 baseline
- **CP2+ Optional**: Enhancements, performance tests, and advanced features deferred to future checkpoints

This separation ensures:
- Clear CP1 invariants maintenance without CP2+ requirements
- Easy review process distinguishing CP1 vs CP2+ scope
- Focused development on CP1 stability and predictability

## CP1 Required Features

### 1. Core Functionality

**Required**:
- ✅ NATS integration: Subscribe to `caf.exec.assign.v1`, publish `ExecResult` to `caf.exec.result.v1`
- ✅ Assignment validation: Validate `ExecAssignment` structure and required fields
- ✅ ACK publication: Publish `ExecAssignmentAck` (accepted/rejected)
- ✅ Basic block execution: HTTP, FS, SQL blocks (simplified implementations acceptable)
- ✅ Status reporting: Clear status codes (success/error/timeout/cancelled)
- ✅ Error handling: Graceful error handling (no crashes)

**Can Be Stubbed** (CP1):
- Advanced resource pools (basic CPU pool is enough)
- Complex retry logic (simple retry is acceptable)
- Advanced observability (basic metrics are sufficient)
- Sandbox mode (can be minimal)

### 2. StepResult Contract (CP1 Invariant)

**Required Contract**:
- ✅ `StepResult` type with required fields: `status`, `error_code`, `metadata`
- ✅ Status mapping: `StepStatus::ok` → `ExecResult.status = "success"`
- ✅ Error code mapping: `ErrorCode` (1xxx-5xxx) → `ExecResult.error_code` (string)
- ✅ Metadata preservation: `ResultMetadata` with correlation IDs (trace_id, run_id, flow_id, step_id, tenant_id)
- ✅ Conversion: `ResultConverter::to_exec_result_json()` for NATS publishing

**Reference**: `ARCHITECTURE_ROLE.md#43-stepresult-contract-cp1-invariant`

### 3. Observability (CP1 Invariants)

**Required**:
- ✅ Structured JSON logging with required fields:
  - `timestamp` (ISO 8601 with microseconds)
  - `level` (ERROR, WARN, INFO, DEBUG)
  - `component` ("worker")
  - `message` (human-readable)
- ✅ CP1 correlation fields (when context available):
  - `tenant_id` (when tenant context available)
  - `run_id` (when run context available) - **CP1 observability invariant**
  - `flow_id` (when flow context available)
  - `step_id` (when step context available)
  - `trace_id` (when trace context available)
- ✅ PII/secret filtering: Automatic filtering of sensitive fields
- ✅ Health endpoint: `GET /_health` returns `200 OK` with JSON `{"status": "healthy", "timestamp": "..."}`

**Excluded from CP1**:
- ❌ Prometheus metrics endpoint (`/metrics`)
- ❌ OpenTelemetry tracing integration
- ❌ Advanced metrics (histograms, percentiles)
- ❌ Grafana dashboards
- ❌ Alertmanager integration

**Reference**: `docs/OBSERVABILITY_CP1_INVARIANTS.md`

### 4. Health Endpoint

**Required**:
- ✅ HTTP endpoint: `GET /_health`
- ✅ Status code: `200 OK`
- ✅ Response format: `{"status": "healthy", "timestamp": "ISO-8601"}`
- ✅ Availability: Endpoint must be accessible when Worker is running

**Excluded from CP1**:
- ❌ Advanced health checks (database, external services)
- ❌ Degraded status reporting
- ❌ Health check dependencies

### 5. Error Handling

**Required**:
- ✅ Validation errors: Invalid `ExecAssignment` → Reject with reason
- ✅ Execution errors: Block execution failed → Report error with `retryable` flag
- ✅ Timeout errors: Step exceeded timeout → Report timeout status
- ✅ System errors: Worker internal errors → Report error, don't crash

**Error Response Format**:
```json
{
  "status": "error",
  "error": {
    "code": "execution_failed",
    "message": "HTTP request failed: connection timeout",
    "retryable": true
  }
}
```

## CP1 Required Tests

### 1. Contract Tests

**Required**:
- ✅ `test_worker_router_contract.cpp` - StepResult → ExecResult conversion tests
  - Status mapping tests (success, error, timeout, cancelled)
  - Error code mapping tests (1xxx-5xxx)
  - Metadata preservation tests (correlation IDs)
- ✅ `router_worker_contract_SUITE.erl` - Router-side ExecResult processing tests
  - ExecResult validation tests
  - Correlation fields preservation tests

**Reference**: `docs/dev/WORKER_ROUTER_CONTRACT_TESTS.md`

### 2. Core Functionality Tests

**Required**:
- ✅ Assignment validation tests: Invalid `ExecAssignment` rejection
- ✅ Block execution tests: HTTP, FS, SQL blocks (basic happy path)
- ✅ Status reporting tests: All status codes (success, error, timeout, cancelled)
- ✅ Error handling tests: Graceful error handling (no crashes)

**Test Files**:
- `tests/test_core.cpp` - Core data structures and contract tests
- `tests/test_block_executor.cpp` - Block execution tests (basic)

### 3. Observability Tests

**Required**:
- ✅ Log format tests: Structured JSON format validation
- ✅ CP1 fields tests: Correlation fields extraction and logging
- ✅ PII filtering tests: Secret filtering validation
- ✅ Health endpoint tests: HTTP endpoint availability and format

**Test Files**:
- `tests/test_observability.cpp` - Observability unit tests
- `tests/test_health_endpoint.cpp` - Health endpoint integration tests

### 4. Integration Tests

**Required**:
- ✅ NATS integration tests: Subscribe to assignments, publish results
- ✅ Router ↔ Worker contract tests: End-to-end contract verification

**Excluded from CP1**:
- ❌ Load tests (performance, throughput)
- ❌ Stress tests (resource exhaustion)
- ❌ Edge case tests (very large inputs, extreme timeouts)
- ❌ Fault injection tests (network failures, service unavailability)

## CP2+ Optional/Enhancement Features

### 1. Advanced Observability

**CP2+**:
- 📋 Prometheus metrics endpoint (`/metrics`)
- 📋 OpenTelemetry tracing integration
- 📋 Advanced metrics (histograms, percentiles, custom metrics)
- 📋 Grafana dashboards
- 📋 Alertmanager integration
- 📋 Distributed tracing with context propagation

**Reference**: `docs/OBSERVABILITY_DASHBOARD.md` (CP2 planning)

### 2. Advanced Resource Management

**CP2+**:
- 📋 Advanced resource pools (GPU, specialized I/O)
- 📋 Complex scheduling algorithms
- 📋 Resource quota enforcement per tenant
- 📋 Dynamic resource allocation

### 3. Advanced Block Types

**CP2+**:
- 📋 AI inference blocks
- 📋 Media processing blocks
- 📋 Crypto operations blocks
- 📋 Plugin system for custom blocks

### 4. Advanced Reliability

**CP2+**:
- 📋 Sandbox mode with full isolation
- 📋 Advanced retry policies (circuit breakers, exponential backoff with jitter)
- 📋 Queue limits and backpressure handling
- 📋 Cancellation support for FS/HTTP blocks
- 📋 Timeout enforcement for all operations

### 5. Performance and Load Tests

**CP2+**:
- 📋 Load tests: Throughput validation (≥500 tasks/s for CP3)
- 📋 Performance tests: Latency benchmarks
- 📋 Stress tests: Resource exhaustion scenarios
- 📋 Edge case tests: Very large inputs, extreme timeouts
- 📋 Fault injection tests: Network failures, service unavailability

**Reference**: `tests/load/` (CP4 load testing)

### 6. Advanced Features

**CP2+**:
- 📋 JetStream integration (durable subscriptions, ACK/NAK)
- 📋 Advanced observability (distributed tracing, advanced metrics)
- 📋 Plugin system for custom blocks
- 📋 Distributed execution

## CP1 vs CP2+ Test Classification

### CP1 Required Tests

| Test Category | Test File | Purpose | CP1 Required |
|--------------|----------|---------|--------------|
| Contract | `test_worker_router_contract.cpp` | StepResult → ExecResult conversion | ✅ |
| Contract | `router_worker_contract_SUITE.erl` | Router-side ExecResult processing | ✅ |
| Core | `test_core.cpp` | Core data structures | ✅ |
| Core | `test_block_executor.cpp` | Basic block execution | ✅ |
| Observability | `test_observability.cpp` | Log format, CP1 fields | ✅ |
| Observability | `test_health_endpoint.cpp` | Health endpoint | ✅ |
| Integration | NATS integration tests | Basic NATS subscribe/publish | ✅ |

### CP2+ Optional Tests

| Test Category | Test File | Purpose | CP2+ |
|--------------|----------|---------|------|
| Performance | Load tests | Throughput validation | 📋 CP3 |
| Performance | Performance tests | Latency benchmarks | 📋 CP2+ |
| Stress | Stress tests | Resource exhaustion | 📋 CP2+ |
| Edge Cases | Edge case tests | Large inputs, extreme timeouts | 📋 CP2+ |
| Fault Injection | Fault injection tests | Network failures | 📋 CP2+ |

## CP1 Acceptance Criteria Summary

### Functional Requirements

- ✅ Worker subscribes to `caf.exec.assign.v1` and receives assignments
- ✅ Worker validates `ExecAssignment` and publishes ACK
- ✅ Worker executes basic blocks (HTTP, FS, SQL) with correct status
- ✅ Worker publishes `ExecResult` with accurate status codes
- ✅ Worker handles errors gracefully (no crashes)
- ✅ Worker health endpoint returns `200 OK`

### Non-Functional Requirements

- ✅ **Stability**: Worker runs without crashes for extended periods
- ✅ **Predictability**: Status codes are consistent and reliable
- ✅ **Observability**: Structured JSON logs with CP1 correlation fields
- ✅ **Contract Compliance**: Follows NATS contracts exactly

### Test Coverage

- ✅ Contract tests (StepResult → ExecResult)
- ✅ Core functionality tests (assignment, blocks, status)
- ✅ Observability tests (logs, CP1 fields, health endpoint)
- ✅ Integration tests (NATS, Router ↔ Worker)

## CP2+ Roadmap

**CP2**:
- Advanced observability (Prometheus, OpenTelemetry)
- JetStream integration
- Advanced retry policies

**CP3**:
- Full block suite (AI, media, crypto)
- ≥500 tasks/s throughput
- Advanced resource pools

**CP4+**:
- Plugin system
- Distributed execution
- Advanced scheduling

## References

- `docs/CP1_ACCEPTANCE_REPORT.md` - CP1 acceptance criteria and verification (see "Component Status" section)
- `ARCHITECTURE_ROLE.md` - Worker architectural role and contracts
- `docs/OBSERVABILITY_CP1_INVARIANTS.md` - CP1 observability invariants
- `docs/dev/WORKER_ROUTER_CONTRACT_TESTS.md` - Contract tests documentation
- `docs/API_CONTRACTS.md` - Router ↔ Worker contracts
- `docs/OBSERVABILITY.md` - General observability requirements

---

**Last Updated**: 2025-01-27  
**Checkpoint**: CP1-LC  
**Status**: Core Profile Definition

