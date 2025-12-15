#include "beamline/worker/core.hpp"
#include <sqlite3.h>
#include <chrono>
#include <sstream>

namespace beamline {
namespace worker {

class SqlBlockExecutor : public BaseBlockExecutor {
public:
    SqlBlockExecutor() : BaseBlockExecutor("sql.query", ResourceClass::cpu) {}
    
    ~SqlBlockExecutor() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    
    caf::expected<void> init(const BlockContext& ctx) override {
        context_ = ctx;
        
        // Initialize SQLite for sandbox mode or specific database connections
        if (context_.sandbox) {
            // Use in-memory database for sandbox mode
            int rc = sqlite3_open(":memory:", &db_);
            if (rc != SQLITE_OK) {
                return caf::make_error(caf::sec::runtime_error, "Failed to open SQLite database");
            }
        }
        
        return caf::unit;
    }
    
    caf::expected<StepResult> execute_impl(const StepRequest& req, const BlockContext& ctx) override {
        auto start_time = std::chrono::steady_clock::now();
        auto metadata = BlockExecutor::metadata_from_context(ctx);
        
        // Validate required inputs
        std::vector<std::string> required_inputs = {"query"};
        if (!validate_required_inputs(req, required_inputs)) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::missing_required_field,
                "Missing required input: query",
                metadata,
                latency_ms
            );
        }
        
        std::string query = req.inputs.at("query");
        std::string connection_string = get_input_or_default(req, "connection", ":memory:");
        
        // Parse parameters if provided
        // Note: Parameter parsing from JSON is not implemented in CP1
        // For CP1, queries are executed as-is without parameter substitution
        // Full parameter binding would require JSON parsing and proper SQLite binding (planned for CP2)
        if (req.inputs.count("params")) {
            // Parameters are provided but not used in CP1
            // In CP2, this would parse JSON and bind parameters to SQLite placeholders
        }
        
        try {
            sqlite3* db = db_;
            bool local_db = false;
            
            // Connect to database if not in sandbox mode or different connection
            if (!ctx.sandbox || connection_string != ":memory:") {
                int rc = sqlite3_open(connection_string.c_str(), &db);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error("Failed to open database: " + connection_string);
                }
                local_db = true;
            }
            
            // Prepare statement
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                if (local_db) sqlite3_close(db);
                throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
            }
            
            // RAII wrapper to ensure statement is finalized
            struct StatementGuard {
                sqlite3_stmt* stmt_;
                bool local_db_;
                sqlite3* db_;
                StatementGuard(sqlite3_stmt* stmt, bool local_db, sqlite3* db) 
                    : stmt_(stmt), local_db_(local_db), db_(db) {}
                ~StatementGuard() {
                    if (stmt_) {
                        sqlite3_finalize(stmt_);
                    }
                    if (local_db_ && db_) {
                        sqlite3_close(db_);
                    }
                }
                // Non-copyable
                StatementGuard(const StatementGuard&) = delete;
                StatementGuard& operator=(const StatementGuard&) = delete;
            };
            
            StatementGuard guard(stmt, local_db, db);
            local_db = false; // Guard will handle cleanup
            
            // Bind parameters (simplified - CP1 does not support parameter binding)
            // For CP1, queries are executed as-is without parameter substitution
            // Full parameter binding would require:
            // 1. Parse JSON parameters
            // 2. Map parameter names to SQLite placeholders (? or :name)
            // 3. Use sqlite3_bind_* functions to bind values
            // This is planned for CP2
            
            // Execute query
            std::vector<std::unordered_map<std::string, std::string>> rows;
            
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                std::unordered_map<std::string, std::string> row;
                int col_count = sqlite3_column_count(stmt);
                
                for (int i = 0; i < col_count; i++) {
                    const char* col_name = sqlite3_column_name(stmt, i);
                    const char* col_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    
                    if (col_name && col_value) {
                        row[col_name] = col_value;
                    }
                }
                
                rows.push_back(row);
            }
            
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Query execution failed: " + std::string(sqlite3_errmsg(db)));
            }
            
            // Get number of affected rows for non-SELECT queries
            int affected_rows = sqlite3_changes(db);
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            // Format results
            std::unordered_map<std::string, std::string> outputs;
            if (!rows.empty()) {
                // Convert rows to JSON string (simplified)
                std::stringstream json_result;
                json_result << "[";
                for (size_t i = 0; i < rows.size(); i++) {
                    if (i > 0) json_result << ",";
                    json_result << "{";
                    bool first = true;
                    for (const auto& [key, value] : rows[i]) {
                        if (!first) json_result << ",";
                        json_result << "\"" << key << "\":\"" << value << "\"";
                        first = false;
                    }
                    json_result << "}";
                }
                json_result << "]";
                outputs["rows"] = json_result.str();
                outputs["row_count"] = std::to_string(rows.size());
            } else {
                outputs["affected_rows"] = std::to_string(affected_rows);
            }
            
            record_success(latency_ms);
            return StepResult::success(metadata, outputs, latency_ms);
            
        } catch (const std::exception& e) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            record_error(latency_ms);
            
            return StepResult::error_result(
                ErrorCode::execution_failed,
                "SQL query execution failed: " + std::string(e.what()),
                metadata,
                latency_ms
            );
        }
    }
    
private:
    sqlite3* db_ = nullptr;
};

} // namespace worker
} // namespace beamline