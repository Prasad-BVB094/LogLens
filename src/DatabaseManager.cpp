#include "loglens/DatabaseManager.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace loglens {

// --- Statement Implementation ---

Statement::Statement(sqlite3* db, const char* sql) : db_(db) {
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        throw std::runtime_error("Failed to prepare statement: " + std::string(sql) + " (" + err + ")");
    }
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

void Statement::bind_int(int index, int val) {
    int rc = sqlite3_bind_int(stmt_, index, val);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
}

void Statement::bind_int64(int index, std::int64_t val) {
    int rc = sqlite3_bind_int64(stmt_, index, val);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
}

void Statement::bind_text(int index, const std::string& val) {
    int rc = sqlite3_bind_text(stmt_, index, val.c_str(), static_cast<int>(val.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
}

void Statement::bind_null(int index) {
    int rc = sqlite3_bind_null(stmt_, index);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
}

int Statement::step() {
    return sqlite3_step(stmt_);
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

// --- TransactionScope Implementation ---

TransactionScope::TransactionScope(sqlite3* db) : db_(db) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to begin transaction: " + err);
    }
}

TransactionScope::~TransactionScope() {
    if (!committed_ && db_) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

void TransactionScope::commit() {
    if (committed_) return;
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to commit transaction: " + err);
    }
    committed_ = true;
}

// --- DatabaseManager Implementation ---

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "Failed to allocate sqlite memory";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error("Cannot open database at '" + db_path_ + "': " + err);
    }

    // Always enforce foreign keys
    execute_sql("PRAGMA foreign_keys = ON;");
    execute_sql("PRAGMA journal_mode = WAL;");
    execute_sql("PRAGMA synchronous = NORMAL;");
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

DatabaseManager::DatabaseManager(DatabaseManager&& other) noexcept
    : db_path_(std::move(other.db_path_)), db_(other.db_) {
    other.db_ = nullptr;
}

DatabaseManager& DatabaseManager::operator=(DatabaseManager&& other) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }
        db_path_ = std::move(other.db_path_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void DatabaseManager::check_sqlite(int rc, const char* context) const {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(db_));
    }
}

void DatabaseManager::execute_sql(const char* sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown SQL error";
        sqlite3_free(err_msg);
        throw std::runtime_error("SQL Execution failed: " + std::string(sql) + " (" + err + ")");
    }
}

int DatabaseManager::get_user_version() const {
    Statement stmt(db_, "PRAGMA user_version;");
    if (stmt.step() == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }
    return 0;
}

void DatabaseManager::set_user_version(int version) {
    std::string sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    execute_sql(sql.c_str());
}

