# Production Logging Guide

**Component**: Worker CAF (`apps/caf/processor/`)  
**Date**: 2025-01-27  
**Status**: Production-Ready

---

## Overview

Worker logs to stdout/stderr in structured JSON format. In production environments, logs should be redirected to files and rotated to prevent disk space issues.

This guide provides examples for common log rotation tools and deployment scenarios.

---

## Log Format

Worker uses structured JSON logging with the following characteristics:

- **Format**: JSON (one log entry per line)
- **Output**: stdout (INFO, WARN, DEBUG) and stderr (ERROR)
- **Timestamp**: ISO 8601 with microseconds (6 digits)
- **CP1 Fields**: tenant_id, run_id, flow_id, step_id, trace_id at top level
- **PII Filtering**: Automatic filtering of sensitive data

Example log entry:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "worker",
  "message": "Step execution started",
  "tenant_id": "tenant_123",
  "run_id": "run_abc123",
  "flow_id": "flow_xyz789",
  "step_id": "step_001",
  "trace_id": "trace_def456",
  "context": {
    "worker_id": "worker_12345",
    "block_type": "http.request",
    "status": "success"
  }
}
```

---

## Log Rotation Strategies

### 1. systemd Service with Log Rotation

**Service File**: `/etc/systemd/system/worker-caf.service`

```ini
[Unit]
Description=Beamline Worker CAF
After=network.target

[Service]
Type=simple
User=worker
Group=worker
WorkingDirectory=/opt/beamline/worker
ExecStart=/usr/bin/beamline_worker
Restart=always
RestartSec=10

# Redirect stdout and stderr to log files
StandardOutput=append:/var/log/worker/worker.log
StandardError=append:/var/log/worker/worker_error.log

# Log rotation via systemd-journald (optional)
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

**Logrotate Configuration**: `/etc/logrotate.d/worker-caf`

```bash
/var/log/worker/*.log {
    daily
    rotate 7
    compress
    delaycompress
    notifempty
    create 0640 worker worker
    sharedscripts
    postrotate
        systemctl reload worker-caf > /dev/null 2>&1 || true
    endscript
}
```

**Setup**:

```bash
# Create log directory
sudo mkdir -p /var/log/worker
sudo chown worker:worker /var/log/worker

# Install service
sudo cp worker-caf.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable worker-caf
sudo systemctl start worker-caf

# Verify logs
tail -f /var/log/worker/worker.log
```

---

### 2. Docker with Log Rotation

**Docker Compose Example**:

```yaml
version: '3.8'

services:
  worker:
    image: beamline/worker-caf:latest
    container_name: worker-caf
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "7"
        labels: "production"
    environment:
      - WORKER_CPU_POOL_SIZE=4
      - WORKER_GPU_POOL_SIZE=2
      - WORKER_IO_POOL_SIZE=2
    restart: unless-stopped
```

**Docker Run Example**:

```bash
docker run -d \
  --name worker-caf \
  --log-driver json-file \
  --log-opt max-size=10m \
  --log-opt max-file=7 \
  beamline/worker-caf:latest
```

**Docker Log Location**: `/var/lib/docker/containers/<container-id>/<container-id>-json.log`

---

### 3. logrotate for Standalone Deployment

**Logrotate Configuration**: `/etc/logrotate.d/worker-caf`

```bash
/var/log/worker/worker.log
/var/log/worker/worker_error.log {
    daily
    rotate 7
    compress
    delaycompress
    notifempty
    create 0640 worker worker
    missingok
    copytruncate
}
```

**Setup Script**:

```bash
#!/bin/bash
# Setup log rotation for Worker

# Create log directory
sudo mkdir -p /var/log/worker
sudo chown worker:worker /var/log/worker

# Redirect stdout/stderr to log files
exec >> /var/log/worker/worker.log 2>> /var/log/worker/worker_error.log

# Start Worker
/usr/bin/beamline_worker
```

---

### 4. Kubernetes Log Rotation

