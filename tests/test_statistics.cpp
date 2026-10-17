#include "loglens/StatisticsAnalyzer.h"
#include <gtest/gtest.h>

using namespace loglens;

TEST(StatisticsAnalyzerTest, EmptyDataset) {
    StatisticsAnalyzer analyzer;
    std::vector<LogEntry> entries;
    auto summary = analyzer.analyze(entries);

    EXPECT_EQ(summary.total_records, 0);
    EXPECT_EQ(summary.info_count, 0);
    EXPECT_EQ(summary.warning_count, 0);
    EXPECT_EQ(summary.error_count, 0);
    EXPECT_EQ(summary.critical_count, 0);
    EXPECT_FALSE(summary.top_failing_service.has_value());
    EXPECT_FALSE(summary.top_error_message.has_value());
}

TEST(StatisticsAnalyzerTest, SeverityAndServiceCounts) {
    StatisticsAnalyzer analyzer;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::INFO, "AUTH", "User login"},
        {2, Timestamp::parse("2026-08-25 10:01:00").value(), Severity::WARNING, "DB", "Slow query"},
        {3, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Timeout to host 10.0.0.1"},
        {4, Timestamp::parse("2026-08-25 10:03:00").value(), Severity::CRITICAL, "DB", "Connection pool dead"},
        {5, Timestamp::parse("2026-08-25 10:04:00").value(), Severity::ERROR, "API", "Timeout to host 10.0.0.2"}
    };

    auto summary = analyzer.analyze(entries);

    EXPECT_EQ(summary.total_records, 5);
    EXPECT_EQ(summary.info_count, 1);
    EXPECT_EQ(summary.warning_count, 1);
    EXPECT_EQ(summary.error_count, 2);
    EXPECT_EQ(summary.critical_count, 1);

    ASSERT_TRUE(summary.top_failing_service.has_value());
    EXPECT_EQ(*summary.top_failing_service, "DB");
    EXPECT_EQ(summary.top_failing_count, 2); // 1 error + 1 critical
}

TEST(StatisticsAnalyzerTest, TieBreakingDeterministic) {
    StatisticsAnalyzer analyzer;
    // Both API and AUTH have exactly 2 failures.
    // Lexicographically, "API" must come before "AUTH".
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "AUTH", "Err 1"},
        {2, Timestamp::parse("2026-08-25 10:01:00").value(), Severity::ERROR, "AUTH", "Err 2"},
        {3, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "API", "Err A"},
        {4, Timestamp::parse("2026-08-25 10:03:00").value(), Severity::ERROR, "API", "Err B"}
    };

    auto summary = analyzer.analyze(entries);

    ASSERT_TRUE(summary.top_failing_service.has_value());
    EXPECT_EQ(*summary.top_failing_service, "API");
    EXPECT_EQ(summary.service_breakdown[0].service, "API");
    EXPECT_EQ(summary.service_breakdown[1].service, "AUTH");
}

TEST(StatisticsAnalyzerTest, MessageNormalization) {
    std::string msg1 = "Connection timeout to replica-01 host 192.168.1.10";
    std::string msg2 = "Connection timeout to replica-02 host 192.168.1.11";

    std::string norm1 = StatisticsAnalyzer::normalize_message(msg1);
    std::string norm2 = StatisticsAnalyzer::normalize_message(msg2);

    EXPECT_EQ(norm1, norm2);
    EXPECT_EQ(norm1, "Connection timeout to replica-<N> host <N>.<N>.<N>.<N>");
}

TEST(StatisticsAnalyzerTest, FilteringOperations) {
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::INFO, "AUTH", "Msg 1"},
        {2, Timestamp::parse("2026-08-25 11:00:00").value(), Severity::ERROR, "DB", "Msg 2"},
        {3, Timestamp::parse("2026-08-26 10:00:00").value(), Severity::ERROR, "AUTH", "Msg 3"}
    };

    auto by_sev = StatisticsAnalyzer::filter_by_severity(entries, Severity::ERROR);
    EXPECT_EQ(by_sev.size(), 2);

    auto by_svc = StatisticsAnalyzer::filter_by_service(entries, "AUTH");
    EXPECT_EQ(by_svc.size(), 2);

    auto by_date = StatisticsAnalyzer::filter_by_date(entries, "2026-08-25");
    EXPECT_EQ(by_date.size(), 2);

    auto by_date2 = StatisticsAnalyzer::filter_by_date(entries, "2026-08-26");
    EXPECT_EQ(by_date2.size(), 1);
}
