#pragma once

#include "loglens/Incident.h"

#include <string>
#include <vector>
#include <cstdint>

namespace loglens {

struct RootCauseRule {
    std::string rule_id{};
    int priority{0}; // Higher priority is evaluated first
    std::string keyword{};
    std::string diagnosis{};

    [[nodiscard]] bool operator<(const RootCauseRule& other) const noexcept {
        if (priority != other.priority) {
            return priority > other.priority; // descending priority
        }
        return rule_id < other.rule_id;
    }
};

struct RCAResult {
    std::string rule_id{"RCA-UNK-00"};
    std::string probable_root_cause{"Unknown cause: pattern unclassified"};
};

class RootCauseEngine {
public:
    RootCauseEngine();

    void add_rule(RootCauseRule rule);
    void clear_rules() noexcept { rules_.clear(); }
    void load_rules_from_file(const std::string& path);

    [[nodiscard]] const std::vector<RootCauseRule>& rules() const noexcept { return rules_; }

    // Evaluates an incident against the rules and assigns root cause
    [[nodiscard]] RCAResult diagnose(const Incident& incident) const;

    // Diagnose from a single message
    [[nodiscard]] RCAResult diagnose_message(std::string_view message) const;

private:
    std::vector<RootCauseRule> rules_{};
    void sort_rules();
};

} // namespace loglens
