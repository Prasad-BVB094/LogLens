#pragma once

#include "loglens/Severity.h"
#include "loglens/Timestamp.h"

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace loglens {

enum class IncidentStatus {
    OPEN,
    RESOLVED
};

inline std::string_view to_string(IncidentStatus s) noexcept {
    switch (s) {
        case IncidentStatus::OPEN: return "OPEN";
        case IncidentStatus::RESOLVED: return "RESOLVED";
    }
    return "UNKNOWN";
}

inline std::optional<IncidentStatus> parse_incident_status(std::string_view sv) noexcept {
    if (sv == "OPEN" || sv == "open") return IncidentStatus::OPEN;
    if (sv == "RESOLVED" || sv == "resolved") return IncidentStatus::RESOLVED;
    return std::nullopt;
}

enum class SLAStatus {
    PENDING,
    MET,
    BREACHED
};

inline std::string_view to_string(SLAStatus s) noexcept {
    switch (s) {
        case SLAStatus::PENDING: return "PENDING";
        case SLAStatus::MET: return "MET";
        case SLAStatus::BREACHED: return "BREACHED";
    }
    return "UNKNOWN";
}

inline std::optional<SLAStatus> parse_sla_status(std::string_view sv) noexcept {
    if (sv == "PENDING" || sv == "pending") return SLAStatus::PENDING;
    if (sv == "MET" || sv == "met") return SLAStatus::MET;
    if (sv == "BREACHED" || sv == "breached") return SLAStatus::BREACHED;
    return std::nullopt;
}

struct Incident {
    std::string incident_id{};
    std::string run_id{};
    std::string service{};
    Severity severity{Severity::ERROR};

    Timestamp start_time{};
    Timestamp last_error_time{};
    std::optional<Timestamp> resolution_time{std::nullopt};

    std::size_t error_count{0};
    IncidentStatus status{IncidentStatus::OPEN};

    std::string probable_root_cause{"Unknown cause"};
    std::string rca_rule_id{"RCA-UNK-00"};

    SLAStatus sla_status{SLAStatus::PENDING};
    std::int64_t allowed_duration_sec{900};
    std::optional<std::int64_t> actual_duration_sec{std::nullopt};

    std::vector<std::string> qualifying_messages{};
};

} // namespace loglens
