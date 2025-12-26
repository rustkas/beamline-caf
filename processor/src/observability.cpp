#include "beamline/worker/observability.hpp"
#include "beamline/worker/core.hpp"
#include "beamline/worker/feature_flags.hpp"
// #include <prometheus/exposer.h>
// #include <opentelemetry/trace/provider.h>
// #include <opentelemetry/exporters/ostream/span_exporter.h>
// #include <opentelemetry/sdk/trace/tracer_provider.h>
// #include <opentelemetry/sdk/trace/simple_processor.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <map>

namespace beamline {
namespace worker {

using json = nlohmann::json;

// PII/Secret fields to filter
static const std::vector<std::string> PII_FIELDS = {
    "password", "api_key", "secret", "token", "access_token", 
    "refresh_token", "authorization", "credit_card", "ssn", 
    "email", "phone"
};

// Helper function to check if a field name should be filtered (case-insensitive)
static bool is_pii_field(const std::string& field_name) {
    std::string lower_field = field_name;
    std::transform(lower_field.begin(), lower_field.end(), lower_field.begin(), ::tolower);
    
    for (const auto& pii_field : PII_FIELDS) {
        if (lower_field == pii_field || lower_field.find(pii_field) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Recursively filter PII from JSON object
static void filter_pii_recursive(json& obj) {
    if (obj.is_object()) {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            std::string key = it.key();
            if (is_pii_field(key)) {
                obj[key] = "[REDACTED]";
            } else if (it.value().is_object() || it.value().is_array()) {
                filter_pii_recursive(it.value());
            }
        }
    } else if (obj.is_array()) {
        for (auto& item : obj) {
            if (item.is_object() || item.is_array()) {
                filter_pii_recursive(item);
            }
        }
    }
}

// Generate ISO 8601 timestamp with microseconds
static std::string get_iso8601_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto duration = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration) % 1000000;
    
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
             microseconds.count());
    
