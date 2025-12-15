#pragma once

#include "beamline/worker/core.hpp"
#include "beamline/worker/feature_flags.hpp"
#include <chrono>
#include <string>

namespace beamline {
namespace worker {

/**
 * CP2 Advanced Retry Policy
 * 
 * Implements:
 * - Exponential backoff (W3-1.1)
 * - Error classification (W3-1.3)
 * - Retry budget management (W3-1.4)
 * 
 * Gated behind CP2_ADVANCED_RETRY_ENABLED feature flag.
 */
class RetryPolicy {
public:
    struct Config {
        int64_t base_delay_ms = 100;      // Base delay for exponential backoff
        int64_t max_delay_ms = 5000;      // Maximum delay between retries
        int64_t total_timeout_ms = 30000; // Total timeout across all retries
        int32_t max_retries = 3;          // Maximum number of retries
    };
    
    RetryPolicy(const Config& config = Config()) : config_(config) {}
    
    /**
     * Calculate delay for retry attempt using exponential backoff
     * Formula: delay = base * 2^attempt (capped at max_delay_ms)
     */
    int64_t calculate_backoff_delay(int32_t attempt) const {
        if (!FeatureFlags::is_advanced_retry_enabled()) {
            // CP1 behavior: fixed backoff
            return 100 * (attempt + 1);
        }
        
        // CP2 behavior: exponential backoff
        int64_t delay = config_.base_delay_ms * (1LL << attempt);
        return std::min(delay, config_.max_delay_ms);
    }
    
    /**
     * Check if error is retryable
     * 
     * Retryable errors:
     * - Network errors (3xxx)
     * - 5xx HTTP errors
     * - Temporary FS errors
     * 
     * Non-retryable errors:
     * - 4xx HTTP errors (client errors)
     * - Validation errors (1xxx)
     * - Permission errors (2003)
     */
    bool is_retryable(ErrorCode error_code, int http_status_code = 0) const {
        if (!FeatureFlags::is_advanced_retry_enabled()) {
            // CP1 behavior: retry all errors
            return true;
        }
        
        // CP2 behavior: error classification
        
        // HTTP errors: 4xx = non-retryable, 5xx = retryable
        if (http_status_code > 0) {
            if (http_status_code >= 400 && http_status_code < 500) {
                return false; // 4xx = client error, non-retryable
            }
            if (http_status_code >= 500) {
                return true; // 5xx = server error, retryable
            }
        }
        
        // Error code classification
        switch (error_code) {
            // Network errors (3xxx) - retryable
            case ErrorCode::network_error:
            case ErrorCode::connection_timeout:
                return true;
            
            // Validation errors (1xxx) - non-retryable
            case ErrorCode::invalid_input:
            case ErrorCode::missing_required_field:
            case ErrorCode::invalid_format:
                return false;
            
            // Permission errors (2003) - non-retryable
            case ErrorCode::permission_denied:
                return false;
            
            // Execution errors (2xxx) - context-dependent
            case ErrorCode::execution_failed:
            case ErrorCode::resource_unavailable:
                // These might be retryable depending on context
                // For now, treat as retryable (can be refined)
                return true;
            
            // System errors (4xxx) - retryable
            case ErrorCode::internal_error:
            case ErrorCode::system_overload:
                return true;
            
            // Cancellation (5xxx) - non-retryable
            case ErrorCode::cancelled_by_user:
            case ErrorCode::cancelled_by_timeout:
                return false;
            
            default:
                // Unknown errors - default to retryable for safety
                return true;
        }
    }
    
    /**
     * Check if retry budget is exhausted
     * 
     * Returns true if total time spent (including backoff delays) exceeds total_timeout_ms
     */
    bool is_budget_exhausted(int64_t total_elapsed_ms, int32_t attempt) const {
        if (!FeatureFlags::is_advanced_retry_enabled()) {
            // CP1 behavior: no budget limit
            return false;
        }
        
        // CP2 behavior: check total timeout
        if (total_elapsed_ms >= config_.total_timeout_ms) {
            return true;
        }
        
        // Check if next retry would exceed budget
        int64_t next_backoff = calculate_backoff_delay(attempt);
        if (total_elapsed_ms + next_backoff >= config_.total_timeout_ms) {
            return true;
        }
        
        return false;
    }
    
    /**
     * Get maximum number of retries
     */
    int32_t max_retries() const {
        return config_.max_retries;
    }
    
    /**
     * Get total timeout
     */
    int64_t total_timeout_ms() const {
        return config_.total_timeout_ms;
    }
    
private:
    Config config_;
};

} // namespace worker
} // namespace beamline

