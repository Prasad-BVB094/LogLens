#include "loglens/IncidentDetector.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include <stdexcept>

namespace loglens {

namespace {

inline std::string trim_str(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

} // namespace

DetectionConfig DetectionConfig::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open detection config file: " + path);
    }

    DetectionConfig conf;
    std::string line;
    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq_pos));
        std::string val = trim_str(line.substr(eq_pos + 1));

        if (key == "window_seconds") {
            auto sec = std::stoll(val);
            if (sec <= 0) throw std::invalid_argument("window_seconds must be positive");
            conf.window_seconds = sec;
        } else if (key == "threshold_count") {
            auto count = std::stoul(val);
            if (count < 1) throw std::invalid_argument("threshold_count must be >= 1");
            conf.threshold_count = count;
        } else if (key == "recovery_pattern") {
            if (val.empty()) throw std::invalid_argument("recovery_pattern cannot be empty");
            conf.recovery_pattern = val;
        } else if (key == "qualifying_levels") {
            conf.qualifying_levels.clear();
            std::stringstream ss(val);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                tok = trim_str(tok);
                auto sev = parse_severity(tok);
                if (sev) conf.qualifying_levels.insert(*sev);
            }
            if (conf.qualifying_levels.empty()) {
                throw std::invalid_argument("qualifying_levels cannot be empty");
            }
        }
    }
    return conf;
}

std::vector<Incident> IncidentDetector::detect_incidents(
    const std::vector<LogEntry>& entries,
    const std::string& run_id) const {

    // Group events by service: qualifying error events and recovery events
    std::map<std::string, std::vector<LogEntry>> service_errors;
    std::map<std::string, std::vector<LogEntry>> service_recoveries;

    for (const auto& entry : entries) {
        if (config_.qualifying_levels.find(entry.severity) != config_.qualifying_levels.end()) {
            service_errors[entry.service].push_back(entry);
        } else if (!config_.recovery_pattern.empty() &&
                   entry.message.find(config_.recovery_pattern) != std::string::npos) {
            service_recoveries[entry.service].push_back(entry);
        }
    }

    std::vector<Incident> incidents;
    std::size_t incident_counter = 1;

    // Process each service independently in deterministic alphabetical order
    for (auto& [service, errors] : service_errors) {
        // Sort errors chronologically (handles out-of-order logs)
        std::sort(errors.begin(), errors.end());

        const auto& recoveries = service_recoveries[service];

        std::size_t n = errors.size();
        if (n < config_.threshold_count) {
            continue;
        }

        std::size_t i = 0;
        while (i + config_.threshold_count <= n) {
            // Check if sliding window starting at i contains threshold_count errors within window_seconds
            const auto& first_event = errors[i];
            const auto& threshold_event = errors[i + config_.threshold_count - 1];

            // Condition: threshold_event.timestamp - first_event.timestamp <= window_seconds
            if (threshold_event.timestamp - first_event.timestamp <= config_.window_seconds) {
                // Incident triggered!
                Incident incident;
                incident.run_id = run_id;
                incident.service = service;
                incident.start_time = first_event.timestamp;
                incident.last_error_time = threshold_event.timestamp;
                incident.severity = Severity::ERROR;
                incident.status = IncidentStatus::OPEN;

                // Collect the initial window events
                for (std::size_t k = i; k < i + config_.threshold_count; ++k) {
                    incident.qualifying_messages.push_back(errors[k].message);
                    if (errors[k].severity == Severity::CRITICAL) {
                        incident.severity = Severity::CRITICAL;
                    }
                }
                incident.error_count = config_.threshold_count;

                // Now extend the cluster: any subsequent errors where error[j] - last_error <= window_seconds
                std::size_t j = i + config_.threshold_count;
                while (j < n && (errors[j].timestamp - incident.last_error_time <= config_.window_seconds)) {
                    incident.last_error_time = errors[j].timestamp;
                    incident.qualifying_messages.push_back(errors[j].message);
                    if (errors[j].severity == Severity::CRITICAL) {
                        incident.severity = Severity::CRITICAL;
                    }
                    ++incident.error_count;
                    ++j;
                }

                // Check for recovery matching: find earliest recovery event for this service
                // that occurred at or after incident.start_time
                for (const auto& rec : recoveries) {
                    if (rec.timestamp >= incident.start_time) {
                        incident.status = IncidentStatus::RESOLVED;
                        incident.resolution_time = rec.timestamp;
                        break;
                    }
                }

                // Assign structured incident ID
                std::ostringstream id_ss;
                id_ss << "INC-" << run_id << "-" << std::setw(3) << std::setfill('0') << incident_counter++;
                incident.incident_id = id_ss.str();

                incidents.push_back(std::move(incident));

                // Advance i past the burst
                i = j;
            } else {
                ++i;
            }
        }
    }

    return incidents;
}

} // namespace loglens
