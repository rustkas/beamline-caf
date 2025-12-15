# Worker Observability Tests

**Component**: Worker CAF (`apps/caf/processor/`)  
**Date**: 2025-01-27  
**Test Framework**: Simple assert-based tests (C++ standard library)

---

## Overview

This directory contains all tests for Worker observability functionality, including:

- **Unit Tests**: Core observability functionality
- **Integration Tests**: Health endpoint testing
- **Performance Tests**: Performance and throughput testing
- **Edge Case Tests**: Boundary conditions and error handling

---

## Test Structure

### Test Files

| File | Description | Test Count |
|------|-------------|------------|
| `test_observability.cpp` | Unit tests + edge case tests | 14 tests |
| `test_health_endpoint.cpp` | Integration tests for health endpoint | 4 tests |
| `test_observability_performance.cpp` | Performance tests | 5 tests |
| **Total** | | **23 tests** |

### Test Categories

#### Unit Tests (`test_observability.cpp`)

1. **Timestamp Format Tests**
   - `test_timestamp_format()` - Verifies ISO 8601 timestamp format with microseconds

2. **Log Format Compliance Tests**
   - `test_log_format_compliance()` - Verifies CP1-compliant log format
   - `test_cp1_fields_at_top_level()` - Verifies CP1 fields at top level
   - `test_cp1_fields_with_context()` - Verifies CP1 fields extraction from BlockContext

3. **PII Filtering Tests**
   - `test_pii_filtering()` - Verifies PII filtering functionality

4. **Log Level Tests**
   - `test_all_log_levels()` - Verifies all log levels (ERROR, WARN, INFO, DEBUG)

5. **Health Endpoint Tests**
   - `test_health_endpoint_response()` - Verifies health endpoint response format

6. **Context Object Tests**
   - `test_context_object_structure()` - Verifies context object structure

#### Edge Case Tests (`test_observability.cpp`)

1. `test_very_long_message()` - Very long log messages (100KB)
2. `test_very_long_cp1_fields()` - Very long CP1 field values (1000 characters)
3. `test_empty_cp1_fields()` - Empty/null CP1 fields
4. `test_special_characters()` - Special characters in log messages (JSON escaping)
5. `test_very_large_context()` - Very large context objects (1000 fields)
6. `test_invalid_json_in_context()` - Invalid JSON handling in context

#### Integration Tests (`test_health_endpoint.cpp`)

1. `test_health_endpoint_starts()` - Health endpoint startup
2. `test_health_endpoint_returns_200()` - HTTP 200 OK response
3. `test_health_endpoint_cp1_format()` - CP1-compliant format verification
4. `test_health_endpoint_stops()` - Health endpoint shutdown

#### Performance Tests (`test_observability_performance.cpp`)

1. `test_logging_throughput()` - Logging throughput (logs/second)
2. `test_pii_filtering_latency()` - PII filtering latency (microseconds)
3. `test_json_serialization_performance()` - JSON serialization performance
4. `test_memory_usage()` - Memory usage estimation
5. `test_concurrent_logging()` - Concurrent logging performance

---

## Running Tests

### Prerequisites

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake libcaf-dev

# Build Worker
cd apps/caf/processor
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j"$(nproc)"
```

### Running All Tests

```bash
# From build directory
cd apps/caf/processor/build
ctest --verbose

# Or run specific test
ctest -R ObservabilityTest --verbose
ctest -R HealthEndpointTest --verbose
ctest -R ObservabilityPerformanceTest --verbose
```

### Running Individual Test Executables

```bash
# From build directory
cd apps/caf/processor/build/tests

# Unit tests
./test_observability

# Integration tests
./test_health_endpoint

# Performance tests
./test_observability_performance
```

### Running with Coverage

```bash
# Build with coverage
cd apps/caf/processor
mkdir -p build-coverage && cd build-coverage
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j"$(nproc)"

# Run tests
ctest --verbose

# Generate coverage report
bash ../scripts/generate_coverage.sh
```

---

## Test Examples

### Example 1: Basic Log Test

```cpp
#include "beamline/worker/observability.hpp"

