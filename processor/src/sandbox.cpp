#include "beamline/worker/core.hpp"
#include <unordered_map>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

namespace beamline {
namespace worker {

class Sandbox {
public:
    Sandbox(const BlockContext& context) : context_(context) {
        initialize_mock_environment();
    }
    
    // Mock execution environment for dry-run and testing
    caf::expected<StepResult> mock_execute(const StepRequest& request) {
        auto start_time = std::chrono::steady_clock::now();
        
        // Simulate execution delay based on block type
        simulate_execution_delay(request.type);
        
        // Generate mock results based on block type
        StepResult result;
        result.status = StepStatus::ok;
        result.latency_ms = generate_mock_latency(request.type);
        
        if (request.type == "http.request") {
            mock_http_request(request, result);
        } else if (request.type == "fs.blob_put") {
            mock_fs_blob_put(request, result);
        } else if (request.type == "fs.blob_get") {
            mock_fs_blob_get(request, result);
        } else if (request.type == "sql.query") {
            mock_sql_query(request, result);
        } else if (request.type == "human.approval") {
            mock_human_approval(request, result);
        } else {
            // Generic mock for unknown block types
            mock_generic_block(request, result);
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto actual_latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        result.latency_ms = actual_latency_ms;
        
        return result;
    }
    
    // Validate that a request can be safely executed in sandbox mode
    caf::expected<void> validate_sandbox_request(const StepRequest& request) {
        // Check for potentially dangerous operations
        if (request.type.find("exec.") == 0 || request.type.find("system.") == 0) {
            return caf::make_error(caf::sec::invalid_argument, "Sandbox mode: system execution blocks not allowed");
        }
        
        // Validate HTTP requests
        if (request.type == "http.request") {
            if (request.inputs.count("url")) {
                std::string url = request.inputs.at("url");
                if (url.find("file://") == 0 || url.find("ftp://") == 0) {
                    return caf::make_error(caf::sec::invalid_argument, "Sandbox mode: file:// and ftp:// URLs not allowed");
                }
            }
        }
        
        // Validate SQL queries
        if (request.type == "sql.query") {
            if (request.inputs.count("query")) {
                std::string query = request.inputs.at("query");
                std::string upper_query = query;
                std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
                
                std::vector<std::string> forbidden_keywords = {
                    "DROP", "DELETE", "TRUNCATE", "ALTER", "CREATE", "GRANT", "REVOKE"
                };
                
                for (const auto& keyword : forbidden_keywords) {
                    if (upper_query.find(keyword) != std::string::npos) {
                        return caf::make_error(caf::sec::invalid_argument, 
                            "Sandbox mode: destructive SQL operations not allowed");
                    }
                }
            }
        }
        
        return caf::unit;
    }
    
private:
    BlockContext context_;
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<int> latency_dist_{10, 500};
    std::uniform_int_distribution<int> success_dist_{0, 100};
    
    void initialize_mock_environment() {
        // Initialize mock data that can be used by sandbox executions
        mock_data_["users"] = R"([{"id": 1, "name": "John Doe"}, {"id": 2, "name": "Jane Smith"}])";
        mock_data_["products"] = R"([{"id": 1, "name": "Product A", "price": 29.99}, {"id": 2, "name": "Product B", "price": 49.99}])";
    }
    
    void simulate_execution_delay(const std::string& block_type) {
        int delay_ms = 50; // Base delay
        
        if (block_type.find("http.") == 0) delay_ms = 100 + (rng_() % 400); // 100-500ms
        else if (block_type.find("fs.") == 0) delay_ms = 20 + (rng_() % 180);  // 20-200ms
        else if (block_type.find("sql.") == 0) delay_ms = 30 + (rng_() % 270); // 30-300ms
        else if (block_type.find("human.") == 0) delay_ms = 1000 + (rng_() % 4000); // 1-5s
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    
    int64_t generate_mock_latency(const std::string& block_type) {
        if (block_type.find("http.") == 0) return 100 + (rng_() % 400);
        else if (block_type.find("fs.") == 0) return 20 + (rng_() % 180);
        else if (block_type.find("sql.") == 0) return 30 + (rng_() % 270);
        else if (block_type.find("human.") == 0) return 1000 + (rng_() % 4000);
        return 50 + (rng_() % 200);
    }
    
    void mock_http_request(const StepRequest& request, StepResult& result) {
        result.outputs["status_code"] = "200";
        result.outputs["body"] = R"({"message": "Mock HTTP response", "timestamp": ")" + 
                                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "}";
        result.outputs["headers"] = R"({"content-type": "application/json", "x-mock": "true"})";
        
        // Simulate occasional failures
        if (success_dist_(rng_) < 5) { // 5% failure rate
            result.status = StepStatus::error;
            result.outputs["status_code"] = "500";
            result.error_message = "Mock server error";
        }
    }
    
    void mock_fs_blob_put(const StepRequest& request, StepResult& result) {
        result.outputs["path"] = request.inputs.count("path") ? request.inputs.at("path") : "/tmp/mock_file.txt";
        result.outputs["size"] = "1024";
        result.outputs["created"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Simulate occasional failures
        if (success_dist_(rng_) < 2) { // 2% failure rate
            result.status = StepStatus::error;
            result.error_message = "Mock file system error";
        }
    }
    
    void mock_fs_blob_get(const StepRequest& request, StepResult& result) {
        result.outputs["path"] = request.inputs.count("path") ? request.inputs.at("path") : "/tmp/mock_file.txt";
        result.outputs["content"] = "Mock file content";
        result.outputs["size"] = "1024";
        result.outputs["modified"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Simulate occasional failures
        if (success_dist_(rng_) < 3) { // 3% failure rate
            result.status = StepStatus::error;
            result.error_message = "Mock file not found";
        }
    }
    
    void mock_sql_query(const StepRequest& request, StepResult& result) {
        // Return mock data based on query type
        if (request.inputs.count("query")) {
            std::string query = request.inputs.at("query");
            std::string upper_query = query;
            std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
            
            if (upper_query.find("SELECT") != std::string::npos) {
                if (upper_query.find("FROM USERS") != std::string::npos) {
                    result.outputs["rows"] = mock_data_["users"];
                    result.outputs["row_count"] = "2";
                } else if (upper_query.find("FROM PRODUCTS") != std::string::npos) {
                    result.outputs["rows"] = mock_data_["products"];
                    result.outputs["row_count"] = "2";
                } else {
                    result.outputs["rows"] = R"([{"id": 1, "name": "Mock Item"}])";
                    result.outputs["row_count"] = "1";
                }
            } else {
                result.outputs["affected_rows"] = "1";
            }
        }
        
        // Simulate occasional failures
        if (success_dist_(rng_) < 1) { // 1% failure rate
            result.status = StepStatus::error;
            result.error_message = "Mock database error";
        }
    }
    
    void mock_human_approval(const StepRequest& request, StepResult& result) {
        result.outputs["approval_id"] = "mock_approval_" + std::to_string(rng_() % 10000);
        result.outputs["status"] = "approved";
        result.outputs["decision"] = "approved";
        result.outputs["approved_by"] = "mock_user";
        result.outputs["approved_at"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        result.outputs["reason"] = "Mock approval for testing";
        
        // Simulate occasional rejections
        if (success_dist_(rng_) < 10) { // 10% rejection rate
            result.outputs["status"] = "rejected";
            result.outputs["decision"] = "rejected";
            result.outputs["reason"] = "Mock rejection for testing";
        }
    }
    
    void mock_generic_block(const StepRequest& request, StepResult& result) {
        result.outputs["mock_result"] = "true";
        result.outputs["block_type"] = request.type;
        result.outputs["execution_id"] = "mock_exec_" + std::to_string(rng_() % 10000);
    }
    
    std::unordered_map<std::string, std::string> mock_data_;
};

} // namespace worker
} // namespace beamline