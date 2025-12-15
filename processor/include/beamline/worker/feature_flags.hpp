#pragma once

#include <string>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace beamline {
namespace worker {

/**
 * CP2 Feature Flags
 * 
 * All CP2 features are gated behind feature flags to preserve CP1 baseline behavior.
 * Feature flags default to `false` (CP1 behavior).
 * 
 * Feature flags can be set via environment variables:
 * - CP2_ADVANCED_RETRY_ENABLED
 * - CP2_COMPLETE_TIMEOUT_ENABLED
 * - CP2_QUEUE_MANAGEMENT_ENABLED
 * - CP2_OBSERVABILITY_METRICS_ENABLED
 */
class FeatureFlags {
public:
    /**
     * Check if CP2 Advanced Retry features are enabled
     * 
     * Gates:
     * - Exponential backoff (W3-1.1)
     * - Error classification (W3-1.3)
     * - Retry budget management (W3-1.4)
     */
    static bool is_advanced_retry_enabled() {
        return get_env_bool("CP2_ADVANCED_RETRY_ENABLED", false);
    }
    
    /**
     * Check if CP2 Complete Timeout features are enabled
     * 
     * Gates:
     * - FS operation timeouts (W3-2.1)
     * - HTTP connection timeout (W3-2.2)
     * - Total timeout across retries (W3-2.3)
     */
    static bool is_complete_timeout_enabled() {
        return get_env_bool("CP2_COMPLETE_TIMEOUT_ENABLED", false);
    }
    
    /**
     * Check if CP2 Queue Management features are enabled
     * 
     * Gates:
     * - Bounded queue (W3-4.1)
     * - Queue depth monitoring (W3-4.2)
     * - Queue rejection handling (W3-4.3)
     */
    static bool is_queue_management_enabled() {
        return get_env_bool("CP2_QUEUE_MANAGEMENT_ENABLED", false);
    }
    
    /**
     * Check if CP2 Observability Metrics features are enabled
     * 
     * Gates:
     * - Prometheus `/metrics` endpoint (O1-1.6)
     * - Metrics collection (O1-1.5)
     * - All Worker metrics
     */
    static bool is_observability_metrics_enabled() {
        return get_env_bool("CP2_OBSERVABILITY_METRICS_ENABLED", false);
    }
    
private:
    /**
     * Get boolean value from environment variable
     * 
     * Returns `true` if environment variable is set to:
     * - "true" (case-insensitive)
     * - "1"
     * - "yes" (case-insensitive)
     * 
     * Returns `default_value` if environment variable is not set or has other value.
     */
    static bool get_env_bool(const char* env_var, bool default_value) {
        const char* value = std::getenv(env_var);
        if (value == nullptr) {
            return default_value;
        }
        
        std::string str_value(value);
        std::transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);
        
        return (str_value == "true" || str_value == "1" || str_value == "yes");
    }
};

} // namespace worker
} // namespace beamline

