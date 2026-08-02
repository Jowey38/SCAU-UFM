#include <gtest/gtest.h>

#include "sim_driver.hpp"

namespace {

scau::apps::sim_driver::RuntimeConfig minimal_config() {
    return {
        .start_time = 0.0,
        .end_time = 600.0,
        .dt_couple = 60.0,
        .dt_surface = 10.0,
        .stcf_case_path = "case.stcf.nc",
    };
}

}  // namespace

TEST(SimDriver, ValidatesOptionalEngineConfigurationFailClosed) {
    auto swmm = minimal_config();
    swmm.enable_swmm = true;
    swmm.dt_swmm = 60.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(swmm), std::invalid_argument);
    swmm.swmm_inp_path = "network.inp";
    EXPECT_NO_THROW(scau::apps::sim_driver::validate_runtime_config(swmm));

    auto dflowfm = minimal_config();
    dflowfm.enable_dflowfm = true;
    dflowfm.dt_dflowfm = 60.0;
    EXPECT_THROW(scau::apps::sim_driver::validate_runtime_config(dflowfm), std::invalid_argument);
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
