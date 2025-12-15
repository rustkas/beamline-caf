# Observability (CP1 → CP2/Pre‑release)

This document describes the observability features for Worker CAF in CP1, including structured JSON logging and health endpoints.

CP2/Pre‑release adds OpenTelemetry tracing (OTLP export), Prometheus `/metrics` on 9092, and Grafana dashboards leveraging CP1 correlation fields for filtering. See `docs/dev/CP2_OBSERVABILITY_PLAN.md` and `docs/OBSERVABILITY_CP2_TEST_PROFILE.md`.

## Structured JSON Logging

Worker uses structured JSON logging for all log entries. Logs are written to standard output (stdout/stderr) in JSON format, one entry per line (JSONL format).

### Log Format

Each log entry is a JSON object with the following structure:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "worker",
  "message": "Step execution completed",
  "tenant_id": "tenant_123",
  "run_id": "run_abc123",
  "flow_id": "flow_xyz789",
  "step_id": "step_001",
  "trace_id": "trace_def4567890abcdef1234567890abcdef",
  "context": {
    "worker_id": "worker_12345",
    "block_type": "http.request",
    "status": "success",
    "latency_ms": 150
  }
}
```

### Required Fields

- **timestamp**: ISO-8601 timestamp in UTC (format: `YYYY-MM-DDTHH:MM:SS.ssssssZ` with 6 digits for microseconds)
- **level**: Log level (`ERROR`, `WARN`, `INFO`, `DEBUG`)
- **component**: Component identifier (always `"worker"` for Worker component)
- **message**: Human-readable log message

### Optional Fields

#### CP1 Correlation Fields

CP1 correlation fields are placed at the **top level** of the log entry (not in a nested object):

- **tenant_id**: Tenant identifier (when available)
- **run_id**: Run identifier for workflow runs (when available)
- **flow_id**: Flow identifier for multi-step flows (when available)
- **step_id**: Step identifier within a flow (when available)
- **trace_id**: Trace identifier for distributed tracing (when available)

All CP1 fields are optional. They are only included when the corresponding context is available.

#### Context

- **context**: Additional structured context (JSON object)
  - **worker_id**: Worker instance identifier (always present)
  - Additional technical details (block_type, status, latency_ms, etc.)

### Log Levels

- **ERROR**: Critical errors requiring immediate attention
- **WARN**: Warnings, potential issues
- **INFO**: Informational messages (step execution, state changes)
- **DEBUG**: Detailed debugging information

### PII Filtering

All log entries are automatically filtered for PII (Personally Identifiable Information) and secrets:

**Filtered Fields**:
- `password`, `api_key`, `secret`, `token`, `access_token`, `refresh_token`
- `authorization`, `credit_card`, `ssn`, `email`, `phone`

**Replacement**: All PII fields are replaced with `"[REDACTED]"` before logging.

**Filtering Method**: Recursive filtering of JSON objects by field name (case-insensitive).

### Log Output Location

Logs are written to:
- **stdout**: INFO, WARN, DEBUG level logs
- **stderr**: ERROR level logs
- Format: JSONL (one JSON object per line)

For production, logs should be redirected to files or log aggregation systems.

### Usage Examples

#### Basic Logging

```cpp
#include "beamline/worker/observability.hpp"

auto observability = std::make_unique<Observability>("worker_12345");

// Basic log without CP1 fields
observability->log_info("Worker starting", "", "", "", "", "", {
    {"cpu_pool_size", "8"},
    {"gpu_pool_size", "2"}
});
```

#### Logging with CP1 Fields

```cpp
// Log with CP1 fields explicitly
observability->log_info(
    "Step execution completed",
    "tenant_123",      // tenant_id
    "run_abc123",      // run_id
    "flow_xyz789",     // flow_id
    "step_001",        // step_id
    "trace_def456",    // trace_id
    {
        {"block_type", "http.request"},
        {"status", "success"},
        {"latency_ms", "150"}
    }
);
```

#### Logging with BlockContext

```cpp
#include "beamline/worker/core.hpp"

// BlockContext contains CP1 fields
BlockContext ctx;
ctx.tenant_id = "tenant_123";
ctx.run_id = "run_abc123";
ctx.flow_id = "flow_xyz789";
ctx.step_id = "step_001";
ctx.trace_id = "trace_def456";

