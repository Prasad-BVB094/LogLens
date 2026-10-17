#include "loglens/LogParser.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace loglens;

TEST(LogParserTest, ValidRecordsAtEverySeverity) {
    LogParser parser;
    std::string data =
        "2026-08-25 10:00:00 INFO AUTH User login ok\n"
        "2026-08-25 10:01:00 WARNING DB High memory usage\n"
        "2026-08-25 10:02:00 ERROR API Connection timeout\n"
        "2026-08-25 10:03:00 CRITICAL SYSTEM Kernel panic\n";

    std::istringstream iss(data);
    ParseResult res = parser.parse_stream(iss);

    EXPECT_EQ(res.total_lines, 4);
    EXPECT_EQ(res.valid_records, 4);
    EXPECT_EQ(res.malformed_records, 0);
    ASSERT_EQ(res.entries.size(), 4);

    EXPECT_EQ(res.entries[0].severity, Severity::INFO);
    EXPECT_EQ(res.entries[0].service, "AUTH");
    EXPECT_EQ(res.entries[0].message, "User login ok");

    EXPECT_EQ(res.entries[1].severity, Severity::WARNING);
    EXPECT_EQ(res.entries[2].severity, Severity::ERROR);
    EXPECT_EQ(res.entries[3].severity, Severity::CRITICAL);
}

TEST(LogParserTest, WarnNormalizationToWarning) {
    LogParser parser;
    std::string data = "2026-08-25 10:00:00 WARN CACHE Eviction rate high\n";
    std::istringstream iss(data);
    ParseResult res = parser.parse_stream(iss);

    ASSERT_EQ(res.valid_records, 1);
    EXPECT_EQ(res.entries[0].severity, Severity::WARNING);
}

TEST(LogParserTest, MessagesWithSpacesAndPunctuation) {
    LogParser parser;
    std::string data = "2026-08-25 10:00:00 ERROR DB Query failed: SELECT * FROM users WHERE id='admin'; error_code=1045\n";
    std::istringstream iss(data);
    ParseResult res = parser.parse_stream(iss);

    ASSERT_EQ(res.valid_records, 1);
    EXPECT_EQ(res.entries[0].message, "Query failed: SELECT * FROM users WHERE id='admin'; error_code=1045");
}

TEST(LogParserTest, CalendarValidationLeapYear) {
    LogParser parser;
    // 2024 is a leap year (Feb 29 valid)
    std::string valid_leap = "2024-02-29 12:00:00 INFO SYS Leap day tick\n";
    std::istringstream iss1(valid_leap);
    ParseResult res1 = parser.parse_stream(iss1);
    EXPECT_EQ(res1.valid_records, 1);

    // 2025 is NOT a leap year (Feb 29 invalid)
    std::string invalid_leap = "2025-02-29 12:00:00 INFO SYS Invalid leap day\n";
    std::istringstream iss2(invalid_leap);
    ParseResult res2 = parser.parse_stream(iss2);
    EXPECT_EQ(res2.valid_records, 0);
    EXPECT_EQ(res2.malformed_records, 1);
}

TEST(LogParserTest, InvalidCalendarDatesAndHours) {
    LogParser parser;
    // Month 13
    std::istringstream iss1("2026-13-01 10:00:00 INFO DB Msg\n");
    EXPECT_EQ(parser.parse_stream(iss1).malformed_records, 1);

    // Day 32
    std::istringstream iss2("2026-01-32 10:00:00 INFO DB Msg\n");
    EXPECT_EQ(parser.parse_stream(iss2).malformed_records, 1);

    // Hour 24
    std::istringstream iss3("2026-01-15 24:00:00 INFO DB Msg\n");
    EXPECT_EQ(parser.parse_stream(iss3).malformed_records, 1);

    // Minute 60
    std::istringstream iss4("2026-01-15 12:60:00 INFO DB Msg\n");
    EXPECT_EQ(parser.parse_stream(iss4).malformed_records, 1);
}

