#!/bin/bash
# Benchmark script for Worker observability overhead
# Measures logging throughput, PII filtering latency, and JSON serialization performance

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
BENCHMARK_OUTPUT="${BENCHMARK_OUTPUT:-$PROJECT_ROOT/benchmark_results.json}"

echo "=== Worker Observability Benchmark ==="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Output: $BENCHMARK_OUTPUT"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build Worker first:"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. && cmake --build ."
    exit 1
fi

# Check if performance test exists
PERF_TEST="$BUILD_DIR/tests/test_observability_performance"
if [ ! -f "$PERF_TEST" ]; then
    echo "Error: Performance test not found: $PERF_TEST"
    echo "Please build tests:"
    echo "  cd build && cmake --build . --target test_observability_performance"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$(dirname "$BENCHMARK_OUTPUT")"
mkdir -p "$OUTPUT_DIR"

# Run performance tests
echo "Running performance tests..."
cd "$BUILD_DIR"

# Capture test output
TEST_OUTPUT=$(./tests/test_observability_performance 2>&1)

# Extract metrics from output
echo ""
echo "=== Benchmark Results ==="
echo "$TEST_OUTPUT"

# Parse results and generate JSON report
echo ""
echo "Generating JSON benchmark report..."

# Create JSON report
cat > "$BENCHMARK_OUTPUT" <<EOF
{
  "benchmark_date": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "component": "worker-caf",
  "test_environment": {
    "hostname": "$(hostname)",
    "os": "$(uname -s)",
    "kernel": "$(uname -r)",
    "cpu_cores": $(nproc),
    "memory_gb": $(free -g | awk '/^Mem:/ {print $2}')
  },
  "results": {
    "logging_throughput": {
      "description": "Logs per second",
      "unit": "logs/second",
      "note": "Extract from test output"
    },
    "pii_filtering_latency": {
      "description": "Average PII filtering latency",
      "unit": "microseconds",
      "note": "Extract from test output"
    },
    "json_serialization_time": {
      "description": "Average JSON serialization time",
      "unit": "microseconds",
      "note": "Extract from test output"
    },
    "memory_overhead": {
      "description": "Estimated memory overhead per log entry",
      "unit": "bytes",
      "note": "Extract from test output"
    },
    "concurrent_logging_throughput": {
      "description": "Concurrent logging throughput",
      "unit": "logs/second",
      "note": "Extract from test output"
    }
  },
  "raw_output": $(jq -Rs . <<< "$TEST_OUTPUT")
}
EOF

echo "Benchmark report saved to: $BENCHMARK_OUTPUT"
echo ""

# Display summary
echo "=== Benchmark Summary ==="
echo "To view detailed results, check: $BENCHMARK_OUTPUT"
echo ""
echo "To compare with/without observability:"
echo "  1. Build Worker without observability (if possible)"
echo "  2. Run benchmark again"
echo "  3. Compare results"
echo ""

# Check if jq is available for pretty printing
if command -v jq &> /dev/null; then
    echo "Pretty-printed results:"
    jq '.' "$BENCHMARK_OUTPUT"
else
    echo "Install jq for pretty-printed JSON: sudo apt-get install -y jq"
    cat "$BENCHMARK_OUTPUT"
fi

echo ""
echo "=== Benchmark Complete ==="

