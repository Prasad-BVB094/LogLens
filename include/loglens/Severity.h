#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <ostream>

namespace loglens {

enum class Severity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

inline std::string_view to_string(Severity s) noexcept {
    switch (s) {
        case Severity::INFO: return "INFO";
        case Severity::WARNING: return "WARNING";
        case Severity::ERROR: return "ERROR";
        case Severity::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

inline std::optional<Severity> parse_severity(std::string_view sv) noexcept {
    if (sv == "INFO" || sv == "info") return Severity::INFO;
    if (sv == "WARN" || sv == "warn" || sv == "WARNING" || sv == "warning") return Severity::WARNING;
    if (sv == "ERROR" || sv == "error") return Severity::ERROR;
    if (sv == "CRITICAL" || sv == "critical") return Severity::CRITICAL;
    return std::nullopt;
}

inline bool is_higher_severity(Severity a, Severity b) noexcept {
    return static_cast<int>(a) > static_cast<int>(b);
}

inline std::ostream& operator<<(std::ostream& os, Severity s) {
    return os << to_string(s);
}

} // namespace loglens