// Use helper function to extract CP1 fields from BlockContext
observability->log_info_with_context(
    "Step execution started",
    ctx,
    {
        {"block_type", "http.request"},
        {"url", "https://api.example.com/process"}
    }
);
```

#### Error Logging

```cpp
// Error logging with CP1 fields
observability->log_error(
    "HTTP request failed",
    "tenant_123",
    "run_abc123",
    "flow_xyz789",
    "step_001",
    "trace_def456",
    {
        {"block_type", "http.request"},
        {"url", "https://api.example.com/process"},
        {"error_code", "CONNECTION_TIMEOUT"},
        {"retryable", "true"}
    }
);
```

#### Debug Logging

```cpp
// Debug logging (detailed information)
observability->log_debug(
    "Processing HTTP response",
    "tenant_123",
    "run_abc123",
    "flow_xyz789",
    "step_001",
    "trace_def456",
    {
        {"block_type", "http.request"},
        {"status_code", "200"},
        {"response_size", "1024"}
    }
);
```

## Health Endpoints

Worker provides health checking via HTTP health endpoint.

### HTTP Health Endpoint

**Path**: `GET /_health`  
**Port**: 9091 (default, configurable via `prometheus_endpoint + 1`)  
**Address**: `0.0.0.0` (all interfaces, configurable)

**Authentication**: None (health endpoint is public)

### Health Check Response

**HTTP Status**: `200 OK`

**Response Format** (JSON):
```json
{
  "status": "healthy",
  "timestamp": "2025-01-27T12:00:00.123456Z"
}
```

**Required Fields**:
- `status` (string): `"healthy"` (service is healthy)
- `timestamp` (string): ISO 8601 timestamp in UTC

### Health Endpoint Configuration

The health endpoint port is derived from the Prometheus metrics endpoint configuration:

- **Default**: Port 9091 (if Prometheus is on 9090)
- **Configuration**: `health_port = prometheus_port + 1`
- **Address**: Same as Prometheus endpoint address (default: `0.0.0.0`)

**Example Configuration**:
```cpp
// In main.cpp
std::string health_address = "0.0.0.0";
uint16_t health_port = 9091; // Default (Prometheus port + 1)

// If prometheus_endpoint is configured as "0.0.0.0:9090"
// Health endpoint will be on "0.0.0.0:9091"
```

### Health Check Tools

#### Using curl

```bash
# Basic health check
curl http://localhost:9091/_health

# Expected: HTTP 200 OK with JSON response
# {
#   "status": "healthy",
#   "timestamp": "2025-01-27T12:00:00.123456Z"
# }
```

#### Using HTTP client

```bash
# Check HTTP status code
curl -s -o /dev/null -w "%{http_code}" http://localhost:9091/_health
# Expected: 200

# Check JSON format
curl -s http://localhost:9091/_health | jq .
```

### Docker Healthcheck

```dockerfile
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:9091/_health || exit 1
```

## Configuration

### Observability Configuration

Observability is initialized with a worker ID:

```cpp
auto observability = std::make_unique<Observability>("worker_" + std::to_string(getpid()));
```

**Worker ID**: Unique identifier for the worker instance (typically includes process ID).

### Health Endpoint Configuration

Health endpoint is started automatically when Worker starts:

```cpp
// In main.cpp
observability->start_health_endpoint(health_address, health_port);
```

**Configuration Options**:
- **Address**: IP address to bind to (default: `0.0.0.0` for all interfaces)
- **Port**: Port number (default: 9091, or `prometheus_port + 1`)

## Local Development

### Viewing Logs

Logs are written to stdout/stderr. To view logs:

```bash
# View all logs
./beamline_worker 2>&1 | jq .

# View only ERROR logs
./beamline_worker 2>&1 | jq 'select(.level == "ERROR")'

# View logs with specific tenant_id
./beamline_worker 2>&1 | jq 'select(.tenant_id == "tenant_123")'

# View logs with specific trace_id
./beamline_worker 2>&1 | jq 'select(.trace_id == "trace_abc123")'
```

### Testing Health Endpoint

```bash
# Check health endpoint
curl http://localhost:9091/_health

# Expected: HTTP 200 OK with JSON
# {
#   "status": "healthy",
#   "timestamp": "2025-01-27T12:00:00.123456Z"
# }
```

### Testing Observability

Use the test script:

```bash
# Run Worker observability tests
bash scripts/observability/test_worker_observability.sh

# With custom URL
WORKER_URL=http://localhost:9091 bash scripts/observability/test_worker_observability.sh
```

## CP1 Compliance

Worker fully complies with CP1 observability invariants:

- ✅ Structured JSON logs with required fields
- ✅ ISO 8601 timestamps with microseconds (6 digits)
- ✅ CP1 correlation fields at top level when available
- ✅ PII/secret filtering (recursive filtering)
- ✅ All log levels (ERROR, WARN, INFO, DEBUG)
- ✅ Health endpoint `GET /_health` with CP1-compliant format

### CP1 Fields Requirements

**When Context Available**:
- **Tenant context**: `tenant_id` must be included
- **Run context**: `tenant_id`, `run_id` must be included
- **Flow context**: `tenant_id`, `run_id`, `flow_id` must be included
- **Step context**: `tenant_id`, `run_id`, `flow_id`, `step_id` must be included
- **Trace context**: `trace_id` should be included when available

**Example with Full CP1 Context**:
```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "worker",
  "message": "Step execution completed",
  "tenant_id": "tenant_123",
  "run_id": "run_abc123",
  "flow_id": "flow_xyz789",
  "step_id": "step_001",
  "trace_id": "trace_def4567890abcdef1234567890abcdef",
  "context": {
    "worker_id": "worker_12345",
    "block_type": "http.request",
    "status": "success",
    "latency_ms": 150
  }
}
```

## CP2 Wave 1 Observability

**CP2 Wave 1 adds Prometheus metrics export** (gated behind `CP2_OBSERVABILITY_METRICS_ENABLED` feature flag):

### Metrics Endpoint

- **Path**: `GET /metrics`
- **Port**: 9092 (separate from health endpoint on 9091)
- **Format**: Prometheus text format (version 0.0.4)
- **Feature Flag**: `CP2_OBSERVABILITY_METRICS_ENABLED` (default: `false`)

**Usage**:
```bash
# Enable CP2 metrics
export CP2_OBSERVABILITY_METRICS_ENABLED=true

