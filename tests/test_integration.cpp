#include "loglens/Application.h"
#include "loglens/DatabaseManager.h"
#include "loglens/IncidentDetector.h"
#include "loglens/LogParser.h"
#include "loglens/RootCauseEngine.h"
#include "loglens/SLAAnalyzer.h"
#include "loglens/StatisticsAnalyzer.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace loglens;

class IntegrationPipelineTest : public ::testing::Test {
protected:
    std::string fixture_log = "fixture_full_lifecycle.log";
    std::string fixture_db = "fixture_full_lifecycle.db";
    std::string perf_log = "fixture_perf_100k.log";

    void SetUp() override {
        cleanup();
    }

    void TearDown() override {
        cleanup();
    }

    void cleanup() {
        std::error_code ec;
        std::filesystem::remove(fixture_log, ec);
        std::filesystem::remove(fixture_db, ec);
        std::filesystem::remove(fixture_db + "-wal", ec);
        std::filesystem::remove(fixture_db + "-shm", ec);
        std::filesystem::remove(perf_log, ec);
    }
};

TEST_F(IntegrationPipelineTest, ComprehensiveFullPipelineWithAllFixtures) {
    // 1. Create a golden fixture containing:
    // - Normal operations
    // - Service DB: resolved incident with SLA MET (duration = 6 mins, allowed = 15 mins)
    // - Service PAYMENT: resolved incident with SLA BREACHED (duration = 16 mins, allowed = 5 mins since CRITICAL)
    // - Service STORAGE: unresolved incident (OPEN)
    // - Malformed records safely handled
    // - Events that do not trigger incidents (e.g. only 1 or 2 errors)
    {
        std::ofstream ofs(fixture_log);
        ofs << "2026-08-25 09:59:00 INFO SYSTEM System initialization completed\n"
            << "malformed non-canonical line to be skipped\n"
            << "2026-08-25 10:00:01 INFO AUTH User login user_id=100\n"
            // Incident 1 (DB): 3 errors in 2 mins -> resolved at 10:06:00 (duration = 6 mins <= 15 mins -> SLA MET)
            << "2026-08-25 10:00:00 ERROR DB Connection timeout to replica-1\n"
            << "2026-08-25 10:01:00 ERROR DB Connection timeout to replica-2\n"
            << "2026-08-25 10:02:00 ERROR DB Connection timeout to replica-3\n"
            << "2026-08-25 10:06:00 INFO DB Service recovered\n"
            // Low volume errors (AUTH): only 2 errors -> NO INCIDENT
            << "2026-08-25 10:10:00 ERROR AUTH Invalid token\n"
            << "2026-08-25 10:11:00 ERROR AUTH Invalid token\n"
            // Incident 2 (PAYMENT): CRITICAL incident -> allowed SLA 300s (5m), resolved after 16 mins -> SLA BREACHED
            << "2026-08-25 10:20:00 ERROR PAYMENT Service unavailable\n"
            << "2026-08-25 10:21:00 CRITICAL PAYMENT Out of memory in worker thread\n"
            << "2026-08-25 10:22:00 ERROR PAYMENT Service unavailable\n"
            << "2026-08-25 10:36:00 INFO PAYMENT Service recovered\n"
            // Incident 3 (STORAGE): 3 errors -> no recovery record -> status OPEN
            << "2026-08-25 11:00:00 ERROR STORAGE Disk full on volume /data\n"
            << "2026-08-25 11:01:00 ERROR STORAGE Disk full on volume /data\n"
            << "2026-08-25 11:02:00 ERROR STORAGE Disk full on volume /data\n"
            << "2026-08-25 11:10:00 INFO SYSTEM Monitoring heartbeat\n";
    }

    std::ostringstream out, err;
    Application app(out, err);

    // Run Analyze
    const char* argv[] = {"loglens", "analyze", fixture_log.c_str(), "--db", fixture_db.c_str()};
    int ret = app.run(5, argv);
    EXPECT_EQ(ret, 0);

    // Verify persisted database records
    DatabaseManager db(fixture_db);
    db.initialize();

    auto opt_run_id = db.get_latest_run_id();
    ASSERT_TRUE(opt_run_id.has_value());
    const std::string run_id = *opt_run_id;

    auto opt_run = db.get_run(run_id);
    ASSERT_TRUE(opt_run.has_value());
    const AnalysisRun run = *opt_run;
    EXPECT_EQ(run.valid_records, 16);
    EXPECT_EQ(run.malformed_records, 1);

    auto incidents = db.query_incidents(run_id);
    ASSERT_EQ(incidents.size(), 3);

    // Incident 1: DB (Resolved, RCA-NET-01, SLA MET)
    EXPECT_EQ(incidents[0].service, "DB");
    EXPECT_EQ(incidents[0].severity, Severity::ERROR);
    EXPECT_EQ(incidents[0].status, IncidentStatus::RESOLVED);
    EXPECT_EQ(incidents[0].rca_rule_id, "RCA-NET-01");
    EXPECT_EQ(incidents[0].sla_status, SLAStatus::MET);
    ASSERT_TRUE(incidents[0].actual_duration_sec.has_value());
    EXPECT_EQ(*incidents[0].actual_duration_sec, 360); // 6 mins

    // Incident 2: PAYMENT (Resolved, RCA-MEM-01, SLA BREACHED)
    EXPECT_EQ(incidents[1].service, "PAYMENT");
    EXPECT_EQ(incidents[1].severity, Severity::CRITICAL);
    EXPECT_EQ(incidents[1].status, IncidentStatus::RESOLVED);
    EXPECT_EQ(incidents[1].rca_rule_id, "RCA-MEM-01");
    EXPECT_EQ(incidents[1].sla_status, SLAStatus::BREACHED);
    ASSERT_TRUE(incidents[1].actual_duration_sec.has_value());
    EXPECT_EQ(*incidents[1].actual_duration_sec, 960); // 16 mins > 300s allowed

    // Incident 3: STORAGE (Open, RCA-STO-01)
    EXPECT_EQ(incidents[2].service, "STORAGE");
    EXPECT_EQ(incidents[2].severity, Severity::ERROR);
    EXPECT_EQ(incidents[2].status, IncidentStatus::OPEN);
    EXPECT_EQ(incidents[2].rca_rule_id, "RCA-STO-01");
    EXPECT_FALSE(incidents[2].resolution_time.has_value());
}

