# Beamline Worker CAF (C++ Actor Framework)

High-performance runtime for executing Flow DSL steps (Blocks) with strong guarantees on atomicity, idempotency, isolation, and observability.

## Overview

The Worker CAF implements the execution engine for the Beamline platform, providing:

- **Scalable Execution**: Actor-based architecture with resource pools (CPU/GPU/IO)
- **Multi-tenant Isolation**: Per-tenant quotas and resource limits
- **Observability**: Prometheus metrics, OpenTelemetry tracing, structured JSON logs
- **Sandbox Mode**: Safe execution environment for testing and dry-runs
- **Block Executors**: Modular system for different types of operations

## Architecture

### Core Components

1. **Worker Actor**: Main coordinator that manages execution lifecycle
2. **Pool Actors**: Resource-specific pools (CPU, GPU, I/O) for load distribution
3. **Executor Actors**: Individual block execution with retry and timeout handling
4. **Scheduler**: Resource-aware task assignment and quota enforcement
5. **Sandbox**: Mock execution environment for safe testing
6. **Observability**: Comprehensive metrics, tracing, and logging

### Block Executors (Phase 1)

- **HTTP Block**: Generic HTTP requests with streaming support
- **FS Block**: File system operations (blob put/get)
- **SQL Block**: Database queries with safe execution
- **Human Block**: Approval workflows with timeout handling

## Building

### Prerequisites

- CMake 3.16+
- C++20 compiler
- CAF (C++ Actor Framework)
- libcurl (for HTTP block)
- SQLite3 (for SQL block)
- Prometheus C++ client
- OpenTelemetry C++

### Build Instructions

```bash
cd apps/caf/processor
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running Tests

```bash
make test
# or
ctest --verbose
```

## Configuration

The worker can be configured via command-line arguments:

```bash
./beamline_worker \
  --cpu-pool-size=8 \
  --gpu-pool-size=2 \
  --io-pool-size=16 \
  --max-memory-mb=2048 \
  --max-cpu-time-ms=7200000 \
  --nats-url=nats://localhost:4222 \
  --prometheus-endpoint=0.0.0.0:9090
```

## Performance Targets

- **Throughput**: ≥500 tasks/s (CP3-LC gate)
- **Latency Budgets**:
  - HTTP requests: ≤500ms (internal)
  - File operations: ≤200ms (local)
  - SQL queries: ≤300ms (simple selects)

## Observability

### Metrics (Prometheus)

- `worker_tasks_total`: Total tasks executed by type and status
- `worker_task_latency_ms`: Task execution latency histogram
- `worker_resource_usage`: CPU time and memory usage
- `worker_pool_queue_depth`: Queue depth per resource pool

### Tracing (OpenTelemetry)

Spans are created for each step execution with attributes:
- `tenant_id`: Tenant identifier
- `flow_id`: Flow execution identifier
- `step_id`: Step identifier
- `block_type`: Type of block being executed
- `worker_id`: Worker instance identifier

### Logs (JSON)

Structured JSON logs with correlation IDs:
```json
{
  "timestamp": 1640995200000,
  "level": "INFO",
  "worker_id": "worker_12345",
  "message": "Step execution completed",
  "fields": {
    "block_type": "http.request",
    "latency_ms": 150,
    "status": "ok"
  }
}
```

## Security

- **RBAC Integration**: Validates scopes before execution
- **Path Restrictions**: File operations limited to safe directories
- **SQL Injection Prevention**: Parameterized queries only
- **Sandbox Isolation**: Separate execution environment for untrusted code

## Multi-tenancy

- **Resource Quotas**: Per-tenant memory and CPU time limits
- **Isolation**: Separate execution contexts and data
- **Fair Scheduling**: Round-robin and priority-based scheduling
- **Usage Tracking**: Comprehensive resource usage monitoring

## Error Handling

- **Retry Logic**: Configurable retry attempts with exponential backoff
- **Timeout Enforcement**: Per-step timeout with cancellation support
- **DLQ Support**: Failed tasks published to Dead Letter Queue
- **Graceful Degradation**: Continues operation when individual components fail

## Testing

The worker includes comprehensive test suites:

- **Unit Tests**: Individual component testing
- **Integration Tests**: End-to-end workflow testing
- **Performance Tests**: Throughput and latency validation
- **Chaos Tests**: Fault injection and recovery testing

## Future Enhancements (Phase 2+)

- **Advanced Blocks**: AI inference, media processing, crypto operations
- **Streaming I/O**: High-performance I/O with io_uring
- **Plugin System**: Dynamic block loading and signature verification
- **Advanced Scheduling**: Machine learning-based resource allocation
- **Distributed Execution**: Multi-worker coordination and load balancing