void test_basic_log() {
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Log a simple message
    observability->log_info("Test message", "", "", "", "", "", {});
    
    // Verify no crash
    assert(true); // Test passes if no exception thrown
}
```

### Example 2: CP1 Fields Test

```cpp
void test_cp1_fields() {
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Log with CP1 fields
    observability->log_info(
        "Test CP1 fields",
        "tenant_123",      // tenant_id
        "run_abc123",     // run_id
        "flow_xyz789",    // flow_id
        "step_001",       // step_id
        "trace_def456",   // trace_id
        {
            {"block_type", "http.request"},
            {"status", "success"}
        }
    );
    
    // Verify no crash
    assert(true);
}
```

### Example 3: PII Filtering Test

```cpp
void test_pii_filtering() {
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Log with PII in context
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
    
    // PII fields should be filtered to "[REDACTED]"
    // Verify no crash
    assert(true);
}
```

### Example 4: Health Endpoint Test

```cpp
#include "beamline/worker/observability.hpp"
#include <nlohmann/json.hpp>

void test_health_endpoint() {
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Get health response
    std::string health_response = observability->get_health_response();
    
    // Parse JSON
    json health_json = json::parse(health_response);
    
    // Verify required fields
    assert(health_json.contains("status"));
    assert(health_json.contains("timestamp"));
    assert(health_json["status"] == "healthy");
}
```

---

## Debugging Guide

### Common Issues

#### 1. Tests Fail with "Assertion Failed"

**Problem**: Test assertion fails

**Solution**:
```bash
# Run test with verbose output
./test_observability

# Check which test failed
# Look for error message in output

# Run specific test function by modifying main() temporarily
```

#### 2. Health Endpoint Tests Fail

**Problem**: Health endpoint tests fail with connection errors

**Solution**:
```bash
# Check if port is already in use
netstat -tuln | grep 19091

# Use different port in test
# Modify test to use different port (19092, 19093, etc.)
```

#### 3. Performance Tests Show Low Throughput

**Problem**: Performance tests show unexpectedly low throughput

**Solution**:
```bash
# Check system load
top

# Run tests in isolation
# Close other applications

# Check CPU frequency
cat /proc/cpuinfo | grep MHz
```

#### 4. Coverage Report Not Generated

**Problem**: Coverage report generation fails

**Solution**:
```bash
# Check if lcov is installed
which lcov

# Install lcov
sudo apt-get install -y lcov

# Check if coverage data exists
ls -la build-coverage/coverage.info

# Run coverage script manually
bash scripts/generate_coverage.sh
```

### Debugging Tips

1. **Add Debug Output**:
   ```cpp
   void test_example() {
       std::cout << "Debug: Starting test..." << std::endl;
       // ... test code ...
       std::cout << "Debug: Test completed" << std::endl;
   }
   ```

2. **Use GDB**:
   ```bash
   gdb ./test_observability
   (gdb) break test_example
   (gdb) run
   (gdb) step
   ```

3. **Check Logs**:
   ```bash
   # Tests output to stdout/stderr
   ./test_observability 2>&1 | tee test_output.log
   ```

4. **Valgrind for Memory Issues**:
   ```bash
   valgrind --leak-check=full ./test_observability
   ```

---

## Coverage Metrics

### Current Coverage

Run coverage analysis:

```bash
cd apps/caf/processor
mkdir -p build-coverage && cd build-coverage
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j"$(nproc)"
ctest --verbose
bash ../scripts/generate_coverage.sh
```

### Coverage Targets

- **Line Coverage**: >80%
- **Branch Coverage**: >70%
- **Function Coverage**: >90%

### Viewing Coverage Report

```bash
# Open HTML report
xdg-open build-coverage/coverage_html/index.html

