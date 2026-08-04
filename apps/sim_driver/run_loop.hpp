#pragma once

#include <functional>
#include <string>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_summary.hpp"
#include "sim_driver.hpp"

namespace scau::apps::sim_driver {

// Optional engine-native name resolution. In mock mode SWMM node names must be
// strict integers; in real mode main() passes a resolver backed by
// SwmmEngine::node_index so the generic loop never sees the concrete engine.
struct RunLoopHooks {
    std::function<int(const std::string&)> resolve_swmm_node{};
};

struct RunLoopResult {
    SimDriverState final_state{SimDriverState::created};
    std::size_t committed_epochs{0U};
    RunSummary summary{};
};

// Minimal executable tri-model run loop (M268). Per dt_couple epoch:
//   1. all dt_surface substeps (fail-closed stop on CFL rollback: the epoch is
//      abandoned BEFORE any engine advances, run lands in review_required);
//   2. project the surface into fresh exchange cells (deficit carry-over);
//   3. one coupled substep via advance_tri_coupling_step (dt_sub = dt_couple);
//   4. unconditional conservative write-back into the surface state;
//   5. record the committed orchestration boundary.
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
