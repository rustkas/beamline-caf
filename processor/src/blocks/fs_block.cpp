#include "beamline/worker/core.hpp"
#include "beamline/worker/timeout_enforcement.hpp"
#include "beamline/worker/feature_flags.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <future>

namespace beamline {
namespace worker {

class FsBlockExecutor : public BaseBlockExecutor {
public:
    FsBlockExecutor() : BaseBlockExecutor("fs.blob_put", ResourceClass::io) {}
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override {
        auto start_time = std::chrono::steady_clock::now();
        auto metadata = BlockExecutor::metadata_from_context(ctx);
        
        // Validate required inputs
        std::vector<std::string> required_inputs = {"path", "content"};
        if (!validate_required_inputs(req, required_inputs)) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::missing_required_field,
                "Missing required inputs: path, content",
                metadata,
                latency_ms
            );
        }
        
        std::string path = req.inputs.at("path");
        std::string content = req.inputs.at("content");
        bool overwrite = get_input_or_default(req, "overwrite") == "true";
        
        try {
            // CP2: Get FS operation timeout
            int64_t fs_timeout_ms = TimeoutEnforcement::get_fs_timeout_ms("write");
            if (fs_timeout_ms == 0) {
                fs_timeout_ms = req.timeout_ms; // Use request timeout if CP2 disabled
            }
            
            // Security check: ensure path is within allowed directories
            if (!is_path_allowed(path)) {
                throw std::runtime_error("Path not allowed: " + path);
            }
            
            // Check if file exists and overwrite is false
            if (!overwrite && std::filesystem::exists(path)) {
                throw std::runtime_error("File already exists and overwrite is false: " + path);
            }
            
            // CP2: Execute FS operations with timeout
            if (FeatureFlags::is_complete_timeout_enabled() && fs_timeout_ms > 0) {
                // Execute file write in async with timeout
                // Capture by value to avoid race conditions
                auto write_future = std::async(std::launch::async, [path, content]() -> std::pair<bool, std::string> {
                    try {
                        // Create directory if it doesn't exist
                        std::filesystem::path filepath(path);
                        std::filesystem::create_directories(filepath.parent_path());
                        
                        // Write file
                        std::ofstream file(path, std::ios::binary);
                        if (!file.is_open()) {
                            return {false, "Failed to open file for writing: " + path};
                        }
                        
                        file.write(content.data(), static_cast<std::streamsize>(content.size()));
                        file.close();
                        return {true, ""};
                    } catch (const std::exception& e) {
                        return {false, e.what()};
                    }
                });
                
                auto status = write_future.wait_for(std::chrono::milliseconds(fs_timeout_ms));
                if (status == std::future_status::timeout) {
                    throw std::runtime_error("FS write operation timeout: exceeded " + 
                                            std::to_string(fs_timeout_ms) + "ms");
                }
                
                auto [operation_success, operation_error] = write_future.get();
                if (!operation_success) {
                    throw std::runtime_error(operation_error);
                }
            } else {
                // CP1 behavior: no timeout enforcement
                // Create directory if it doesn't exist
                std::filesystem::path filepath(path);
                std::filesystem::create_directories(filepath.parent_path());
                
                // Write file
                std::ofstream file(path, std::ios::binary);
                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open file for writing: " + path);
                }
                
                file.write(content.data(), content.size());
                file.close();
            }
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            std::unordered_map<std::string, std::string> outputs;
            outputs["path"] = path;
            outputs["size"] = std::to_string(content.size());
            outputs["created"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            
            record_success(latency_ms, 0, content.size());
            return StepResult::success(metadata, outputs, latency_ms);
            
        } catch (const std::exception& e) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            ErrorCode error_code = ErrorCode::execution_failed;
            std::string error_msg = "File write error: " + std::string(e.what());
            
            // Check for permission errors
            if (error_msg.find("permission") != std::string::npos || 
                error_msg.find("Permission") != std::string::npos) {
                error_code = ErrorCode::permission_denied;
            }
            
            return StepResult::error_result(error_code, error_msg, metadata, latency_ms);
        }
    }
    
private:
    bool is_path_allowed(const std::string& path) {
        // Basic security: only allow writing to specific directories
        // In a real implementation, this would be configurable
        std::vector<std::string> allowed_prefixes = {
            "/tmp/beamline/",
            "/var/lib/beamline/data/",
            "./data/"
        };
        
        for (const auto& prefix : allowed_prefixes) {
            if (path.find(prefix) == 0) {
                return true;
            }
        }
        
        return false;
    }
};

