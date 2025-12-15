#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace worker {

enum class StatusCode { ok, error, timeout, canceled };

enum class ResourceClass { cpu, gpu, io };

struct RetryPolicy {
  int max_retries;
  int backoff_ms;
};

struct Resources {
  ResourceClass cls;
  int memory_mb;
  int concurrency;
};

struct BlockContext {
  std::string tenant_id;
  std::string trace_id;
  std::string flow_id;
  std::string step_id;
  bool sandbox;
  std::vector<std::string> rbac_scopes;
};

struct StepRequest {
  std::string type;
  std::unordered_map<std::string, std::string> inputs;
  Resources resources;
  int timeout_ms;
  RetryPolicy retry;
  std::unordered_map<std::string, std::string> guardrails;
  BlockContext ctx;
};

struct StepResult {
  StatusCode status;
  std::unordered_map<std::string, std::string> outputs;
  std::string error;
  int latency_ms;
  int retries_used;
};

struct BlockMetrics {
  std::atomic<uint64_t> success_count;
  std::atomic<uint64_t> error_count;
  std::atomic<uint64_t> latency_total_ms;
  BlockMetrics() : success_count(0), error_count(0), latency_total_ms(0) {}
};

}