TEST_F(IntegrationPipelineTest, BenchmarkLargeScaleDataset100k) {
    // Generate 100,000 deterministic records
    const std::size_t num_records = 100000;
    {
        std::ofstream ofs(perf_log);
        for (std::size_t i = 0; i < num_records; ++i) {
            // Distribute across services and severities
            std::string sev = (i % 50 == 0) ? "ERROR" : "INFO";
            std::string svc = (i % 2 == 0) ? "API" : "DB";
            // Epoch starting at 2026-08-25 00:00:00 = 1787616000
            Timestamp ts = Timestamp::from_epoch(1787616000 + static_cast<std::int64_t>(i));
            ofs << ts.to_string() << " " << sev << " " << svc << " Benchmark record sample payload index=" << i << "\n";
        }
    }

    auto start_time = std::chrono::steady_clock::now();

    LogParser parser;
    ParseResult parse_result = parser.parse_file(perf_log);

    auto parse_time = std::chrono::steady_clock::now();

    EXPECT_EQ(parse_result.total_lines, num_records);
    EXPECT_EQ(parse_result.valid_records, num_records);
    EXPECT_EQ(parse_result.malformed_records, 0);

    StatisticsAnalyzer stats_analyzer;
    StatisticsSummary summary = stats_analyzer.analyze(parse_result.entries);

    auto stats_time = std::chrono::steady_clock::now();

    EXPECT_EQ(summary.total_records, num_records);
    EXPECT_EQ(summary.error_count, num_records / 50);

    IncidentDetector detector;
    auto incidents = detector.detect_incidents(parse_result.entries, "RUN-PERF-100K");

    auto end_time = std::chrono::steady_clock::now();

    auto parse_ms = std::chrono::duration_cast<std::chrono::milliseconds>(parse_time - start_time).count();
    auto stats_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stats_time - parse_time).count();
    auto detect_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - stats_time).count();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "\n[Performance Benchmark Results - 100,000 Log Records]\n"
              << "  Parse Duration:     " << parse_ms << " ms\n"
              << "  Stats Aggregation:  " << stats_ms << " ms\n"
              << "  Incident Detection: " << detect_ms << " ms\n"
              << "  Total Processing:   " << total_ms << " ms\n"
              << "  Throughput:         " << (num_records * 1000 / (total_ms > 0 ? total_ms : 1)) << " records/sec\n"
              << "  Incidents Detected: " << incidents.size() << "\n\n";

    EXPECT_GT(incidents.size(), 0);
}