class FsGetBlockExecutor : public BaseBlockExecutor {
public:
    FsGetBlockExecutor() : BaseBlockExecutor("fs.blob_get", ResourceClass::io) {}
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override {
        auto start_time = std::chrono::steady_clock::now();
        auto metadata = BlockExecutor::metadata_from_context(ctx);
        
        // Validate required inputs
        std::vector<std::string> required_inputs = {"path"};
        if (!validate_required_inputs(req, required_inputs)) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::missing_required_field,
                "Missing required input: path",
                metadata,
                latency_ms
            );
        }
        
        std::string path = req.inputs.at("path");
        
        try {
            // CP2: Get FS operation timeout
            int64_t fs_timeout_ms = TimeoutEnforcement::get_fs_timeout_ms("read");
            if (fs_timeout_ms == 0) {
                fs_timeout_ms = req.timeout_ms; // Use request timeout if CP2 disabled
            }
            
            // Security check: ensure path is within allowed directories
            if (!is_path_allowed(path)) {
                throw std::runtime_error("Path not allowed: " + path);
            }
            
            std::string content;
            std::streamsize file_size = 0;
            
            if (FeatureFlags::is_complete_timeout_enabled() && fs_timeout_ms > 0) {
                // Execute file read in async with timeout
                // Capture by value to avoid race conditions
                auto read_future = std::async(std::launch::async, [path]() -> std::tuple<bool, std::string, std::streamsize> {
                    try {
                        // Check if file exists
                        if (!std::filesystem::exists(path)) {
                            return {false, "File not found: " + path, 0};
                        }
                        
                        // Read file
                        std::ifstream file(path, std::ios::binary | std::ios::ate);
                        if (!file.is_open()) {
                            return {false, "Failed to open file for reading: " + path, 0};
                        }
                        
                        std::streamsize size = file.tellg();
                        file.seekg(0, std::ios::beg);
                        
                        std::string file_content;
                        file_content.resize(static_cast<size_t>(size));
                        file.read(file_content.data(), size);
                        file.close();
                        return {true, std::move(file_content), size};
                    } catch (const std::exception& e) {
                        return {false, e.what(), 0};
                    }
                });
                
                auto status = read_future.wait_for(std::chrono::milliseconds(fs_timeout_ms));
                if (status == std::future_status::timeout) {
                    throw std::runtime_error("FS read operation timeout: exceeded " + 
                                            std::to_string(fs_timeout_ms) + "ms");
                }
                
                auto [operation_success, read_content, read_file_size] = read_future.get();
                if (!operation_success) {
                    throw std::runtime_error(read_content); // In error case, read_content contains error message
                }
                content = std::move(read_content);
                file_size = read_file_size;
            } else {
                // CP1 behavior: no timeout enforcement
                // Check if file exists
                if (!std::filesystem::exists(path)) {
                    throw std::runtime_error("File not found: " + path);
                }
                
                // Read file
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open file for reading: " + path);
                }
                
                file_size = file.tellg();
                file.seekg(0, std::ios::beg);
                
                content.resize(file_size);
                file.read(content.data(), file_size);
                file.close();
            }
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            std::unordered_map<std::string, std::string> outputs;
            outputs["path"] = path;
            outputs["content"] = content;
            outputs["size"] = std::to_string(static_cast<int64_t>(file_size));
            outputs["modified"] = std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
            
            record_success(latency_ms, 0, file_size);
            return StepResult::success(metadata, outputs, latency_ms);
            
        } catch (const std::exception& e) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            ErrorCode error_code = ErrorCode::execution_failed;
            std::string error_msg = "File read error: " + std::string(e.what());
            
            // Check for file not found
            if (error_msg.find("not found") != std::string::npos || 
                error_msg.find("Not found") != std::string::npos) {
                error_code = ErrorCode::resource_unavailable;
            }
            
            return StepResult::error_result(error_code, error_msg, metadata, latency_ms);
        }
    }
    
private:
    bool is_path_allowed(const std::string& path) {
        // Basic security: only allow reading from specific directories
        // In a real implementation, this would be configurable
        std::vector<std::string> allowed_prefixes = {
            "/tmp/beamline/",
            "/var/lib/beamline/data/",
            "./data/"
        };
        
        for (const auto& prefix : allowed_prefixes) {
            if (path.find(prefix) == 0) {
                return true;
            }
        }
        
        return false;
    }
};

} // namespace worker
} // namespace beamline