# Query metrics endpoint
curl http://localhost:9092/metrics
```

### CP2 Wave 1 Metrics

**Step Execution Metrics**:
- `worker_step_executions_total{step_type, execution_status, tenant_id, run_id, flow_id, step_id}` (Counter)
- `worker_step_execution_duration_seconds{step_type, execution_status, tenant_id, run_id, flow_id, step_id}` (Histogram)
- `worker_step_errors_total{step_type, error_code, tenant_id, run_id, flow_id, step_id}` (Counter)

**Flow Execution Metrics**:
- `worker_flow_execution_duration_seconds{tenant_id, run_id, flow_id}` (Histogram)

**Queue Metrics**:
- `worker_queue_depth{resource_pool}` (Gauge)
- `worker_active_tasks{resource_pool}` (Gauge)

**Health Metrics**:
- `worker_health_status{check}` (Gauge, 1 = healthy, 0 = unhealthy)

### Label Cardinality

**High-cardinality labels** (`tenant_id`, `run_id`, `flow_id`, `step_id`) are only included in detailed metrics when available. This prevents Prometheus cardinality explosion while maintaining detailed observability when needed.

**Aggregate metrics** (without high-cardinality labels) are available for:
- Overall step execution rates
- Queue depth by resource pool
- Health status

### Grafana Dashboard

**Location**: `apps/caf/processor/dashboards/worker-observability.json`

**Panels**:
- Step execution rate (by step_type and execution_status)
- Step execution duration (p50/p95/p99)
- Step error rate (by step_type and error_code)
- Queue depth by resource pool
- Active tasks by resource pool
- Flow execution duration (p95/p99)
- Health status
- Step success/error ratio

**Import**:
```bash
# Import dashboard into Grafana
curl -X POST http://localhost:3000/api/dashboards/db \
  -H "Content-Type: application/json" \
  -d @apps/caf/processor/dashboards/worker-observability.json
```

### Alerting Rules

**Location**: `apps/caf/processor/alerts/worker-alerts.yml`

**Alerts**:
- `WorkerHighTaskErrorRate`: Error rate > 10% for 5 minutes
- `WorkerHighTaskLatency`: p95 latency > 2s for 5 minutes
- `WorkerVeryHighTaskLatency`: p99 latency > 5s for 5 minutes
- `WorkerHighQueueDepth`: Queue depth > 100 for 5 minutes
- `WorkerCriticalQueueDepth`: Queue depth > 500 for 2 minutes
- `WorkerUnhealthy`: Health status == 0 for 1 minute
- `WorkerHighActiveTasks`: Active tasks > 50 for 5 minutes
- `WorkerHighFlowExecutionDuration`: p95 flow duration > 30s for 5 minutes

**Configuration**:
```yaml
# prometheus.yml
rule_files:
  - /path/to/apps/caf/processor/alerts/worker-alerts.yml
