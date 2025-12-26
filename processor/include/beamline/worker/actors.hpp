#pragma once

#include "beamline/worker/core.hpp"
#include "beamline/worker/observability.hpp"
#include <caf/actor.hpp>
#include <caf/actor_system.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/typed_actor.hpp>
#include <caf/typed_event_based_actor.hpp>
#include <queue>

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
    WorkerActorState(caf::scheduled_actor* self, WorkerConfig config);
    
    worker_actor::behavior_type make_behavior();
    
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

class WorkerActorImpl : public caf::typed_event_based_actor<
    caf::reacts_to<caf::atom_value, StepRequest>,
    caf::reacts_to<caf::atom_value, std::string>,
    caf::reacts_to<caf::atom_value>,
    caf::reacts_to<caf::atom_value, BlockContext>
> {
public:
    WorkerActorImpl(caf::actor_config& cfg, WorkerConfig config)
        : caf::typed_event_based_actor<
            caf::reacts_to<caf::atom_value, StepRequest>,
            caf::reacts_to<caf::atom_value, std::string>,
            caf::reacts_to<caf::atom_value>,
            caf::reacts_to<caf::atom_value, BlockContext>
          >(cfg),
          state_(this, std::move(config)) {}

    behavior_type make_behavior() override {
        return state_.make_behavior();
    }
private:
    WorkerActorState state_;
};

using WorkerActor = WorkerActorImpl;

// Pool actor for resource management
using pool_actor = caf::typed_actor<
    caf::reacts_to<caf::atom_value, StepRequest>, // execute step
    caf::reacts_to<caf::atom_value, std::string>, // cancel step
    caf::reacts_to<caf::atom_value> // get pool metrics
>;

struct PoolConfig {
    ResourceClass resource_class;
    int max_concurrency;
    
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, PoolConfig& config) {
        return f(config.resource_class, config.max_concurrency);
    }
};

class PoolActorState {
public:
    PoolActorState(caf::scheduled_actor* self, PoolConfig config);
    
    pool_actor::behavior_type make_behavior();
    
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

class PoolActorImpl : public caf::typed_event_based_actor<
    caf::reacts_to<caf::atom_value, StepRequest>,
    caf::reacts_to<caf::atom_value, std::string>,
    caf::reacts_to<caf::atom_value>
> {
public:
    PoolActorImpl(caf::actor_config& cfg, PoolConfig config)
        : caf::typed_event_based_actor<
            caf::reacts_to<caf::atom_value, StepRequest>,
            caf::reacts_to<caf::atom_value, std::string>,
            caf::reacts_to<caf::atom_value>
          >(cfg),
          state_(this, std::move(config)) {}

    behavior_type make_behavior() override {
        return state_.make_behavior();
    }
private:
    PoolActorState state_;
};

// Block executor actor
using executor_actor = caf::typed_actor<
    caf::reacts_to<caf::atom_value, StepRequest>, // execute step
    caf::reacts_to<caf::atom_value, std::string>, // cancel step
    caf::reacts_to<caf::atom_value> // get executor metrics
>;

class ExecutorActorState {
public:
    ExecutorActorState(caf::scheduled_actor* self, std::shared_ptr<BlockExecutor> executor);
    
    executor_actor::behavior_type make_behavior();
    
private:
    caf::actor_system& system_;
    std::shared_ptr<BlockExecutor> executor_;
    std::unordered_map<std::string, caf::actor_addr> running_steps_;
    std::shared_ptr<Observability> observability_; // CP2: For metrics collection
    
    caf::expected<StepResult> execute_with_retry(const StepRequest& req);
    caf::expected<StepResult> execute_single_attempt(const StepRequest& req);
    void record_step_metrics(const StepRequest& req, const StepResult& result, double duration_seconds); // CP2: Record metrics
};

class ExecutorActorImpl : public caf::typed_event_based_actor<
    caf::reacts_to<caf::atom_value, StepRequest>,
    caf::reacts_to<caf::atom_value, std::string>,
    caf::reacts_to<caf::atom_value>
> {
public:
    ExecutorActorImpl(caf::actor_config& cfg, std::shared_ptr<BlockExecutor> executor)
        : caf::typed_event_based_actor<
            caf::reacts_to<caf::atom_value, StepRequest>,
            caf::reacts_to<caf::atom_value, std::string>,
            caf::reacts_to<caf::atom_value>
          >(cfg),
          state_(this, std::move(executor)) {}
          
    behavior_type make_behavior() override {
        return state_.make_behavior();
    }
private:
    ExecutorActorState state_;
};

} // namespace worker
} // namespace beamline