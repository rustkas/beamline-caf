#pragma once
#include "worker/types.hpp"

namespace worker {

class BlockExecutor {
 public:
  virtual ~BlockExecutor() = default;
  virtual StatusCode init(const BlockContext& ctx) = 0;
  virtual StepResult execute(const StepRequest& req) = 0;
  virtual StatusCode cancel(const BlockContext& ctx) = 0;
  virtual BlockMetrics metrics() const = 0;
};

}