```

### CP2 Wave 1 Feature Flag

**`CP2_OBSERVABILITY_METRICS_ENABLED`** (default: `false`):
- Gates all CP2 Wave 1 metrics collection and export
- When `false`: CP1 behavior (no metrics endpoint, no metrics collection)
- When `true`: CP2 behavior (metrics endpoint on 9092, all metrics collected)

**Environment Variable**:
```bash
export CP2_OBSERVABILITY_METRICS_ENABLED=true
```

## MVP Scope

**Included in CP1**:
- ✅ Structured JSON logging with CP1 correlation fields
- ✅ ISO 8601 timestamps with microseconds
- ✅ PII filtering
- ✅ Health endpoint via HTTP
- ✅ All log levels (ERROR, WARN, INFO, DEBUG)

**Excluded from CP1** (future iterations):
- ❌ Prometheus metrics export (implemented but not required for CP1)
- ❌ OpenTelemetry distributed tracing (implemented but not required for CP1)
- ❌ Grafana dashboards
- ❌ Log aggregation (Loki)
- ❌ Alertmanager integration

**CP2 Wave 1** (gated behind `CP2_OBSERVABILITY_METRICS_ENABLED`):
- ✅ Prometheus `/metrics` endpoint on port 9092
- ✅ Core Worker metrics (step executions, duration, errors, queue depth, active tasks, health)
- ✅ Grafana dashboard JSON
- ✅ Alerting rules YAML

## Best Practices

### Logging Best Practices

1. **Use Appropriate Log Levels**:
   - **ERROR**: Only for critical errors that require immediate attention
   - **WARN**: For warnings and potential issues that should be monitored
   - **INFO**: For important events (step execution, state changes)
   - **DEBUG**: For detailed debugging information (use sparingly in production)

2. **Include CP1 Fields When Available**:
   ```cpp
   // Good: Include all available CP1 fields
   observability->log_info(
       "Step execution completed",
       ctx.tenant_id, ctx.run_id, ctx.flow_id, ctx.step_id, ctx.trace_id,
       {{"block_type", "http.request"}, {"status", "success"}}
   );
   
   // Avoid: Missing CP1 fields when available
   observability->log_info("Step execution completed", "", "", "", "", "", {});
   ```

3. **Use Context for Technical Details**:
   ```cpp
   // Good: Technical details in context
   observability->log_info("HTTP request completed", tenant_id, run_id, flow_id, step_id, trace_id, {
       {"block_type", "http.request"},
       {"status_code", "200"},
       {"latency_ms", "150"},
       {"response_size", "1024"}
   });
   ```

4. **Avoid PII in Logs**:
   ```cpp
   // Bad: PII in context (will be filtered, but better to avoid)
   observability->log_info("User login", tenant_id, run_id, flow_id, step_id, trace_id, {
       {"email", "user@example.com"},  // Will be filtered, but avoid
       {"password", "secret123"}        // Will be filtered, but avoid
   });
   
   // Good: No PII in context
   observability->log_info("User login", tenant_id, run_id, flow_id, step_id, trace_id, {
       {"user_id", "user_12345"},  // Use ID instead of email
       {"login_method", "password"}
   });
   ```

5. **Use Descriptive Messages**:
   ```cpp
   // Good: Descriptive message
   observability->log_info("HTTP request completed successfully", tenant_id, run_id, flow_id, step_id, trace_id, {
       {"block_type", "http.request"},
       {"status_code", "200"}
   });
   
   // Bad: Vague message
   observability->log_info("Done", tenant_id, run_id, flow_id, step_id, trace_id, {});
   ```

6. **Log Errors with Context**:
   ```cpp
   // Good: Error with full context
   observability->log_error(
       "HTTP request failed: connection timeout",
       tenant_id, run_id, flow_id, step_id, trace_id,
       {
           {"block_type", "http.request"},
           {"url", "https://api.example.com/process"},
           {"error_code", "CONNECTION_TIMEOUT"},
           {"retryable", "true"},
           {"retry_count", "3"}
       }
   );
   ```

### Health Endpoint Best Practices

1. **Monitor Health Endpoint**:
   - Set up monitoring to check health endpoint regularly
   - Alert on non-200 responses
   - Track health check latency

2. **Use Health Endpoint for Load Balancers**:
   - Configure load balancers to use `/_health` endpoint
   - Set appropriate timeout (e.g., 3 seconds)
   - Configure retry logic

3. **Health Endpoint Should Be Fast**:
   - Health endpoint should respond quickly (< 100ms)
   - Avoid heavy operations in health check
   - Return cached status if possible

### Performance Best Practices

1. **Minimize Logging Overhead**:
   - Use appropriate log levels (avoid DEBUG in production)
   - Batch related log entries when possible
   - Avoid logging in hot paths

2. **Optimize Context Size**:
   - Keep context objects small
   - Avoid large strings in context
   - Use structured data efficiently

3. **Monitor Log Volume**:
   - Track log volume per component
   - Set up alerts for excessive logging
   - Review and optimize high-volume loggers

---

## Migration Guide

### Migrating from Non-CP1 Log Format

If you have existing code using a different log format, follow these steps to migrate to CP1-compliant logging:

#### Step 1: Update Log Calls

**Before** (non-CP1 format):
```cpp
// Old format (if existed)
log("INFO", "Step execution completed", {
    {"correlation": {
        {"tenant_id", "tenant_123"},
        {"run_id", "run_abc123"}
    }},
    {"message", "Step execution completed"}
});
```

**After** (CP1 format):
```cpp
// CP1 format (CP1 fields at top level)
observability->log_info(
    "Step execution completed",
    "tenant_123",      // tenant_id (top level)
    "run_abc123",      // run_id (top level)
    "flow_xyz789",     // flow_id (top level)
    "step_001",        // step_id (top level)
    "trace_def456",    // trace_id (top level)
    {
        {"block_type", "http.request"},  // Technical details in context
        {"status", "success"}
    }
);
```

#### Step 2: Extract CP1 Fields from Context

**Before**:
```cpp
// CP1 fields in nested object
std::unordered_map<std::string, std::string> context = {
    {"correlation.tenant_id", "tenant_123"},
    {"correlation.run_id", "run_abc123"},
    {"message", "Step execution completed"}
};
```

**After**:
```cpp
// CP1 fields as separate parameters
observability->log_info(
    "Step execution completed",
    "tenant_123",  // Extracted from context
    "run_abc123",  // Extracted from context
    "",            // flow_id not available
    "",            // step_id not available
    "",            // trace_id not available
    {
        {"message", "Step execution completed"}  // Only technical details
    }
);
```

#### Step 3: Use BlockContext Helper

**Before**:
```cpp
// Manual extraction
std::string tenant_id = extract_tenant_id(context);
std::string run_id = extract_run_id(context);
// ... etc
```

**After**:
```cpp
// Use BlockContext helper
BlockContext ctx;
ctx.tenant_id = "tenant_123";
ctx.run_id = "run_abc123";
ctx.flow_id = "flow_xyz789";
ctx.step_id = "step_001";
ctx.trace_id = "trace_def456";

