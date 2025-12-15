#include "beamline/worker/core.hpp"
#include "beamline/worker/actors.hpp"
#include <unordered_map>
#include <memory>

namespace beamline {
namespace worker {

class Scheduler {
public:
    Scheduler(caf::actor_system& system, const WorkerConfig& config)
        : system_(system), config_(config) {
        initialize_resource_pools();
    }
    
    caf::expected<caf::actor> schedule_step(const StepRequest& request, const BlockContext& context) {
        // Determine resource class based on block type and requirements
        ResourceClass resource_class = determine_resource_class(request);
        
        // Check tenant quotas and limits
        auto quota_check = check_tenant_quotas(context.tenant_id, resource_class);
        if (!quota_check) {
            return quota_check.error();
        }
        
        // Get appropriate pool actor
        auto pool_actor = get_pool_for_resource(resource_class);
        if (!pool_actor) {
            return caf::make_error(caf::sec::runtime_error, "No available pool for resource class");
        }
        
        return *pool_actor;
    }
    
    caf::expected<void> cancel_step(const std::string& step_id, [[maybe_unused]] const BlockContext& context) {
        // Broadcast cancel to all pools - they'll handle the specific step cancellation
        // Context parameter is part of the interface but not used in this implementation
        for (auto& pool_pair : resource_pools_) {
            system_.send(pool_pair.second, caf::atom("cancel"), step_id);
        }
        return caf::unit;
    }
    
    std::unordered_map<std::string, int64_t> get_pool_metrics() {
        std::unordered_map<std::string, int64_t> metrics;
        // Collect metrics from pool actors by sending metrics request
        // For CP1, return default values; full implementation would query pool actors
        metrics["cpu_pool_load"] = 0;
        metrics["gpu_pool_load"] = 0;
        metrics["io_pool_load"] = 0;
        return metrics;
    }
    
private:
    caf::actor_system& system_;
    WorkerConfig config_;
    std::unordered_map<std::string, caf::actor> resource_pools_;
    std::unordered_map<std::string, std::unordered_map<ResourceClass, int64_t>> tenant_usage_;
    
    void initialize_resource_pools() {
        // Create CPU pool
        resource_pools_["cpu"] = system_.spawn<PoolActorImpl>(ResourceClass::cpu, config_.cpu_pool_size);
        
        // Create GPU pool
        resource_pools_["gpu"] = system_.spawn<PoolActorImpl>(ResourceClass::gpu, config_.gpu_pool_size);
        
        // Create I/O pool
        resource_pools_["io"] = system_.spawn<PoolActorImpl>(ResourceClass::io, config_.io_pool_size);
    }
    
    ResourceClass determine_resource_class(const StepRequest& request) {
        // Check explicit resource class specification
        if (request.resources.count("class")) {
            const std::string& resource_class = request.resources.at("class");
            if (resource_class == "gpu") return ResourceClass::gpu;
            if (resource_class == "io") return ResourceClass::io;
        }
        
        // Determine based on block type
        if (request.type.find("http.") == 0 || request.type.find("fs.") == 0) {
            return ResourceClass::io;
        }
        
        if (request.type.find("ai.") == 0 || request.type.find("media.") == 0) {
            return ResourceClass::gpu;
        }
        
        // Default to CPU
        return ResourceClass::cpu;
    }
    
    caf::expected<void> check_tenant_quotas(const std::string& tenant_id, ResourceClass resource_class) {
        auto& usage = tenant_usage_[tenant_id];
        
        // Check memory quota
        if (usage[ResourceClass::cpu] > config_.max_memory_per_tenant_mb * 1024 * 1024) {
            return caf::make_error(caf::sec::runtime_error, "Tenant memory quota exceeded");
        }
        
        // Check CPU time quota
        if (usage[ResourceClass::cpu] > config_.max_cpu_time_per_tenant_ms) {
            return caf::make_error(caf::sec::runtime_error, "Tenant CPU time quota exceeded");
        }
        
        return caf::unit;
    }
    
    caf::optional<caf::actor> get_pool_for_resource(ResourceClass resource_class) {
        switch (resource_class) {
            case ResourceClass::cpu:
                return resource_pools_["cpu"];
            case ResourceClass::gpu:
                return resource_pools_["gpu"];
            case ResourceClass::io:
                return resource_pools_["io"];
            default:
                return caf::none;
        }
    }
};

} // namespace worker
} // namespace beamline