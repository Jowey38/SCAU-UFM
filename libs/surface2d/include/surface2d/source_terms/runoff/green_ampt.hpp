#pragma once

#include "core/types.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace scau::surface2d {

// Result of one Green-Ampt substep. infiltrated_depth is the depth (m) that
// left the surface into the soil this substep; cumulative_infiltration is the
// updated F_inf; ponding_started is true when supply exceeded capacity.
struct GreenAmptStep {
    core::Real infiltrated_depth{0.0};
    core::Real cumulative_infiltration{0.0};
    bool ponding_started{false};
};

// Forward-Euler Mein-Larson substep. available_depth is the water depth (m)
// offered to the soil this substep (rain excess plus any ponded water share).
// f_inf_floor (> 0) floors F for the 1/F capacity term. Fail-closed on
// non-finite or out-of-domain inputs; never returns negative infiltration and
// never infiltrates more than available_depth.
[[nodiscard]] GreenAmptStep green_ampt_infiltration_step(
    const SoilParams& params,
    core::Real cumulative_infiltration_before,
    core::Real available_depth,
    core::Real dt,
    core::Real f_inf_floor);

}  // namespace scau::surface2d