observability->log_info_with_context(
    "Step execution completed",
    ctx,  // CP1 fields extracted automatically
    {
        {"block_type", "http.request"},
        {"status", "success"}
    }
);
```

#### Step 4: Update Log Parsing

**Before** (parsing nested correlation object):
```bash
# Old format parsing
jq '.correlation.tenant_id' log.json
```

**After** (parsing top-level CP1 fields):
```bash
# CP1 format parsing
jq '.tenant_id' log.json
jq '.run_id' log.json
jq '.flow_id' log.json
```

#### Step 5: Update Log Aggregation Queries

**Before**:
```bash
# Old format query
jq 'select(.correlation.tenant_id == "tenant_123")' logs.jsonl
```

**After**:
```bash
# CP1 format query
jq 'select(.tenant_id == "tenant_123")' logs.jsonl
jq 'select(.run_id == "run_abc123")' logs.jsonl
```

### Migration Checklist

- [ ] Update all log calls to use CP1 format
- [ ] Extract CP1 fields from nested objects to top level
- [ ] Use BlockContext helper when available
- [ ] Update log parsing scripts
- [ ] Update log aggregation queries
- [ ] Test log format compliance
- [ ] Verify PII filtering still works
- [ ] Update documentation

---

## API Reference

### Observability Class

#### Constructor

```cpp
Observability(const std::string& worker_id);
```

**Parameters**:
- `worker_id` (string): Unique identifier for the worker instance

**Example**:
```cpp
auto observability = std::make_unique<Observability>("worker_12345");
```

#### Logging Methods

##### log_info

```cpp
void log_info(
    const std::string& message,
    const std::string& tenant_id = "",
    const std::string& run_id = "",
    const std::string& flow_id = "",
    const std::string& step_id = "",
    const std::string& trace_id = "",
    const std::unordered_map<std::string, std::string>& context = {}
);
```

**Description**: Log an informational message.

**Parameters**:
- `message` (string): Human-readable log message
- `tenant_id` (string, optional): Tenant identifier
- `run_id` (string, optional): Run identifier
- `flow_id` (string, optional): Flow identifier
- `step_id` (string, optional): Step identifier
- `trace_id` (string, optional): Trace identifier
- `context` (map, optional): Additional structured context

**Output**: JSON log entry to stdout

**Example**:
```cpp
observability->log_info(
    "Step execution completed",
    "tenant_123", "run_abc123", "flow_xyz789", "step_001", "trace_def456",
    {{"block_type", "http.request"}, {"status", "success"}}
);
```

##### log_warn

```cpp
void log_warn(
    const std::string& message,
    const std::string& tenant_id = "",
    const std::string& run_id = "",
    const std::string& flow_id = "",
    const std::string& step_id = "",
    const std::string& trace_id = "",
    const std::unordered_map<std::string, std::string>& context = {}
);
```

**Description**: Log a warning message.

**Parameters**: Same as `log_info`

**Output**: JSON log entry to stdout

##### log_error

```cpp
void log_error(
    const std::string& message,
    const std::string& tenant_id = "",
    const std::string& run_id = "",
    const std::string& flow_id = "",
    const std::string& step_id = "",
    const std::string& trace_id = "",
    const std::unordered_map<std::string, std::string>& context = {}
);
```

**Description**: Log an error message.

**Parameters**: Same as `log_info`

**Output**: JSON log entry to stderr

##### log_debug

```cpp
void log_debug(
    const std::string& message,
    const std::string& tenant_id = "",
    const std::string& run_id = "",
    const std::string& flow_id = "",
    const std::string& step_id = "",
    const std::string& trace_id = "",
    const std::unordered_map<std::string, std::string>& context = {}
);
```

**Description**: Log a debug message.

**Parameters**: Same as `log_info`

**Output**: JSON log entry to stdout

##### log_info_with_context

```cpp
void log_info_with_context(
    const std::string& message,
    const BlockContext& ctx,
    const std::unordered_map<std::string, std::string>& context = {}
);
```

**Description**: Log an informational message using BlockContext for CP1 fields.

**Parameters**:
- `message` (string): Human-readable log message
- `ctx` (BlockContext): Block context containing CP1 fields
- `context` (map, optional): Additional structured context

**Example**:
```cpp
BlockContext ctx;
ctx.tenant_id = "tenant_123";
ctx.run_id = "run_abc123";
ctx.flow_id = "flow_xyz789";
ctx.step_id = "step_001";
ctx.trace_id = "trace_def456";

