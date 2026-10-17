#pragma once

#include "loglens/Incident.h"
#include "loglens/LogEntry.h"
#include "loglens/Severity.h"
#include "loglens/Timestamp.h"

#include <sqlite3.h>

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <stdexcept>

namespace loglens {

struct AnalysisRun {
    std::string run_id{};
    std::string file_path{};
    std::size_t total_lines{0};
    std::size_t valid_records{0};
    std::size_t malformed_records{0};
    std::string start_timestamp{};
    std::string end_timestamp{};
    std::string created_at{};
};

struct DatabaseInfo {
    int schema_version{0};
    std::size_t total_runs{0};
    std::size_t total_logs{0};
    std::size_t total_incidents{0};
    std::string database_path{};
    std::int64_t file_size_bytes{0};
};

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // Non-copyable, movable
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&& other) noexcept;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept;

    // Initializes tables, indexes, and schema migrations
    void initialize();

    // Transactional persistence of analysis run, log records, and incidents
    void save_analysis_run(
        const AnalysisRun& run,
        const std::vector<LogEntry>& entries,
        const std::vector<Incident>& incidents);

    // Queries
    [[nodiscard]] std::optional<std::string> get_latest_run_id() const;
    [[nodiscard]] std::optional<AnalysisRun> get_run(const std::string& run_id) const;
    [[nodiscard]] std::vector<AnalysisRun> get_all_runs() const;

    [[nodiscard]] std::vector<LogEntry> query_logs(
        const std::string& run_id,
        std::optional<Severity> severity_filter = std::nullopt,
        std::optional<std::string> service_filter = std::nullopt,
        std::optional<std::string> date_filter = std::nullopt) const;

    [[nodiscard]] std::vector<Incident> query_incidents(
        const std::string& run_id,
        std::optional<IncidentStatus> status_filter = std::nullopt) const;

    [[nodiscard]] std::optional<Incident> get_incident_by_id(const std::string& incident_id) const;

    [[nodiscard]] DatabaseInfo get_db_info() const;

    [[nodiscard]] const std::string& db_path() const noexcept { return db_path_; }

private:
    std::string db_path_{};
    sqlite3* db_{nullptr};

    void check_sqlite(int rc, const char* context) const;
    void execute_sql(const char* sql);
    int get_user_version() const;
    void set_user_version(int version);
};

// RAII Statement Wrapper
class Statement {
public:
    Statement(sqlite3* db, const char* sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bind_int(int index, int val);
    void bind_int64(int index, std::int64_t val);
    void bind_text(int index, const std::string& val);
    void bind_null(int index);

    int step();
    void reset();
    sqlite3_stmt* get() const noexcept { return stmt_; }

private:
    sqlite3_stmt* stmt_{nullptr};
    sqlite3* db_{nullptr};
};

// RAII Transaction Helper
class TransactionScope {
public:
    explicit TransactionScope(sqlite3* db);
    ~TransactionScope();

    void commit();

private:
    sqlite3* db_{nullptr};
    bool committed_{false};
};

} // namespace loglens
