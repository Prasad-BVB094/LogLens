#include "loglens/SLAAnalyzer.h"
#include <gtest/gtest.h>

using namespace loglens;

TEST(SLAAnalyzerTest, ResolutionBelowDeadlineIsMet) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::ERROR; // Default allowed: 900 seconds
    inc.status = IncidentStatus::RESOLVED;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();
    inc.resolution_time = Timestamp::parse("2026-08-25 10:10:00").value(); // 600s

    analyzer.evaluate(inc);

    EXPECT_EQ(inc.sla_status, SLAStatus::MET);
    ASSERT_TRUE(inc.actual_duration_sec.has_value());
    EXPECT_EQ(*inc.actual_duration_sec, 600);
    EXPECT_EQ(inc.allowed_duration_sec, 900);
}

TEST(SLAAnalyzerTest, ResolutionExactlyAtDeadlineIsMet) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::CRITICAL; // Default allowed: 300 seconds (5 mins)
    inc.status = IncidentStatus::RESOLVED;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();
    inc.resolution_time = Timestamp::parse("2026-08-25 10:05:00").value(); // exactly 300s

    analyzer.evaluate(inc);

    EXPECT_EQ(inc.sla_status, SLAStatus::MET);
    ASSERT_TRUE(inc.actual_duration_sec.has_value());
    EXPECT_EQ(*inc.actual_duration_sec, 300);
}

TEST(SLAAnalyzerTest, ResolutionAfterDeadlineIsBreached) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::CRITICAL; // Default allowed: 300 seconds
    inc.status = IncidentStatus::RESOLVED;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();
    inc.resolution_time = Timestamp::parse("2026-08-25 10:05:01").value(); // 301s

    analyzer.evaluate(inc);

    EXPECT_EQ(inc.sla_status, SLAStatus::BREACHED);
    ASSERT_TRUE(inc.actual_duration_sec.has_value());
    EXPECT_EQ(*inc.actual_duration_sec, 301);
}

TEST(SLAAnalyzerTest, AllDefaultSeverityThresholds) {
    SLATargets targets;
    EXPECT_EQ(targets.get_allowed_seconds(Severity::CRITICAL), 300);
    EXPECT_EQ(targets.get_allowed_seconds(Severity::ERROR), 900);
    EXPECT_EQ(targets.get_allowed_seconds(Severity::WARNING), 1800);
}

TEST(SLAAnalyzerTest, OpenIncidentWithoutReferenceTimeIsPending) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::ERROR;
    inc.status = IncidentStatus::OPEN;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();

    analyzer.evaluate(inc, std::nullopt);

    EXPECT_EQ(inc.sla_status, SLAStatus::PENDING);
    EXPECT_FALSE(inc.actual_duration_sec.has_value());
}

TEST(SLAAnalyzerTest, OpenIncidentWithReferenceTimeEvaluation) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::ERROR; // allowed: 900s
    inc.status = IncidentStatus::OPEN;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();

    // Reference time at 10:10:00 (600s elapsed, <= 900s) -> PENDING
    Timestamp ref1 = Timestamp::parse("2026-08-25 10:10:00").value();
    analyzer.evaluate(inc, ref1);
    EXPECT_EQ(inc.sla_status, SLAStatus::PENDING);

    // Reference time at 10:20:00 (1200s elapsed, > 900s) -> BREACHED
    Timestamp ref2 = Timestamp::parse("2026-08-25 10:20:00").value();
    analyzer.evaluate(inc, ref2);
    EXPECT_EQ(inc.sla_status, SLAStatus::BREACHED);
}

TEST(SLAAnalyzerTest, NegativeDurationRejection) {
    SLAAnalyzer analyzer;
    Incident inc;
    inc.severity = Severity::ERROR;
    inc.status = IncidentStatus::RESOLVED;
    inc.start_time = Timestamp::parse("2026-08-25 10:00:00").value();
    // Resolution time erroneously before start time!
    inc.resolution_time = Timestamp::parse("2026-08-25 09:59:00").value();

    EXPECT_THROW(analyzer.evaluate(inc), std::invalid_argument);
}