void DatabaseManager::initialize() {
    execute_sql(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL DEFAULT (datetime('now', 'utc'))
        );

        CREATE TABLE IF NOT EXISTS analysis_runs (
            run_id TEXT PRIMARY KEY,
            file_path TEXT NOT NULL,
            total_lines INTEGER NOT NULL,
            valid_records INTEGER NOT NULL,
            malformed_records INTEGER NOT NULL,
            start_timestamp TEXT,
            end_timestamp TEXT,
            created_at TEXT NOT NULL DEFAULT (datetime('now', 'utc'))
        );

        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            line_number INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            epoch_seconds INTEGER NOT NULL,
            severity TEXT NOT NULL,
            service TEXT NOT NULL,
            message TEXT NOT NULL,
            FOREIGN KEY(run_id) REFERENCES analysis_runs(run_id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_logs_run_epoch ON logs(run_id, epoch_seconds);
        CREATE INDEX IF NOT EXISTS idx_logs_run_service ON logs(run_id, service);
        CREATE INDEX IF NOT EXISTS idx_logs_run_severity ON logs(run_id, severity);

        CREATE TABLE IF NOT EXISTS incidents (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            incident_id TEXT NOT NULL UNIQUE,
            run_id TEXT NOT NULL,
            service TEXT NOT NULL,
            severity TEXT NOT NULL,
            start_time TEXT NOT NULL,
            start_epoch INTEGER NOT NULL,
            last_error_time TEXT NOT NULL,
            last_error_epoch INTEGER NOT NULL,
            resolution_time TEXT,
            resolution_epoch INTEGER,
            error_count INTEGER NOT NULL,
            status TEXT NOT NULL CHECK(status IN ('OPEN', 'RESOLVED')),
            probable_root_cause TEXT NOT NULL,
            rca_rule_id TEXT NOT NULL,
            sla_status TEXT NOT NULL CHECK(sla_status IN ('MET', 'BREACHED', 'PENDING')),
            allowed_duration_sec INTEGER NOT NULL,
            actual_duration_sec INTEGER,
            FOREIGN KEY(run_id) REFERENCES analysis_runs(run_id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_incidents_run_id ON incidents(run_id);
        CREATE INDEX IF NOT EXISTS idx_incidents_service ON incidents(service);
        CREATE INDEX IF NOT EXISTS idx_incidents_status ON incidents(status);
    )");

    int ver = get_user_version();
    if (ver == 0) {
        execute_sql("INSERT OR IGNORE INTO schema_migrations (version) VALUES (1);");
        set_user_version(1);
    }
}

void DatabaseManager::save_analysis_run(
    const AnalysisRun& run,
    const std::vector<LogEntry>& entries,
    const std::vector<Incident>& incidents) {

    TransactionScope tx(db_);

    // 1. Insert analysis_run
    {
        Statement stmt(db_, R"(
            INSERT INTO analysis_runs (
                run_id, file_path, total_lines, valid_records, malformed_records,
                start_timestamp, end_timestamp, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, datetime('now', 'utc'));
        )");
        stmt.bind_text(1, run.run_id);
        stmt.bind_text(2, run.file_path);
        stmt.bind_int64(3, static_cast<std::int64_t>(run.total_lines));
        stmt.bind_int64(4, static_cast<std::int64_t>(run.valid_records));
        stmt.bind_int64(5, static_cast<std::int64_t>(run.malformed_records));
        stmt.bind_text(6, run.start_timestamp);
        stmt.bind_text(7, run.end_timestamp);
        if (stmt.step() != SQLITE_DONE) {
            throw std::runtime_error("Failed to insert analysis_run: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    // 2. Insert log entries
    {
        Statement stmt(db_, R"(
            INSERT INTO logs (
                run_id, line_number, timestamp, epoch_seconds, severity, service, message
            ) VALUES (?, ?, ?, ?, ?, ?, ?);
        )");

        for (const auto& entry : entries) {
            stmt.reset();
            stmt.bind_text(1, run.run_id);
            stmt.bind_int64(2, static_cast<std::int64_t>(entry.line_number));
            stmt.bind_text(3, entry.timestamp.to_string());
            stmt.bind_int64(4, entry.timestamp.epoch_seconds());
            stmt.bind_text(5, std::string(to_string(entry.severity)));
            stmt.bind_text(6, entry.service);
            stmt.bind_text(7, entry.message);
            if (stmt.step() != SQLITE_DONE) {
                throw std::runtime_error("Failed to insert log entry: " + std::string(sqlite3_errmsg(db_)));
            }
        }
    }

    // 3. Insert incidents
    {
        Statement stmt(db_, R"(
            INSERT INTO incidents (
                incident_id, run_id, service, severity,
                start_time, start_epoch, last_error_time, last_error_epoch,
                resolution_time, resolution_epoch, error_count, status,
                probable_root_cause, rca_rule_id, sla_status,
                allowed_duration_sec, actual_duration_sec
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )");

        for (const auto& inc : incidents) {
            stmt.reset();
            stmt.bind_text(1, inc.incident_id);
            stmt.bind_text(2, inc.run_id);
            stmt.bind_text(3, inc.service);
            stmt.bind_text(4, std::string(to_string(inc.severity)));

            stmt.bind_text(5, inc.start_time.to_string());
            stmt.bind_int64(6, inc.start_time.epoch_seconds());

            stmt.bind_text(7, inc.last_error_time.to_string());
            stmt.bind_int64(8, inc.last_error_time.epoch_seconds());

            if (inc.resolution_time) {
                stmt.bind_text(9, inc.resolution_time->to_string());
                stmt.bind_int64(10, inc.resolution_time->epoch_seconds());
            } else {
                stmt.bind_null(9);
                stmt.bind_null(10);
            }

            stmt.bind_int64(11, static_cast<std::int64_t>(inc.error_count));
            stmt.bind_text(12, std::string(to_string(inc.status)));
            stmt.bind_text(13, inc.probable_root_cause);
            stmt.bind_text(14, inc.rca_rule_id);
            stmt.bind_text(15, std::string(to_string(inc.sla_status)));
            stmt.bind_int64(16, inc.allowed_duration_sec);

            if (inc.actual_duration_sec) {
                stmt.bind_int64(17, *inc.actual_duration_sec);
            } else {
                stmt.bind_null(17);
            }

            if (stmt.step() != SQLITE_DONE) {
                throw std::runtime_error("Failed to insert incident: " + std::string(sqlite3_errmsg(db_)));
            }
        }
    }

    tx.commit();
}

std::optional<std::string> DatabaseManager::get_latest_run_id() const {
    Statement stmt(db_, "SELECT run_id FROM analysis_runs ORDER BY created_at DESC LIMIT 1;");
    if (stmt.step() == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt.get(), 0);
        if (text) {
            return std::string(reinterpret_cast<const char*>(text));
        }
    }
    return std::nullopt;
}

std::optional<AnalysisRun> DatabaseManager::get_run(const std::string& run_id) const {
    Statement stmt(db_, "SELECT run_id, file_path, total_lines, valid_records, malformed_records, start_timestamp, end_timestamp, created_at FROM analysis_runs WHERE run_id = ?;");
    stmt.bind_text(1, run_id);
    if (stmt.step() == SQLITE_ROW) {
        AnalysisRun r;
        r.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        r.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        r.total_lines = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 2));
        r.valid_records = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 3));
        r.malformed_records = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 4));
        const auto* start_ts = sqlite3_column_text(stmt.get(), 5);
        if (start_ts) r.start_timestamp = reinterpret_cast<const char*>(start_ts);
        const auto* end_ts = sqlite3_column_text(stmt.get(), 6);
        if (end_ts) r.end_timestamp = reinterpret_cast<const char*>(end_ts);
        const auto* created = sqlite3_column_text(stmt.get(), 7);
        if (created) r.created_at = reinterpret_cast<const char*>(created);
        return r;
    }
    return std::nullopt;
}

