#include "loglens/Application.h"
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace loglens;

class CliTest : public ::testing::Test {
protected:
    std::string test_db = "test_cli.db";
    std::string test_log = "test_cli.log";

    void SetUp() override {
        std::error_code ec;
        std::filesystem::remove(test_db, ec);
        std::filesystem::remove(test_log, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(test_db, ec);
        std::filesystem::remove(test_log, ec);
    }
};

TEST_F(CliTest, HelpAndVersionFlags) {
    std::ostringstream out, err;
    Application app(out, err);

    const char* argv_help[] = {"loglens", "--help"};
    EXPECT_EQ(app.run(2, argv_help), 0);
    EXPECT_NE(out.str().find("Usage: loglens"), std::string::npos);

    out.str("");
    const char* argv_ver[] = {"loglens", "--version"};
    EXPECT_EQ(app.run(2, argv_ver), 0);
    EXPECT_NE(out.str().find("LogLens version"), std::string::npos);
}

TEST_F(CliTest, InvalidCommandReportsError) {
    std::ostringstream out, err;
    Application app(out, err);

    const char* argv[] = {"loglens", "nonexistent-cmd"};
    EXPECT_EQ(app.run(2, argv), 1);
    EXPECT_NE(err.str().find("Unknown command"), std::string::npos);
}

TEST_F(CliTest, MissingAnalyzeFileArgument) {
    std::ostringstream out, err;
    Application app(out, err);

    const char* argv[] = {"loglens", "analyze"};
    EXPECT_EQ(app.run(2, argv), 1);
    EXPECT_NE(err.str().find("requires a log file path"), std::string::npos);
}

TEST_F(CliTest, AnalyzeNonExistentFile) {
    std::ostringstream out, err;
    Application app(out, err);

    const char* argv[] = {"loglens", "analyze", "this_file_does_not_exist_at_all.log"};
    EXPECT_EQ(app.run(3, argv), 2);
    EXPECT_NE(err.str().find("Log file does not exist"), std::string::npos);
}

TEST_F(CliTest, EndToEndCliWorkflow) {
    // Write sample log file
    {
        std::ofstream ofs(test_log);
        ofs << "2026-08-25 10:00:01 INFO AUTH Login success\n"
            << "2026-08-25 10:01:00 ERROR DB Connection timeout\n"
            << "2026-08-25 10:02:00 ERROR DB Connection timeout\n"
            << "2026-08-25 10:03:00 ERROR DB Connection timeout\n"
            << "2026-08-25 10:07:00 INFO DB Service recovered\n";
    }

    std::ostringstream out, err;
    Application app(out, err);

    // 1. Analyze
    const char* argv_analyze[] = {"loglens", "analyze", test_log.c_str(), "--db", test_db.c_str()};
    EXPECT_EQ(app.run(5, argv_analyze), 0);
    EXPECT_NE(out.str().find("Detected Operational Incidents"), std::string::npos);

    // 2. Stats
    out.str(""); err.str("");
    const char* argv_stats[] = {"loglens", "stats", "--db", test_db.c_str()};
    EXPECT_EQ(app.run(4, argv_stats), 0);
    EXPECT_NE(out.str().find("Top Failing Service: DB"), std::string::npos);

    // 3. Filter
    out.str(""); err.str("");
    const char* argv_filter[] = {"loglens", "filter", "--service", "AUTH", "--db", test_db.c_str()};
    EXPECT_EQ(app.run(6, argv_filter), 0);
    EXPECT_NE(out.str().find("Login success"), std::string::npos);

    // 4. Incidents
    out.str(""); err.str("");
    const char* argv_inc[] = {"loglens", "incidents", "--db", test_db.c_str()};
    EXPECT_EQ(app.run(4, argv_inc), 0);
    EXPECT_NE(out.str().find("RESOLVED"), std::string::npos);

    // 5. DB Info
    out.str(""); err.str("");
    const char* argv_db[] = {"loglens", "db-info", "--db", test_db.c_str()};
    EXPECT_EQ(app.run(4, argv_db), 0);
    EXPECT_NE(out.str().find("Schema Version:    1"), std::string::npos);
}
