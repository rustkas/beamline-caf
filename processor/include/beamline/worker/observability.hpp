#pragma once

#include "beamline/worker/core.hpp"
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <opentelemetry/trace/tracer.h>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

namespace beamline {
namespace worker {

class Observability {
public:
    Observability(const std::string& worker_id);
    ~Observability();
    
    // Metrics (CP1 - legacy)
    void increment_task_total(const std::string& block_type, const std::string& status);
    void record_task_latency(const std::string& block_type, int64_t latency_ms);
    void record_resource_usage(const std::string& block_type, int64_t cpu_time_ms, int64_t mem_bytes);
    void set_pool_queue_depth(ResourceClass resource_class, int64_t depth);
    
    // CP2 Wave 1 Metrics (gated behind CP2_OBSERVABILITY_METRICS_ENABLED)
    void record_step_execution(const std::string& step_type, 
                               const std::string& execution_status,
                               const std::string& tenant_id = "",
                               const std::string& run_id = "",
                               const std::string& flow_id = "",
                               const std::string& step_id = "");
    
    void record_step_execution_duration(const std::string& step_type,
                                        const std::string& execution_status,
                                        double duration_seconds,
                                        const std::string& tenant_id = "",
                                        const std::string& run_id = "",
                                        const std::string& flow_id = "",
                                        const std::string& step_id = "");
    
    void record_step_error(const std::string& step_type,
                           const std::string& error_code,
                           const std::string& tenant_id = "",
                           const std::string& run_id = "",
                           const std::string& flow_id = "",
                           const std::string& step_id = "");
    
    void record_flow_execution_duration(double duration_seconds,
                                         const std::string& tenant_id = "",
                                         const std::string& run_id = "",
                                         const std::string& flow_id = "");
    
    void set_queue_depth(const std::string& resource_pool, int64_t depth);
    
    void set_active_tasks(const std::string& resource_pool, int64_t count);
    
    void set_health_status(const std::string& check, int64_t status); // 1 = healthy, 0 = unhealthy
    
    // CP2 Wave 1 Metrics Endpoint (gated behind CP2_OBSERVABILITY_METRICS_ENABLED)
    void start_metrics_endpoint(const std::string& address, uint16_t port);
    void stop_metrics_endpoint();
    std::string get_metrics_response(); // Prometheus text format
    
    // Tracing
    std::unique_ptr<opentelemetry::trace::Span> start_step_span(
        const std::string& operation,
        const std::string& tenant_id,
        const std::string& flow_id,
        const std::string& step_id,
        const std::string& block_type,
        const std::string& trace_id
    );
    
    // Logging
    void log_info(const std::string& message,
                  const std::string& tenant_id = "",
                  const std::string& run_id = "",
                  const std::string& flow_id = "",
                  const std::string& step_id = "",
                  const std::string& trace_id = "",
                  const std::unordered_map<std::string, std::string>& context = {});
    
    void log_warn(const std::string& message,
                  const std::string& tenant_id = "",
                  const std::string& run_id = "",
                  const std::string& flow_id = "",
                  const std::string& step_id = "",
                  const std::string& trace_id = "",
                  const std::unordered_map<std::string, std::string>& context = {});
    
    void log_error(const std::string& message,
                   const std::string& tenant_id = "",
                   const std::string& run_id = "",
                   const std::string& flow_id = "",
                   const std::string& step_id = "",
                   const std::string& trace_id = "",
                   const std::unordered_map<std::string, std::string>& context = {});
    
    void log_debug(const std::string& message,
                   const std::string& tenant_id = "",
                   const std::string& run_id = "",
                   const std::string& flow_id = "",
                   const std::string& step_id = "",
                   const std::string& trace_id = "",
                   const std::unordered_map<std::string, std::string>& context = {});
    
    // Helper functions to extract CP1 fields from BlockContext
    void log_info_with_context(const std::string& message,
                               const BlockContext& ctx,
                               const std::unordered_map<std::string, std::string>& context = {});
    
    void log_warn_with_context(const std::string& message,
                               const BlockContext& ctx,
                               const std::unordered_map<std::string, std::string>& context = {});
    
    void log_error_with_context(const std::string& message,
                                const BlockContext& ctx,
                                const std::unordered_map<std::string, std::string>& context = {});
    
    void log_debug_with_context(const std::string& message,
                                const BlockContext& ctx,
                                const std::unordered_map<std::string, std::string>& context = {});
    
    // Prometheus registry access
    std::shared_ptr<prometheus::Registry> registry() { return registry_; }
    
    // Health endpoint
    void start_health_endpoint(const std::string& address, uint16_t port);
    void stop_health_endpoint();
    
private:
    std::string worker_id_;
    std::shared_ptr<prometheus::Registry> registry_;
    
    // Metrics (CP1 - legacy)
    prometheus::Family<prometheus::Counter>* task_total_family_;
    prometheus::Family<prometheus::Histogram>* task_latency_family_;
    prometheus::Family<prometheus::Gauge>* resource_usage_family_;
    prometheus::Family<prometheus::Gauge>* pool_queue_depth_family_;
    
    // CP2 Wave 1 Metrics
    prometheus::Family<prometheus::Counter>* step_executions_total_family_;
    prometheus::Family<prometheus::Histogram>* step_execution_duration_seconds_family_;
    prometheus::Family<prometheus::Counter>* step_errors_total_family_;
    prometheus::Family<prometheus::Histogram>* flow_execution_duration_seconds_family_;
    prometheus::Family<prometheus::Gauge>* queue_depth_family_;
    prometheus::Family<prometheus::Gauge>* active_tasks_family_;
    prometheus::Family<prometheus::Gauge>* health_status_family_;
    
    // Tracer
    std::shared_ptr<opentelemetry::trace::Tracer> tracer_;
    
    // Health endpoint
    std::thread health_server_thread_;
    std::atomic<bool> health_server_running_{false};
    int health_server_socket_{-1};
    
    // CP2 Wave 1 Metrics endpoint (port 9092)
    std::thread metrics_server_thread_;
    std::atomic<bool> metrics_server_running_{false};
    int metrics_server_socket_{-1};
    
    void initialize_metrics();
    void initialize_cp2_metrics(); // CP2 Wave 1 metrics initialization
    void initialize_tracing();
    void health_server_loop(int socket_fd);
    void metrics_server_loop(int socket_fd); // CP2 Wave 1 metrics endpoint
    std::string get_health_response();
    std::string format_json_log(const std::string& level,
                                 const std::string& message,
                                 const std::string& tenant_id,
                                 const std::string& run_id,
                                 const std::string& flow_id,
                                 const std::string& step_id,
                                 const std::string& trace_id,
                                 const std::unordered_map<std::string, std::string>& context);
};

} // namespace worker
} // namespace beamline