std::vector<AnalysisRun> DatabaseManager::get_all_runs() const {
    Statement stmt(db_, "SELECT run_id, file_path, total_lines, valid_records, malformed_records, start_timestamp, end_timestamp, created_at FROM analysis_runs ORDER BY created_at DESC;");
    std::vector<AnalysisRun> runs;
    while (stmt.step() == SQLITE_ROW) {
        AnalysisRun r;
        r.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        r.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        r.total_lines = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 2));
        r.valid_records = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 3));
        r.malformed_records = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 4));
        const auto* start_ts = sqlite3_column_text(stmt.get(), 5);
        if (start_ts) r.start_timestamp = reinterpret_cast<const char*>(start_ts);
        const auto* end_ts = sqlite3_column_text(stmt.get(), 6);
        if (end_ts) r.end_timestamp = reinterpret_cast<const char*>(end_ts);
        const auto* created = sqlite3_column_text(stmt.get(), 7);
        if (created) r.created_at = reinterpret_cast<const char*>(created);
        runs.push_back(std::move(r));
    }
    return runs;
}

std::vector<LogEntry> DatabaseManager::query_logs(
    const std::string& run_id,
    std::optional<Severity> severity_filter,
    std::optional<std::string> service_filter,
    std::optional<std::string> date_filter) const {

    std::string sql = "SELECT line_number, timestamp, epoch_seconds, severity, service, message FROM logs WHERE run_id = ?";
    if (severity_filter) sql += " AND severity = ?";
    if (service_filter) sql += " AND service = ?";
    if (date_filter) sql += " AND substr(timestamp, 1, 10) = ?";
    sql += " ORDER BY epoch_seconds ASC, line_number ASC;";

    Statement stmt(db_, sql.c_str());
    int param_idx = 1;
    stmt.bind_text(param_idx++, run_id);
    if (severity_filter) {
        stmt.bind_text(param_idx++, std::string(to_string(*severity_filter)));
    }
    if (service_filter) {
        stmt.bind_text(param_idx++, *service_filter);
    }
    if (date_filter) {
        stmt.bind_text(param_idx++, *date_filter);
    }

    std::vector<LogEntry> logs;
    while (stmt.step() == SQLITE_ROW) {
        std::size_t line_no = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 0));
        std::int64_t epoch = sqlite3_column_int64(stmt.get(), 2);
        std::string sev_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        std::string svc_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        std::string msg_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 5));

        Severity sev = parse_severity(sev_str).value_or(Severity::INFO);
        logs.emplace_back(line_no, Timestamp::from_epoch(epoch), sev, svc_str, msg_str);
    }
    return logs;
}

