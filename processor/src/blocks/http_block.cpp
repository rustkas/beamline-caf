#include "beamline/worker/core.hpp"
#include "beamline/worker/base_block_executor.hpp"
#include "beamline/worker/timeout_enforcement.hpp"
#include "beamline/worker/feature_flags.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <chrono>

namespace beamline {
namespace worker {

using json = nlohmann::json;

class HttpBlockExecutor : public BaseBlockExecutor {
public:
    HttpBlockExecutor() : BaseBlockExecutor("http.request", ResourceClass::io) {}
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override {
        auto start_time = std::chrono::steady_clock::now();
        auto metadata = BlockExecutor::metadata_from_context(ctx);
        
        // Validate required inputs
        std::vector<std::string> required_inputs = {"url", "method"};
        if (!validate_required_inputs(req, required_inputs)) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::missing_required_field,
                "Missing required inputs: url, method",
                metadata,
                latency_ms
            );
        }
        
        std::string url = req.inputs.at("url");
        std::string method = req.inputs.at("method");
        std::string body = get_input_or_default(req, "body");
        std::string headers_json = get_input_or_default(req, "headers", "{}");
        
        try {
            // Parse headers
            json headers;
            try {
                headers = json::parse(headers_json);
            } catch (const json::parse_error& e) {
                auto end_time = std::chrono::steady_clock::now();
                auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                record_error(latency_ms);
                
                return StepResult::error_result(
                    ErrorCode::invalid_format,
                    "Invalid headers JSON: " + std::string(e.what()),
                    metadata,
                    latency_ms
                );
            }
            
            // Execute HTTP request
            auto http_result = perform_http_request(url, method, body, headers, req.timeout_ms);
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            // Prepare outputs
            std::unordered_map<std::string, std::string> outputs;
            outputs["status_code"] = std::to_string(http_result.status_code);
            outputs["body"] = http_result.body;
            outputs["headers"] = http_result.headers;
            
            if (http_result.status_code >= 200 && http_result.status_code < 300) {
                record_success(latency_ms);
                return StepResult::success(metadata, outputs, latency_ms);
            } else {
                record_error(latency_ms);
                return StepResult::error_result(
                    ErrorCode::http_error,
                    "HTTP request failed with status: " + std::to_string(http_result.status_code),
                    metadata,
                    latency_ms
                );
            }
            
        } catch (const std::exception& e) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            // Determine error code based on exception type
            ErrorCode error_code = ErrorCode::network_error;
            std::string error_msg = "HTTP request exception: " + std::string(e.what());
            
            // Check for timeout-related errors
            if (error_msg.find("timeout") != std::string::npos || 
                error_msg.find("TIMEOUT") != std::string::npos) {
                error_code = ErrorCode::connection_timeout;
            }
            
            return StepResult::error_result(error_code, error_msg, metadata, latency_ms);
        }
    }
    
private:
    struct HttpResponse {
        int status_code;
        std::string body;
        std::string headers;
    };
    
    HttpResponse perform_http_request(const std::string& url, const std::string& method, 
                                     const std::string& body, const json& headers, 
                                     int64_t timeout_ms) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        std::string response_body;
        std::string response_headers;
        long response_code = 0;
        
        // Set basic options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        
        // CP2: Separate connection timeout and total timeout
        if (FeatureFlags::is_complete_timeout_enabled()) {
            int64_t connection_timeout = TimeoutEnforcement::get_http_connection_timeout_ms();
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connection_timeout);
            
            // Total timeout = connection timeout + request timeout
            int64_t request_timeout = timeout_ms - connection_timeout;
            if (request_timeout > 0) {
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request_timeout);
            } else {
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
            }
        } else {
            // CP1 behavior: single timeout
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        }
        
        // Set method
        if (method == "GET") {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        } else if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        } else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_READDATA, body.c_str());
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)body.size());
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        // Set headers
        struct curl_slist* header_list = nullptr;
        for (auto& [key, value] : headers.items()) {
            std::string value_str;
            if (value.is_string()) {
                value_str = value.get<std::string>();
            } else {
                value_str = value.dump();
            }
            std::string header = key + ": " + value_str;
            header_list = curl_slist_append(header_list, header.c_str());
        }
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            curl_slist_free_all(header_list);
            curl_easy_cleanup(curl);
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }
        
        // Get response code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Cleanup
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        
        return {static_cast<int>(response_code), response_body, response_headers};
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        if (userp == nullptr || contents == nullptr) {
            return 0;
        }
        const char* data = static_cast<const char*>(contents);
        userp->append(data, size * nmemb);
        return size * nmemb;
    }
    
    static size_t header_callback(char* buffer, size_t size, size_t nitems, std::string* userdata) {
        userdata->append(buffer, size * nitems);
        return size * nitems;
    }
};

} // namespace worker
} // namespace beamline