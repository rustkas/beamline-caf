# Observability Metrics Dashboard (CP2 Planning)

**Component**: Worker CAF (`apps/caf/processor/`)  
**Date**: 2025-01-27  
**Status**: 📋 **CP2 PLANNING** (Not Implemented)

---

## Overview

This document describes the planned observability metrics dashboard for Worker when Prometheus metrics are implemented in CP2. This is a **planning document only** - implementation is deferred to CP2.

---

## Planned Metrics

### 1. Task Execution Metrics

**Metrics**:
- `worker_tasks_total` - Total number of tasks executed (counter)
  - Labels: `block_type`, `status` (ok, error, timeout)
- `worker_task_latency_ms` - Task execution latency (histogram)
  - Labels: `block_type`
  - Buckets: 50ms, 100ms, 200ms, 500ms, 1000ms, 2000ms, 5000ms

**Dashboard Panels**:
- Task execution rate (tasks/second)
- Task success/error rate
- Task latency distribution (p50, p95, p99)
- Task latency by block type

---

### 2. Resource Usage Metrics

**Metrics**:
- `worker_resource_usage` - Resource usage metrics (gauge)
  - Labels: `block_type`, `resource` (cpu_time_ms, memory_bytes)
- `worker_pool_queue_depth` - Queue depth for worker pools (gauge)
  - Labels: `resource_class` (cpu, gpu, io)

**Dashboard Panels**:
- CPU time per block type
- Memory usage per block type
- Pool queue depth by resource class
- Resource utilization trends

---

### 3. Health Metrics

**Metrics**:
- `worker_health_status` - Health status (gauge)
  - Values: 1 (healthy), 0 (unhealthy)
- `worker_health_check_duration_ms` - Health check duration (histogram)

**Dashboard Panels**:
- Health status over time
- Health check latency
- Health endpoint availability

---

## Grafana Dashboard JSON Template

**File**: `apps/caf/processor/dashboards/worker-observability.json` (to be created in CP2)

```json
{
  "dashboard": {
    "title": "Worker Observability Dashboard",
    "tags": ["worker", "caf", "observability"],
    "timezone": "browser",
    "panels": [
      {
        "id": 1,
        "title": "Task Execution Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(worker_tasks_total[5m])",
            "legendFormat": "{{block_type}} - {{status}}"
          }
        ]
      },
      {
        "id": 2,
        "title": "Task Latency (p95)",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.95, rate(worker_task_latency_ms_bucket[5m]))",
            "legendFormat": "{{block_type}}"
          }
        ]
      },
      {
        "id": 3,
        "title": "Pool Queue Depth",
        "type": "graph",
        "targets": [
          {
            "expr": "worker_pool_queue_depth",
            "legendFormat": "{{resource_class}}"
          }
        ]
      },
      {
        "id": 4,
        "title": "Resource Usage",
        "type": "graph",
        "targets": [
          {
            "expr": "worker_resource_usage{resource=\"cpu_time_ms\"}",
            "legendFormat": "{{block_type}} - CPU"
          },
          {
            "expr": "worker_resource_usage{resource=\"memory_bytes\"}",
            "legendFormat": "{{block_type}} - Memory"
          }
        ]
      },
      {
        "id": 5,
        "title": "Health Status",
        "type": "stat",
        "targets": [
          {
            "expr": "worker_health_status",
            "legendFormat": "Health"
          }
        ]
      }
    ]
  }
}
```

**Note**: This is a placeholder template. Actual dashboard will be created in CP2 when Prometheus metrics are implemented.

---

## Alerting Rules (CP2 Planning)

**File**: `apps/caf/processor/alerts/worker-alerts.yml` (to be created in CP2)

### High Task Error Rate

```yaml
- alert: WorkerHighTaskErrorRate
  expr: rate(worker_tasks_total{status="error"}[5m]) > 0.1
  for: 5m
  labels:
    severity: warning
  annotations:
    summary: "High task error rate detected"
    description: "Worker task error rate is {{ $value }} errors/second"
```

### High Task Latency