    return std::string(buf);
}

Observability::Observability(const std::string& worker_id) : worker_id_(worker_id) {
    initialize_metrics();
    initialize_tracing();
}

Observability::~Observability() {
    stop_health_endpoint();
    stop_metrics_endpoint();
}

void Observability::initialize_metrics() {
    /*
    registry_ = std::make_shared<prometheus::Registry>();
    
    // Task total counter family
    task_total_family_ = &prometheus::BuildCounter()
        .Name("worker_tasks_total")
        .Help("Total number of tasks executed")
        .Labels({{"worker_id", worker_id_}})
        .Register(*registry_);
    
    // Task latency histogram family
    task_latency_family_ = &prometheus::BuildHistogram()
        .Name("worker_task_latency_ms")
        .Help("Task execution latency in milliseconds")
        .Labels({{"worker_id", worker_id_}})
        .Buckets({50, 100, 200, 500, 1000, 2000, 5000})
        .Register(*registry_);
    
    // Resource usage gauge family
    resource_usage_family_ = &prometheus::BuildGauge()
        .Name("worker_resource_usage")
        .Help("Resource usage metrics")
        .Labels({{"worker_id", worker_id_}})
        .Register(*registry_);
    
    // Pool queue depth gauge family
    pool_queue_depth_family_ = &prometheus::BuildGauge()
        .Name("worker_pool_queue_depth")
        .Help("Queue depth for worker pools")
        .Labels({{"worker_id", worker_id_}})
        .Register(*registry_);
    
    // CP2 Wave 1 Metrics - initialize if feature flag is enabled
    if (FeatureFlags::is_observability_metrics_enabled()) {
        initialize_cp2_metrics();
    }
    */
}

void Observability::initialize_cp2_metrics() {
    /*
    // CP2 Wave 1 Metrics - only initialize if feature flag is enabled
    // This method is called from initialize_metrics() if CP2_OBSERVABILITY_METRICS_ENABLED=true
    
    // Step executions counter
    step_executions_total_family_ = &prometheus::BuildCounter()
        .Name("worker_step_executions_total")
        .Help("Total number of step executions")
        .Register(*registry_);
    
    // Step execution duration histogram
    step_execution_duration_seconds_family_ = &prometheus::BuildHistogram()
        .Name("worker_step_execution_duration_seconds")
        .Help("Step execution duration in seconds")
        .Buckets({0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 2.0, 5.0})
        .Register(*registry_);
    
    // Step errors counter
    step_errors_total_family_ = &prometheus::BuildCounter()
        .Name("worker_step_errors_total")
        .Help("Total number of step errors")
        .Register(*registry_);
    
    // Flow execution duration histogram
    flow_execution_duration_seconds_family_ = &prometheus::BuildHistogram()
        .Name("worker_flow_execution_duration_seconds")
        .Help("Flow execution duration in seconds")
        .Buckets({0.01, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0})
        .Register(*registry_);
    
    // Queue depth gauge
    queue_depth_family_ = &prometheus::BuildGauge()
        .Name("worker_queue_depth")
        .Help("Current queue depth")
        .Register(*registry_);
    
    // Active tasks gauge
    active_tasks_family_ = &prometheus::BuildGauge()
        .Name("worker_active_tasks")
        .Help("Current number of active tasks")
        .Register(*registry_);
    
    // Health status gauge
    health_status_family_ = &prometheus::BuildGauge()
        .Name("worker_health_status")
        .Help("Health status (1 = healthy, 0 = unhealthy)")
        .Register(*registry_);
    */
}

void Observability::initialize_tracing() {
    /*
    // Create OStream exporter for now (can be replaced with OTLP exporter)
    auto exporter = std::make_unique<opentelemetry::exporter::trace::OStreamSpanExporter>();
    
    // Create processor
    auto processor = std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(std::move(exporter));
    
    // Create provider
    auto provider = std::make_shared<opentelemetry::sdk::trace::TracerProvider>(std::move(processor));
    
    // Set as global provider
    opentelemetry::trace::Provider::SetTracerProvider(provider);
    
    // Create tracer for this worker
    tracer_ = provider->GetTracer("beamline_worker", "1.0.0");
    */
}

void Observability::increment_task_total(const std::string& /*block_type*/, const std::string& /*status*/) {
    /*
    auto& counter = task_total_family_->Add({
        {"block_type", block_type},
        {"status", status}
    });
    counter.Increment();
    */
}

void Observability::record_task_latency(const std::string& /*block_type*/, int64_t /*latency_ms*/) {
    /*
    auto& histogram = task_latency_family_->Add({
        {"block_type", block_type}
    });
    histogram.Observe(latency_ms);
    */
}

void Observability::record_resource_usage(const std::string& /*block_type*/, int64_t /*cpu_time_ms*/, int64_t /*mem_bytes*/) {
    /*
    // Record CPU time
    auto& cpu_gauge = resource_usage_family_->Add({
        {"block_type", block_type},
        {"resource", "cpu_time_ms"}
    });
    cpu_gauge.Set(cpu_time_ms);
    
    // Record memory usage
    auto& mem_gauge = resource_usage_family_->Add({
        {"block_type", block_type},
        {"resource", "memory_bytes"}
    });
    mem_gauge.Set(mem_bytes);
    */
}

void Observability::set_pool_queue_depth(ResourceClass resource_class, int64_t /*depth*/) {
    std::string resource_name;
    switch (resource_class) {
        case ResourceClass::cpu:
            resource_name = "cpu";
            break;
        case ResourceClass::gpu:
            resource_name = "gpu";
            break;
        case ResourceClass::io:
            resource_name = "io";
            break;
    }
    
    /*
    auto& gauge = pool_queue_depth_family_->Add({
        {"resource_class", resource_name}
    });
    gauge.Set(depth);
    */
}

// CP2 Wave 1 Metrics Implementation

void Observability::record_step_execution(const std::string& step_type,
                                          const std::string& execution_status,
                                          const std::string& tenant_id,
                                          const std::string& run_id,
                                          const std::string& flow_id,
                                          const std::string& step_id) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    std::map<std::string, std::string> labels = {
        {"step_type", step_type},
        {"execution_status", execution_status}
    };
    
    // Add CP1 correlation fields only if provided (to control cardinality)
    if (!tenant_id.empty()) labels["tenant_id"] = tenant_id;
    if (!run_id.empty()) labels["run_id"] = run_id;
    if (!flow_id.empty()) labels["flow_id"] = flow_id;
    if (!step_id.empty()) labels["step_id"] = step_id;
    
    /*
    auto& counter = step_executions_total_family_->Add(labels);
    counter.Increment();
    */
}

