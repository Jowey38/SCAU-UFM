#include "coupling/driver/whole_system_mass_audit.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace scau::coupling::driver {

namespace {

void require_nonnegative_finite(double value, const char* field) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(field) + " must be finite and non-negative");
    }
}

void require_finite(double value, const char* field) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(field) + " must be finite");
    }
}

void validate_sample(const WholeSystemMassSample& sample, const char* side) {
    require_finite(sample.logical_time, "logical_time");
    require_nonnegative_finite(sample.surface_volume, "surface_volume");
    require_nonnegative_finite(sample.surface_reference_volume,
                               "surface_reference_volume");
    require_nonnegative_finite(sample.coupling_deficit_volume,
                               "coupling_deficit_volume");
    if (sample.swmm_storage_volume.has_value()) {
        require_nonnegative_finite(*sample.swmm_storage_volume, "swmm_storage_volume");
    }
    if (sample.dflowfm_volume.has_value()) {
        require_nonnegative_finite(*sample.dflowfm_volume, "dflowfm_volume");
    }
    if (sample.swmm_external_net_volume.has_value()) {
        require_finite(*sample.swmm_external_net_volume, "swmm_external_net_volume");
    }
    if (sample.dflowfm_external_net_volume.has_value()) {
        require_finite(*sample.dflowfm_external_net_volume,
                       "dflowfm_external_net_volume");
    }
    require_finite(sample.cumulative_boundary_inflow_volume,
                   "cumulative_boundary_inflow_volume");
    require_finite(sample.cumulative_rainfall_volume,
                   "cumulative_rainfall_volume");
    require_finite(sample.cumulative_infiltration_volume,
                   "cumulative_infiltration_volume");
    require_finite(sample.cumulative_abstraction_volume,
                   "cumulative_abstraction_volume");
    require_finite(sample.cumulative_depression_storage_delta_volume,
                   "cumulative_depression_storage_delta_volume");
    static_cast<void>(side);
}

double storage_total(const WholeSystemMassSample& sample) {
    return sample.surface_volume +
           sample.swmm_storage_volume.value_or(0.0) +
           sample.dflowfm_volume.value_or(0.0) +
           sample.cumulative_depression_storage_delta_volume;
}

}  // namespace

WholeSystemMassAuditReport audit_whole_system_mass(
    const WholeSystemMassSample& baseline,
    const WholeSystemMassSample& current,
    const WholeSystemMassTolerance& tolerance) {
    validate_sample(baseline, "baseline");
    validate_sample(current, "current");
    if (current.epoch < baseline.epoch || current.logical_time < baseline.logical_time) {
        throw std::invalid_argument(
            "whole-system mass audit current epoch/time must not precede baseline");
    }
    if (baseline.swmm_storage_volume.has_value() !=
        current.swmm_storage_volume.has_value()) {
        throw std::invalid_argument(
            "whole-system mass audit SWMM storage scope differs between samples");
    }
    if (baseline.dflowfm_volume.has_value() !=
        current.dflowfm_volume.has_value()) {
        throw std::invalid_argument(
            "whole-system mass audit D-Flow FM storage scope differs between samples");
    }
    if (!std::isfinite(tolerance.engine_residual_absolute) ||
        tolerance.engine_residual_absolute < 0.0 ||
        !std::isfinite(tolerance.engine_residual_relative) ||
        tolerance.engine_residual_relative < 0.0) {
        throw std::invalid_argument(
            "whole-system mass audit tolerances must be finite and non-negative");
    }

    WholeSystemMassAuditReport report{};
    report.baseline = baseline;
    report.current = current;
    report.baseline_storage_total = storage_total(baseline);
    report.current_storage_total = storage_total(current);
    const bool swmm_external_scope =
        baseline.swmm_external_net_volume.has_value() &&
        current.swmm_external_net_volume.has_value();
    const bool dflowfm_external_scope =
        baseline.dflowfm_external_net_volume.has_value() &&
        current.dflowfm_external_net_volume.has_value();
    report.scope_complete = swmm_external_scope && dflowfm_external_scope;
    report.external_net_volume =
        (current.cumulative_boundary_inflow_volume -
         baseline.cumulative_boundary_inflow_volume) +
        (current.cumulative_rainfall_volume - baseline.cumulative_rainfall_volume) -
        (current.cumulative_infiltration_volume -
         baseline.cumulative_infiltration_volume) -
        (current.cumulative_abstraction_volume -
         baseline.cumulative_abstraction_volume) +
        (swmm_external_scope
             ? *current.swmm_external_net_volume - *baseline.swmm_external_net_volume
             : 0.0) +
        (dflowfm_external_scope
             ? *current.dflowfm_external_net_volume -
                   *baseline.dflowfm_external_net_volume
             : 0.0);
    report.residual =
        (report.current_storage_total - report.baseline_storage_total) -
        report.external_net_volume;
    report.epsilon_deficit =
        std::max(1.0e-10, 1.0e-12 * baseline.surface_reference_volume);
    report.applied_tolerance = report.epsilon_deficit;
    if (!tolerance.strict) {
        const double engine_tolerance =
            tolerance.engine_residual_absolute +
            tolerance.engine_residual_relative *
                std::max(1.0, report.baseline_storage_total);
        report.applied_tolerance =
            std::max(report.applied_tolerance, engine_tolerance);
    }
    report.conserved = report.scope_complete &&
                       std::abs(report.residual) <= report.applied_tolerance;
    report.verdict = report.conserved
                         ? WholeSystemMassVerdict::conserved
                         : WholeSystemMassVerdict::review_required;
    return report;
}

std::vector<DeficitAgeObservation> update_deficit_ages(
    const std::vector<double>& account_volumes,
    const std::vector<DeficitAgeObservation>& previous) {
    if (!previous.empty() && previous.size() != account_volumes.size()) {
        throw std::invalid_argument(
            "deficit age previous observation count must match account count");
    }
    std::vector<DeficitAgeObservation> result;
    result.reserve(account_volumes.size());
    for (std::size_t index = 0U; index < account_volumes.size(); ++index) {
        const double volume = account_volumes[index];
        require_nonnegative_finite(volume, "deficit account volume");
        DeficitAgeObservation observation{};
        observation.account_index = index;
        observation.volume = volume;
        if (volume > 0.0) {
            observation.deficit_age_steps =
                previous.empty() ? 1U : previous[index].deficit_age_steps + 1U;
        }
        result.push_back(observation);
    }
    return result;
}

}  // namespace scau::coupling::driver