```yaml
- alert: WorkerHighTaskLatency
  expr: histogram_quantile(0.95, rate(worker_task_latency_ms_bucket[5m])) > 2000
  for: 5m
  labels:
    severity: warning
  annotations:
    summary: "High task latency detected"
    description: "95th percentile task latency is {{ $value }}ms"
```

### Pool Queue Depth High

```yaml
- alert: WorkerPoolQueueDepthHigh
  expr: worker_pool_queue_depth > 100
  for: 5m
  labels:
    severity: warning
  annotations:
    summary: "High pool queue depth detected"
    description: "Pool {{ $labels.resource_class }} queue depth is {{ $value }}"
```

### Worker Unhealthy

```yaml
- alert: WorkerUnhealthy
  expr: worker_health_status == 0
  for: 1m
  labels:
    severity: critical
  annotations:
    summary: "Worker is unhealthy"
    description: "Worker health check is failing"
```

**Note**: These alerting rules are placeholders. Actual rules will be created in CP2.

---

## Metric Naming Conventions

### Prefix

All Worker metrics use the prefix `worker_`:
- `worker_tasks_total`
- `worker_task_latency_ms`
- `worker_resource_usage`
- `worker_pool_queue_depth`
- `worker_health_status`

### Labels

**Standard Labels**:
- `worker_id` - Worker instance identifier
- `block_type` - Type of block being executed
- `status` - Task status (ok, error, timeout)
- `resource_class` - Resource class (cpu, gpu, io)
- `resource` - Resource type (cpu_time_ms, memory_bytes)

**CP1 Correlation Labels** (when available):
- `tenant_id` - Tenant identifier
- `run_id` - Run identifier
- `flow_id` - Flow identifier
- `step_id` - Step identifier
- `trace_id` - Trace identifier

### Units

- **Time**: milliseconds (ms) or seconds (s)
- **Memory**: bytes
- **Counters**: total (no unit)
- **Rates**: per second (calculated via `rate()`)

---

## Dashboard Layout (Planned)

### Row 1: Overview
- Task execution rate
- Task success/error rate
- Health status

### Row 2: Performance
- Task latency (p50, p95, p99)
- Task latency by block type
- Resource usage trends

### Row 3: Resources
- Pool queue depth by resource class
- CPU time per block type
- Memory usage per block type

### Row 4: Health
- Health status over time
- Health check latency
- Error rate trends

---

## Implementation Notes (CP2)

### Prometheus Exporter

Worker will expose Prometheus metrics on a dedicated HTTP endpoint:
- **Path**: `/metrics`
- **Port**: 9090 (default, configurable)
- **Format**: Prometheus text format

### Metrics Collection

Metrics will be collected from:
- Task execution events
- Resource pool statistics
- Health check results
- Block executor metrics

### Integration Points

- **Prometheus**: Scrape `/metrics` endpoint
- **Grafana**: Query Prometheus for dashboard data
- **Alertmanager**: Evaluate alerting rules and send notifications

---

## References

- `apps/caf/processor/docs/OBSERVABILITY.md` - Current observability documentation
- `docs/OBSERVABILITY_CP1_INVARIANTS.md` - CP1 observability invariants
- `docs/OBSERVABILITY.md` - General observability requirements

---

## Status

**Current Status**: 📋 **PLANNING ONLY**

- ✅ Dashboard structure planned
- ✅ Metrics defined
- ✅ Alerting rules planned
- ❌ Prometheus metrics not implemented (CP2)
- ❌ Grafana dashboard not created (CP2)
- ❌ Alertmanager integration not implemented (CP2)

**Next Steps** (CP2):
1. Implement Prometheus metrics exporter
2. Add metrics collection to Worker
3. Create Grafana dashboard
4. Configure Alertmanager rules
5. Test dashboard and alerts

---

## Support

For questions about CP2 planning:
- Review: `apps/caf/processor/docs/OBSERVABILITY.md`
- Check: `docs/OBSERVABILITY_CP1_INVARIANTS.md`
- Contact: Architecture team for CP2 planning

