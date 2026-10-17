#pragma once

#include "loglens/Incident.h"
#include "loglens/Severity.h"
#include "loglens/Timestamp.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace loglens {

struct SLATargets {
    std::int64_t critical_seconds{300};  // 5 minutes
    std::int64_t error_seconds{900};     // 15 minutes
    std::int64_t warning_seconds{1800};  // 30 minutes

    [[nodiscard]] std::int64_t get_allowed_seconds(Severity severity) const noexcept {
        switch (severity) {
            case Severity::CRITICAL: return critical_seconds;
            case Severity::ERROR: return error_seconds;
            case Severity::WARNING: return warning_seconds;
            case Severity::INFO: return warning_seconds;
        }
        return error_seconds;
    }
};

class SLAAnalyzer {
public:
    explicit SLAAnalyzer(SLATargets targets = SLATargets{}) noexcept
        : targets_(targets) {}

    [[nodiscard]] const SLATargets& targets() const noexcept { return targets_; }
    void set_targets(SLATargets targets) noexcept { targets_ = targets; }

    // Evaluates SLA compliance for an individual incident
    void evaluate(Incident& incident, std::optional<Timestamp> reference_time = std::nullopt) const;

    // Evaluates SLA compliance across all incidents
    void evaluate_all(std::vector<Incident>& incidents, std::optional<Timestamp> reference_time = std::nullopt) const;

private:
    SLATargets targets_{};
};

} // namespace loglens
