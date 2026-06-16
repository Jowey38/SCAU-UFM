#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Interface-block thresholds (main-spec 5.5 step 7 / 5.6, default-parameter
// table). epsilon_omega is the hard/soft boundary on the edge connectivity
// weight; phi_edge_min is the soft-block threshold on the edge-normal effective
// conveyance.
inline constexpr core::Real EpsilonOmega = 1.0e-4;
inline constexpr core::Real PhiEdgeMin = 0.01;

// Edge interface classification (main-spec 5.5 step 7 / 5.6):
//   HardBlock  (omega_edge <= 0): wall. No advective flux AND no WB
//              pressure/source pairing contribution.
//   SoftBlock  (0 < omega_edge < EpsilonOmega OR phi_e_n < PhiEdgeMin):
//              advective flux = 0, but the WB pairing term is retained.
//   Regular    (omega_edge >= EpsilonOmega AND phi_e_n >= PhiEdgeMin):
//              normal well-balanced discretization.
enum class EdgeBlockClass {
    Regular,
    SoftBlock,
    HardBlock,
};

struct EdgeClassification {
    EdgeBlockClass block_class{EdgeBlockClass::Regular};
    bool advective_flux_zeroed{false};  // true for SoftBlock and HardBlock
    bool wb_pairing_assembled{true};    // false only for HardBlock
};

// Pure classifier over the edge DPM scalars. Side-effect free and hot-path
// safe; field-domain validation belongs to the DPM field validators.
[[nodiscard]] EdgeClassification classify_edge(core::Real omega_edge, core::Real phi_e_n);

}  // namespace scau::surface2d