std::vector<Incident> DatabaseManager::query_incidents(
    const std::string& run_id,
    std::optional<IncidentStatus> status_filter) const {

    std::string sql = R"(
        SELECT incident_id, run_id, service, severity,
               start_epoch, last_error_epoch, resolution_epoch,
               error_count, status, probable_root_cause, rca_rule_id,
               sla_status, allowed_duration_sec, actual_duration_sec
        FROM incidents
        WHERE run_id = ?
    )";
    if (status_filter) {
        sql += " AND status = ?";
    }
    sql += " ORDER BY start_epoch ASC;";

    Statement stmt(db_, sql.c_str());
    stmt.bind_text(1, run_id);
    if (status_filter) {
        stmt.bind_text(2, std::string(to_string(*status_filter)));
    }

    std::vector<Incident> list;
    while (stmt.step() == SQLITE_ROW) {
        Incident inc;
        inc.incident_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        inc.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        inc.service = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        inc.severity = parse_severity(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3))).value_or(Severity::ERROR);

        inc.start_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 4));
        inc.last_error_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 5));

        if (sqlite3_column_type(stmt.get(), 6) != SQLITE_NULL) {
            inc.resolution_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 6));
        }

        inc.error_count = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 7));
        inc.status = parse_incident_status(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 8))).value_or(IncidentStatus::OPEN);
        inc.probable_root_cause = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 9));
        inc.rca_rule_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 10));
        inc.sla_status = parse_sla_status(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 11))).value_or(SLAStatus::PENDING);
        inc.allowed_duration_sec = sqlite3_column_int64(stmt.get(), 12);

        if (sqlite3_column_type(stmt.get(), 13) != SQLITE_NULL) {
            inc.actual_duration_sec = sqlite3_column_int64(stmt.get(), 13);
        }

        list.push_back(std::move(inc));
    }
    return list;
}

std::optional<Incident> DatabaseManager::get_incident_by_id(const std::string& incident_id) const {
    Statement stmt(db_, R"(
        SELECT incident_id, run_id, service, severity,
               start_epoch, last_error_epoch, resolution_epoch,
               error_count, status, probable_root_cause, rca_rule_id,
               sla_status, allowed_duration_sec, actual_duration_sec
        FROM incidents
        WHERE incident_id = ?;
    )");
    stmt.bind_text(1, incident_id);

    if (stmt.step() == SQLITE_ROW) {
        Incident inc;
        inc.incident_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        inc.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        inc.service = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        inc.severity = parse_severity(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3))).value_or(Severity::ERROR);

        inc.start_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 4));
        inc.last_error_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 5));

        if (sqlite3_column_type(stmt.get(), 6) != SQLITE_NULL) {
            inc.resolution_time = Timestamp::from_epoch(sqlite3_column_int64(stmt.get(), 6));
        }

        inc.error_count = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 7));
        inc.status = parse_incident_status(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 8))).value_or(IncidentStatus::OPEN);
        inc.probable_root_cause = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 9));
        inc.rca_rule_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 10));
        inc.sla_status = parse_sla_status(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 11))).value_or(SLAStatus::PENDING);
        inc.allowed_duration_sec = sqlite3_column_int64(stmt.get(), 12);

        if (sqlite3_column_type(stmt.get(), 13) != SQLITE_NULL) {
            inc.actual_duration_sec = sqlite3_column_int64(stmt.get(), 13);
        }

        return inc;
    }
    return std::nullopt;
}

DatabaseInfo DatabaseManager::get_db_info() const {
    DatabaseInfo info;
    info.database_path = db_path_;
    info.schema_version = get_user_version();

    {
        Statement s(db_, "SELECT count(*) FROM analysis_runs;");
        if (s.step() == SQLITE_ROW) info.total_runs = static_cast<std::size_t>(sqlite3_column_int64(s.get(), 0));
    }
    {
        Statement s(db_, "SELECT count(*) FROM logs;");
        if (s.step() == SQLITE_ROW) info.total_logs = static_cast<std::size_t>(sqlite3_column_int64(s.get(), 0));
    }
    {
        Statement s(db_, "SELECT count(*) FROM incidents;");
        if (s.step() == SQLITE_ROW) info.total_incidents = static_cast<std::size_t>(sqlite3_column_int64(s.get(), 0));
    }

    std::error_code ec;
    auto sz = std::filesystem::file_size(db_path_, ec);
    if (!ec) {
        info.file_size_bytes = static_cast<std::int64_t>(sz);
    }

    return info;
}

} // namespace loglens
