#pragma once

#include "beamline/worker/feature_flags.hpp"
#include <chrono>
#include <functional>
#include <future>
#include <thread>

namespace beamline {
namespace worker {

/**
 * CP2 Timeout Enforcement
 * 
 * Implements:
 * - FS operation timeouts (W3-2.1)
 * - HTTP connection timeout (W3-2.2)
 * - Total timeout across retries (W3-2.3)
 * 
 * Gated behind CP2_COMPLETE_TIMEOUT_ENABLED feature flag.
 */
class TimeoutEnforcement {
public:
    /**
     * Execute operation with timeout
     * 
     * Returns true if operation completed within timeout, false if timeout occurred
     */
    template<typename Result>
    static bool execute_with_timeout(
        std::function<Result()> operation,
        int64_t timeout_ms,
        Result& result,
        Result timeout_result) {
        
        if (!FeatureFlags::is_complete_timeout_enabled()) {
            // CP1 behavior: no timeout enforcement
            result = operation();
            return true;
        }
        
        // CP2 behavior: enforce timeout using async execution
        auto future = std::async(std::launch::async, operation);
        
        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
        
        if (status == std::future_status::timeout) {
            // Timeout occurred
            result = timeout_result;
            return false;
        }
        
        // Operation completed
        result = future.get();
        return true;
    }
    
    /**
     * Get FS operation timeout based on operation type
     */
    static int64_t get_fs_timeout_ms(const std::string& operation_type) {
        if (!FeatureFlags::is_complete_timeout_enabled()) {
            // CP1 behavior: no timeout
            return 0; // 0 means no timeout
        }
        
        // CP2 behavior: operation-specific timeouts
        if (operation_type == "read" || operation_type == "fs.blob_get") {
            return 5000; // 5 seconds for read operations
        } else if (operation_type == "write" || operation_type == "fs.blob_put") {
            return 10000; // 10 seconds for write operations
        } else if (operation_type == "delete") {
            return 3000; // 3 seconds for delete operations
        }
        
        return 5000; // Default timeout
    }
    
    /**
     * Get HTTP connection timeout
     */
    static int64_t get_http_connection_timeout_ms() {
        if (!FeatureFlags::is_complete_timeout_enabled()) {
            // CP1 behavior: no separate connection timeout
            return 0; // 0 means use total timeout
        }
        
        // CP2 behavior: separate connection timeout
        return 5000; // 5 seconds for connection establishment
    }
    
    /**
     * Get HTTP total timeout (including connection + request)
     */
    static int64_t get_http_total_timeout_ms(int64_t request_timeout_ms) {
        if (!FeatureFlags::is_complete_timeout_enabled()) {
            // CP1 behavior: use request timeout only
            return request_timeout_ms;
        }
        
        // CP2 behavior: connection timeout + request timeout
        return get_http_connection_timeout_ms() + request_timeout_ms;
    }
};

} // namespace worker
} // namespace beamline