observability->log_info_with_context(
    "Step execution completed",
    ctx,
    {{"block_type", "http.request"}, {"status", "success"}}
);
```

**Similar methods**: `log_warn_with_context`, `log_error_with_context`, `log_debug_with_context`

#### Health Endpoint Methods

##### start_health_endpoint

```cpp
void start_health_endpoint(const std::string& address, uint16_t port);
```

**Description**: Start the health endpoint HTTP server.

**Parameters**:
- `address` (string): IP address to bind to (e.g., "0.0.0.0" for all interfaces)
- `port` (uint16_t): Port number (e.g., 9091)

**Example**:
```cpp
observability->start_health_endpoint("0.0.0.0", 9091);
```

##### stop_health_endpoint

```cpp
void stop_health_endpoint();
```

**Description**: Stop the health endpoint HTTP server.

**Example**:
```cpp
observability->stop_health_endpoint();
```

##### get_health_response

```cpp
std::string get_health_response();
```

**Description**: Get the health endpoint response JSON (without HTTP).

**Returns**: JSON string with `status` and `timestamp` fields

**Example**:
```cpp
std::string response = observability->get_health_response();
// Returns: {"status":"healthy","timestamp":"2025-01-27T12:00:00.123456Z"}
```

#### Internal Methods

##### format_json_log

```cpp
std::string format_json_log(
    const std::string& level,
    const std::string& message,
    const std::string& tenant_id,
    const std::string& run_id,
    const std::string& flow_id,
    const std::string& step_id,
    const std::string& trace_id,
    const std::unordered_map<std::string, std::string>& context
);
```

**Description**: Format a log entry as JSON string (internal method, but can be used for testing).

**Returns**: JSON string representation of log entry

---

## Test Infrastructure Documentation

### CMake Configuration

Tests are configured in `apps/caf/processor/tests/CMakeLists.txt`:

```cmake
# Test executables
add_executable(test_observability test_observability.cpp)
add_executable(test_health_endpoint test_health_endpoint.cpp)
add_executable(test_observability_performance test_observability_performance.cpp)

