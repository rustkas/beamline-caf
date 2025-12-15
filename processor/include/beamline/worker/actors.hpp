#pragma once

#include "beamline/worker/core.hpp"
#include <caf/actor.hpp>
#include <caf/actor_system.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/typed_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

namespace beamline {
namespace worker {

// Worker actor interface
using worker_actor = caf::typed_actor<
    caf::reacts_to<caf::atom_value, StepRequest>, // execute step
    caf::reacts_to<caf::atom_value, std::string>, // cancel step
    caf::reacts_to<caf::atom_value>, // get metrics
    caf::reacts_to<caf::atom_value, BlockContext> // update context
>;

// Worker actor state
class WorkerActorState {
public:
    WorkerActorState(caf::actor_system& system, const WorkerConfig& config);
    
    caf::behavior make_behavior();
    
private:
    caf::actor_system& system_;
    WorkerConfig config_;
    std::unordered_map<std::string, caf::actor> pools_;
    std::unordered_map<std::string, std::shared_ptr<BlockExecutor>> executors_;
    std::unique_ptr<Observability> observability_;
    
    void initialize_pools();
    void register_executors();
    caf::actor get_pool_for_resource(ResourceClass resource_class);
};

using WorkerActor = worker_actor::stateful_impl<WorkerActorState>;

// Pool actor for resource management
using pool_actor = caf::typed_actor<
    caf::reacts_to<caf::atom_value, StepRequest>, // execute step
    caf::reacts_to<caf::atom_value, std::string>, // cancel step
    caf::reacts_to<caf::atom_value> // get pool metrics
>;

class PoolActorState {
public:
    PoolActorState(caf::actor_system& system, ResourceClass resource_class, int max_concurrency);
    
    caf::behavior make_behavior();
    
private:
    caf::actor_system& system_;
    ResourceClass resource_class_;
    int max_concurrency_;
    int current_load_ = 0;
    std::queue<std::pair<caf::actor_addr, StepRequest>> pending_requests_;
    int max_queue_size_ = 1000; // CP2: Bounded queue size (configurable)
    std::shared_ptr<Observability> observability_; // CP2: For metrics collection
    
    void process_pending();
    size_t get_queue_depth() const;
    void update_queue_metrics(); // CP2: Update queue depth and active tasks metrics
};

using PoolActorImpl = pool_actor::stateful_impl<PoolActorState>;

// Block executor actor
using executor_actor = caf::typed_actor<
    caf::reacts_to<caf::atom_value, StepRequest>, // execute step
    caf::reacts_to<caf::atom_value, std::string>, // cancel step
    caf::reacts_to<caf::atom_value> // get executor metrics
>;

class ExecutorActorState {
public:
    ExecutorActorState(caf::actor_system& system, std::shared_ptr<BlockExecutor> executor);
    
    caf::behavior make_behavior();
    
private:
    caf::actor_system& system_;
    std::shared_ptr<BlockExecutor> executor_;
    std::unordered_map<std::string, caf::actor_addr> running_steps_;
    std::shared_ptr<Observability> observability_; // CP2: For metrics collection
    
    caf::expected<StepResult> execute_with_retry(const StepRequest& req);
    caf::expected<StepResult> execute_single_attempt(const StepRequest& req);
    void record_step_metrics(const StepRequest& req, const StepResult& result, double duration_seconds); // CP2: Record metrics
};

using ExecutorActorImpl = executor_actor::stateful_impl<ExecutorActorState>;

} // namespace worker
} // namespace beamline