void Observability::record_step_execution_duration(const std::string& step_type,
                                                   const std::string& execution_status,
                                                   double /*duration_seconds*/,
                                                   const std::string& tenant_id,
                                                   const std::string& run_id,
                                                   const std::string& flow_id,
                                                   const std::string& step_id) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    std::map<std::string, std::string> labels = {
        {"step_type", step_type},
        {"execution_status", execution_status}
    };
    
    // Add CP1 correlation fields only if provided
    if (!tenant_id.empty()) labels["tenant_id"] = tenant_id;
    if (!run_id.empty()) labels["run_id"] = run_id;
    if (!flow_id.empty()) labels["flow_id"] = flow_id;
    if (!step_id.empty()) labels["step_id"] = step_id;
    
    /*
    auto& histogram = step_execution_duration_seconds_family_->Add(labels);
    histogram.Observe(duration_seconds);
    */
}

void Observability::record_step_error(const std::string& step_type,
                                     const std::string& error_code,
                                     const std::string& tenant_id,
                                     const std::string& run_id,
                                     const std::string& flow_id,
                                     const std::string& step_id) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    std::map<std::string, std::string> labels = {
        {"step_type", step_type},
        {"error_code", error_code}
    };
    
    // Add CP1 correlation fields only if provided
    if (!tenant_id.empty()) labels["tenant_id"] = tenant_id;
    if (!run_id.empty()) labels["run_id"] = run_id;
    if (!flow_id.empty()) labels["flow_id"] = flow_id;
    if (!step_id.empty()) labels["step_id"] = step_id;
    
    /*
    auto& counter = step_errors_total_family_->Add(labels);
    counter.Increment();
    */
}

void Observability::record_flow_execution_duration(double /*duration_seconds*/,
                                                   const std::string& tenant_id,
                                                   const std::string& run_id,
                                                   const std::string& flow_id) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    std::map<std::string, std::string> labels;
    
    // Add CP1 correlation fields only if provided
    if (!tenant_id.empty()) labels["tenant_id"] = tenant_id;
    if (!run_id.empty()) labels["run_id"] = run_id;
    if (!flow_id.empty()) labels["flow_id"] = flow_id;
    
    /*
    auto& histogram = flow_execution_duration_seconds_family_->Add(labels);
    histogram.Observe(duration_seconds);
    */
}

void Observability::set_queue_depth(const std::string& resource_pool, int64_t /*depth*/) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    // Silence unused variable warning for resource_pool if metrics disabled
    (void)resource_pool;
    
    /*
    auto& gauge = queue_depth_family_->Add({{"resource_pool", resource_pool}});
    gauge.Set(depth);
    */
}

void Observability::set_active_tasks(const std::string& resource_pool, int64_t /*count*/) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    // Silence unused variable warning for resource_pool if metrics disabled
    (void)resource_pool;
    
    /*
    auto& gauge = active_tasks_family_->Add({{"resource_pool", resource_pool}});
    gauge.Set(count);
    */
}

void Observability::set_health_status(const std::string& check, int64_t /*status*/) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    // Silence unused variable warning for check if metrics disabled
    (void)check;
    
    /*
    auto& gauge = health_status_family_->Add({{"check", check}});
    gauge.Set(status);
    */
}

/*
std::unique_ptr<opentelemetry::trace::Span> Observability::start_step_span(
    const std::string& operation,
    const std::string& tenant_id,
    const std::string& flow_id,
    const std::string& step_id,
    const std::string& block_type,
    const std::string& trace_id) {
    
    opentelemetry::trace::StartSpanOptions options;
    
    // Set parent trace ID if provided
    if (!trace_id.empty()) {
        // CP2: Parse trace_id and set as parent span context
        // This requires OpenTelemetry trace context parsing (planned for CP2)
        // For CP1, trace_id is included in log entries but not used for span parenting
    }
    
    auto span = tracer_->StartSpan(operation, {
        {"tenant_id", tenant_id},
        {"flow_id", flow_id},
        {"step_id", step_id},
        {"block_type", block_type},
        {"worker_id", worker_id_}
    }, options);
    
    return span;
}
*/

void Observability::log_info(const std::string& message,
                             const std::string& tenant_id,
                             const std::string& run_id,
                             const std::string& flow_id,
                             const std::string& step_id,
                             const std::string& trace_id,
                             const std::unordered_map<std::string, std::string>& context) {
    std::cout << format_json_log("INFO", message, tenant_id, run_id, flow_id, step_id, trace_id, context) << std::endl;
}

