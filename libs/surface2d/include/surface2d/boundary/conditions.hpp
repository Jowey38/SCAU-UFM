#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

enum class BoundaryKind {
    Wall,
    Open,
    // Prescribed inflow through a closed boundary edge: q >= 0 [m^2/s] per
    // unit edge length enters the inside cell as a mass flux with zero
    // boundary momentum (source-type inflow). With q = 0 the edge behaves
    // exactly like Wall (same WB pressure), so a lake at rest stays at rest.
    DischargeInflow,
    // Prescribed stage eta_bc [m]: a ghost state with depth
    // max(0, eta_bc - z_b_inside) and the inside velocity feeds the same
    // HLLC path as Open, so water flows in or out toward the prescribed level.
    WaterLevel,
};

struct BoundaryConditions {
    std::vector<BoundaryKind> edges;
    // Per-edge prescribed values, indexed like edges. Both vectors may stay
    // empty when no edge uses the corresponding kind:
    // - discharge_per_width: q [m^2/s] for DischargeInflow edges (empty means
    //   q = 0 everywhere).
    // - water_level: eta_bc [m]; REQUIRED (mesh-edge-sized) as soon as any
    //   edge is WaterLevel, because stage has no safe implicit default.
    // Time series (hydrographs / stage curves) are the driver's concern:
    // update these values between steps.
    std::vector<core::Real> discharge_per_width;
    std::vector<core::Real> water_level;

    [[nodiscard]] static BoundaryConditions for_mesh(const mesh::Mesh& mesh);

    [[nodiscard]] core::Real discharge_at(std::size_t edge_index) const {
        return edge_index < discharge_per_width.size() ? discharge_per_width[edge_index] : 0.0;
    }
};

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh);

}  // namespace scau::surface2d
