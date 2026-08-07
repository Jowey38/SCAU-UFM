#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "sim_driver.hpp"

namespace {

scau::apps::sim_driver::RuntimeConfig minimal_config() {
    scau::apps::sim_driver::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = 600.0;
    config.dt_couple = 60.0;
    config.dt_surface = 10.0;
    config.stcf_case_path = "case.stcf.nc";
    config.initial_eta = 1.0;
    return config;
}

scau::apps::sim_driver::SurfaceDrainageLinkConfig drainage_link(std::size_t cell) {
    scau::apps::sim_driver::SurfaceDrainageLinkConfig link{};
    link.cell = cell;
    link.node_name = "J1";
    link.crest_level = 0.5;
    link.exchange_width = 1.5;
    return link;
}

scau::apps::sim_driver::SurfaceRiverLinkConfig river_link(std::size_t cell) {
    scau::apps::sim_driver::SurfaceRiverLinkConfig link{};
    link.cell = cell;
    link.location_id = 5;
    link.native_lateral_id = "lat1";
    link.crest_level = 0.4;
    link.exchange_width = 2.0;
    return link;
}

}  // namespace

TEST(SimDriver, ValidatesOptionalEngineConfigurationFailClosed) {
    auto swmm = minimal_config();
    swmm.enable_swmm = true;
    swmm.dt_swmm = 60.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(swmm), std::invalid_argument);
    swmm.swmm_inp_path = "network.inp";
    // Still fail-closed: an enabled engine with no coupling link is a dangling
    // configuration in the tri-model loop.
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(swmm), std::invalid_argument);
    swmm.surface_drainage.push_back(drainage_link(0U));
    EXPECT_NO_THROW(scau::apps::sim_driver::validate_runtime_config(swmm));

    // Engine sub-stepping is not supported: dt_swmm must equal dt_couple.
    swmm.dt_swmm = 30.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(swmm), std::invalid_argument);

    auto dflowfm = minimal_config();
    dflowfm.enable_dflowfm = true;
    dflowfm.dt_dflowfm = 60.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(dflowfm), std::invalid_argument);
    dflowfm.dflowfm_mdu_path = "river.mdu";
    dflowfm.surface_river.push_back(river_link(1U));
    EXPECT_NO_THROW(scau::apps::sim_driver::validate_runtime_config(dflowfm));

    // Links without their engine are rejected.
    auto dangling = minimal_config();
    dangling.surface_drainage.push_back(drainage_link(0U));
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(dangling), std::invalid_argument);
}

TEST(SimDriver, ValidatesRunLoopFieldsFailClosed) {
    // initial_eta is required: STCF carries no hydrodynamic initial state.
    auto missing_eta = minimal_config();
    missing_eta.initial_eta = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(missing_eta),
                 std::invalid_argument);

    // dt_couple must be an integer multiple of dt_surface.
    auto ragged = minimal_config();
    ragged.dt_surface = 7.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(ragged), std::invalid_argument);

    // Run horizon must be an integer number of coupling epochs.
    auto ragged_horizon = minimal_config();
    ragged_horizon.end_time = 590.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(ragged_horizon),
                 std::invalid_argument);

    // Single-writer rule: one surface cell may not appear in two links.
    auto duplicated = minimal_config();
    duplicated.enable_swmm = true;
    duplicated.dt_swmm = 60.0;
    duplicated.swmm_inp_path = "network.inp";
    duplicated.enable_dflowfm = true;
    duplicated.dt_dflowfm = 60.0;
    duplicated.dflowfm_mdu_path = "river.mdu";
    duplicated.surface_drainage.push_back(drainage_link(2U));
    duplicated.surface_river.push_back(river_link(2U));
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(duplicated),
                 std::invalid_argument);

    auto bad_writeoff = minimal_config();
    bad_writeoff.n_writeoff_steps = 0U;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(bad_writeoff),
                 std::invalid_argument);

    auto bad_cfl = minimal_config();
    bad_cfl.cfl_safety = 1.5;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(bad_cfl), std::invalid_argument);
}

TEST(SimDriver, EnforcesLifecycleAndCommittedStepBoundary) {
    using scau::apps::sim_driver::SimDriverState;
    scau::apps::sim_driver::SimDriver driver;
    EXPECT_EQ(driver.state(), SimDriverState::created);
    EXPECT_THROW(driver.start(), std::logic_error);

    driver.configure(minimal_config());
    EXPECT_EQ(driver.state(), SimDriverState::configured);
    driver.initialize();
    driver.start();
    driver.record_committed_coupling_step();
    driver.record_committed_coupling_step();
    EXPECT_EQ(driver.completed_coupling_steps(), 2U);
    driver.complete();
    EXPECT_EQ(driver.state(), SimDriverState::completed);
    EXPECT_THROW(driver.abort(), std::logic_error);
}

TEST(SimDriver, SupportsReviewAndAbortTerminalStates) {
    scau::apps::sim_driver::SimDriver review;
    review.configure(minimal_config());
    review.initialize();
    review.start();
    review.require_review();
    EXPECT_EQ(review.state(), scau::apps::sim_driver::SimDriverState::review_required);

    scau::apps::sim_driver::SimDriver aborted;
    aborted.configure(minimal_config());
    aborted.abort();
    EXPECT_EQ(aborted.state(), scau::apps::sim_driver::SimDriverState::aborted);
}