void Observability::log_warn(const std::string& message,
                              const std::string& tenant_id,
                              const std::string& run_id,
                              const std::string& flow_id,
                              const std::string& step_id,
                              const std::string& trace_id,
                              const std::unordered_map<std::string, std::string>& context) {
    std::cout << format_json_log("WARN", message, tenant_id, run_id, flow_id, step_id, trace_id, context) << std::endl;
}

void Observability::log_error(const std::string& message,
                               const std::string& tenant_id,
                               const std::string& run_id,
                               const std::string& flow_id,
                               const std::string& step_id,
                               const std::string& trace_id,
                               const std::unordered_map<std::string, std::string>& context) {
    std::cerr << format_json_log("ERROR", message, tenant_id, run_id, flow_id, step_id, trace_id, context) << std::endl;
}

void Observability::log_debug(const std::string& message,
                               const std::string& tenant_id,
                               const std::string& run_id,
                               const std::string& flow_id,
                               const std::string& step_id,
                               const std::string& trace_id,
                               const std::unordered_map<std::string, std::string>& context) {
    std::cout << format_json_log("DEBUG", message, tenant_id, run_id, flow_id, step_id, trace_id, context) << std::endl;
}

void Observability::log_info_with_context(const std::string& message,
                                          const BlockContext& ctx,
                                          const std::unordered_map<std::string, std::string>& context) {
    log_info(message, ctx.tenant_id, ctx.run_id, ctx.flow_id, ctx.step_id, ctx.trace_id, context);
}

void Observability::log_warn_with_context(const std::string& message,
                                          const BlockContext& ctx,
                                          const std::unordered_map<std::string, std::string>& context) {
    log_warn(message, ctx.tenant_id, ctx.run_id, ctx.flow_id, ctx.step_id, ctx.trace_id, context);
}

void Observability::log_error_with_context(const std::string& message,
                                           const BlockContext& ctx,
                                           const std::unordered_map<std::string, std::string>& context) {
    log_error(message, ctx.tenant_id, ctx.run_id, ctx.flow_id, ctx.step_id, ctx.trace_id, context);
}

void Observability::log_debug_with_context(const std::string& message,
                                           const BlockContext& ctx,
                                           const std::unordered_map<std::string, std::string>& context) {
    log_debug(message, ctx.tenant_id, ctx.run_id, ctx.flow_id, ctx.step_id, ctx.trace_id, context);
}

std::string Observability::format_json_log(const std::string& level,
                                           const std::string& message,
                                           const std::string& tenant_id,
                                           const std::string& run_id,
                                           const std::string& flow_id,
                                           const std::string& step_id,
                                           const std::string& trace_id,
                                           const std::unordered_map<std::string, std::string>& context) {
    json log_entry;
    
    // Required fields (always present)
    log_entry["timestamp"] = get_iso8601_timestamp();
    log_entry["level"] = level;
    log_entry["component"] = "worker";
    log_entry["message"] = message;
    
    // CP1 fields (at top level, when provided)
    if (!tenant_id.empty()) {
        log_entry["tenant_id"] = tenant_id;
    }
    if (!run_id.empty()) {
        log_entry["run_id"] = run_id;
    }
    if (!flow_id.empty()) {
        log_entry["flow_id"] = flow_id;
    }
    if (!step_id.empty()) {
        log_entry["step_id"] = step_id;
    }
    if (!trace_id.empty()) {
        log_entry["trace_id"] = trace_id;
    }
    
    // Context object (technical details)
    json context_obj;
    context_obj["worker_id"] = worker_id_;
    
    // Add context fields
    for (const auto& [key, value] : context) {
        context_obj[key] = value;
    }
    
    // Filter PII from context
    filter_pii_recursive(context_obj);
    
    // Add context if not empty
    if (!context_obj.empty()) {
        log_entry["context"] = context_obj;
    }
    
    return log_entry.dump();
}

std::string Observability::get_health_response() {
    json health_response;
    health_response["status"] = "healthy";
    health_response["timestamp"] = get_iso8601_timestamp();
    return health_response.dump();
}

