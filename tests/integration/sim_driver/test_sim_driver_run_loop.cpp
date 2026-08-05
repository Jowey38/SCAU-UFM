#include <cstdlib>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "run_summary.hpp"
#include "sim_driver.hpp"

// Full SimDriver run-loop integration over a real strict STCF case (generated
// by scau_preproc, mixed-minimal profile: one quad + one triangle) with both
// mock engines. Locks the M268 orchestration contract:
//   - surface substeps advance before any engine advances;
//   - the coupled substep uses head-driven links with Q_limit arbitration;
//   - the write-back is conservative: V0 + returned - drained == Vfinal;
//   - CFL rollback before engine advancement lands in review_required with
//     zero committed epochs.

namespace {

namespace sim = scau::apps::sim_driver;

std::string case_path_from_env() {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, "SCAU_SIM_RUN_LOOP_CASE") != 0 || value == nullptr) {
        throw std::runtime_error("SCAU_SIM_RUN_LOOP_CASE is not set");
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv("SCAU_SIM_RUN_LOOP_CASE");
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error("SCAU_SIM_RUN_LOOP_CASE is not set");
    }
    return value;
#endif
}

// mixed-minimal: cell 0 quad (phi_t=1.0, z_b=0.0), cell 1 triangle
// (phi_t=0.8, z_b=0.1). initial_eta=1.0 wets both cells.
sim::RuntimeConfig run_loop_config() {
    sim::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = 10.0;
    config.dt_couple = 1.0;
    config.dt_surface = 0.05;
    config.dt_swmm = 1.0;
    config.dt_dflowfm = 1.0;
    config.enable_swmm = true;
    config.enable_dflowfm = true;
    config.stcf_case_path = case_path_from_env();
    config.swmm_inp_path = "mock.inp";
    config.dflowfm_mdu_path = "mock.mdu";
    config.initial_eta = 1.0;
    // mixed-minimal has a spatial phi_t jump (1.0 / 0.8) on the internal edge;
    // exact phi_t*h*A closure under nonzero velocity requires the opt-in G23
    // CVC correction (default-off project-wide).
    config.enable_cvc_spatial_phi_t_correction = true;

    sim::SurfaceDrainageLinkConfig drainage{};
    drainage.cell = 0U;
    drainage.node_name = "11";
    drainage.crest_level = 0.5;
    drainage.exchange_width = 1.0;
    config.surface_drainage.push_back(drainage);

    sim::SurfaceRiverLinkConfig river{};
    river.cell = 1U;
    river.location_id = 5;
    river.native_lateral_id = "lat1";
    river.crest_level = 0.5;
    river.exchange_width = 1.0;
    config.surface_river.push_back(river);
    return config;
}

}  // namespace

TEST(SimDriverRunLoop, CompletesTriModelRunWithConservativeWriteBack) {
    sim::SimDriver driver;
    driver.configure(run_loop_config());

    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    swmm.set_node_head_fixture(11, 0.0);          // below crest: drain direction
    swmm.set_node_overflow_fixture(11, 0.01);     // constant manhole return
    dflowfm.set_water_level_fixture(5, 0.2);      // below crest: drain direction

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm);

    EXPECT_EQ(result.final_state, sim::SimDriverState::completed);
    EXPECT_EQ(result.committed_epochs, 10U);
    ASSERT_EQ(result.summary.epochs.size(), 10U);
    EXPECT_EQ(result.summary.outcome, "completed");
    EXPECT_DOUBLE_EQ(result.summary.final_time, 10.0);

    // Engines advanced exactly once per committed epoch, after surface steps.
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 10.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 10.0);

    // Something actually drained through the head-driven links and something
    // returned through the manhole overflow.
    EXPECT_GT(result.summary.total_drained_volume, 0.0);
    EXPECT_GT(result.summary.total_returned_volume, 0.0);
    EXPECT_DOUBLE_EQ(result.summary.total_boundary_inflow_volume, 0.0);

    // Whole-domain conservative closure with wall-only boundaries: the
    // analytic initial physical storage of mixed-minimal at eta = 1.0 is
    // quad 1.0*1.0*1.0 + triangle 0.9*0.8*0.5 = 1.36 m3, and every removal
    // or addition goes through the audited coupling write-back.
    const double initial_volume = 1.0 * 1.0 * 1.0 + 0.9 * 0.8 * 0.5;
    EXPECT_NEAR(result.summary.final_surface_physical_volume +
                    result.summary.total_drained_volume -
                    result.summary.total_returned_volume,
                initial_volume, 1.0e-9);

    // Epoch records are monotone in time and finite in mass, and every
    // committed epoch carries a committed checkpoint record (M269).
    double previous_time = 0.0;
    for (const sim::EpochRecord& record : result.summary.epochs) {
        EXPECT_GT(record.logical_time, previous_time);
        previous_time = record.logical_time;
        EXPECT_EQ(record.checkpoint_status, "committed");
        EXPECT_FALSE(record.surface_content_hash.empty());
        EXPECT_FALSE(record.coupling_content_hash.empty());
        EXPECT_GE(record.coupling_surface_mass_before, 0.0);
        EXPECT_GE(record.coupling_surface_mass_after, 0.0);
        EXPECT_GE(record.coupling_deficit_mass_after, 0.0);
        EXPECT_LE(record.max_cell_cfl, 1.0);
    }

    // Deficit ledger stays non-negative and finite.
    EXPECT_GE(result.summary.final_coupling_deficit_volume, 0.0);

    // Summary JSON is emitted and carries the outcome.
    const std::string json = sim::to_json(result.summary);
    EXPECT_NE(json.find("\"outcome\": \"completed\""), std::string::npos);
    EXPECT_NE(json.find("\"committed_epochs\": 10"), std::string::npos);

    swmm.finalize();
    dflowfm.finalize();
}

TEST(SimDriverRunLoop, CflRollbackStopsBeforeEnginesAdvance) {
    auto config = run_loop_config();
    // dt_surface = dt_couple = 1.0 s blows past c_rollback on this mesh
    // (wave speed sqrt(g*h) ~ 3.1 m/s on ~1 m cells).
    config.dt_surface = 1.0;

    sim::SimDriver driver;
    driver.configure(config);

    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    swmm.set_node_head_fixture(11, 0.0);
    dflowfm.set_water_level_fixture(5, 0.2);

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm);

    EXPECT_EQ(result.final_state, sim::SimDriverState::review_required);
    EXPECT_EQ(result.committed_epochs, 0U);
    EXPECT_EQ(result.summary.outcome, "review_required");
    EXPECT_NE(result.summary.reason.find("cfl_rollback"), std::string::npos);
    // Fail-closed ordering: no engine advanced.
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 0.0);
}

TEST(SimDriverRunLoop, RejectsOutOfRangeCoupledCellBeforeEngineWrites) {
    auto config = run_loop_config();
    config.surface_drainage[0].cell = 99U;  // mixed-minimal has 2 cells

    sim::SimDriver driver;
    driver.configure(config);

    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    EXPECT_THROW(static_cast<void>(sim::run_simulation(driver, swmm, dflowfm)),
                 std::invalid_argument);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 0.0);
}
