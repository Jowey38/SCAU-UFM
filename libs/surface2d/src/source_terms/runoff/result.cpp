#include "surface2d/source_terms/runoff/result.hpp"

#include <algorithm>
#include <cmath>

namespace scau::surface2d {

core::Real runoff_mass_balance_error(const RunoffGenerationResult& result) {
    const core::Real sinks =
        result.surface_added_volume +
        result.infiltration_volume +
        result.abstraction_volume +
        result.depression_storage_delta_volume +
        result.roof_to_swmm_accepted_volume +
        result.roof_pending_delta_volume +
        result.roof_overflow_to_surface_volume +
        result.rejected_fail_closed_volume;
    return result.rainfall_volume - sinks;
}

bool check_runoff_mass_closure(
    RunoffGenerationResult& result,
    core::Real epsilon_mass_abs,
    core::Real epsilon_mass_rel) {
    const core::Real error = runoff_mass_balance_error(result);
    const core::Real tolerance =
        epsilon_mass_abs + epsilon_mass_rel * std::max(core::Real(1.0), result.rainfall_volume);
    const bool closed = std::abs(error) <= tolerance;
    if (!closed) {
        result.flags.mass_balance_violation = true;
    }
    return closed;
}

}  // namespace scau::surface2d
