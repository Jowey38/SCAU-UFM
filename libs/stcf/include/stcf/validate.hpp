#pragma once

#include <cstddef>

#include "core/types.hpp"
#include "stcf/schema.hpp"

namespace scau::stcf {

// Main spec section 11.2 defaults for the PreProc-time DPM consistency gate.
struct StcfValidationConfig {
    core::Real epsilon_phi{1.0e-6};
    core::Real cond_max{1.0e4};
    core::Real epsilon_det{1.0e-10};
};

// Staged fail-closed STCF v5 validation (main spec sections 4.2 / 9.2 and
// legacy.10): schema version, shape agreement, phi_t stage, Phi_c stage,
// edge stage, soil stage. Any violation throws core::ScauError naming the
// stage and the offending cell/edge index. This is the PreProc/export gate;
// it never runs inside the solver hot path.
void validate_stcf_dataset(
    const StcfDataset& dataset,
    std::size_t expected_cell_count,
    std::size_t expected_edge_count,
    const StcfValidationConfig& config = {});

// Convenience overload: derives the expected counts from phi_t/omega_edge
// and still enforces cross-vector agreement.
void validate_stcf_dataset(const StcfDataset& dataset, const StcfValidationConfig& config = {});

}  // namespace scau::stcf
