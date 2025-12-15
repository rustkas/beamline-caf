#!/bin/bash
# Generate code coverage report for Worker observability tests
# Uses gcov/lcov for coverage analysis

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-coverage}"

echo "=== Generating Code Coverage Report ==="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build with coverage enabled:"
    echo "  mkdir -p build-coverage && cd build-coverage"
    echo "  cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug"
    echo "  cmake --build ."
    exit 1
fi

# Check if lcov is installed
if ! command -v lcov &> /dev/null; then
    echo "Error: lcov is not installed"
    echo "Install with: sudo apt-get install -y lcov"
    exit 1
fi

# Check if gcov is installed
if ! command -v gcov &> /dev/null; then
    echo "Error: gcov is not installed"
    echo "Install with: sudo apt-get install -y gcov"
    exit 1
fi

cd "$BUILD_DIR"

# Run tests first to generate coverage data
echo "Running tests to generate coverage data..."
ctest --verbose || echo "Warning: Some tests may have failed, but continuing with coverage generation"

# Capture coverage data
echo ""
echo "Capturing coverage data..."
lcov --capture --directory . --output-file coverage.info

# Remove system headers and test files from coverage
echo "Filtering coverage data..."
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --remove coverage.info '*/tests/*' --output-file coverage.info
lcov --remove coverage.info '*/build/*' --output-file coverage.info

# Generate HTML report
echo "Generating HTML coverage report..."
genhtml coverage.info --output-directory coverage_html --title "Worker Observability Coverage Report"

# Display summary
echo ""
echo "=== Coverage Summary ==="
lcov --summary coverage.info

echo ""
echo "=== Coverage Report Generated ==="
echo "HTML report: $BUILD_DIR/coverage_html/index.html"
echo "Coverage info: $BUILD_DIR/coverage.info"
echo ""

# Check coverage thresholds
echo "Checking coverage thresholds..."
COVERAGE_SUMMARY=$(lcov --summary coverage.info 2>&1)

# Extract line coverage percentage
LINE_COVERAGE=$(echo "$COVERAGE_SUMMARY" | grep -oP 'lines\.\.\.\.\.\.: \K[0-9.]+' | head -1)
BRANCH_COVERAGE=$(echo "$COVERAGE_SUMMARY" | grep -oP 'branches\.\.\.\.: \K[0-9.]+' | head -1)
FUNCTION_COVERAGE=$(echo "$COVERAGE_SUMMARY" | grep -oP 'functions\.\.\.: \K[0-9.]+' | head -1)

echo "Line coverage: ${LINE_COVERAGE:-0}% (target: >80%)"
echo "Branch coverage: ${BRANCH_COVERAGE:-0}% (target: >70%)"
echo "Function coverage: ${FUNCTION_COVERAGE:-0}% (target: >90%)"

# Check if thresholds are met
THRESHOLD_PASSED=true

if (( $(echo "${LINE_COVERAGE:-0} < 80" | bc -l) )); then
    echo "Warning: Line coverage below threshold (80%)"
    THRESHOLD_PASSED=false
fi

if (( $(echo "${BRANCH_COVERAGE:-0} < 70" | bc -l) )); then
    echo "Warning: Branch coverage below threshold (70%)"
    THRESHOLD_PASSED=false
fi

if (( $(echo "${FUNCTION_COVERAGE:-0} < 90" | bc -l) )); then
    echo "Warning: Function coverage below threshold (90%)"
    THRESHOLD_PASSED=false
fi

if [ "$THRESHOLD_PASSED" = true ]; then
    echo ""
    echo "✓ All coverage thresholds met"
    exit 0
else
    echo ""
    echo "⚠ Some coverage thresholds not met (non-blocking)"
    exit 0  # Don't fail, just warn
fi

