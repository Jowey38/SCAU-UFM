#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/types.hpp"

namespace scau::stcf {

// STCF v5 on-disk schema version (main spec section 9.2: every .stcf.nc must
// carry schema_version = 5).
inline constexpr int kSchemaVersion = 5;

// Main spec section 4.2: soil LUT capacity MAX_SOIL_TYPES.
inline constexpr std::size_t kMaxSoilTypes = 16;

// Green-Ampt soil parameter entry (symbols reference: K_s, psi_f, theta_s,
// theta_i). psi_f must be strictly positive; theta_i < theta_s <= 1.
struct SoilParamsEntry {
    core::Real K_s{0.0};
    core::Real psi_f{0.0};
    core::Real theta_s{0.0};
    core::Real theta_i{0.0};
};

// Per-cell STCF v5 fields carried by this slice. SoA layout matching the
// main spec section 4.2 GPU-facing schema. The drag quartet, Chebyshev LUT,
// semantic labels and topo moments enter with the slices that consume them.
struct CellStcfFields {
    std::vector<core::Real> phi_t;
    std::vector<core::Real> phi_xx;
    std::vector<core::Real> phi_xy;
    std::vector<core::Real> phi_yy;
    std::vector<core::Real> manning_n;
    std::vector<core::Real> z_b;
    std::vector<std::uint8_t> soil_type;
};

// Per-edge STCF v5 fields. phi_e_n (edge-normal effective conveyance) and
// omega_edge (connectivity/open-close weight) are distinct machine-facing
// names and must never be merged (symbols reference).
struct EdgeStcfFields {
    std::vector<core::Real> omega_edge;
    std::vector<core::Real> phi_e_n;
    std::vector<core::Real> phi_et;
};

struct StcfDataset {
    int schema_version{kSchemaVersion};
    CellStcfFields cells;
    EdgeStcfFields edges;
    std::vector<SoilParamsEntry> soil_params;
};

// Uniform valid dataset: phi_t = 1, identity conveyance tensor, flat bed,
// fully open edges, one physically valid soil entry. Fixture defaults, not
// spec defaults.
[[nodiscard]] StcfDataset make_uniform_dataset(std::size_t cell_count, std::size_t edge_count);

}  // namespace scau::stcf
