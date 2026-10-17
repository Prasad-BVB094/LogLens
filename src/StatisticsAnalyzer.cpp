#include "loglens/StatisticsAnalyzer.h"

#include <algorithm>
#include <cctype>

namespace loglens {

std::string StatisticsAnalyzer::normalize_message(std::string_view message) {
    // Replaces dynamic numeric tokens and parameterized values to find recurring template patterns
    std::string result;
    result.reserve(message.size());

    bool in_digits = false;
    for (std::size_t i = 0; i < message.size(); ++i) {
        char c = message[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            if (!in_digits) {
                result += "<N>";
                in_digits = true;
            }
        } else {
            in_digits = false;
            result += c;
        }
    }
    return result;
}

StatisticsSummary StatisticsAnalyzer::analyze(const std::vector<LogEntry>& entries) const {
    StatisticsSummary summary;
    summary.total_records = entries.size();

    std::unordered_map<std::string, ServiceStats> svc_map;
    std::unordered_map<std::string, std::size_t> error_msg_map;

    for (const auto& entry : entries) {
        switch (entry.severity) {
            case Severity::INFO:
                ++summary.info_count;
                break;
            case Severity::WARNING:
                ++summary.warning_count;
                break;
            case Severity::ERROR:
                ++summary.error_count;
                break;
            case Severity::CRITICAL:
                ++summary.critical_count;
                break;
        }

        auto& s_stat = svc_map[entry.service];
        s_stat.service = entry.service;
        ++s_stat.total_count;

        if (entry.severity == Severity::ERROR) {
            ++s_stat.error_count;
        } else if (entry.severity == Severity::CRITICAL) {
            ++s_stat.critical_count;
        }

        if (entry.severity == Severity::ERROR || entry.severity == Severity::CRITICAL) {
            std::string norm = normalize_message(entry.message);
            ++error_msg_map[norm];
        }
    }

    // Convert map to vector and sort with deterministic tie-breaking
    summary.service_breakdown.reserve(svc_map.size());
    for (auto& pair : svc_map) {
        summary.service_breakdown.push_back(std::move(pair.second));
    }

    // Sort service breakdown: highest failure count first, tie-break by name
    std::sort(summary.service_breakdown.begin(), summary.service_breakdown.end(),
        [](const ServiceStats& a, const ServiceStats& b) {
            if (a.failure_count() != b.failure_count()) {
                return a.failure_count() > b.failure_count();
            }
            if (a.total_count != b.total_count) {
                return a.total_count > b.total_count;
            }
            return a.service < b.service;
        });

    if (!summary.service_breakdown.empty() && summary.service_breakdown.front().failure_count() > 0) {
        summary.top_failing_service = summary.service_breakdown.front().service;
        summary.top_failing_count = summary.service_breakdown.front().failure_count();
    }

    // Determine top recurring error template
    if (!error_msg_map.empty()) {
        std::vector<std::pair<std::string, std::size_t>> error_list(error_msg_map.begin(), error_msg_map.end());
        std::sort(error_list.begin(), error_list.end(),
            [](const auto& a, const auto& b) {
                if (a.second != b.second) {
                    return a.second > b.second;
                }
                return a.first < b.first;
            });
        summary.top_error_message = error_list.front().first;
        summary.top_error_message_count = error_list.front().second;
    }

    return summary;
}

std::vector<LogEntry> StatisticsAnalyzer::filter_by_severity(
    const std::vector<LogEntry>& entries, Severity level) {
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries) {
        if (entry.severity == level) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<LogEntry> StatisticsAnalyzer::filter_by_service(
    const std::vector<LogEntry>& entries, std::string_view service) {
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries) {
        if (entry.service == service) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<LogEntry> StatisticsAnalyzer::filter_by_date(
    const std::vector<LogEntry>& entries, std::string_view date_yyyy_mm_dd) {
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries) {
        if (entry.timestamp.date_only() == date_yyyy_mm_dd) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

} // namespace loglens
