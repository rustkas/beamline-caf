#pragma once

#include "beamline/worker/base_block_executor.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace beamline {
namespace worker {

class HttpBlockExecutor : public BaseBlockExecutor {
public:
    HttpBlockExecutor();
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override;

private:
    struct HttpResponse {
        int status_code;
        std::string body;
        std::string headers;
    };
    
    HttpResponse perform_http_request(const std::string& url, const std::string& method, 
                                     const std::string& body, const nlohmann::json& headers, 
                                     int64_t timeout_ms);
                                     
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, std::string* userdata);
};

} // namespace worker
} // namespace beamline
