#pragma once

#include "loglens/Incident.h"
#include "loglens/Severity.h"

#include <string>
#include <vector>
#include <optional>

namespace loglens {

enum class Command {
    HELP,
    VERSION,
    ANALYZE,
    STATS,
    FILTER,
    INCIDENTS,
    INCIDENT_DETAIL,
    REPORT,
    DB_INFO,
    UNKNOWN
};

struct ParsedOptions {
    Command command{Command::UNKNOWN};
    bool show_help{false};
    bool show_version{false};

    // Subcommand arguments
    std::string target_file{};
    std::string incident_id{};

    // Flags & Filters
    std::string db_path{"loglens.db"};
    std::string config_detection{};
    std::string config_rca{};
    std::string run_id{};
    bool strict_mode{false};

    std::optional<Severity> filter_level{std::nullopt};
    std::optional<std::string> filter_service{std::nullopt};
    std::optional<std::string> filter_date{std::nullopt};
    std::optional<IncidentStatus> filter_status{std::nullopt};

    std::string error_message{};
};

class ArgumentParser {
public:
    static ParsedOptions parse(int argc, const char* const argv[]);
    static void print_usage(std::ostream& os);
    static void print_version(std::ostream& os);
};

} // namespace loglens
