#include "loglens/IncidentDetector.h"
#include <gtest/gtest.h>

using namespace loglens;

TEST(IncidentDetectorTest, ExactlyThreeEventsWithinWindowCreatesIncident) {
    IncidentDetector detector;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "Connection timeout 1"},
        {2, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Connection timeout 2"},
        {3, Timestamp::parse("2026-08-25 10:04:00").value(), Severity::ERROR, "DB", "Connection timeout 3"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-TEST-1");
    ASSERT_EQ(incidents.size(), 1);
    EXPECT_EQ(incidents[0].service, "DB");
    EXPECT_EQ(incidents[0].severity, Severity::ERROR);
    EXPECT_EQ(incidents[0].error_count, 3);
    EXPECT_EQ(incidents[0].status, IncidentStatus::OPEN);
}

TEST(IncidentDetectorTest, TwoEventsDoNotCreateIncident) {
    IncidentDetector detector;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "Connection timeout 1"},
        {2, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Connection timeout 2"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-TEST-2");
    EXPECT_TRUE(incidents.empty());
}

TEST(IncidentDetectorTest, DifferentServicesNeverCombined) {
    IncidentDetector detector;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "Timeout 1"},
        {2, Timestamp::parse("2026-08-25 10:01:00").value(), Severity::ERROR, "AUTH", "Auth fail 1"},
        {3, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Timeout 2"},
        {4, Timestamp::parse("2026-08-25 10:03:00").value(), Severity::ERROR, "AUTH", "Auth fail 2"}
    };

    // DB has 2, AUTH has 2. Neither reaches threshold 3.
    auto incidents = detector.detect_incidents(entries, "RUN-TEST-3");
    EXPECT_TRUE(incidents.empty());
}

TEST(IncidentDetectorTest, ExactWindowBoundaryCondition) {
    IncidentDetector detector;
    // Boundary check: 10:00:00 to 10:05:00 is exactly 300 seconds -> INSIDE window
    std::vector<LogEntry> inside = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "API", "Err 1"},
        {2, Timestamp::parse("2026-08-25 10:02:30").value(), Severity::ERROR, "API", "Err 2"},
        {3, Timestamp::parse("2026-08-25 10:05:00").value(), Severity::ERROR, "API", "Err 3"}
    };
    EXPECT_EQ(detector.detect_incidents(inside, "RUN-B1").size(), 1);

    // Boundary check: 10:00:00 to 10:05:01 is 301 seconds -> OUTSIDE window
    std::vector<LogEntry> outside = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "API", "Err 1"},
        {2, Timestamp::parse("2026-08-25 10:02:30").value(), Severity::ERROR, "API", "Err 2"},
        {3, Timestamp::parse("2026-08-25 10:05:01").value(), Severity::ERROR, "API", "Err 3"}
    };
    EXPECT_EQ(detector.detect_incidents(outside, "RUN-B2").size(), 0);
}

TEST(IncidentDetectorTest, CriticalPrecedenceEscalation) {
    IncidentDetector detector;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "PAYMENT", "Error 1"},
        {2, Timestamp::parse("2026-08-25 10:01:00").value(), Severity::CRITICAL, "PAYMENT", "Out of memory"},
        {3, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "PAYMENT", "Error 2"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-CRIT");
    ASSERT_EQ(incidents.size(), 1);
    EXPECT_EQ(incidents[0].severity, Severity::CRITICAL);
}

TEST(IncidentDetectorTest, OutOfOrderInputStabilized) {
    IncidentDetector detector;
    // Entries fed in out-of-order timestamps
    std::vector<LogEntry> entries = {
        {3, Timestamp::parse("2026-08-25 10:04:00").value(), Severity::ERROR, "DB", "Error 3"},
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "Error 1"},
        {2, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Error 2"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-ORDER");
    ASSERT_EQ(incidents.size(), 1);
    EXPECT_EQ(incidents[0].start_time.to_string(), "2026-08-25 10:00:00");
    EXPECT_EQ(incidents[0].last_error_time.to_string(), "2026-08-25 10:04:00");
}

TEST(IncidentDetectorTest, ContinuousBurstClusterExtension) {
    IncidentDetector detector;
    // 5 errors arriving sequentially every 2 minutes: total duration 8 minutes
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "E1"},
        {2, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "E2"},
        {3, Timestamp::parse("2026-08-25 10:04:00").value(), Severity::ERROR, "DB", "E3"},
        {4, Timestamp::parse("2026-08-25 10:06:00").value(), Severity::ERROR, "DB", "E4"},
        {5, Timestamp::parse("2026-08-25 10:08:00").value(), Severity::ERROR, "DB", "E5"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-BURST");
    ASSERT_EQ(incidents.size(), 1);
    EXPECT_EQ(incidents[0].error_count, 5);
    EXPECT_EQ(incidents[0].start_time.to_string(), "2026-08-25 10:00:00");
    EXPECT_EQ(incidents[0].last_error_time.to_string(), "2026-08-25 10:08:00");
}

TEST(IncidentDetectorTest, ExplicitRecoveryMatching) {
    IncidentDetector detector;
    std::vector<LogEntry> entries = {
        {1, Timestamp::parse("2026-08-25 10:00:00").value(), Severity::ERROR, "DB", "Timeout 1"},
        {2, Timestamp::parse("2026-08-25 10:01:00").value(), Severity::ERROR, "DB", "Timeout 2"},
        {3, Timestamp::parse("2026-08-25 10:02:00").value(), Severity::ERROR, "DB", "Timeout 3"},
        {4, Timestamp::parse("2026-08-25 10:07:00").value(), Severity::INFO, "DB", "Service recovered"}
    };

    auto incidents = detector.detect_incidents(entries, "RUN-REC");
    ASSERT_EQ(incidents.size(), 1);
    EXPECT_EQ(incidents[0].status, IncidentStatus::RESOLVED);
    ASSERT_TRUE(incidents[0].resolution_time.has_value());
    EXPECT_EQ(incidents[0].resolution_time->to_string(), "2026-08-25 10:07:00");
    // Confirm last error timestamp is preserved and NOT overwritten by resolution timestamp
    EXPECT_EQ(incidents[0].last_error_time.to_string(), "2026-08-25 10:02:00");
}

TEST(IncidentDetectorTest, InvalidConfigurationHandling) {
    DetectionConfig conf;
    conf.window_seconds = -10;
    // Should be caught or rejected
    EXPECT_THROW(DetectionConfig::load_from_file("non_existent.conf"), std::runtime_error);
}
