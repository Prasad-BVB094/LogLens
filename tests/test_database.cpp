#include "loglens/DatabaseManager.h"
#include <gtest/gtest.h>
#include <filesystem>

using namespace loglens;

class DatabaseManagerTest : public ::testing::Test {
protected:
    std::string test_db_path = "test_loglens_temp.db";

    void SetUp() override {
        std::error_code ec;
        std::filesystem::remove(test_db_path, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(test_db_path, ec);
        std::filesystem::remove(test_db_path + "-wal", ec);
        std::filesystem::remove(test_db_path + "-shm", ec);
    }
};

TEST_F(DatabaseManagerTest, InitializationAndSchemaVersion) {
    {
        DatabaseManager db(test_db_path);
        db.initialize();
        auto info = db.get_db_info();
        EXPECT_EQ(info.schema_version, 1);
        EXPECT_EQ(info.total_runs, 0);
        EXPECT_EQ(info.total_logs, 0);
        EXPECT_EQ(info.total_incidents, 0);
    }
}

TEST_F(DatabaseManagerTest, InsertAndQueryOperations) {
    DatabaseManager db(test_db_path);
    db.initialize();

    AnalysisRun run;
    run.run_id = "RUN-1001";
    run.file_path = "test.log";
    run.total_lines = 10;
    run.valid_records = 8;
    run.malformed_records = 2;
    run.start_timestamp = "2026-08-25 10:00:00";
    run.end_timestamp = "2026-08-25 10:10:00";

    std::vector<LogEntry> logs = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::INFO, "AUTH", "User login"},
        {2, Timestamp::parse("2026-08-25 10:05:00").value(), Severity::ERROR, "DB", "Connection timeout"}
    };

    std::vector<Incident> incidents;
    Incident inc;
    inc.incident_id = "INC-1001-001";
    inc.run_id = run.run_id;
    inc.service = "DB";
    inc.severity = Severity::ERROR;
    inc.start_time = Timestamp::parse("2026-08-25 10:05:00").value();
    inc.last_error_time = Timestamp::parse("2026-08-25 10:05:00").value();
    inc.resolution_time = std::nullopt; // Nullable resolution timestamp
    inc.error_count = 3;
    inc.status = IncidentStatus::OPEN;
    inc.probable_root_cause = "Network latency";
    inc.rca_rule_id = "RCA-NET-01";
    inc.sla_status = SLAStatus::PENDING;
    inc.allowed_duration_sec = 900;
    incidents.push_back(inc);

    db.save_analysis_run(run, logs, incidents);

    auto latest_id = db.get_latest_run_id();
    ASSERT_TRUE(latest_id.has_value());
    EXPECT_EQ(*latest_id, "RUN-1001");

    auto queried_run = db.get_run("RUN-1001");
    ASSERT_TRUE(queried_run.has_value());
    EXPECT_EQ(queried_run->valid_records, 8);

    auto queried_logs = db.query_logs("RUN-1001");
    EXPECT_EQ(queried_logs.size(), 2);

    auto queried_incidents = db.query_incidents("RUN-1001");
    ASSERT_EQ(queried_incidents.size(), 1);
    EXPECT_EQ(queried_incidents[0].incident_id, "INC-1001-001");
    EXPECT_EQ(queried_incidents[0].status, IncidentStatus::OPEN);
    EXPECT_FALSE(queried_incidents[0].resolution_time.has_value());
}

TEST_F(DatabaseManagerTest, PreparedStatementWithSpecialCharactersAndQuotes) {
    DatabaseManager db(test_db_path);
    db.initialize();

    AnalysisRun run;
    run.run_id = "RUN-QUOTES";
    run.file_path = "path/with spaces and 'quotes'/app.log";
    run.total_lines = 1;
    run.valid_records = 1;
    run.malformed_records = 0;

    std::vector<LogEntry> logs = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "SQL Injection attempt: ''; DROP TABLE logs; -- \"test\""}
    };

    db.save_analysis_run(run, logs, {});

    auto queried = db.query_logs("RUN-QUOTES");
    ASSERT_EQ(queried.size(), 1);
    EXPECT_EQ(queried[0].message, "SQL Injection attempt: ''; DROP TABLE logs; -- \"test\"");

    // Verify database tables still intact
    auto info = db.get_db_info();
    EXPECT_EQ(info.total_logs, 1);
}

TEST_F(DatabaseManagerTest, TransactionRollbackOnFailure) {
    DatabaseManager db(test_db_path);
    db.initialize();

    AnalysisRun run;
    run.run_id = "RUN-FAIL";
    run.file_path = "test.log";

    // Create 2 incidents with the SAME duplicate incident_id
    // This will trigger UNIQUE constraint failure on the second incident insert!
    std::vector<Incident> incidents;
    Incident inc1;
    inc1.incident_id = "INC-DUP";
    inc1.run_id = run.run_id;
    inc1.service = "DB";
    inc1.start_time = Timestamp::from_epoch(100);
    inc1.last_error_time = Timestamp::from_epoch(100);
    incidents.push_back(inc1);

    Incident inc2 = inc1; // duplicate ID
    incidents.push_back(inc2);

    std::vector<LogEntry> logs = {
        {1, Timestamp::from_epoch(100), Severity::INFO, "DB", "Msg"}
    };

    EXPECT_THROW(db.save_analysis_run(run, logs, incidents), std::runtime_error);

    // Verify rollback: run and logs should NOT be in database
    EXPECT_FALSE(db.get_run("RUN-FAIL").has_value());
    EXPECT_EQ(db.query_logs("RUN-FAIL").size(), 0);
    EXPECT_EQ(db.query_incidents("RUN-FAIL").size(), 0);
}
