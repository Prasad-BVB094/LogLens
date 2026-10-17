#pragma once

#include "loglens/Severity.h"
#include "loglens/Timestamp.h"

#include <string>
#include <utility>

namespace loglens {

struct LogEntry {
    std::size_t line_number{0};
    Timestamp timestamp{};
    Severity severity{Severity::INFO};
    std::string service{};
    std::string message{};

    LogEntry() = default;
    LogEntry(std::size_t line_no, Timestamp ts, Severity sev, std::string svc, std::string msg)
        : line_number(line_no),
          timestamp(ts),
          severity(sev),
          service(std::move(svc)),
          message(std::move(msg)) {}

    [[nodiscard]] bool operator<(const LogEntry& other) const noexcept {
        if (timestamp != other.timestamp) {
            return timestamp < other.timestamp;
        }
        return line_number < other.line_number;
    }
};

} // namespace loglens
