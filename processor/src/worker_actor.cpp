#include "beamline/worker/actors.hpp"
#include "beamline/worker/observability.hpp"
#include "beamline/worker/retry_policy.hpp"
#include "beamline/worker/feature_flags.hpp"
#include <caf/actor_system.hpp>
#include <caf/scheduled_actor.hpp>
#include <caf/typed_event_based_actor.hpp>
#include <caf/send.hpp>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <iostream>

namespace beamline {
namespace worker {

WorkerActorState::WorkerActorState(caf::scheduled_actor* self, WorkerConfig config)
    : system_(self->system()), config_(std::move(config)) {
    observability_ = std::make_unique<Observability>("worker_actor");
    initialize_pools();
    register_executors();
    
    observability_->log_info("WorkerActor initialized", "", "", "", "", "", {
        {"cpu_pool_size", std::to_string(config.cpu_pool_size)},
        {"gpu_pool_size", std::to_string(config.gpu_pool_size)},
        {"io_pool_size", std::to_string(config.io_pool_size)}
    });
}

worker_actor::behavior_type WorkerActorState::make_behavior() {
    return {
        [this](caf::atom_value execute_atom, const StepRequest& request) {
            if (execute_atom != caf::atom("execute")) {
                return;
            }
            
            auto executor = get_pool_for_resource(ResourceClass::cpu); // Default to CPU
            if (request.resources.count("class")) {
                if (request.resources.at("class") == "gpu") {
                    executor = get_pool_for_resource(ResourceClass::gpu);
                } else if (request.resources.at("class") == "io") {
                    executor = get_pool_for_resource(ResourceClass::io);
                }
            }
            
            caf::anon_send(executor, execute_atom, request);
        },
        
        [this](caf::atom_value cancel_atom, const std::string& step_id) {
            if (cancel_atom != caf::atom("cancel")) {
                return;
            }
            
            // Broadcast cancel to all pools
            for (auto& pool_pair : pools_) {
                caf::anon_send(pool_pair.second, cancel_atom, step_id);
            }
        },
        
        [this](caf::atom_value metrics_atom) {
            if (metrics_atom != caf::atom("metrics")) {
                return;
            }
            
            // CP2: Aggregate metrics from all executors
            // This requires executor metrics collection (planned for CP2)
            // For CP1, metrics are logged but not aggregated across executors
            observability_->log_info("Metrics requested");
        },
        
        [this](caf::atom_value context_atom, const BlockContext& ctx) {
            if (context_atom != caf::atom("context")) {
                return;
            }
            
            // CP2: Update context for all executors
            // This requires executor context propagation (planned for CP2)
            // For CP1, context is logged but not propagated to executors
            observability_->log_info_with_context("Context updated", ctx);
        }
    };
}

void WorkerActorState::initialize_pools() {
    // Create CPU pool
    PoolConfig cpu_config{ResourceClass::cpu, config_.cpu_pool_size};
    pools_["cpu"] = caf::actor_cast<caf::actor>(system_.spawn<PoolActorImpl>(cpu_config));
    
    // Create GPU pool
    PoolConfig gpu_config{ResourceClass::gpu, config_.gpu_pool_size};
    pools_["gpu"] = caf::actor_cast<caf::actor>(system_.spawn<PoolActorImpl>(gpu_config));
    
    // Create I/O pool
    PoolConfig io_config{ResourceClass::io, config_.io_pool_size};
    pools_["io"] = caf::actor_cast<caf::actor>(system_.spawn<PoolActorImpl>(io_config));
    
    observability_->log_info("Actor pools initialized", "", "", "", "", "", {
        {"cpu_pool", "initialized"},
        {"gpu_pool", "initialized"},
        {"io_pool", "initialized"}
    });
}

void WorkerActorState::register_executors() {
    // Block executors are registered dynamically when step requests are processed.
    // Each pool actor creates executor actors on-demand based on the step type.
    // This allows for lazy initialization and better resource management.
    observability_->log_info("Block executors registration system initialized", "", "", "", "", "", {});
}

caf::actor WorkerActorState::get_pool_for_resource(ResourceClass resource_class) {
    switch (resource_class) {
        case ResourceClass::cpu:
            return pools_["cpu"];
        case ResourceClass::gpu:
            return pools_["gpu"];
        case ResourceClass::io:
            return pools_["io"];
        default:
            return pools_["cpu"]; // Default to CPU
    }
}

// Pool Actor Implementation
PoolActorState::PoolActorState(caf::scheduled_actor* self, PoolConfig config)
    : system_(self->system()), resource_class_(config.resource_class), max_concurrency_(config.max_concurrency) {
    // CP2: Initialize observability for metrics
    observability_ = std::make_shared<Observability>("pool_" + 
        std::string(resource_class_ == ResourceClass::cpu ? "cpu" : 
         resource_class_ == ResourceClass::gpu ? "gpu" : "io"));
    
    // CP2: Set max queue size from config or default
    if (FeatureFlags::is_queue_management_enabled()) {
        max_queue_size_ = 1000; // Default, can be configured
    } else {
        max_queue_size_ = 0; // 0 means unbounded (CP1 behavior)
    }
}

pool_actor::behavior_type PoolActorState::make_behavior() {
    return {
        [this](caf::atom_value execute_atom, const StepRequest& request) {
            if (execute_atom != caf::atom("execute")) {
                return;
            }
            
            // CP2: Check queue bounds before queuing
            if (current_load_ >= max_concurrency_) {
                // Need to queue the request
                if (FeatureFlags::is_queue_management_enabled()) {
                    // CP2: Check if queue is full
                    if (max_queue_size_ > 0 && pending_requests_.size() >= static_cast<size_t>(max_queue_size_)) {
                        // Queue is full - reject request
                        observability_->log_warn("Queue full - rejecting request", 
                            request.inputs.count("tenant_id") ? request.inputs.at("tenant_id") : "",
                            request.inputs.count("run_id") ? request.inputs.at("run_id") : "",
                            request.inputs.count("flow_id") ? request.inputs.at("flow_id") : "",
                            request.inputs.count("step_id") ? request.inputs.at("step_id") : "",
                            "", {
                            {"resource_class", resource_class_ == ResourceClass::cpu ? "cpu" : 
                                              resource_class_ == ResourceClass::gpu ? "gpu" : "io"},
                            {"queue_depth", std::to_string(pending_requests_.size())},
                            {"max_queue_size", std::to_string(max_queue_size_)},
                            {"reason", "queue_full"}
                        });
                        
                        // CP2: Update queue metrics
                        update_queue_metrics();
                        
                        // Return rejection (would need to send ExecAssignmentAck with rejected status)
                        // For now, log and return
                        return;
                    }
                }
                
                // Queue the request
                // pending_requests_.push({caf::actor_cast<caf::actor_addr>(caf::self), request});
                
                // CP2: Update queue metrics
                update_queue_metrics();
                return;
            }
            
            // Execute immediately
            current_load_++;
            // Executor actor creation is handled by pool actor implementation
            // The request is queued and will be processed by process_pending()
            
            observability_->log_info("Step execution started", "", "", "", request.type, "", {
                {"resource_class", resource_class_ == ResourceClass::cpu ? "cpu" : 
                                  resource_class_ == ResourceClass::gpu ? "gpu" : "io"}
            });
            
            // Process pending requests if capacity available
            process_pending();
            
            // CP2: Update active tasks metric
            update_queue_metrics();
        },
        
        [this](caf::atom_value cancel_atom, const std::string& step_id) {
            if (cancel_atom != caf::atom("cancel")) {
                return;
            }
            
            // Cancel specific step by removing from pending queue and stopping execution
            // Remove from pending queue
            std::queue<std::pair<caf::actor_addr, StepRequest>> new_queue;
            while (!pending_requests_.empty()) {
                auto [requester, req] = pending_requests_.front();
                pending_requests_.pop();
                if (req.inputs.count("step_id") && req.inputs.at("step_id") != step_id) {
                    new_queue.push({requester, req});
                }
            }
            pending_requests_ = std::move(new_queue);
            
            observability_->log_info("Step cancellation requested", "", "", "", step_id);
        },
        
        [this](caf::atom_value metrics_atom) {
            if (metrics_atom != caf::atom("metrics")) {
                return;
            }
            
            // Return pool metrics
            std::cout << "Pool metrics: load=" << current_load_ 
                                << ", pending=" << pending_requests_.size() << std::endl;
            
            // CP2: Update metrics if feature flag enabled
            if (FeatureFlags::is_observability_metrics_enabled()) {
                update_queue_metrics();
            }
        }
    };
}

void PoolActorState::process_pending() {
    while (current_load_ < max_concurrency_ && !pending_requests_.empty()) {
        auto request_pair = pending_requests_.front();
        pending_requests_.pop();
        
        caf::actor_addr requester = request_pair.first;
        StepRequest request = request_pair.second;
        
        current_load_++;
        
        // Log processing start
        observability_->log_info("Processing queued request", "", "", "", request.type, "", {
            {"resource_class", resource_class_ == ResourceClass::cpu ? "cpu" : 
                              resource_class_ == ResourceClass::gpu ? "gpu" : "io"},
            {"queue_depth", std::to_string(pending_requests_.size())}
        });
        
        // Send request back to requester for execution
        caf::anon_send(caf::actor_cast<caf::actor>(requester), caf::atom("execute"), request);
        
        // CP2: Update queue metrics after processing
        update_queue_metrics();
    }
}

size_t PoolActorState::get_queue_depth() const {
    return pending_requests_.size();
}

void PoolActorState::update_queue_metrics() {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    std::string resource_pool;
    switch (resource_class_) {
        case ResourceClass::cpu:
            resource_pool = "cpu";
            break;
        case ResourceClass::gpu:
            resource_pool = "gpu";
            break;
        case ResourceClass::io:
            resource_pool = "io";
            break;
    }
    
    // Update queue depth metric
    observability_->set_queue_depth(resource_pool, static_cast<int64_t>(get_queue_depth()));
    
    // Update active tasks metric
    observability_->set_active_tasks(resource_pool, static_cast<int64_t>(current_load_));
}

// Executor Actor Implementation
ExecutorActorState::ExecutorActorState(caf::scheduled_actor* self, std::shared_ptr<BlockExecutor> executor)
    : system_(self->system()),
      executor_(executor) {
    // CP2: Initialize observability for metrics
    observability_ = std::make_shared<Observability>("executor");
}

executor_actor::behavior_type ExecutorActorState::make_behavior() {
    return {
        [this](caf::atom_value execute_atom, const StepRequest& request) {
            if (execute_atom != caf::atom("execute")) {
                return;
            }
            
            auto result = execute_with_retry(request);
            if (result) {
                std::cout << "Step completed successfully" << std::endl;
                
                // CP2: Record metrics
                double duration_seconds = static_cast<double>(result->latency_ms) / 1000.0;
                record_step_metrics(request, *result, duration_seconds);
            } else {
                std::cout << "Step failed: " << std::to_string(result.error().code()) << std::endl;
            }
        },
        
        [this](caf::atom_value cancel_atom, const std::string& step_id) {
            if (cancel_atom != caf::atom("cancel")) {
                return;
            }
            
            auto result = executor_->cancel(step_id);
            if (result) {
                // caf::aout(caf::self) << "Step canceled: " << step_id << std::endl;
                observability_->log_info("Step canceled", "", "", "", step_id);
            } else {
                // caf::aout(caf::self) << "Failed to cancel step: " << result.error() << std::endl;
                observability_->log_error("Failed to cancel step", std::to_string(result.error().code()), step_id);
            }
        },
        
        [this](caf::atom_value metrics_atom) {
            if (metrics_atom != caf::atom("metrics")) {
                return;
            }
            
            auto metrics = executor_->metrics();
            // caf::aout(caf::self) << "Executor metrics: latency=" << metrics.latency_ms 
            //                     << "ms, success=" << metrics.success_count 
            //                     << ", errors=" << metrics.error_count << std::endl;
            observability_->log_info("Executor metrics", "", "", "", "", "", {
                {"latency_ms", std::to_string(metrics.latency_ms)},
                {"success_count", std::to_string(metrics.success_count)},
                {"error_count", std::to_string(metrics.error_count)}
            });
        }
    };
}

caf::expected<StepResult> ExecutorActorState::execute_with_retry(const StepRequest& req) {
    // Initialize retry policy
    RetryPolicy::Config retry_config;
    retry_config.base_delay_ms = 100;
    retry_config.max_delay_ms = 5000;
    retry_config.total_timeout_ms = req.timeout_ms; // Use request timeout as total timeout
    retry_config.max_retries = req.retry_count;
    RetryPolicy retry_policy(retry_config);
    
    auto total_start_time = std::chrono::steady_clock::now();
    StepResult final_result;
    int http_status_code = 0; // Extract from result if available
    
    for (int attempt = 0; attempt <= retry_policy.max_retries(); attempt++) {
        // Check retry budget before attempting
        auto current_time = std::chrono::steady_clock::now();
        auto total_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - total_start_time).count();
        
        if (retry_policy.is_budget_exhausted(total_elapsed_ms, attempt)) {
            // Budget exhausted - return timeout result
            final_result.retries_used = attempt;
            final_result.status = StepStatus::timeout;
            final_result.error_code = ErrorCode::cancelled_by_timeout;
            final_result.error_message = "Retry budget exhausted: total timeout exceeded";
            return final_result;
        }
        
        auto result = execute_single_attempt(req);
        if (result) {
            final_result = *result;
            final_result.retries_used = attempt;
            
            // Extract HTTP status code if available (for error classification)
            if (req.type == "http.request" && result->outputs.count("status_code")) {
                try {
                    http_status_code = std::stoi(result->outputs.at("status_code"));
                } catch (...) {
                    http_status_code = 0;
                }
            }
            
            if (final_result.status == StepStatus::ok) {
                return final_result;
            }
            
            // Check if error is retryable
            if (!retry_policy.is_retryable(final_result.error_code, http_status_code)) {
                // Non-retryable error - return immediately
                return final_result;
            }
        } else {
            // Execution failed - check if retryable
            // For now, assume network/system errors are retryable
            if (!retry_policy.is_retryable(ErrorCode::network_error, 0)) {
                // Non-retryable - return error
                final_result.status = StepStatus::error;
                final_result.error_code = ErrorCode::execution_failed;
                final_result.error_message = "Execution failed and error is non-retryable";
                final_result.retries_used = attempt;
                return final_result;
            }
        }
        
        // If not the last attempt, wait before retry with exponential backoff
        if (attempt < retry_policy.max_retries()) {
            int64_t backoff_delay = retry_policy.calculate_backoff_delay(attempt);
            
            // Check if backoff would exceed budget
            auto time_after_backoff = std::chrono::steady_clock::now() + 
                std::chrono::milliseconds(backoff_delay);
            auto total_after_backoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                time_after_backoff - total_start_time).count();
            
            if (total_after_backoff_ms >= retry_config.total_timeout_ms) {
                // Backoff would exceed budget - return timeout
                final_result.retries_used = attempt;
                final_result.status = StepStatus::timeout;
                final_result.error_code = ErrorCode::cancelled_by_timeout;
                final_result.error_message = "Retry budget exhausted: backoff delay would exceed total timeout";
                return final_result;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_delay));
        }
    }
    
    return final_result;
}

