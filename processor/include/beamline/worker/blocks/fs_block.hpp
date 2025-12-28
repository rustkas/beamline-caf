#pragma once

#include "beamline/worker/base_block_executor.hpp"
#include <string>

namespace beamline {
namespace worker {

class FsBlockExecutor : public BaseBlockExecutor {
public:
    FsBlockExecutor();
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override;
private:
    bool is_path_allowed(const std::string& path);
};

class FsGetBlockExecutor : public BaseBlockExecutor {
public:
    FsGetBlockExecutor();
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override;
private:
    bool is_path_allowed(const std::string& path);
};

} // namespace worker
} // namespace beamline
