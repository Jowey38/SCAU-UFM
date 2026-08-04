#include <exception>
#include <iostream>
#include <string>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "run_summary.hpp"
#include "runtime_config_io.hpp"
#include "sim_driver.hpp"

#if defined(SCAU_HAS_SWMM5)
#include "coupling/drainage/swmm_engine.hpp"
#endif
#if defined(SCAU_HAS_DFLOWFM_BMI_RUNTIME)
#include "coupling/river/dflowfm_engine.hpp"
#endif

namespace {

namespace sim = scau::apps::sim_driver;

int run_with_engines(sim::SimDriver& driver,
                     scau::coupling::drainage::ISwmmEngine& swmm,
                     scau::coupling::river::IDFlowFMEngine& dflowfm,
                     const sim::RunLoopHooks& hooks) {
    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);
    const std::string& output_path = driver.config().output_summary_path;
    if (!output_path.empty()) {
        sim::write_summary_json(output_path, result.summary);
    }
    std::cout << "scau_sim: " << result.summary.outcome
              << " (committed_epochs=" << result.committed_epochs << ")";
    if (!result.summary.reason.empty()) {
        std::cout << " reason: " << result.summary.reason;
    }
    std::cout << "\n";
    return result.final_state == sim::SimDriverState::completed ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: scau_sim <config-file>\n";
        return 2;
    }

    sim::SimDriver driver;
    try {
        driver.configure(sim::read_runtime_config_file(argv[1]));
        const sim::RuntimeConfig& config = driver.config();

        if (config.engine_mode == sim::EngineMode::mock) {
            scau::coupling::drainage::MockSwmmEngine swmm;
            scau::coupling::river::MockDFlowFMEngine dflowfm;
            swmm.initialize(config.swmm_inp_path);
            dflowfm.initialize(config.dflowfm_mdu_path);
            const int code = run_with_engines(driver, swmm, dflowfm, sim::RunLoopHooks{});
            swmm.finalize();
            dflowfm.finalize();
            return code;
        }

#if defined(SCAU_HAS_SWMM5) && defined(SCAU_HAS_DFLOWFM_BMI_RUNTIME)
        scau::coupling::drainage::SwmmEngine swmm;
        scau::coupling::river::DFlowFMEngine dflowfm;
        swmm.initialize(config.swmm_inp_path);
        dflowfm.initialize(config.dflowfm_mdu_path);
        sim::RunLoopHooks hooks{};
        hooks.resolve_swmm_node = [&swmm](const std::string& node_name) {
            return swmm.node_index(node_name);
        };
        const int code = run_with_engines(driver, swmm, dflowfm, hooks);
        swmm.finalize();
        dflowfm.finalize();
        return code;
#else
        std::cerr << "scau_sim: engine_mode=real requires SCAU_EMBED_SWMM and "
                     "SCAU_ENABLE_DFLOWFM_BMI_RUNTIME builds\n";
        return 2;
#endif
    } catch (const std::exception& error) {
        std::cerr << "scau_sim: error: " << error.what() << "\n";
        try {
            if (driver.state() != sim::SimDriverState::completed &&
                driver.state() != sim::SimDriverState::aborted) {
                driver.abort();
            }
        } catch (const std::exception&) {
            // Abort bookkeeping failure must not mask the original error.
        }
        return 2;
    }
}
