#pragma once

#include <functional>
#include <string>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_summary.hpp"
#include "sim_driver.hpp"

namespace scau::apps::sim_driver {

// Optional engine-native accessors. In mock mode SWMM node names must be
// strict integers; in real mode main() passes a resolver backed by
// SwmmEngine::node_index so the generic loop never sees the concrete engine.
// The elapsed-time getters feed the per-epoch checkpoint records; when absent
// the epoch logical time is recorded instead.
struct RunLoopHooks {
    std::function<int(const std::string&)> resolve_swmm_node{};
    std::function<double()> swmm_elapsed_time{};
    std::function<double()> dflowfm_elapsed_time{};
    // Whole-system physical storage provider. Required when
    // enable_whole_system_mass_audit is true; real mode binds the concrete
    // SwmmEngine::total_stored_volume without widening ISwmmEngine.
    std::function<double()> swmm_storage_volume{};
    // Complete cumulative external engine net-volume providers (inflow
    // positive, outflow/loss negative). Absence makes the real audit
    // scope-incomplete / REVIEW_REQUIRED; it never falls back to tolerance.
    // Provider returns the complete SWMM external-net observation after the
    // CouplingLib-owned API lateral component has been removed exactly once.
    std::function<double()> swmm_external_net_volume{};
    std::function<double()> dflowfm_external_net_volume{};
};

struct RunLoopResult {
    SimDriverState final_state{SimDriverState::created};
    std::size_t committed_epochs{0U};
    RunSummary summary{};
};

// Minimal executable tri-model run loop (M268) with the M269 epoch commit
// protocol. Per dt_couple epoch:
//   1. all dt_surface substeps (fail-closed stop on CFL rollback: the epoch is
//      abandoned BEFORE any engine advances, the run restores the last
//      committed state and lands in review_required);
//   2. project the surface into fresh exchange cells (deficit carry-over);
//   3. one coupled substep via advance_tri_coupling_step (dt_sub = dt_couple);
//   4. unconditional conservative write-back into the surface state;
//   5. coordinate_checkpoint_commit over all module records; ONLY a committed
//      verdict advances record_committed_coupling_step. Failures at or after
//      the coupled substep are treated as engine-advanced: rollback is
//      REFUSED (SWMM cannot rewind), the decision evidence is recorded, and
//      the run lands in review_required.
// Requires a config with both engines enabled. The CALLER owns the engine
// lifecycle: both engines must already be initialized (mock fixtures are set
// after MockSwmmEngine::initialize, which clears them), and the caller
// finalizes them after the run.
[[nodiscard]] RunLoopResult run_simulation(
    SimDriver& driver,
    coupling::drainage::ISwmmEngine& swmm,
    coupling::river::IDFlowFMEngine& dflowfm,
    const RunLoopHooks& hooks = {});

}  // namespace scau::apps::sim_driver