caf::expected<StepResult> ExecutorActorState::execute_single_attempt(const StepRequest& req) {
    auto start_time = std::chrono::steady_clock::now();
    
    auto result = executor_->execute(req);
    
    auto end_time = std::chrono::steady_clock::now();
    auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    // auto duration_seconds = static_cast<double>(latency_ms) / 1000.0;
    
    if (result) {
        result->latency_ms = latency_ms;
        
        // CP2: Record metrics if feature flag enabled
        // Note: This requires observability instance - would need to pass it or get from context
        // For now, metrics are recorded at higher level (WorkerActor or PoolActor)
    }
    
    return result;
}

void ExecutorActorState::record_step_metrics(const StepRequest& req, const StepResult& result, double duration_seconds) {
    if (!FeatureFlags::is_observability_metrics_enabled()) {
        return;
    }
    
    // Extract CP1 correlation fields from result metadata
    std::string tenant_id = result.metadata.tenant_id;
    std::string run_id = result.metadata.run_id;
    std::string flow_id = result.metadata.flow_id;
    std::string step_id = result.metadata.step_id;
    
    // Determine execution status
    std::string execution_status;
    switch (result.status) {
        case StepStatus::ok:
            execution_status = "success";
            break;
        case StepStatus::error:
            execution_status = "error";
            break;
        case StepStatus::timeout:
            execution_status = "timeout";
            break;
        case StepStatus::cancelled:
            execution_status = "cancelled";
            break;
    }
    
    // Record step execution
    observability_->record_step_execution(req.type, execution_status, tenant_id, run_id, flow_id, step_id);
    
    // Record step execution duration
    observability_->record_step_execution_duration(req.type, execution_status, duration_seconds, 
                                                   tenant_id, run_id, flow_id, step_id);
    
    // Record step errors if status is error
    if (result.status == StepStatus::error) {
        std::string error_code_str = std::to_string(static_cast<int>(result.error_code));
        observability_->record_step_error(req.type, error_code_str, tenant_id, run_id, flow_id, step_id);
    }
}

} // namespace worker
} // namespace beamline