TEST(LogParserTest, MissingFieldsAndUnknownSeverity) {
    LogParser parser;
    // Missing message
    std::istringstream iss1("2026-08-25 10:00:00 INFO AUTH\n");
    EXPECT_EQ(parser.parse_stream(iss1).malformed_records, 1);

    // Missing service and message
    std::istringstream iss2("2026-08-25 10:00:00 INFO\n");
    EXPECT_EQ(parser.parse_stream(iss2).malformed_records, 1);

    // Unknown severity level
    std::istringstream iss3("2026-08-25 10:00:00 TRACE AUTH User login\n");
    EXPECT_EQ(parser.parse_stream(iss3).malformed_records, 1);
}

TEST(LogParserTest, EmptyInputAndBlankLines) {
    LogParser parser;
    std::string data = "\n\n   \n\t\n";
    std::istringstream iss(data);
    ParseResult res = parser.parse_stream(iss);

    EXPECT_EQ(res.total_lines, 4);
    EXPECT_EQ(res.valid_records, 0);
    EXPECT_EQ(res.malformed_records, 0); // Blank lines are safely skipped
}

TEST(LogParserTest, MalformedFollowedByValidRecordInTolerantMode) {
    LogParser parser(ParserMode::Tolerant);
    std::string data =
        "garbage line that does not match anything\n"
        "2026-08-25 10:00:01 INFO AUTH Login success\n"
        "another corrupt entry\n"
        "2026-08-25 10:00:02 ERROR DB Failed query\n";

    std::istringstream iss(data);
    ParseResult res = parser.parse_stream(iss);

    EXPECT_EQ(res.total_lines, 4);
    EXPECT_EQ(res.valid_records, 2);
    EXPECT_EQ(res.malformed_records, 2);
    ASSERT_EQ(res.entries.size(), 2);
    EXPECT_EQ(res.entries[0].line_number, 2);
    EXPECT_EQ(res.entries[1].line_number, 4);
}

TEST(LogParserTest, StrictModeThrowsOnMalformedLine) {
    LogParser parser(ParserMode::Strict);
    std::string data =
        "2026-08-25 10:00:01 INFO AUTH Login success\n"
        "bad line here\n";

    std::istringstream iss(data);
    EXPECT_THROW(parser.parse_stream(iss), std::runtime_error);
}

TEST(LogParserTest, MissingFileHandling) {
    LogParser parser(ParserMode::Tolerant);
    ParseResult res = parser.parse_file("non_existent_file_path.log");
    EXPECT_EQ(res.valid_records, 0);
    ASSERT_FALSE(res.diagnostics.empty());

    LogParser strict_parser(ParserMode::Strict);
    EXPECT_THROW(strict_parser.parse_file("non_existent_file_path.log"), std::runtime_error);
}

TEST(LogParserTest, TimestampBoundaryFormattingAndEpochConversions) {
    // Epoch 0
    Timestamp epoch0 = Timestamp::from_epoch(0);
    EXPECT_EQ(epoch0.to_string(), "1970-01-01 00:00:00");
    EXPECT_EQ(epoch0.date_only(), "1970-01-01");

    // fromEpochSeconds alias
    Timestamp epochAlias = Timestamp::fromEpochSeconds(0);
    EXPECT_EQ(epochAlias.to_string(), "1970-01-01 00:00:00");

    // Known date: 2026-08-25 10:00:00
    auto parsed = Timestamp::parse("2026-08-25 10:00:00");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->to_string(), "2026-08-25 10:00:00");
    EXPECT_EQ(parsed->date_only(), "2026-08-25");

    // Roundtrip verification
    Timestamp roundtrip = Timestamp::from_epoch(parsed->epoch_seconds());
    EXPECT_EQ(roundtrip.to_string(), "2026-08-25 10:00:00");

    // Negative epoch handling safely without crashes or truncation
    Timestamp neg = Timestamp::from_epoch(-100);
    EXPECT_FALSE(neg.to_string().empty());
    EXPECT_FALSE(neg.date_only().empty());

    // Year boundary check: year 2200
    auto max_supported = Timestamp::parse("2200-12-31 23:59:59");
    ASSERT_TRUE(max_supported.has_value());
    EXPECT_EQ(max_supported->to_string(), "2200-12-31 23:59:59");

    // Out of range rejected
    EXPECT_FALSE(Timestamp::parse("1969-12-31 23:59:59").has_value());
    EXPECT_FALSE(Timestamp::parse("2201-01-01 00:00:00").has_value());
}