# Link with main project libraries
target_link_libraries(test_observability
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add tests
add_test(NAME ObservabilityTest COMMAND test_observability)
add_test(NAME HealthEndpointTest COMMAND test_health_endpoint)
add_test(NAME ObservabilityPerformanceTest COMMAND test_observability_performance)
```

### Test Framework

Worker tests use **simple assert-based testing** (C++ standard library):

- **No external framework**: Uses `assert()` and `std::cout` for output
- **Simple structure**: Each test is a function, called from `main()`
- **Easy to understand**: Minimal boilerplate

**Test Structure**:
```cpp
#include <iostream>
#include <cassert>
#include "beamline/worker/observability.hpp"

void test_example() {
    std::cout << "Testing example..." << std::endl;
    auto observability = std::make_unique<Observability>("test");
    assert(observability != nullptr);
    std::cout << "✓ Test passed" << std::endl;
}

int main() {
    test_example();
    return 0;
}
```

### Test Execution Workflow

1. **Build Tests**:
   ```bash
   cd apps/caf/processor
   mkdir -p build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build . -- -j"$(nproc)"
   ```

2. **Run Tests**:
   ```bash
   # Run all tests
   ctest --verbose
   
   # Run specific test
   ctest -R ObservabilityTest --verbose
   ```

3. **Run with Coverage**:
   ```bash
   mkdir -p build-coverage && cd build-coverage
   cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
   cmake --build . -- -j"$(nproc)"
   ctest --verbose
   bash ../scripts/generate_coverage.sh
   ```

### Test Environment Setup

**Prerequisites**:
- CMake 3.16+
- C++20 compiler
- CAF (C++ Actor Framework)
- nlohmann/json (for JSON parsing in tests)

**Installation**:
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libcaf-dev

# Or use package manager for your distribution
```

### Test Categories

1. **Unit Tests**: Test individual functions and methods
2. **Integration Tests**: Test component interactions (e.g., health endpoint)
3. **Performance Tests**: Test performance and throughput
4. **Edge Case Tests**: Test boundary conditions and error handling

For detailed test documentation, see `apps/caf/processor/tests/README.md`.

---

## Troubleshooting

### Common Issues

#### Logs Not Appearing

**Problem**: Logs are not being written to stdout/stderr.

**Solutions**:
1. **Check if Worker is running**:
   ```bash
   ps aux | grep beamline_worker
   ```

2. **Verify stdout/stderr redirection**:
   ```bash
   # Check if logs are redirected to file
   ./beamline_worker > worker.log 2>&1
   tail -f worker.log
   ```

3. **Check for errors in stderr**:
   ```bash
   ./beamline_worker 2>&1 | grep -i error
   ```

4. **Verify observability is initialized**:
   ```cpp
   // In code, ensure Observability is created
   auto observability = std::make_unique<Observability>("worker_12345");
   ```

#### Health Endpoint Not Responding

**Problem**: HTTP health endpoint (`GET /_health`) fails or returns errors.

**Solutions**:
1. **Verify Worker is running**:
   ```bash
   ps aux | grep beamline_worker
   ```

2. **Check if port is correct (default: 9091)**:
   ```bash
   netstat -tuln | grep 9091
   # Or
   ss -tuln | grep 9091
   ```

3. **Test health endpoint manually**:
   ```bash
   curl http://localhost:9091/_health
   # Expected: HTTP 200 OK with JSON
   # {
   #   "status": "healthy",
   #   "timestamp": "2025-01-27T12:00:00.123456Z"
   # }
   ```

4. **Check if health endpoint is started**:
   ```cpp
   // In code, ensure health endpoint is started
   observability->start_health_endpoint("0.0.0.0", 9091);
   ```

5. **Check for port conflicts**:
   ```bash
   # Check if another process is using port 9091
   lsof -i :9091
   # Or
   netstat -tuln | grep 9091
   ```

6. **Verify firewall settings**:
   ```bash
   # Check if firewall is blocking port
   sudo ufw status
   # Or
   sudo iptables -L -n | grep 9091
   ```

#### PII Not Being Filtered

**Problem**: Sensitive data appears in logs instead of `[REDACTED]`.

**Solutions**:
1. **Verify PII filtering is working**:
   ```bash
   # Check logs for PII fields
   ./beamline_worker 2>&1 | jq 'select(.context.api_key != null)'
   # Should show "[REDACTED]" values, not actual secrets
   ```

2. **Check that sensitive fields match filter patterns**:
   - Fields: `password`, `api_key`, `secret`, `token`, `access_token`, `refresh_token`, `authorization`, `credit_card`, `ssn`, `email`, `phone`
   - Filtering is case-insensitive

3. **Test PII filtering manually**:
   ```cpp
   // Test with PII in context
   observability->log_info(
       "Test PII filtering",
       "tenant_123", "run_abc", "flow_xyz", "step_001", "trace_def",
       {
           {"api_key", "sk-1234567890abcdef"},
           {"password", "secret_password"},
           {"email", "user@example.com"},
           {"block_type", "http.request"}
       }
   );
   
   // Check output - PII fields should be "[REDACTED]"
   ```

4. **Verify recursive filtering**:
   ```bash
   # Check nested objects are filtered
   ./beamline_worker 2>&1 | jq '.context | to_entries[] | select(.value == "[REDACTED]")'
   ```

#### Invalid JSON in Logs

**Problem**: Log entries are not valid JSON or cannot be parsed.

**Solutions**:
1. **Check for special characters in log messages**:
   ```bash
   # Validate JSON format
   ./beamline_worker 2>&1 | jq '.'
   # Should parse without errors
   ```

2. **Verify JSON serialization**:
   ```bash
   # Check each log line is valid JSON
   ./beamline_worker 2>&1 | while IFS= read -r line; do
       echo "$line" | jq . > /dev/null || echo "Invalid JSON: $line"
   done
   ```

3. **Check for encoding issues**:
   ```bash
   # Check file encoding
   file worker.log
   # Should be UTF-8
   ```

4. **Review observability implementation**:
   - Check `format_json_log()` function in `observability.cpp`
   - Verify nlohmann::json is used correctly
   - Check for unescaped special characters

#### CP1 Fields Missing in Logs

**Problem**: CP1 correlation fields (`tenant_id`, `run_id`, `flow_id`, `step_id`, `trace_id`) are not appearing in logs.

**Solutions**:
1. **Verify CP1 fields are provided**:
   ```cpp
   // Check that CP1 fields are passed to log functions
   observability->log_info(
       "Test message",
       "tenant_123",  // tenant_id
       "run_abc123",  // run_id
       "flow_xyz789", // flow_id
       "step_001",    // step_id
       "trace_def456", // trace_id
       {}
   );
   ```

2. **Check logs for CP1 fields**:
   ```bash
   # Verify CP1 fields are at top level
   ./beamline_worker 2>&1 | jq 'select(.tenant_id != null)'
   ```

3. **Use BlockContext helper**:
   ```cpp
   // Use BlockContext to ensure CP1 fields are extracted
   BlockContext ctx;
   ctx.tenant_id = "tenant_123";
   ctx.run_id = "run_abc123";
   ctx.flow_id = "flow_xyz789";
   ctx.step_id = "step_001";
   ctx.trace_id = "trace_def456";
   
   observability->log_info_with_context("Test message", ctx, {});
   ```

4. **Verify CP1 fields are at top level**:
   ```bash
   # Check CP1 fields are not in nested object
   ./beamline_worker 2>&1 | jq 'select(.correlation != null)'
   # Should return empty (CP1 fields should be at top level, not in correlation object)
   ```

#### Health Endpoint Returns Wrong Format

**Problem**: Health endpoint response is not CP1-compliant (missing fields or wrong format).

**Solutions**:
1. **Verify response format**:
   ```bash
   curl http://localhost:9091/_health | jq .
   # Expected:
   # {
   #   "status": "healthy",
   #   "timestamp": "2025-01-27T12:00:00.123456Z"
   # }
   ```

2. **Check status field value**:
   ```bash
   # Status must be "healthy", not "ok" or "up"
   curl -s http://localhost:9091/_health | jq '.status'
   # Should output: "healthy"
   ```

3. **Verify timestamp format**:
   ```bash
   # Timestamp must be ISO 8601 with microseconds (6 digits)
   curl -s http://localhost:9091/_health | jq '.timestamp'
   # Should match: YYYY-MM-DDTHH:MM:SS.ssssssZ
   ```

4. **Check implementation**:
   - Review `get_health_response()` in `observability.cpp`
   - Verify ISO 8601 timestamp generation
   - Check status value is "healthy"

#### Performance Issues

**Problem**: Logging is slow or causing performance degradation.

**Solutions**:
1. **Check log volume**:
   ```bash
   # Count log entries per second
   ./beamline_worker 2>&1 | pv -l > /dev/null
   ```

2. **Reduce DEBUG logging in production**:
   ```cpp
   // Avoid excessive DEBUG logging in production
   // Use INFO level for production, DEBUG only for development
   ```

3. **Optimize context size**:
   ```cpp
   // Keep context objects small
   // Avoid large strings or arrays in context
   ```

4. **Run performance tests**:
   ```bash
   # Run performance tests to measure overhead
   cd build
   ./tests/test_observability_performance
   ```

5. **Check for blocking I/O**:
   - Verify logging doesn't block main execution
   - Check stdout/stderr buffering
   - Consider async logging if needed (future enhancement)

#### Timestamp Format Issues

**Problem**: Timestamps are not in ISO 8601 format or missing microseconds.

**Solutions**:
1. **Verify timestamp format**:
   ```bash
   # Check timestamp format in logs
   ./beamline_worker 2>&1 | jq '.timestamp'
   # Should match: YYYY-MM-DDTHH:MM:SS.ssssssZ (6 digits for microseconds)
   ```

2. **Check timestamp generation**:
   - Review `get_iso8601_timestamp()` in `observability.cpp`
   - Verify microseconds are included (6 digits)
   - Check timezone is UTC (Z suffix)

3. **Test timestamp format**:
   ```bash
   # Extract and validate timestamps
   ./beamline_worker 2>&1 | jq -r '.timestamp' | while read ts; do
       # Should match ISO 8601 with microseconds
       echo "$ts" | grep -E '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{6}Z$'
   done
   ```

### Debugging Tips

#### Enable Verbose Logging

```bash
# Run Worker with verbose output
./beamline_worker 2>&1 | tee worker.log

# Filter by log level
./beamline_worker 2>&1 | jq 'select(.level == "ERROR")'
./beamline_worker 2>&1 | jq 'select(.level == "DEBUG")'
```

#### Check Log Format

```bash
# Validate all log entries are valid JSON
./beamline_worker 2>&1 | while IFS= read -r line; do
    echo "$line" | jq . > /dev/null || echo "Invalid JSON: $line"
done
```

#### Trace Specific Request

```bash
# Filter logs by trace_id
./beamline_worker 2>&1 | jq 'select(.trace_id == "trace_abc123")'

# Filter logs by tenant_id
./beamline_worker 2>&1 | jq 'select(.tenant_id == "tenant_123")'

# Filter logs by run_id
./beamline_worker 2>&1 | jq 'select(.run_id == "run_abc123")'
```

#### Test Health Endpoint

```bash
# Basic health check
curl -v http://localhost:9091/_health

# Check HTTP status code
curl -s -o /dev/null -w "%{http_code}" http://localhost:9091/_health
# Expected: 200

# Check response time
time curl -s http://localhost:9091/_health > /dev/null
```

#### Verify PII Filtering

```bash
# Check for PII fields in logs
./beamline_worker 2>&1 | jq 'select(.context | to_entries[] | .key | test("(?i)(password|api_key|secret|token|email|phone)"))'

# Should show "[REDACTED]" values, not actual secrets
./beamline_worker 2>&1 | jq '.context | to_entries[] | select(.value == "[REDACTED]")'
```

### Getting Help

If you encounter issues not covered here:

1. **Check Documentation**:
   - `apps/caf/processor/docs/OBSERVABILITY.md` - This document
   - `apps/caf/processor/docs/PRODUCTION_LOGGING.md` - Production logging guide
   - `apps/caf/processor/tests/README.md` - Test documentation

2. **Run Tests**:
   ```bash
   # Run observability tests
   cd build
   ctest -R ObservabilityTest --verbose
   ctest -R HealthEndpointTest --verbose
   ```

3. **Check Logs**:
   ```bash
   # Review Worker logs for errors
   ./beamline_worker 2>&1 | jq 'select(.level == "ERROR")'
   ```

4. **Review Implementation**:
   - `apps/caf/processor/src/observability.cpp` - Observability implementation
   - `apps/caf/processor/include/beamline/worker/observability.hpp` - Header file

---

## References

- `docs/OBSERVABILITY_CP1_INVARIANTS.md` - CP1 observability invariants specification
- `docs/OBSERVABILITY.md` - General observability requirements
- `docs/OBSERVABILITY_CONVENTIONS.md` - Logging conventions
- `src/observability.cpp` - Observability implementation
- `include/beamline/worker/observability.hpp` - Observability header
- `scripts/observability/test_worker_observability.sh` - Test script
- `scripts/observability/validate_observability_e2e.sh` - E2E validation script
- `apps/caf/processor/tests/README.md` - Test documentation
- `apps/caf/processor/docs/PRODUCTION_LOGGING.md` - Production logging guide
