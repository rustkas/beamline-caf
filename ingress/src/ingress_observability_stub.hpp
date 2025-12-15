// Observability Adapter Stub for C++ CAF Ingress
// Provides integration points for Prometheus and OpenTelemetry
// This is a stub implementation - replace with actual libraries in production

#ifndef INGRESS_OBSERVABILITY_STUB_HPP
#define INGRESS_OBSERVABILITY_STUB_HPP

#include <string>
#include <map>

namespace beamline {
namespace ingress {
namespace observability {

// Prometheus metrics stub
class PrometheusStub {
public:
    static void init();
    static void increment_counter(const std::string& name, 
                                   const std::map<std::string, std::string>& labels = {});
    static void record_histogram(const std::string& name, double value,
                                 const std::map<std::string, std::string>& labels = {});
    static void set_gauge(const std::string& name, double value,
                          const std::map<std::string, std::string>& labels = {});
    static std::string export_metrics();
};

// OpenTelemetry tracing stub
class OpenTelemetryStub {
public:
    struct Span {
        std::string trace_id;
        std::string span_id;
        std::string name;
        std::map<std::string, std::string> attributes;
    };
    
    static void init();
    static Span start_span(const std::string& name,
                           const std::map<std::string, std::string>& attributes = {});
    static void end_span(const Span& span, bool success = true);
    static void set_attribute(const Span& span, const std::string& key, const std::string& value);
    static std::string extract_trace_context(const std::map<std::string, std::string>& headers);
    static std::map<std::string, std::string> inject_trace_context(const Span& span);
};

// Structured logging stub
class LoggerStub {
public:
    enum class Level {
        ERROR,
        WARN,
        INFO,
        DEBUG
    };
    
    static void log(Level level, const std::string& message,
                    const std::map<std::string, std::string>& context = {});
    static void error(const std::string& message,
                      const std::map<std::string, std::string>& context = {});
    static void warn(const std::string& message,
                     const std::map<std::string, std::string>& context = {});
    static void info(const std::string& message,
                     const std::map<std::string, std::string>& context = {});
    static void debug(const std::string& message,
                      const std::map<std::string, std::string>& context = {});
    
private:
    static std::string sanitize_context(const std::map<std::string, std::string>& context);
    static bool is_pii_field(const std::string& field);
};

} // namespace observability
} // namespace ingress
} // namespace beamline

#endif // INGRESS_OBSERVABILITY_STUB_HPP