**Deployment with Log Rotation**:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: worker-caf
spec:
  replicas: 3
  template:
    metadata:
      labels:
        app: worker-caf
    spec:
      containers:
      - name: worker
        image: beamline/worker-caf:latest
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "1000m"
        # Log rotation via Kubernetes (handled by kubelet)
        # Logs are automatically rotated at node level
```

**Kubernetes Log Location**: `/var/log/pods/<namespace>_<pod-name>_<pod-id>/<container-name>/`

**Log Rotation**: Handled automatically by kubelet (default: 10MB per container, 5 files)

---

## Log Aggregation

### Loki Integration

Worker logs are JSON-formatted and can be easily ingested by Loki:

**Promtail Configuration**: `promtail-config.yaml`

```yaml
server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://loki:3100/loki/api/v1/push

scrape_configs:
  - job_name: worker-caf
    static_configs:
      - targets:
          - localhost
        labels:
          job: worker-caf
          __path__: /var/log/worker/*.log
    pipeline_stages:
      - json:
          expressions:
            timestamp: timestamp
            level: level
            component: component
            message: message
            tenant_id: tenant_id
            run_id: run_id
            flow_id: flow_id
            step_id: step_id
            trace_id: trace_id
      - labels:
          level:
          component:
          tenant_id:
          run_id:
          flow_id:
          step_id:
          trace_id:
```

### ELK Stack Integration

**Filebeat Configuration**: `filebeat.yml`

```yaml
filebeat.inputs:
  - type: log
    enabled: true
    paths:
      - /var/log/worker/*.log
    json.keys_under_root: true
    json.add_error_key: true
    fields:
      component: worker-caf
      environment: production

output.elasticsearch:
  hosts: ["elasticsearch:9200"]
  index: "worker-caf-%{+yyyy.MM.dd}"

processors:
  - add_host_metadata:
      when.not.contains.tags: forwarded
```

---

## Best Practices

### 1. Log Retention

- **Development**: 3-7 days
- **Staging**: 7-14 days
- **Production**: 14-30 days (depending on compliance requirements)

### 2. Log Size Limits

- **Max file size**: 10-50MB per log file
- **Max files**: 5-10 rotated files
- **Total disk space**: Monitor and alert on disk usage

### 3. Log Monitoring

- Monitor log file sizes
- Alert on ERROR log rate
- Track log rotation failures
- Monitor disk space usage

### 4. Security

- Set appropriate file permissions (640 for logs)
- Restrict access to log directories
- Encrypt log files at rest (if required)
- Filter PII before logging (already implemented in Worker)

---

## Troubleshooting

### Logs Not Rotating

**Check logrotate status**:

```bash
sudo logrotate -d /etc/logrotate.d/worker-caf  # Dry run
sudo logrotate -f /etc/logrotate.d/worker-caf  # Force rotation
```

**Check logrotate logs**:

```bash
grep logrotate /var/log/syslog
```

### Disk Space Issues

**Check disk usage**:

```bash
du -sh /var/log/worker/
df -h /var/log/worker/
```

**Manual cleanup**:

```bash
# Remove old logs
find /var/log/worker/ -name "*.log.*" -mtime +7 -delete

# Compress old logs
gzip /var/log/worker/*.log.*
```

### Log Format Issues

**Verify JSON format**:

```bash
# Check if logs are valid JSON
tail -n 100 /var/log/worker/worker.log | jq .
```

**Check for PII leaks**:

```bash
# Search for potential PII
grep -i "password\|api_key\|secret" /var/log/worker/worker.log
# Should show "[REDACTED]" if PII filtering is working
```

---

## References

- `apps/caf/processor/docs/OBSERVABILITY.md` - Observability documentation
- `docs/OBSERVABILITY_CP1_INVARIANTS.md` - CP1 observability invariants
- `docs/OBSERVABILITY.md` - General observability requirements

---

## Support

For issues or questions:
- Check logs: `/var/log/worker/worker.log`
- Check error logs: `/var/log/worker/worker_error.log`
- Review documentation: `apps/caf/processor/docs/OBSERVABILITY.md`

