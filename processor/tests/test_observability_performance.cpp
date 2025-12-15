#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>
#include "beamline/worker/core.hpp"
#include "beamline/worker/observability.hpp"
#include <nlohmann/json.hpp>

using namespace beamline::worker;
using json = nlohmann::json;

// Performance test results
struct PerformanceResults {
    double logs_per_second;
    double pii_filtering_latency_us;
    double json_serialization_time_us;
    size_t memory_overhead_bytes;
    double cpu_overhead_percent;
};

// Measure logging throughput
void test_logging_throughput() {
    std::cout << "Testing logging throughput..." << std::endl;
    
    auto observability = std::make_unique<Observability>("perf_test");
    
    const int num_logs = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_logs; i++) {
        observability->log_info("Performance test message", "tenant_123", "run_abc", "flow_xyz", "step_001", "trace_def", {
            {"iteration", std::to_string(i)},
            {"test", "throughput"}
        });
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double logs_per_second = (num_logs * 1000000.0) / duration.count();
    
    std::cout << "  Logs: " << num_logs << std::endl;
    std::cout << "  Duration: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Throughput: " << logs_per_second << " logs/second" << std::endl;
    std::cout << "✓ Logging throughput test completed" << std::endl;
}

// Measure PII filtering latency
void test_pii_filtering_latency() {
    std::cout << "Testing PII filtering latency..." << std::endl;
    
    auto observability = std::make_unique<Observability>("perf_test");
    
    const int num_iterations = 1000;
    std::unordered_map<std::string, std::string> context_with_pii = {
        {"api_key", "sk-1234567890abcdef"},
        {"password", "secret_password"},
        {"email", "user@example.com"},
        {"credit_card", "4111111111111111"},
        {"ssn", "123-45-6789"},
        {"block_type", "http.request"},
        {"status", "success"}
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; i++) {
        observability->log_info("PII filtering test", "tenant_123", "run_abc", "flow_xyz", "step_001", "trace_def", context_with_pii);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_latency_us = duration.count() / (double)num_iterations;
    
    std::cout << "  Iterations: " << num_iterations << std::endl;
    std::cout << "  Total duration: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Average latency: " << avg_latency_us << " microseconds per log entry" << std::endl;
    std::cout << "✓ PII filtering latency test completed" << std::endl;
}

// Measure JSON serialization performance
void test_json_serialization_performance() {
    std::cout << "Testing JSON serialization performance..." << std::endl;
    
    auto observability = std::make_unique<Observability>("perf_test");
    
    const int num_iterations = 1000;
    std::unordered_map<std::string, std::string> large_context;
    for (int i = 0; i < 50; i++) {
        large_context["field_" + std::to_string(i)] = "value_" + std::to_string(i);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; i++) {
        observability->log_info("JSON serialization test", "tenant_123", "run_abc", "flow_xyz", "step_001", "trace_def", large_context);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = duration.count() / (double)num_iterations;
    
    std::cout << "  Iterations: " << num_iterations << std::endl;
    std::cout << "  Total duration: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Average time: " << avg_time_us << " microseconds per log entry" << std::endl;
    std::cout << "✓ JSON serialization performance test completed" << std::endl;
}

// Measure memory usage (approximate)
void test_memory_usage() {
    std::cout << "Testing memory usage..." << std::endl;
    
    auto observability = std::make_unique<Observability>("perf_test");
    
    // Create a large context object
    std::unordered_map<std::string, std::string> large_context;
    for (int i = 0; i < 100; i++) {
        large_context["field_" + std::to_string(i)] = "value_" + std::to_string(i) + "_with_some_data";
    }
    
    // Estimate memory overhead (rough approximation)
    size_t context_size = 0;
    for (const auto& [key, value] : large_context) {
        context_size += key.size() + value.size();
    }
    
    std::cout << "  Context size: ~" << context_size << " bytes" << std::endl;
    std::cout << "  Estimated overhead: ~" << (context_size * 1.2) << " bytes per log entry (with JSON overhead)" << std::endl;
    std::cout << "✓ Memory usage test completed" << std::endl;
}

// Measure concurrent logging performance
void test_concurrent_logging() {
    std::cout << "Testing concurrent logging performance..." << std::endl;
    
    auto observability = std::make_unique<Observability>("perf_test");
    
    const int num_threads = 4;
    const int logs_per_thread = 1000;
    std::atomic<int> completed_logs{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&observability, &completed_logs, logs_per_thread, t]() {
            for (int i = 0; i < logs_per_thread; i++) {
                observability->log_info("Concurrent test message", "tenant_123", "run_abc", "flow_xyz", "step_001", "trace_def", {
                    {"thread_id", std::to_string(t)},
                    {"iteration", std::to_string(i)}
                });
                completed_logs++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double logs_per_second = (completed_logs.load() * 1000000.0) / duration.count();
    
    std::cout << "  Threads: " << num_threads << std::endl;
    std::cout << "  Logs per thread: " << logs_per_thread << std::endl;
    std::cout << "  Total logs: " << completed_logs.load() << std::endl;
    std::cout << "  Duration: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Throughput: " << logs_per_second << " logs/second" << std::endl;
    std::cout << "✓ Concurrent logging performance test completed" << std::endl;
}

int main() {
    std::cout << "=== Worker Observability Performance Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_logging_throughput();
        std::cout << std::endl;
        
        test_pii_filtering_latency();
        std::cout << std::endl;
        
        test_json_serialization_performance();
        std::cout << std::endl;
        
        test_memory_usage();
        std::cout << std::endl;
        
        test_concurrent_logging();
        std::cout << std::endl;
        
        std::cout << "=== All Performance Tests Completed ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Performance test failed: " << e.what() << std::endl;
        return 1;
    }
}

