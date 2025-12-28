#include <iostream>
#include <caf/actor_system.hpp>
#include <caf/actor_system_config.hpp>
#include <caf/io/middleman.hpp>
#include <caf/openssl/manager.hpp>
#include "beamline/worker/core.hpp"
#include "beamline/worker/actors.hpp"
#include "beamline/worker/ingress_actor.hpp"
#include "beamline/worker/observability.hpp"
#include <unistd.h>

class WorkerConfig : public caf::actor_system_config {
public:
    WorkerConfig() {
        opt_group{custom_options_, "global"}
            .add(worker_config.cpu_pool_size, "cpu-pool-size", "CPU pool size")
            .add(worker_config.gpu_pool_size, "gpu-pool-size", "GPU pool size")
            .add(worker_config.io_pool_size, "io-pool-size", "I/O pool size")
            .add(worker_config.max_memory_per_tenant_mb, "max-memory-mb", "Max memory per tenant (MB)")
            .add(worker_config.max_cpu_time_per_tenant_ms, "max-cpu-time-ms", "Max CPU time per tenant (ms)")
            .add(worker_config.sandbox_mode, "sandbox", "Enable sandbox mode")
            .add(worker_config.nats_url, "nats-url", "NATS server URL")
            .add(worker_config.prometheus_endpoint, "prometheus-endpoint", "Prometheus metrics endpoint");
        
        // Load CAF modules
        load<caf::io::middleman>();
        load<caf::openssl::manager>();
    }
    
    beamline::worker::WorkerConfig worker_config;
};

void caf_main(caf::actor_system& system, const WorkerConfig& config) {
    std::unique_ptr<beamline::worker::Observability> observability;
    
    try {
        // Initialize observability first
        observability = std::make_unique<beamline::worker::Observability>("worker_" + std::to_string(getpid()));
        observability->log_info("Worker starting", "", "", "", "", "", {
            {"cpu_pool_size", std::to_string(config.worker_config.cpu_pool_size)},
            {"gpu_pool_size", std::to_string(config.worker_config.gpu_pool_size)},
            {"io_pool_size", std::to_string(config.worker_config.io_pool_size)},
            {"sandbox_mode", config.worker_config.sandbox_mode ? "true" : "false"}
        });
        
        // Start health endpoint
        // Parse prometheus_endpoint (format: "address:port")
        std::string health_address = "0.0.0.0";
        uint16_t health_port = 9091; // Default health port (9090 is for Prometheus metrics)
        
        size_t colon_pos = config.worker_config.prometheus_endpoint.find(':');
        if (colon_pos != std::string::npos) {
            health_address = config.worker_config.prometheus_endpoint.substr(0, colon_pos);
            std::string port_str = config.worker_config.prometheus_endpoint.substr(colon_pos + 1);
            health_port = static_cast<uint16_t>(std::stoi(port_str) + 1); // Health on next port
        }
        
        observability->start_health_endpoint(health_address, health_port);
        
        // CP2: Start metrics endpoint on port 9092 (if feature flag enabled)
        // Parse prometheus_endpoint to get base port
        uint16_t metrics_port = 9092; // Default CP2 metrics port
        if (colon_pos != std::string::npos) {
            std::string port_str = config.worker_config.prometheus_endpoint.substr(colon_pos + 1);
            int base_port = std::stoi(port_str);
            metrics_port = static_cast<uint16_t>(base_port + 2); // Metrics on base_port + 2
        }
        observability->start_metrics_endpoint(health_address, metrics_port);
        
        // Set initial health status metric
        observability->set_health_status("worker", 1); // 1 = healthy
        
        // Create worker actor
        // Actor is kept alive by the actor system, no need to store reference
        auto worker_actor = system.spawn<beamline::worker::WorkerActor>(config.worker_config);
        
        // Create ingress actor
        system.spawn<beamline::worker::ingress_actor>(config.worker_config.nats_url, caf::actor_cast<caf::actor>(worker_actor));
        
        // Keep the system running
        observability->log_info("Worker CAF runtime is running. Press Enter to exit...", "", "", "", "", "", {});
        std::cin.get();
        
        observability->log_info("Worker shutting down", "", "", "", "", "", {});
        
    } catch (const std::exception& e) {
        if (observability) {
            observability->log_error("Worker fatal error", "", "", "", "", "", {{"error", e.what()}});
        } else {
            // Fallback to stderr if observability is not initialized
            std::cerr << "Worker fatal error (observability not initialized): " << e.what() << std::endl;
        }
        return;
    }
}

int main(int argc, char** argv) {
    WorkerConfig config;
    
    // Parse command line arguments
    if (auto err = config.parse(argc, argv)) {
        // Use stderr for argument parsing errors (before observability is initialized)
        std::cerr << "Failed to parse arguments: " << caf::to_string(err) << std::endl;
        return 1;
    }
    
    // Run the actor system
    caf::actor_system system(config);
    caf_main(system, config);
    
    return 0;
}
