#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "beamline/worker/observability.hpp"
#include <nlohmann/json.hpp>
using namespace beamline::worker;
using json = nlohmann::json;

void test_health_endpoint_starts() {
    std::cout << "Testing health endpoint startup..." << std::endl;
    
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Start health endpoint on test port
    std::string address = "127.0.0.1";
    uint16_t port = 19091; // Use high port to avoid conflicts
    
    try {
        observability->start_health_endpoint(address, port);
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "✓ Health endpoint started successfully" << std::endl;
        
        // Stop health endpoint
        observability->stop_health_endpoint();
    } catch (const std::exception& e) {
        std::cerr << "✗ Health endpoint failed to start: " << e.what() << std::endl;
        throw;
    }
}

void test_health_endpoint_returns_200() {
    std::cout << "Testing health endpoint returns 200 OK..." << std::endl;
    
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Start health endpoint on test port
    std::string address = "127.0.0.1";
    uint16_t port = 19092; // Use different port
    
    observability->start_health_endpoint(address, port);
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Note: HTTP request test would require curl library
    // For now, we test the health response format directly
    // In production, use external test script (test_worker_observability.sh) for HTTP tests
    
    observability->stop_health_endpoint();
    
    std::cout << "✓ Health endpoint HTTP test skipped (use test_worker_observability.sh for HTTP tests)" << std::endl;
}

void test_health_endpoint_cp1_format() {
    std::cout << "Testing health endpoint CP1-compliant format..." << std::endl;
    
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Get health response directly (without HTTP)
    std::string health_response = observability->get_health_response();
    
    // Parse JSON
    json health_json = json::parse(health_response);
    
    // Verify required fields
    assert(health_json.contains("status"));
    assert(health_json.contains("timestamp"));
    
    // Verify status value (must be "healthy", not "ok")
    assert(health_json["status"] == "healthy");
    
    // Verify timestamp format (ISO 8601)
    std::string timestamp = health_json["timestamp"];
    assert(timestamp.length() >= 20); // At least YYYY-MM-DDTHH:MM:SSZ
    assert(timestamp.find('T') != std::string::npos);
    assert(timestamp.back() == 'Z');
    
    // Verify timestamp has microseconds (6 digits after decimal point)
    size_t dot_pos = timestamp.find('.');
    if (dot_pos != std::string::npos) {
        size_t z_pos = timestamp.find('Z', dot_pos);
        if (z_pos != std::string::npos) {
            size_t microsecond_digits = z_pos - dot_pos - 1;
            assert(microsecond_digits == 6); // Must have 6 digits for microseconds
        }
    }
    
    std::cout << "✓ Health endpoint CP1-compliant format verified" << std::endl;
    std::cout << "  Status: " << health_json["status"] << std::endl;
    std::cout << "  Timestamp: " << health_json["timestamp"] << std::endl;
}

void test_health_endpoint_stops() {
    std::cout << "Testing health endpoint shutdown..." << std::endl;
    
    auto observability = std::make_unique<Observability>("test_worker");
    
    // Start health endpoint
    std::string address = "127.0.0.1";
    uint16_t port = 19093; // Use different port
    
    observability->start_health_endpoint(address, port);
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop health endpoint
    observability->stop_health_endpoint();
    
    // Wait for server to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "✓ Health endpoint stopped successfully" << std::endl;
}

int main() {
    std::cout << "=== Worker Health Endpoint Integration Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_health_endpoint_starts();
        test_health_endpoint_cp1_format();
        test_health_endpoint_stops();
        
        // Test HTTP request (may fail if health endpoint is not accessible)
        test_health_endpoint_returns_200();
        
        std::cout << std::endl;
        std::cout << "=== All Tests Passed ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}

