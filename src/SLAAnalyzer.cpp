#include "loglens/SLAAnalyzer.h"

#include <stdexcept>

namespace loglens {

void SLAAnalyzer::evaluate(Incident& incident, std::optional<Timestamp> reference_time) const {
    incident.allowed_duration_sec = targets_.get_allowed_seconds(incident.severity);

    if (incident.status == IncidentStatus::RESOLVED) {
        if (!incident.resolution_time) {
            throw std::runtime_error("Resolved incident " + incident.incident_id + " lacks resolution_time");
        }
        if (*incident.resolution_time < incident.start_time) {
            throw std::invalid_argument("Resolution time cannot precede start time in incident " + incident.incident_id);
        }

        std::int64_t actual = *incident.resolution_time - incident.start_time;
        incident.actual_duration_sec = actual;

        // If actual duration <= allowed, SLA is MET (exact boundary is MET)
        if (actual <= incident.allowed_duration_sec) {
            incident.sla_status = SLAStatus::MET;
        } else {
            incident.sla_status = SLAStatus::BREACHED;
        }
    } else {
        // OPEN incident
        incident.actual_duration_sec = std::nullopt;

        if (reference_time.has_value()) {
            if (*reference_time < incident.start_time) {
                throw std::invalid_argument("Reference time cannot precede incident start time");
            }
            std::int64_t elapsed = *reference_time - incident.start_time;
            if (elapsed > incident.allowed_duration_sec) {
                incident.sla_status = SLAStatus::BREACHED;
            } else {
                incident.sla_status = SLAStatus::PENDING;
            }
        } else {
            incident.sla_status = SLAStatus::PENDING;
        }
    }
}

void SLAAnalyzer::evaluate_all(std::vector<Incident>& incidents, std::optional<Timestamp> reference_time) const {
    for (auto& inc : incidents) {
        evaluate(inc, reference_time);
    }
}

} // namespace loglens
