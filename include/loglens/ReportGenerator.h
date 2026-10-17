#pragma once

#include "loglens/DatabaseManager.h"
#include "loglens/Incident.h"
#include "loglens/LogEntry.h"
#include "loglens/StatisticsAnalyzer.h"

#include <ostream>
#include <vector>

namespace loglens {

class ReportGenerator {
public:
    static void print_header(std::ostream& os, const std::string& title);
    static void print_summary(std::ostream& os, const AnalysisRun& run, const StatisticsSummary& stats);
    static void print_incidents(std::ostream& os, const std::vector<Incident>& incidents);
    static void print_incident_detail(std::ostream& os, const Incident& incident);
    static void print_filtered_logs(std::ostream& os, const std::vector<LogEntry>& entries);
    static void print_db_info(std::ostream& os, const DatabaseInfo& info);
    static void print_executive_report(
        std::ostream& os,
        const AnalysisRun& run,
        const StatisticsSummary& stats,
        const std::vector<Incident>& incidents);
};

} // namespace loglens
