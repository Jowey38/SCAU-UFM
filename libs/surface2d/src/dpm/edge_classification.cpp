#include "surface2d/dpm/edge_classification.hpp"

namespace scau::surface2d {

EdgeClassification classify_edge(core::Real omega_edge, core::Real phi_e_n) {
    if (omega_edge <= 0.0) {
        return EdgeClassification{
            .block_class = EdgeBlockClass::HardBlock,
            .advective_flux_zeroed = true,
            .wb_pairing_assembled = false,
        };
    }
    if (omega_edge < EpsilonOmega || phi_e_n < PhiEdgeMin) {
        return EdgeClassification{
            .block_class = EdgeBlockClass::SoftBlock,
            .advective_flux_zeroed = true,
            .wb_pairing_assembled = true,
        };
    }
    return EdgeClassification{
        .block_class = EdgeBlockClass::Regular,
        .advective_flux_zeroed = false,
        .wb_pairing_assembled = true,
    };
}

}  // namespace scau::surface2d
