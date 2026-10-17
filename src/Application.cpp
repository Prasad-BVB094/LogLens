#include "loglens/Application.h"
#include "loglens/DatabaseManager.h"
#include "loglens/IncidentDetector.h"
#include "loglens/LogParser.h"
#include "loglens/ReportGenerator.h"
#include "loglens/RootCauseEngine.h"
#include "loglens/SLAAnalyzer.h"
#include "loglens/StatisticsAnalyzer.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace loglens {

namespace {

std::string generate_run_id() {
    auto now = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::ostringstream ss;
    ss << "RUN-" << epoch_ms;
    return ss.str();
}

} // namespace

int Application::run(int argc, const char* const argv[]) {
    try {
        ParsedOptions opts = ArgumentParser::parse(argc, argv);

        if (opts.show_help) {
            ArgumentParser::print_usage(out_);
            return 0;
        }

        if (opts.show_version) {
            ArgumentParser::print_version(out_);
            return 0;
        }

        if (!opts.error_message.empty()) {
            err_ << "Error: " << opts.error_message << "\n\n";
            ArgumentParser::print_usage(err_);
            return 1;
        }

        switch (opts.command) {
            case Command::ANALYZE:
                return handle_analyze(opts);
            case Command::STATS:
                return handle_stats(opts);
            case Command::FILTER:
                return handle_filter(opts);
            case Command::INCIDENTS:
                return handle_incidents(opts);
            case Command::INCIDENT_DETAIL:
                return handle_incident_detail(opts);
            case Command::REPORT:
                return handle_report(opts);
            case Command::DB_INFO:
                return handle_db_info(opts);
            case Command::HELP:
            case Command::VERSION:
            case Command::UNKNOWN:
                ArgumentParser::print_usage(err_);
                return 1;
        }
    } catch (const std::exception& ex) {
        err_ << "Fatal Error: " << ex.what() << "\n";
        return 2;
    }
    return 0;
}

int Application::handle_analyze(const ParsedOptions& opts) {
    if (!std::filesystem::exists(opts.target_file)) {
        err_ << "Error: Log file does not exist: '" << opts.target_file << "'\n";
        return 2;
    }

    LogParser parser(opts.strict_mode ? ParserMode::Strict : ParserMode::Tolerant);
    ParseResult parse_result;
    try {
        parse_result = parser.parse_file(opts.target_file);
    } catch (const std::exception& ex) {
        err_ << "Parse failed: " << ex.what() << "\n";
        return 2;
    }

    if (parse_result.total_lines == 0) {
        err_ << "Warning: File is empty.\n";
    }

    // Aggregates statistics
    StatisticsAnalyzer stats_analyzer;
    StatisticsSummary summary = stats_analyzer.analyze(parse_result.entries);

    // Incident detection
    IncidentDetector detector;
    if (!opts.config_detection.empty()) {
        try {
            detector.set_config(DetectionConfig::load_from_file(opts.config_detection));
        } catch (const std::exception& ex) {
            err_ << "Detection config error: " << ex.what() << "\n";
            return 1;
        }
    }

    std::string run_id = generate_run_id();
    std::vector<Incident> incidents = detector.detect_incidents(parse_result.entries, run_id);

    // Root Cause Analysis
    RootCauseEngine rca_engine;
    if (!opts.config_rca.empty()) {
        try {
            rca_engine.load_rules_from_file(opts.config_rca);
        } catch (const std::exception& ex) {
            err_ << "RCA config error: " << ex.what() << "\n";
            return 1;
        }
    }

    for (auto& inc : incidents) {
        auto rca = rca_engine.diagnose(inc);
        inc.probable_root_cause = rca.probable_root_cause;
        inc.rca_rule_id = rca.rule_id;
    }

    // SLA Evaluation (evaluate open incidents against the final entry timestamp if available)
    SLAAnalyzer sla_analyzer;
    std::optional<Timestamp> ref_time = std::nullopt;
    if (!parse_result.entries.empty()) {
        ref_time = parse_result.entries.back().timestamp;
    }
    sla_analyzer.evaluate_all(incidents, ref_time);

    // Persistence
    DatabaseManager db(opts.db_path);
    db.initialize();

    AnalysisRun run;
    run.run_id = run_id;
    run.file_path = opts.target_file;
    run.total_lines = parse_result.total_lines;
    run.valid_records = parse_result.valid_records;
    run.malformed_records = parse_result.malformed_records;
    if (!parse_result.entries.empty()) {
        run.start_timestamp = parse_result.entries.front().timestamp.to_string();
        run.end_timestamp = parse_result.entries.back().timestamp.to_string();
    }

    db.save_analysis_run(run, parse_result.entries, incidents);

    // Output Executive Report
    ReportGenerator::print_executive_report(out_, run, summary, incidents);
    return 0;
}

