#pragma once

#include "loglens/LogEntry.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>
#include <utility>

namespace loglens {

struct ServiceStats {
    std::string service{};
    std::size_t total_count{0};
    std::size_t error_count{0};
    std::size_t critical_count{0};

    [[nodiscard]] std::size_t failure_count() const noexcept {
        return error_count + critical_count;
    }
};

struct StatisticsSummary {
    std::size_t total_records{0};
    std::size_t info_count{0};
    std::size_t warning_count{0};
    std::size_t error_count{0};
    std::size_t critical_count{0};

    std::vector<ServiceStats> service_breakdown{};
    std::optional<std::string> top_failing_service{};
    std::size_t top_failing_count{0};

    std::optional<std::string> top_error_message{};
    std::size_t top_error_message_count{0};
};

class StatisticsAnalyzer {
public:
    StatisticsAnalyzer() = default;

    // Aggregates statistics across all provided entries
    [[nodiscard]] StatisticsSummary analyze(const std::vector<LogEntry>& entries) const;

    // Normalization policy for error messages: replaces digits and dynamic assignments (e.g. user=1024, ip addresses)
    [[nodiscard]] static std::string normalize_message(std::string_view message);

    // Filtering utilities
    [[nodiscard]] static std::vector<LogEntry> filter_by_severity(
        const std::vector<LogEntry>& entries, Severity level);

    [[nodiscard]] static std::vector<LogEntry> filter_by_service(
        const std::vector<LogEntry>& entries, std::string_view service);

    [[nodiscard]] static std::vector<LogEntry> filter_by_date(
        const std::vector<LogEntry>& entries, std::string_view date_yyyy_mm_dd);
};

} // namespace loglens
