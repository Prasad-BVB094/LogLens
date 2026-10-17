#pragma once

#include "loglens/Incident.h"
#include "loglens/LogEntry.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>

namespace loglens {

struct DetectionConfig {
    std::int64_t window_seconds{300}; // 5 minutes
    std::size_t threshold_count{3};   // 3 errors
    std::unordered_set<Severity> qualifying_levels{Severity::ERROR, Severity::CRITICAL};
    std::string recovery_pattern{"Service recovered"};

    static DetectionConfig load_from_file(const std::string& path);
};

class IncidentDetector {
public:
    explicit IncidentDetector(DetectionConfig config = DetectionConfig{}) noexcept
        : config_(std::move(config)) {}

    [[nodiscard]] const DetectionConfig& config() const noexcept { return config_; }
    void set_config(DetectionConfig config) noexcept { config_ = std::move(config); }

    // Detects incidents across all entries for a specific analysis run
    [[nodiscard]] std::vector<Incident> detect_incidents(
        const std::vector<LogEntry>& entries,
        const std::string& run_id) const;

private:
    DetectionConfig config_{};
};

} // namespace loglens