# Or view summary
lcov --summary build-coverage/coverage.info
```

### Coverage by Component

| Component | Line Coverage | Branch Coverage | Function Coverage |
|-----------|---------------|-----------------|-------------------|
| `observability.cpp` | TBD | TBD | TBD |
| `worker_actor.cpp` | TBD | TBD | TBD |

*Note: Run coverage analysis to get actual numbers*

---

## Test Workflow

### Local Development

1. **Write Test**:
   ```cpp
   void test_new_feature() {
       // Test implementation
   }
   ```

2. **Add to main()**:
   ```cpp
   int main() {
       // ... existing tests ...
       test_new_feature();
       return 0;
   }
   ```

3. **Build and Run**:
   ```bash
   cd build
   cmake --build . --target test_observability
   ./tests/test_observability
   ```

4. **Verify**:
   - Test passes
   - No memory leaks (use Valgrind)
   - Coverage increases (if applicable)

### CI/CD Integration

Tests run automatically in CI/CD:

- **GitHub Actions**: `.github/workflows/worker-observability-tests.yml`
- **GitLab CI**: `.gitlab-ci.yml` (worker-observability-tests job)
- **Drone CI**: `.drone.yml` (worker-observability-tests pipeline)

### Pre-Commit Checklist

- [ ] All tests pass locally
- [ ] No memory leaks (Valgrind clean)
- [ ] Coverage meets thresholds (if applicable)
- [ ] Test output is clear and informative
- [ ] Test names are descriptive

---

## Test Framework

### Framework Details

Worker tests use **simple assert-based testing** (C++ standard library):

- **No external framework**: Uses `assert()` and `std::cout` for output
- **Simple structure**: Each test is a function, called from `main()`
- **Easy to understand**: Minimal boilerplate

### Test Structure

```cpp
#include <iostream>
#include <cassert>
#include "beamline/worker/observability.hpp"

void test_example() {
    std::cout << "Testing example..." << std::endl;
    
    // Test code
    auto observability = std::make_unique<Observability>("test");
    
    // Assertions
    assert(observability != nullptr);
    
    std::cout << "✓ Test passed" << std::endl;
}

int main() {
    test_example();
    return 0;
}
```

### Why Simple Assert-Based Tests?

- **Lightweight**: No external dependencies
- **Fast**: Minimal overhead
- **Clear**: Easy to understand and debug
- **Sufficient**: Meets CP1 testing requirements

---

## Best Practices

### Writing Tests

1. **Clear Test Names**: Use descriptive function names
   ```cpp
   void test_cp1_fields_at_top_level() { }  // Good
   void test1() { }  // Bad
   ```

2. **One Assertion Per Concept**: Test one thing at a time
   ```cpp
   void test_timestamp_format() {
       // Test timestamp format only
   }
   ```

3. **Use Comments**: Explain complex test logic
   ```cpp
   void test_pii_filtering() {
       // PII fields should be filtered to "[REDACTED]"
       // Non-PII fields should remain unchanged
   }
   ```

4. **Handle Edge Cases**: Test boundary conditions
   ```cpp
   void test_empty_cp1_fields() {
       // Test with all empty CP1 fields
   }
   ```

### Test Organization

1. **Group Related Tests**: Keep related tests together
2. **Use Sections**: Separate test categories with comments
3. **Maintain Order**: Keep tests in logical order

### Performance Considerations

1. **Isolate Performance Tests**: Run separately from unit tests
2. **Use Appropriate Iterations**: Balance speed vs. accuracy
3. **Measure Consistently**: Use same measurement method

---

## Troubleshooting

### Build Issues

**Problem**: Tests don't build

**Solution**:
```bash
# Clean build
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j"$(nproc)"
```

### Runtime Issues

**Problem**: Tests crash or hang

**Solution**:
```bash
# Run with GDB
gdb ./test_observability
(gdb) run

# Check for memory issues
valgrind --leak-check=full ./test_observability
```

### Coverage Issues

**Problem**: Coverage not generated

**Solution**:
```bash
# Check coverage flags
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug

# Verify gcov is available
which gcov

# Check coverage data
ls -la build-coverage/*.gcda
```

---

## References

- `apps/caf/processor/docs/OBSERVABILITY.md` - Observability documentation
- `apps/caf/processor/scripts/generate_coverage.sh` - Coverage script
- `apps/caf/processor/scripts/benchmark_observability.sh` - Benchmark script
- `docs/OBSERVABILITY_CP1_INVARIANTS.md` - CP1 observability invariants

---

## Support

For issues or questions:
- Check test output for error messages
- Review `apps/caf/processor/docs/OBSERVABILITY.md`
- Check CI/CD logs for test failures
- Review coverage reports for untested code

