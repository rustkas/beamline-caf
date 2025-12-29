# Beamline CAF Components

> C++ Actor Framework based components for the Beamline platform

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CAF](https://img.shields.io/badge/CAF-0.18-green.svg)](https://actor-framework.org/)

## 📋 Overview

This repository contains high-performance C++ components built with the [C++ Actor Framework (CAF)](https://actor-framework.org/) for the Beamline workflow orchestration platform. The components provide scalable, observable, and fault-tolerant execution environments for workflow steps.

## 🏗️ Components

### 1. **Processor** (`processor/`)

High-performance workflow execution engine with actor-based architecture.

**Features:**
- ⚡ **High Throughput**: ≥500 tasks/s execution capacity
- 🔒 **Multi-tenant Isolation**: Per-tenant quotas and resource limits
- 📊 **Full Observability**: Prometheus metrics, OpenTelemetry tracing, JSON logs
- 🛡️ **Sandbox Mode**: Safe execution environment for testing
- 🧩 **Modular Executors**: Pluggable block executors (HTTP, FS, SQL, Human)

**Resource Pools:**
- CPU Pool (compute-intensive tasks)
- GPU Pool (ML/AI workloads)
- I/O Pool (network and disk operations)

**[→ Processor Documentation](processor/README.md)**

### 2. **Ingress** (`ingress/`)

Observability stub for ingress components (work in progress).

**Features:**
- 🔍 **Observability Stubs**: Ready for integration
- 📈 **Metrics Collection**: Prometheus-compatible metrics

**Status:** 🚧 Early development

## 🚀 Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install -y \
  cmake \
  build-essential \
  libcaf-dev \
  libcurl4-openssl-dev \
  libsqlite3-dev \
  libprometheus-cpp-dev

# macOS (Homebrew)
brew install cmake caf curl sqlite3
```

### Building All Components

```bash
# Clone repository
git clone https://github.com/YOUR_ORG/beamline-caf.git
cd beamline-caf

# Build processor
cd processor
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest --verbose
```

### Running the Processor

```bash
cd processor/build

./beamline_worker \
  --cpu-pool-size=8 \
  --gpu-pool-size=2 \
  --io-pool-size=16 \
  --max-memory-mb=2048 \
  --nats-url=nats://localhost:4222 \
  --prometheus-endpoint=0.0.0.0:9090
```

## 📦 Project Structure

```
apps/caf/
├── processor/              # Main workflow execution engine
│   ├── src/               # Source files
│   ├── include/           # Header files
│   ├── tests/             # Unit and integration tests
│   ├── scripts/           # Build and deployment scripts
│   ├── docs/              # Documentation
│   ├── alerts/            # Prometheus alert rules
│   ├── dashboards/        # Grafana dashboards
│   └── CMakeLists.txt     # Build configuration
│
├── ingress/               # Ingress observability (WIP)
│   └── src/               # Observability stubs
│
├── build/                 # Build artifacts (gitignored)
└── README.md             # This file
```

## 🧪 Testing

### Run All Tests

```bash
cd processor/build
ctest --verbose
```

### Test Categories

- **Unit Tests**: Component-level testing
- **Integration Tests**: End-to-end workflow testing
- **Performance Tests**: Throughput and latency validation
- **Chaos Tests**: Fault injection and recovery

### Coverage Reports

```bash
cd processor
cmake -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
make test
make coverage
```

## 📊 Observability

### Metrics (Prometheus)

**Processor Metrics:**
- `worker_tasks_total`: Total tasks by type and status
- `worker_task_latency_ms`: Task execution latency (histogram)
- `worker_resource_usage`: CPU time and memory usage
- `worker_pool_queue_depth`: Queue depth per pool

**Endpoint:** `http://localhost:9090/metrics`

### Tracing (OpenTelemetry)

Each task execution creates spans with:
- `tenant_id`: Tenant identifier
- `flow_id`: Flow execution identifier
- `step_id`: Step identifier
- `block_type`: Block type (http, fs, sql, human)
- `worker_id`: Worker instance ID

**Exporter:** OTLP (OpenTelemetry Protocol)

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

## 🎯 Performance Targets

| Metric | Target | Gate |
|--------|--------|------|
| **Throughput** | ≥500 tasks/s | CP3-LC |
| **HTTP Latency** | ≤500ms | Internal requests |
| **File I/O Latency** | ≤200ms | Local operations |
| **SQL Latency** | ≤300ms | Simple selects |
| **Memory** | ≤2GB | Per worker |

## 🔒 Security

- **RBAC Integration**: Validates scopes before execution
- **Path Restrictions**: File operations limited to safe directories
- **SQL Injection Prevention**: Parameterized queries only
- **Sandbox Isolation**: Separate execution environment for untrusted code
- **Resource Limits**: Per-tenant CPU and memory quotas

## 🚦 Multi-tenancy

- **Resource Quotas**: Per-tenant memory and CPU time limits
- **Isolation**: Separate execution contexts and data
- **Fair Scheduling**: Round-robin and priority-based scheduling
- **Usage Tracking**: Comprehensive resource usage monitoring

## 🛠️ Development

### Code Style

```bash
# Format code
clang-format -i src/**/*.cpp include/**/*.hpp

# Lint code
clang-tidy src/**/*.cpp -- -std=c++20
```

### Building with Sanitizers

```bash
# Address Sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
make -j$(nproc)

# Thread Sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..
make -j$(nproc)
```

### Debugging

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with GDB
gdb ./beamline_worker
```

## 📚 Documentation

- **[Processor README](processor/README.md)** - Detailed processor documentation
- **[Processor Docs](processor/docs/)** - Architecture and design docs
- **[API Reference](processor/docs/API.md)** - Public API documentation (if exists)

## 🗺️ Roadmap

### Phase 1 (Current)
- ✅ Core worker architecture
- ✅ Basic block executors (HTTP, FS, SQL, Human)
- ✅ Observability (metrics, traces, logs)
- ✅ Sandbox mode
- ✅ Multi-tenant isolation

### Phase 2 (Planned)
- ⏳ Advanced blocks (AI inference, media processing)
- ⏳ Streaming I/O with io_uring
- ⏳ Plugin system with dynamic loading
- ⏳ ML-based resource scheduling
- ⏳ Distributed execution

### Phase 3 (Future)
- 📋 Multi-worker coordination
- 📋 Advanced fault tolerance
- 📋 Auto-scaling
- 📋 Cost optimization

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## 🔗 Related Projects

- **[Beamline Router](../otp/router)** - Erlang/OTP routing and orchestration
- **[C-Gateway](../c-gateway)** - High-performance C gateway
- **[Beamline Platform](https://github.com/YOUR_ORG/beamline)** - Main platform repository

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/YOUR_ORG/beamline-caf/issues)
- **Documentation**: [Wiki](https://github.com/YOUR_ORG/beamline-caf/wiki)
- **Email**: support@beamline.example.com

---

**Built with ❤️ using C++ Actor Framework**
