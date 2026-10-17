#include "loglens/ReportGenerator.h"

#include <iomanip>
#include <iostream>

namespace loglens {

void ReportGenerator::print_header(std::ostream& os, const std::string& title) {
    os << "======================================================================\n";
    os << "  " << title << "\n";
    os << "======================================================================\n";
}

void ReportGenerator::print_summary(std::ostream& os, const AnalysisRun& run, const StatisticsSummary& stats) {
    print_header(os, "LogLens System Failure & Telemetry Statistics");
    os << " Run ID:            " << run.run_id << "\n";
    os << " Ingested File:     " << run.file_path << "\n";
    os << " Total Lines:       " << run.total_lines << "\n";
    os << " Valid Records:     " << run.valid_records << "\n";
    os << " Malformed Records: " << run.malformed_records << "\n";
    os << " Time Window:       " << (run.start_timestamp.empty() ? "N/A" : run.start_timestamp)
       << " -> " << (run.end_timestamp.empty() ? "N/A" : run.end_timestamp) << "\n";
    os << "----------------------------------------------------------------------\n";
    os << " Severity Distribution:\n";
    os << "   INFO:     " << std::setw(8) << stats.info_count << "\n";
    os << "   WARNING:  " << std::setw(8) << stats.warning_count << "\n";
    os << "   ERROR:    " << std::setw(8) << stats.error_count << "\n";
    os << "   CRITICAL: " << std::setw(8) << stats.critical_count << "\n";
    os << "----------------------------------------------------------------------\n";
    os << " Service Failure Ranking:\n";
    if (stats.service_breakdown.empty()) {
        os << "   (No service records)\n";
    } else {
        os << "   " << std::left << std::setw(18) << "Service"
           << std::right << std::setw(8) << "Total"
           << std::setw(8) << "Errors"
           << std::setw(10) << "Critical"
           << std::setw(10) << "Failures\n";
        for (const auto& s : stats.service_breakdown) {
            os << "   " << std::left << std::setw(18) << s.service
               << std::right << std::setw(8) << s.total_count
               << std::setw(8) << s.error_count
               << std::setw(10) << s.critical_count
               << std::setw(10) << s.failure_count() << "\n";
        }
    }
    os << "----------------------------------------------------------------------\n";
    os << " Top Failing Service: " << (stats.top_failing_service ? *stats.top_failing_service : "None")
       << " (" << stats.top_failing_count << " failure events)\n";
    os << " Top Error Template:  " << (stats.top_error_message ? *stats.top_error_message : "None")
       << " (occurred " << stats.top_error_message_count << " times)\n";
    os << "======================================================================\n";
}

void ReportGenerator::print_incidents(std::ostream& os, const std::vector<Incident>& incidents) {
    print_header(os, "Detected Operational Incidents & SLA Compliance");
    if (incidents.empty()) {
        os << " No qualifying incidents detected for this analysis run.\n";
        os << "======================================================================\n";
        return;
    }

    os << " " << std::left << std::setw(28) << "Incident ID"
       << std::setw(12) << "Service"
       << std::setw(12) << "Severity"
       << std::setw(12) << "Status"
       << std::setw(12) << "SLA"
       << std::setw(10) << "Errors"
       << "Probable Root Cause\n";
    os << "------------------------------------------------------------------------------------------------------\n";

    for (const auto& inc : incidents) {
        os << " " << std::left << std::setw(28) << inc.incident_id
           << std::setw(12) << inc.service
           << std::setw(12) << to_string(inc.severity)
           << std::setw(12) << to_string(inc.status)
           << std::setw(12) << to_string(inc.sla_status)
           << std::setw(10) << inc.error_count
           << inc.probable_root_cause << " [" << inc.rca_rule_id << "]\n";
    }
    os << "======================================================================================================\n";
}

void ReportGenerator::print_incident_detail(std::ostream& os, const Incident& incident) {
    print_header(os, "Incident Deep Dive: " + incident.incident_id);
    os << " Incident ID:          " << incident.incident_id << "\n";
    os << " Associated Run ID:    " << incident.run_id << "\n";
    os << " Target Service:       " << incident.service << "\n";
    os << " Severity Level:       " << to_string(incident.severity) << "\n";
    os << " Incident Status:      " << to_string(incident.status) << "\n";
    os << " Error Count:          " << incident.error_count << "\n";
    os << " Outage Started:       " << incident.start_time.to_string() << "\n";
    os << " Last Active Error:    " << incident.last_error_time.to_string() << "\n";
    os << " Resolution Time:      " << (incident.resolution_time ? incident.resolution_time->to_string() : "Pending (Unresolved)") << "\n";
    os << " Duration:             " << (incident.actual_duration_sec ? (std::to_string(*incident.actual_duration_sec) + " seconds") : "Ongoing") << "\n";
    os << " Allowed SLA:          " << incident.allowed_duration_sec << " seconds\n";
    os << " SLA Evaluation:       " << to_string(incident.sla_status) << "\n";
    os << " Probable Root Cause:  " << incident.probable_root_cause << "\n";
    os << " Rule Triggered:       " << incident.rca_rule_id << "\n";
    os << "----------------------------------------------------------------------\n";
    os << " Triggering Error Signals (" << incident.qualifying_messages.size() << " samples):\n";
    for (std::size_t i = 0; i < incident.qualifying_messages.size(); ++i) {
        os << "   [" << (i + 1) << "] " << incident.qualifying_messages[i] << "\n";
    }
    os << "======================================================================\n";
}

void ReportGenerator::print_filtered_logs(std::ostream& os, const std::vector<LogEntry>& entries) {
    print_header(os, "Filtered Log Stream (" + std::to_string(entries.size()) + " records)");
    for (const auto& e : entries) {
        os << e.timestamp.to_string() << " "
           << std::left << std::setw(8) << to_string(e.severity) << " "
           << std::setw(12) << e.service << " "
           << e.message << "\n";
    }
    os << "======================================================================\n";
}

void ReportGenerator::print_db_info(std::ostream& os, const DatabaseInfo& info) {
    print_header(os, "LogLens SQLite Database Metadata");
    os << " Database File:     " << info.database_path << "\n";
    os << " File Size:         " << (info.file_size_bytes / 1024) << " KB (" << info.file_size_bytes << " bytes)\n";
    os << " Schema Version:    " << info.schema_version << "\n";
    os << " Stored Runs:       " << info.total_runs << "\n";
    os << " Stored Log Lines:  " << info.total_logs << "\n";
    os << " Stored Incidents:  " << info.total_incidents << "\n";
    os << "======================================================================\n";
}

void ReportGenerator::print_executive_report(
    std::ostream& os,
    const AnalysisRun& run,
    const StatisticsSummary& stats,
    const std::vector<Incident>& incidents) {

    print_summary(os, run, stats);
    os << "\n";
    print_incidents(os, incidents);
}

} // namespace loglens
