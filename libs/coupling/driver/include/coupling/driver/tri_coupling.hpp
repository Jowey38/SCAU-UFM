#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

// Pairwise bidirectional tri-coupling step driver:
//   surface(2D) <-> drainage(SWMM)   drain via shared-cell arbitration,
//                                    return via node overflow
//   surface(2D) <-> river(D-Flow FM) drain via shared-cell arbitration,
//                                    return via caller-computed spill q_return
//   drainage    <-> river            outfall discharge -> river lateral via the
//                                    core 1D-1D interface exchange, river water
//                                    level -> outfall stage
//
// Boundary contract: the driver only sees CouplingLib DTOs and the abstract
// engine interfaces; SWMM and D-Flow FM native objects never see each other.
// All arbitration (Q_limit / V_limit / priority_weight / deficit) is
// core-owned; the driver merely routes core decisions into engine state
// writes and engine state reads into core requests.

// 2D cell <-> SWMM node coupling link for one dt_sub.
//
// Two intent modes per link:
// - explicit: `q_request` (and for river links `q_return`) are caller-given.
// - head-driven: when `geometry` is set, both direction and magnitude are
//   computed by core::compute_head_driven_exchange_flow from
//   `surface_water_level` vs the engine-side water level read at the start
//   of the step (weir / Villemonte submerged weir / orifice / smoothstep);
//   the explicit fields are ignored.
struct SurfaceDrainageLink {
    std::size_t cell_index{0U};
    int node_id{0};
    double q_request{0.0};        // surface -> pipe drain intent (explicit mode)
    double priority_weight{1.0};
    std::optional<core::ExchangeFlowGeometry> geometry{};
    double surface_water_level{0.0};  // 2D water surface elevation (head-driven mode)
};

// 2D cell <-> river location coupling link for one dt_sub.
struct SurfaceRiverLink {
    std::size_t cell_index{0U};
    int location_id{0};
    double q_request{0.0};        // surface -> river drain intent (explicit mode)
    double priority_weight{1.0};
    double q_return{0.0};         // river -> surface spill (explicit mode)
    std::optional<core::ExchangeFlowGeometry> geometry{};
    double surface_water_level{0.0};  // 2D water surface elevation (head-driven mode)
};

// SWMM outfall -> river lateral interface link for one dt_sub.
struct DrainageRiverLink {
    int outfall_node_id{0};
    int river_location_id{0};
    double q_capacity{0.0};       // river acceptance capacity this dt_sub
    bool drive_outfall_stage{true};  // write river water level back to outfall
};

struct TriCouplingStepConfig {
    std::vector<SurfaceDrainageLink> surface_drainage{};
    std::vector<SurfaceRiverLink> surface_river{};
    std::vector<DrainageRiverLink> drainage_river{};
    // Project-standard D-Flow FM variable names (symbols reference). When the
    // real BMI kernel exposes different native names (e.g. "s1" for water
    // level), override these per configuration.
    std::string river_lateral_discharge_variable{"lateral_discharge"};
    std::string river_water_level_variable{"water_level"};
    bool step_engines{true};
};

struct TriCouplingStepReport {
    std::vector<core::SharedExchangeDecision> surface_decisions{};
    std::vector<core::EngineInterfaceExchangeDecision> interface_decisions{};
    std::vector<core::ReturnExchangeDecision> return_decisions{};
    core::SystemMassAudit surface_mass_before{};
    core::SystemMassAudit surface_mass_after{};
};

// Advances one coupled dt_sub:
//   1. shared-cell arbitration of all surface->engine drain intents
//   2. 1D-1D interface exchange (outfall discharge -> river lateral capacity)
//   3. acceptance: accumulated per-node lateral inflow / per-location lateral
//      discharge written into the engines, river stage onto outfalls
//   4. engine stepping (swmm.step / dflowfm.update) when step_engines is set
//   5. return flows: node overflow and river spill applied back onto the
//      surface through the core event queue, then replay_pending()
//
// Fail-closed: invalid dt_sub, duplicate engine endpoints, or invalid link
// fields throw std::invalid_argument before any engine state is written.
[[nodiscard]] TriCouplingStepReport advance_tri_coupling_step(
    core::CouplingState& state,
    drainage::ISwmmEngine& swmm,
    river::IDFlowFMEngine& dflowfm,
    const TriCouplingStepConfig& config,
    double dt_sub,
    double h_wet = 1.0e-6);

}  // namespace scau::coupling::driver
