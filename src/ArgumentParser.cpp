#include "loglens/ArgumentParser.h"

#include <iostream>
#include <iomanip>

namespace loglens {

void ArgumentParser::print_version(std::ostream& os) {
    os << "LogLens version 1.0.0 (C++17, SQLite3)\n";
    os << "Built for deterministic log analysis, incident detection, RCA, and SLA tracking.\n";
}

void ArgumentParser::print_usage(std::ostream& os) {
    os << "Usage: loglens <command> [options]\n\n"
       << "Commands:\n"
       << "  analyze <file>         Parse log file, compute stats, detect incidents & persist to SQLite\n"
       << "                         Options: [--db PATH] [--strict] [--config-detection PATH] [--config-rca PATH]\n"
       << "  stats                  Display overall failure statistics and service rankings\n"
       << "                         Options: [--run-id ID] [--db PATH]\n"
       << "  filter                 Filter and display log lines matching criteria\n"
       << "                         Options: [--level LEVEL] [--service SERVICE] [--date YYYY-MM-DD] [--run-id ID] [--db PATH]\n"
       << "  incidents              List detected incidents and SLA compliance\n"
       << "                         Options: [--status OPEN|RESOLVED] [--run-id ID] [--db PATH]\n"
       << "  incident <ID>          Display deep dive details for a specific incident ID\n"
       << "                         Options: [--db PATH]\n"
       << "  report                 Generate comprehensive executive analysis report\n"
       << "                         Options: [--run-id ID] [--db PATH]\n"
       << "  db-info                Display SQLite database metadata and stored row counts\n"
       << "                         Options: [--db PATH]\n\n"
       << "Global Options:\n"
       << "  -h, --help             Show this help message and exit\n"
       << "  -v, --version          Show version information and exit\n"
       << "  --db PATH              Override default SQLite database path (default: loglens.db)\n\n"
       << "Examples:\n"
       << "  loglens analyze data/sample.log\n"
       << "  loglens stats\n"
       << "  loglens filter --level ERROR\n"
       << "  loglens filter --service DB\n"
       << "  loglens filter --date 2026-08-25\n"
       << "  loglens incidents --status RESOLVED\n"
       << "  loglens incident INC-001\n"
       << "  loglens report\n"
       << "  loglens db-info\n";
}

ParsedOptions ArgumentParser::parse(int argc, const char* const argv[]) {
    ParsedOptions opts;

    if (argc <= 1) {
        opts.show_help = true;
        opts.command = Command::HELP;
        return opts;
    }

    std::string first_arg = argv[1];
    if (first_arg == "-h" || first_arg == "--help" || first_arg == "help") {
        opts.show_help = true;
        opts.command = Command::HELP;
        return opts;
    }
    if (first_arg == "-v" || first_arg == "--version" || first_arg == "version") {
        opts.show_version = true;
        opts.command = Command::VERSION;
        return opts;
    }

    int idx = 1;
    if (first_arg == "analyze") {
        opts.command = Command::ANALYZE;
        ++idx;
        if (idx < argc && argv[idx][0] != '-') {
            opts.target_file = argv[idx++];
        } else {
            opts.error_message = "Command 'analyze' requires a log file path argument.";
            return opts;
        }
    } else if (first_arg == "stats") {
        opts.command = Command::STATS;
        ++idx;
    } else if (first_arg == "filter") {
        opts.command = Command::FILTER;
        ++idx;
    } else if (first_arg == "incidents") {
        opts.command = Command::INCIDENTS;
        ++idx;
    } else if (first_arg == "incident") {
        opts.command = Command::INCIDENT_DETAIL;
        ++idx;
        if (idx < argc && argv[idx][0] != '-') {
            opts.incident_id = argv[idx++];
        } else {
            opts.error_message = "Command 'incident' requires an incident ID argument.";
            return opts;
        }
    } else if (first_arg == "report") {
        opts.command = Command::REPORT;
        ++idx;
    } else if (first_arg == "db-info") {
        opts.command = Command::DB_INFO;
        ++idx;
    } else {
        opts.error_message = "Unknown command '" + first_arg + "'. Use --help for available commands.";
        return opts;
    }

    // Parse options flags
    while (idx < argc) {
        std::string arg = argv[idx++];

        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return opts;
        } else if (arg == "--db") {
            if (idx >= argc) {
                opts.error_message = "--db flag requires a path argument";
                return opts;
            }
            opts.db_path = argv[idx++];
        } else if (arg == "--run-id") {
            if (idx >= argc) {
                opts.error_message = "--run-id flag requires a run ID argument";
                return opts;
            }
            opts.run_id = argv[idx++];
        } else if (arg == "--config-detection") {
            if (idx >= argc) {
                opts.error_message = "--config-detection requires a path argument";
                return opts;
            }
            opts.config_detection = argv[idx++];
        } else if (arg == "--config-rca") {
            if (idx >= argc) {
                opts.error_message = "--config-rca requires a path argument";
                return opts;
            }
            opts.config_rca = argv[idx++];
        } else if (arg == "--strict") {
            opts.strict_mode = true;
        } else if (arg == "--level") {
            if (idx >= argc) {
                opts.error_message = "--level flag requires a severity (INFO, WARNING, ERROR, CRITICAL)";
                return opts;
            }
            std::string lev = argv[idx++];
            auto parsed = parse_severity(lev);
            if (!parsed) {
                opts.error_message = "Invalid severity level: '" + lev + "'";
                return opts;
            }
            opts.filter_level = *parsed;
        } else if (arg == "--service") {
            if (idx >= argc) {
                opts.error_message = "--service flag requires a service name";
                return opts;
            }
            opts.filter_service = argv[idx++];
        } else if (arg == "--date") {
            if (idx >= argc) {
                opts.error_message = "--date flag requires a date in YYYY-MM-DD format";
                return opts;
            }
            opts.filter_date = argv[idx++];
        } else if (arg == "--status") {
            if (idx >= argc) {
                opts.error_message = "--status flag requires OPEN or RESOLVED";
                return opts;
            }
            std::string st = argv[idx++];
            auto parsed = parse_incident_status(st);
            if (!parsed) {
                opts.error_message = "Invalid incident status: '" + st + "' (expected OPEN or RESOLVED)";
                return opts;
            }
            opts.filter_status = *parsed;
        } else {
            opts.error_message = "Unrecognized option '" + arg + "'";
            return opts;
        }
    }

    // Validation for filter command: at least one filter criterion required
    if (opts.command == Command::FILTER && !opts.filter_level && !opts.filter_service && !opts.filter_date) {
        opts.error_message = "Command 'filter' requires at least one of --level, --service, or --date";
        return opts;
    }

    return opts;
}

} // namespace loglens
