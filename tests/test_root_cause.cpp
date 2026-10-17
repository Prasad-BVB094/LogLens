#include "loglens/RootCauseEngine.h"
#include <gtest/gtest.h>

using namespace loglens;

TEST(RootCauseEngineTest, DefaultRulesDiagnosis) {
    RootCauseEngine engine;

    EXPECT_EQ(engine.diagnose_message("Process killed: Out of memory in worker thread").rule_id, "RCA-MEM-01");
    EXPECT_EQ(engine.diagnose_message("Fatal signal: segmentation fault at 0x00").rule_id, "RCA-SYS-01");
    EXPECT_EQ(engine.diagnose_message("Write error: disk full on volume /data").rule_id, "RCA-STO-01");
    EXPECT_EQ(engine.diagnose_message("Network exception: connection timeout to db-replica").rule_id, "RCA-NET-01");
    EXPECT_EQ(engine.diagnose_message("Access denied: authentication failed for user").rule_id, "RCA-SEC-01");
    EXPECT_EQ(engine.diagnose_message("HTTP 503: service unavailable").rule_id, "RCA-SVC-01");
}

TEST(RootCauseEngineTest, CaseInsensitiveMatching) {
    RootCauseEngine engine;

    auto res1 = engine.diagnose_message("FATAL: OUT OF MEMORY IN BUFFER");
    EXPECT_EQ(res1.rule_id, "RCA-MEM-01");

    auto res2 = engine.diagnose_message("ERROR: DiSk FuLl CaNnOt WrItE");
    EXPECT_EQ(res2.rule_id, "RCA-STO-01");

    auto res3 = engine.diagnose_message("WARNING: CoNnEcTiOn ReFuSeD bY pEeR");
    EXPECT_EQ(res3.rule_id, "RCA-NET-02");
}

TEST(RootCauseEngineTest, UnknownMessageFallback) {
    RootCauseEngine engine;
    auto res = engine.diagnose_message("Something totally unknown and unclassified happened here");
    EXPECT_EQ(res.rule_id, "RCA-UNK-00");
    EXPECT_EQ(res.probable_root_cause, "Unknown cause: pattern unclassified");
}

TEST(RootCauseEngineTest, PriorityConflictResolution) {
    RootCauseEngine engine;
    Incident inc;
    // Incident contains both a connection timeout symptom (priority 70)
    // and an out of memory symptom (priority 100).
    // The higher priority root cause (out of memory) must win!
    inc.qualifying_messages = {
        "Service unavailable upstream",
        "Connection timeout to backend",
        "Worker thread failed with out of memory error"
    };

    auto res = engine.diagnose(inc);
    EXPECT_EQ(res.rule_id, "RCA-MEM-01");
    EXPECT_NE(res.rule_id, "RCA-NET-01");
}

TEST(RootCauseEngineTest, InvalidConfigFileHandling) {
    RootCauseEngine engine;
    EXPECT_THROW(engine.load_rules_from_file("missing_rules.conf"), std::runtime_error);
}