int Application::handle_stats(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    std::string run_id = opts.run_id;
    if (run_id.empty()) {
        auto latest = db.get_latest_run_id();
        if (!latest) {
            err_ << "Error: No analysis runs found in database. Run 'loglens analyze <file>' first.\n";
            return 2;
        }
        run_id = *latest;
    }

    auto run_opt = db.get_run(run_id);
    if (!run_opt) {
        err_ << "Error: Run ID '" << run_id << "' not found.\n";
        return 1;
    }

    auto logs = db.query_logs(run_id);
    StatisticsAnalyzer analyzer;
    auto summary = analyzer.analyze(logs);

    ReportGenerator::print_summary(out_, *run_opt, summary);
    return 0;
}

int Application::handle_filter(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    std::string run_id = opts.run_id;
    if (run_id.empty()) {
        auto latest = db.get_latest_run_id();
        if (!latest) {
            err_ << "Error: No analysis runs found in database.\n";
            return 2;
        }
        run_id = *latest;
    }

    auto logs = db.query_logs(run_id, opts.filter_level, opts.filter_service, opts.filter_date);
    ReportGenerator::print_filtered_logs(out_, logs);
    return 0;
}

int Application::handle_incidents(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    std::string run_id = opts.run_id;
    if (run_id.empty()) {
        auto latest = db.get_latest_run_id();
        if (!latest) {
            err_ << "Error: No analysis runs found in database.\n";
            return 2;
        }
        run_id = *latest;
    }

    auto incidents = db.query_incidents(run_id, opts.filter_status);
    ReportGenerator::print_incidents(out_, incidents);
    return 0;
}

int Application::handle_incident_detail(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    auto inc = db.get_incident_by_id(opts.incident_id);
    if (!inc) {
        err_ << "Error: Incident '" << opts.incident_id << "' not found.\n";
        return 1;
    }

    if (inc->qualifying_messages.empty()) {
        auto logs = db.query_logs(inc->run_id, std::nullopt, inc->service, std::nullopt);
        for (const auto& log : logs) {
            if ((log.severity == Severity::ERROR || log.severity == Severity::CRITICAL) &&
                log.timestamp >= inc->start_time && log.timestamp <= inc->last_error_time) {
                inc->qualifying_messages.push_back(log.message);
            }
        }
    }

    ReportGenerator::print_incident_detail(out_, *inc);
    return 0;
}

int Application::handle_report(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    std::string run_id = opts.run_id;
    if (run_id.empty()) {
        auto latest = db.get_latest_run_id();
        if (!latest) {
            err_ << "Error: No analysis runs found in database.\n";
            return 2;
        }
        run_id = *latest;
    }

    auto run_opt = db.get_run(run_id);
    if (!run_opt) {
        err_ << "Error: Run ID '" << run_id << "' not found.\n";
        return 1;
    }

    auto logs = db.query_logs(run_id);
    StatisticsAnalyzer analyzer;
    auto summary = analyzer.analyze(logs);

    auto incidents = db.query_incidents(run_id);

    ReportGenerator::print_executive_report(out_, *run_opt, summary, incidents);
    return 0;
}

int Application::handle_db_info(const ParsedOptions& opts) {
    DatabaseManager db(opts.db_path);
    db.initialize();

    auto info = db.get_db_info();
    ReportGenerator::print_db_info(out_, info);
    return 0;
}

} // namespace loglens
