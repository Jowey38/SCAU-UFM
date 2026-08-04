#pragma once

#include <cstddef>
#include <vector>

#include "coupling/core/state.hpp"
#include "core/types.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"

namespace scau::coupling::driver {

// Surface2D <-> CouplingState adapter for the SimDriver run loop.
//
// CouplingLib owns exchange-cell semantics; Surface2DCore owns the 2D field.
// This seam is the single place where the two representations meet:
//   - build_exchange_cells projects the CURRENT surface state into fresh
//     exchange cells (V = phi_t * h * A) before each coupled substep;
//   - apply_exchange_write_back pushes post-replay exchange volumes back into
//     the surface state after the coupled substep.
//
// CouplingState has no public cell mutator by design, so the run loop rebuilds
// the exchange cells every coupling epoch. Deficit ledgers must survive that
// rebuild: build_exchange_cells carries both the aggregate and the
// endpoint-owned shared deficit accounts over from the previous epoch's
// CouplingState. RuntimeCounters therefore reset each epoch and are
// per-epoch diagnostics only.

// Exchange cell i of the CouplingState maps to surface cell
// surface_cells[i] of the Surface2D mesh. One surface cell may appear at
// most once (single-writer rule across all coupling links).
struct Surface2DCouplingMap {
    std::vector<std::size_t> surface_cells{};
};

// Fail-closed: empty map, out-of-range surface cell index, duplicate surface
// cell, or size mismatch between state / DPM / geometry throw
// std::invalid_argument.
void validate_surface2d_coupling_map(
    const Surface2DCouplingMap& map,
    const surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry);

// Builds fresh exchange cells from the current surface state:
//   volume = phi_t * h * A  (canonical physical storage mapping)
// Deficit accounts are carried over from `previous` when non-null; `previous`
// must then hold exactly map.surface_cells.size() cells in the same order.
// Fail-closed: non-finite or negative h, phi_t <= 0, area <= 0, or a
// `previous` size mismatch throw std::invalid_argument.
[[nodiscard]] std::vector<core::ExchangeCellState> build_exchange_cells(
    const surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry,
    const Surface2DCouplingMap& map,
    const core::CouplingState* previous);

struct ExchangeWriteBackReport {
    double drained_volume{0.0};   // total volume that left the surface (m3)
    double returned_volume{0.0};  // total volume returned to the surface (m3)
};

// Pushes post-replay exchange volumes back into the surface state:
//   h_new = volume_after / (phi_t * A),  eta = z_b + h_new
// Momentum convention (recorded in the M268 plan doc):
//   - drain (volume decreased): water leaves at the cell velocity, so hu/hv
//     scale by h_new / h_old and the velocity u = hu/h is preserved;
//   - return (volume increased): returned water carries zero momentum, so
//     hu/hv stay unchanged and the velocity dilutes.
// Fail-closed: size mismatches, a non-empty pending event queue on
// `coupling_after` (write-back must run post-replay), or a non-finite /
// negative resulting depth throw; std::logic_error for the pending-queue
// violation, std::invalid_argument otherwise.
[[nodiscard]] ExchangeWriteBackReport apply_exchange_write_back(
    surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry,
    const Surface2DCouplingMap& map,
    const std::vector<core::ExchangeCellState>& cells_before,
    const core::CouplingState& coupling_after,
    const std::vector<scau::core::Real>& bed_elevations);

}  // namespace scau::coupling::driver