void Observability::health_server_loop(int socket_fd) {
    char buffer[4096];
    
    while (health_server_running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (health_server_running_) {
                // Error accepting connection, but server still running
                continue;
            }
            break;
        }
        
        // Read request
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            // Simple HTTP request parsing - check if it's GET /_health
            std::string request(buffer);
            if (request.find("GET /_health") != std::string::npos || 
                request.find("GET /_health HTTP") != std::string::npos) {
                
                // Generate health response
                std::string response_body = get_health_response();
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
                    "\r\n" + response_body;
                
                send(client_fd, response.c_str(), response.length(), 0);
            } else {
                // 404 for other paths
                std::string response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 13\r\n"
                    "\r\n"
                    "404 Not Found";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        }
        
        close(client_fd);
    }
}

void Observability::start_health_endpoint(const std::string& address, uint16_t port) {
    if (health_server_running_) {
        return; // Already running
    }
    
    // Parse address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (address == "0.0.0.0" || address.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);
    }
    
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_error("Failed to create health endpoint socket", "", "", "", "", "", {
            {"error", "socket creation failed"}
        });
        return;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind health endpoint socket", "", "", "", "", "", {
            {"error", "bind failed"},
            {"address", address},
            {"port", std::to_string(port)}
        });
        close(socket_fd);
        return;
    }
    
    // Listen
    if (listen(socket_fd, 5) < 0) {
        log_error("Failed to listen on health endpoint socket", "", "", "", "", "", {
            {"error", "listen failed"}
        });
        close(socket_fd);
        return;
    }
    
    health_server_socket_ = socket_fd;
    health_server_running_ = true;
    
    // Start server thread
    health_server_thread_ = std::thread(&Observability::health_server_loop, this, socket_fd);
    
    log_info("Health endpoint started", "", "", "", "", "", {
        {"address", address},
        {"port", std::to_string(port)}
    });
}

void Observability::stop_health_endpoint() {
    if (!health_server_running_) {
        return;
    }
    
    health_server_running_ = false;
    
    // Close socket to wake up accept()
    if (health_server_socket_ >= 0) {
        close(health_server_socket_);
        health_server_socket_ = -1;
    }
    
    // Wait for thread to finish
    if (health_server_thread_.joinable()) {
        health_server_thread_.join();
    }
    
    log_info("Health endpoint stopped");
}

// CP2 Wave 1 Metrics Endpoint Implementation

std::string Observability::get_metrics_response() {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return ""; // Return empty if feature flag disabled
    }
    return "# Worker Metrics (CP2 Wave 1)\n# Registry not initialized\n";
    /*
    if (!registry_) {
        return "# Worker Metrics (CP2 Wave 1)\n# Registry not initialized\n";
    }
    
    std::ostringstream oss;
    
    // Collect metrics from registry and format as Prometheus text format
    // prometheus-cpp uses Collect() method to get metrics
    auto collected = registry_->Collect();
    
    // Group metrics by name for HELP/TYPE headers
    std::map<std::string, std::vector<prometheus::MetricFamily>> grouped;
    for (const auto& family : collected) {
        grouped[family.name].push_back(family);
    }
    
    // Format each metric family
    for (const auto& [name, families] : grouped) {
        if (families.empty()) continue;
        
        const auto& family = families[0];
        
        // HELP line
        if (!family.help.empty()) {
            oss << "# HELP " << name << " " << family.help << "\n";
        }
        
        // TYPE line
        std::string type_str;
        switch (family.type) {
            case prometheus::MetricType::Counter:
                type_str = "counter";
                break;
            case prometheus::MetricType::Gauge:
                type_str = "gauge";
                break;
            case prometheus::MetricType::Histogram:
                type_str = "histogram";
                break;
            case prometheus::MetricType::Summary:
                type_str = "summary";
                break;
            default:
                type_str = "untyped";
                break;
        }
        oss << "# TYPE " << name << " " << type_str << "\n";
        
        // Metric lines
        for (const auto& metric : family.metric) {
            oss << name;
            
            // Labels
            if (!metric.label.empty()) {
                oss << "{";
                bool first = true;
                for (const auto& label : metric.label) {
                    if (!first) oss << ",";
                    first = false;
                    oss << label.name << "=\"" << label.value << "\"";
                }
                oss << "}";
            }
            
            oss << " ";
            
            // Value based on type
            switch (family.type) {
                case prometheus::MetricType::Counter:
                    if (metric.counter.has_value()) {
                        oss << metric.counter->value;
                    }
                    break;
                case prometheus::MetricType::Gauge:
                    if (metric.gauge.has_value()) {
                        oss << metric.gauge->value;
                    }
                    break;
                case prometheus::MetricType::Histogram:
                    if (metric.histogram.has_value()) {
                        // Histogram: export buckets, sum, count
                        const auto& hist = metric.histogram.value();
                        
                        // Export buckets
                        for (const auto& bucket : hist.bucket) {
                            oss << name << "_bucket{";
                            if (!metric.label.empty()) {
                                bool first = true;
                                for (const auto& label : metric.label) {
                                    if (!first) oss << ",";
                                    first = false;
                                    oss << label.name << "=\"" << label.value << "\"";
                                }
                            }
                            oss << ",le=\"" << bucket.upper_bound << "\"} " << bucket.cumulative_count << "\n";
                        }
                        
                        // Export sum
                        oss << name << "_sum{";
                        if (!metric.label.empty()) {
                            bool first = true;
                            for (const auto& label : metric.label) {
                                if (!first) oss << ",";
                                first = false;
                                oss << label.name << "=\"" << label.value << "\"";
                            }
                        }
                        oss << "} " << hist.sample_sum << "\n";
                        
                        // Export count
                        oss << name << "_count{";
                        if (!metric.label.empty()) {
                            bool first = true;
                            for (const auto& label : metric.label) {
                                if (!first) oss << ",";
                                first = false;
                                oss << label.name << "=\"" << label.value << "\"";
                            }
                        }
                        oss << "} " << hist.sample_count;
                    }
                    break;
                default:
                    oss << "0";
                    break;
            }
            
            oss << "\n";
        }
    }
    
    return oss.str();
    */
}

