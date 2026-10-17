#include "loglens/RootCauseEngine.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace loglens {

namespace {

inline std::string to_lower_str(std::string_view sv) {
    std::string s;
    s.reserve(sv.size());
    for (char c : sv) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

inline std::string trim_str(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

} // namespace

RootCauseEngine::RootCauseEngine() {
    // Populate default priority rules
    add_rule({"RCA-MEM-01", 100, "out of memory", "Memory pressure: Host exhausted RAM or allocation limit reached"});
    add_rule({"RCA-MEM-02", 95, "allocation failure", "Memory pressure: Dynamic memory allocation failure"});
    add_rule({"RCA-SYS-01", 90, "segmentation fault", "Memory corruption or illegal pointer dereference"});
    add_rule({"RCA-SYS-02", 85, "access violation", "Unhandled memory fault or null dereference"});
    add_rule({"RCA-STO-01", 80, "disk full", "Storage exhaustion: File system has no remaining space"});
    add_rule({"RCA-STO-02", 75, "no space left on device", "Storage capacity issue: Volume limit reached"});
    add_rule({"RCA-NET-01", 70, "connection timeout", "Network latency or downstream host unreachable"});
    add_rule({"RCA-NET-02", 68, "connection refused", "Service port closed or process not listening"});
    add_rule({"RCA-NET-03", 65, "network unreachable", "Network route partition or network interface down"});
    add_rule({"RCA-SEC-01", 60, "authentication failed", "Security failure: Invalid credentials or expired token"});
    add_rule({"RCA-SEC-02", 55, "unauthorized", "Access control: Insufficient permission or invalid signature"});
    add_rule({"RCA-SVC-01", 50, "service unavailable", "Dependency outage: Upstream service returned 503"});
    add_rule({"RCA-SVC-02", 45, "bad gateway", "Gateway failure: Upstream reverse proxy returned 502"});
    add_rule({"RCA-SVC-03", 40, "gateway timeout", "Gateway failure: Upstream response timed out"});
}

void RootCauseEngine::add_rule(RootCauseRule rule) {
    rule.keyword = to_lower_str(rule.keyword);
    rules_.push_back(std::move(rule));
    sort_rules();
}

void RootCauseEngine::sort_rules() {
    std::sort(rules_.begin(), rules_.end());
}

void RootCauseEngine::load_rules_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open RCA rules file: " + path);
    }

    std::vector<RootCauseRule> loaded;
    std::string line;
    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string part;
        std::vector<std::string> tokens;
        while (std::getline(ss, part, '|')) {
            tokens.push_back(trim_str(part));
        }

        if (tokens.size() < 4) {
            throw std::invalid_argument("Malformed RCA rule line (expected 4 pipe-delimited fields): " + line);
        }

        RootCauseRule r;
        r.rule_id = tokens[0];
        r.priority = std::stoi(tokens[1]);
        r.keyword = to_lower_str(tokens[2]);
        r.diagnosis = tokens[3];

        if (r.rule_id.empty() || r.keyword.empty() || r.diagnosis.empty()) {
            throw std::invalid_argument("Empty field in RCA rule: " + line);
        }

        loaded.push_back(std::move(r));
    }

    rules_ = std::move(loaded);
    sort_rules();
}

RCAResult RootCauseEngine::diagnose_message(std::string_view message) const {
    std::string lower_msg = to_lower_str(message);
    for (const auto& rule : rules_) {
        if (lower_msg.find(rule.keyword) != std::string::npos) {
            return RCAResult{rule.rule_id, rule.diagnosis};
        }
    }
    return RCAResult{};
}

RCAResult RootCauseEngine::diagnose(const Incident& incident) const {
    // Check all messages associated with the incident against priority-ordered rules
    // Because rules_ is sorted descending by priority, the first rule that matches ANY message is the winner
    for (const auto& rule : rules_) {
        for (const auto& msg : incident.qualifying_messages) {
            std::string lower_msg = to_lower_str(msg);
            if (lower_msg.find(rule.keyword) != std::string::npos) {
                return RCAResult{rule.rule_id, rule.diagnosis};
            }
        }
    }
    return RCAResult{};
}

} // namespace loglens
