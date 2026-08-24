#pragma once

#include <cstddef>
#include <map>
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

// SWMM outfall -> river injection semantics (M278).
enum class DrainageRiverInjectionMode {
    // bug-210 legacy path: the sampled instantaneous outfall rate is applied
    // as a river lateral in the SAME substep. NOT volume conservative;
    // audit-guarded and excluded from the conservation goldens.
    sampled_rate_legacy,
    // M278 governed path: emitted volume is measured post-step from the
    // cumulative massbal node-outflow register delta, buffered in the
    // driver-owned interface ledger and injected at the NEXT substep through
    // evaluate_engine_interface_exchange (capacity-clamped; unmet volume
    // stays buffered and ages like a deficit). Reverse (backwater) deltas
    // are debited from the river as negative laterals through the same
    // governance.
    emitted_volume,
};

// SWMM outfall -> river lateral interface link for one dt_sub.
struct DrainageRiverLink {
    int outfall_node_id{0};
    int river_location_id{0};
    double q_capacity{0.0};       // river acceptance capacity this dt_sub
    bool drive_outfall_stage{true};  // write river water level back to outfall
    DrainageRiverInjectionMode injection_mode{DrainageRiverInjectionMode::sampled_rate_legacy};
};

// One in-flight volume account of the M278 interface buffer ledger. volume
// is driver-owned storage for the whole-system audit (never a tolerance);
// age_epochs counts consecutive committed epochs with a non-zero balance
// (mirrors mass_deficit_account aging; cleared accounts reset to zero).
struct InterfaceBufferAccount {
    double volume{0.0};
    std::size_t age_epochs{0U};
};

// Caller-owned (run-loop lifetime) state of the M278 interface buffer
// ledger, keyed by SWMM outfall node id. emitted holds boundary volume that
// left SWMM but has not yet been injected into the river; reverse holds
// backwater volume that entered SWMM through a driven outfall stage but has
// not yet been debited from the river. The audit in-flight storage term is
// sum(emitted) - sum(reverse).
struct DrainageRiverInterfaceState {
    std::map<int, InterfaceBufferAccount> emitted{};
    std::map<int, InterfaceBufferAccount> reverse{};
    // Last observed cumulative node-outflow register (m3), keyed by outfall
    // node id; absent means "never observed" and defaults to the engine's
    // zero-at-initialize register origin.
    std::map<int, double> last_cumulative_outflow_m3{};

    [[nodiscard]] double inflight_volume() const;
};

// Epoch-commit aging for the interface buffer ledger (call exactly once per
// committed coupling epoch, alongside deficit aging).
void age_drainage_river_interface_buffers(DrainageRiverInterfaceState& interface_state);

struct DFlowFMLateralIdMapping {
    int location_id{0};
    std::string native_lateral_id{};
};

struct TriCouplingStepConfig {
    std::vector<SurfaceDrainageLink> surface_drainage{};
    std::vector<SurfaceRiverLink> surface_river{};
    std::vector<DrainageRiverLink> drainage_river{};
    // Case-owned mapping from CouplingLib's integer river endpoint to the
    // D-Flow FM native compound lateral ID. Native IDs remain outside core DTOs.
    std::vector<DFlowFMLateralIdMapping> river_lateral_ids{};
    // Project-standard D-Flow FM variable names (symbols reference). When the
    // real BMI kernel exposes different native names (e.g. "s1" for water
    // level), override these per configuration.
    std::string river_lateral_discharge_variable{"lateral_discharge"};
    std::string river_water_level_variable{"water_level"};
    bool step_engines{true};
};

// Per-link M278 interface ledger movements for one dt_sub (link order).
struct DrainageRiverInterfaceReport {
    int outfall_node_id{0};
    double emitted_volume_m3{0.0};   // post-step positive register delta
    double reverse_volume_m3{0.0};   // post-step negative register delta (magnitude)
    double injected_volume_m3{0.0};  // v_granted into the river this substep
    double debited_volume_m3{0.0};   // negative-lateral magnitude this substep
};

struct TriCouplingStepReport {
    std::vector<core::SharedExchangeDecision> surface_decisions{};
    std::vector<core::EngineInterfaceExchangeDecision> interface_decisions{};
    std::vector<core::ReturnExchangeDecision> return_decisions{};
    std::vector<DrainageRiverInterfaceReport> interface_buffer_reports{};
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
// Legacy entry point: throws std::invalid_argument when any drainage-river
// link requests the governed emitted_volume mode (that mode needs the
// caller-owned interface ledger below).
[[nodiscard]] TriCouplingStepReport advance_tri_coupling_step(
    core::CouplingState& state,
    drainage::ISwmmEngine& swmm,
    river::IDFlowFMEngine& dflowfm,
    const TriCouplingStepConfig& config,
    double dt_sub,
    double h_wet = 1.0e-6);

// M278 governed entry point: interface_state persists across substeps and
// epochs (caller-owned). Emitted-volume injection and backwater reverse
// debit run through the interface buffer ledger; legacy-mode links behave
// exactly as in the legacy entry point.
[[nodiscard]] TriCouplingStepReport advance_tri_coupling_step(
    core::CouplingState& state,
    drainage::ISwmmEngine& swmm,
    river::IDFlowFMEngine& dflowfm,
    const TriCouplingStepConfig& config,
    DrainageRiverInterfaceState& interface_state,
    double dt_sub,
    double h_wet = 1.0e-6);

}  // namespace scau::coupling::driver