void Observability::metrics_server_loop(int socket_fd) {
    char buffer[4096];
    
    while (metrics_server_running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (metrics_server_running_) {
                continue;
            }
            break;
        }
        
        // Read request
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            // Simple HTTP request parsing - check if it's GET /metrics
            std::string request(buffer);
            if (request.find("GET /metrics") != std::string::npos || 
                request.find("GET /metrics HTTP") != std::string::npos) {
                
                // Generate metrics response
                std::string response_body = get_metrics_response();
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain; version=0.0.4\r\n"
                    "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
                    "\r\n" + response_body;
                
                send(client_fd, response.c_str(), response.length(), 0);
            } else {
                // 404 for other paths
                std::string response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 13\r\n"
                    "\r\n"
                    "404 Not Found";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        }
        
        close(client_fd);
    }
}

void Observability::start_metrics_endpoint(const std::string& address, uint16_t port) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return; // Don't start if feature flag disabled
    }
    
    if (metrics_server_running_) {
        return; // Already running
    }
    
    // Parse address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (address == "0.0.0.0" || address.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);
    }
    
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_error("Failed to create metrics endpoint socket", "", "", "", "", "", {
            {"error", "socket creation failed"}
        });
        return;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind metrics endpoint socket", "", "", "", "", "", {
            {"error", "bind failed"},
            {"address", address},
            {"port", std::to_string(port)}
        });
        close(socket_fd);
        return;
    }
    
    // Listen
    if (listen(socket_fd, 5) < 0) {
        log_error("Failed to listen on metrics endpoint socket", "", "", "", "", "", {
            {"error", "listen failed"}
        });
        close(socket_fd);
        return;
    }
    
    metrics_server_socket_ = socket_fd;
    metrics_server_running_ = true;
    
    // Start server thread
    metrics_server_thread_ = std::thread(&Observability::metrics_server_loop, this, socket_fd);
    
    log_info("Metrics endpoint started", "", "", "", "", "", {
        {"address", address},
        {"port", std::to_string(port)}
    });
}

void Observability::stop_metrics_endpoint() {
    if (!metrics_server_running_) {
        return;
    }
    
    metrics_server_running_ = false;
    
    // Close socket to wake up accept()
    if (metrics_server_socket_ >= 0) {
        close(metrics_server_socket_);
        metrics_server_socket_ = -1;
    }
    
    // Wait for thread to finish
    if (metrics_server_thread_.joinable()) {
        metrics_server_thread_.join();
    }
    
    log_info("Metrics endpoint stopped");
}

} // namespace worker
} // namespace beamline