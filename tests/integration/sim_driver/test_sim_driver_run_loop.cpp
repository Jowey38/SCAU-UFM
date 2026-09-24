#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <netcdf.h>

#include "surface_timeseries.hpp"
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

    // Per-link ledger records (M288-C linked view): one drainage link on cell 0
    // to node 11, plus one river link on cell 1 to location 5; the constant
    // 0.01 m3/s overflow fixture returns exactly 0.01 m3 per 1 s epoch.
    // Link records are GROSS ledger movements; drained_volume/returned_volume
    // are NET per-cell write-back deltas (a cell that both drains and receives
    // in one epoch nets out). Gross and net must agree on the balance.
    double granted_sum = 0.0, returned_sum = 0.0;
    for (const sim::EpochRecord& record : result.summary.epochs) {
        ASSERT_EQ(record.link_exchanges.size(), 2U);
        const auto& drain = record.link_exchanges[0];
        EXPECT_EQ(drain.engine, "drainage");
        EXPECT_EQ(drain.node, 11);
        EXPECT_EQ(drain.cell, 0U);
        EXPECT_DOUBLE_EQ(drain.v_returned, 0.01);
        const auto& river = record.link_exchanges[1];
        EXPECT_EQ(river.engine, "river");
        EXPECT_EQ(river.node, 5);
        EXPECT_EQ(river.cell, 1U);
        EXPECT_DOUBLE_EQ(river.v_returned, 0.0);
        double epoch_granted = 0.0, epoch_returned = 0.0;
        for (const auto& link : record.link_exchanges) {
            epoch_granted += link.v_granted + link.v_repay;
            epoch_returned += link.v_returned;
        }
        EXPECT_NEAR(epoch_granted - epoch_returned,
                    record.drained_volume - record.returned_volume, 1.0e-12);
        EXPECT_GE(epoch_granted, record.drained_volume - 1.0e-12);   // gross >= net
        EXPECT_GE(epoch_returned, record.returned_volume - 1.0e-12);
        granted_sum += epoch_granted;
        returned_sum += epoch_returned;
    }
    EXPECT_NEAR(granted_sum - returned_sum,
                result.summary.total_drained_volume - result.summary.total_returned_volume, 1.0e-12);
    EXPECT_DOUBLE_EQ(returned_sum, 10 * 0.01);                        // exact gross overflow return
    EXPECT_NE(json.find("\"link_exchanges\": [{\"engine\": \"drainage\", \"node\": 11, "
                        "\"node_name\": \"11\", \"cell\": 0"),
              std::string::npos);
    EXPECT_EQ(result.summary.epochs[0].link_exchanges[1].node_name, "lat1");

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

TEST(SimDriverRunLoop, DrainageOnlyNeverCallsDisabledRiver) {
    auto config = run_loop_config();
    config.enable_dflowfm = false;
    config.surface_river.clear();
    config.dflowfm_mdu_path.clear();
    sim::SimDriver driver;
    driver.configure(config);
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    swmm.set_node_head_fixture(11, 0.0);
    sim::RunLoopHooks hooks{};
    hooks.dflowfm_elapsed_time = []() -> double {
        throw std::logic_error("disabled river clock was queried");
    };
    const auto result = sim::run_simulation(driver, swmm, dflowfm, hooks);
    EXPECT_EQ(result.final_state, sim::SimDriverState::completed);
    EXPECT_EQ(result.committed_epochs, 10U);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 10.0);
    EXPECT_GT(result.summary.total_drained_volume, 0.0);
    EXPECT_NEAR(result.summary.final_surface_physical_volume +
                    result.summary.total_drained_volume -
                    result.summary.total_returned_volume,
                1.36, 1.0e-9);
    for (const auto& epoch : result.summary.epochs) {
        EXPECT_EQ(epoch.checkpoint_status, "committed");
    }
}

TEST(SimDriverRunLoop, OptionalTimeseriesPreservesStateAndWritesCommittedFrames) {
    auto config = run_loop_config();
    config.enable_dflowfm = false;
    config.surface_river.clear();
    const auto output = std::filesystem::temp_directory_path() /
        ("scau_timeseries_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".nc");
    const auto run = [](const sim::RuntimeConfig& cfg) {
        sim::SimDriver driver;
        driver.configure(cfg);
        scau::coupling::drainage::MockSwmmEngine swmm;
        scau::coupling::river::MockDFlowFMEngine river;
        swmm.initialize("mock.inp");
        swmm.set_node_head_fixture(11, 0.0);
        return sim::run_simulation(driver, swmm, river);
    };
    const auto baseline = run(config);
    config.surface_timeseries_path = output.string();
    config.surface_output_every_epochs = 3U;
    const auto observed = run(config);
    EXPECT_EQ(observed.summary.final_surface_state_hash, baseline.summary.final_surface_state_hash);
    EXPECT_DOUBLE_EQ(observed.summary.total_drained_volume, baseline.summary.total_drained_volume);
    EXPECT_FALSE(std::filesystem::exists(output.string() + ".partial"));
    int file = -1;
    ASSERT_EQ(nc_open(output.string().c_str(), NC_NOWRITE, &file), NC_NOERR);
    int dim = -1;
    ASSERT_EQ(nc_inq_dimid(file, "time", &dim), NC_NOERR);
    std::size_t count = 0;
    ASSERT_EQ(nc_inq_dimlen(file, dim, &count), NC_NOERR);
    EXPECT_EQ(count, 5U);
    int var = -1;
    ASSERT_EQ(nc_inq_varid(file, "time", &var), NC_NOERR);
    std::vector<double> times(count);
    ASSERT_EQ(nc_get_var_double(file, var, times.data()), NC_NOERR);
    EXPECT_EQ(times, (std::vector<double>{0.0, 3.0, 6.0, 9.0, 10.0}));
    EXPECT_EQ(nc_inq_varid(file, "wet_mask", &var), NC_NOERR);
    // Provenance: the file binds source bytes and the final state; the sidecar
    // manifest binds the published output bytes and must agree with both.
    std::size_t len = 0;
    ASSERT_EQ(nc_inq_attlen(file, NC_GLOBAL, "final_surface_state_hash", &len), NC_NOERR);
    std::string attr_hash(len, '\0');
    ASSERT_EQ(nc_get_att_text(file, NC_GLOBAL, "final_surface_state_hash", attr_hash.data()), NC_NOERR);
    EXPECT_EQ(attr_hash, observed.summary.final_surface_state_hash);
    ASSERT_EQ(nc_inq_attlen(file, NC_GLOBAL, "source_stcf_hash", &len), NC_NOERR);
    std::string source_hash(len, '\0');
    ASSERT_EQ(nc_get_att_text(file, NC_GLOBAL, "source_stcf_hash", source_hash.data()), NC_NOERR);
    EXPECT_EQ(source_hash, sim::hash_file_bytes(config.stcf_case_path));
    EXPECT_EQ(nc_close(file), NC_NOERR);
    const auto manifest_path = output.string() + ".manifest.json";
    ASSERT_TRUE(std::filesystem::exists(manifest_path));
    const std::string manifest = [&] {
        std::ifstream stream(manifest_path);  // closed before the file is removed below
        return std::string((std::istreambuf_iterator<char>(stream)), {});
    }();
    EXPECT_NE(manifest.find("\"output_hash\": \"" + sim::hash_file_bytes(output) + "\""), std::string::npos);
    EXPECT_NE(manifest.find("\"final_surface_state_hash\": \"" + attr_hash + "\""), std::string::npos);
    EXPECT_NE(manifest.find("\"source_stcf_hash\": \"" + source_hash + "\""), std::string::npos);
    EXPECT_NE(manifest.find("\"committed_epochs\": 10"), std::string::npos);
    EXPECT_NE(manifest.find("\"frames\": 5"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(manifest_path + ".partial"));
    // Run identity in the summary must agree with the manifest so a linked
    // view can bind the two without relying on the final state alone.
    EXPECT_EQ(observed.summary.source_stcf_hash, source_hash);
    EXPECT_DOUBLE_EQ(observed.summary.dt_couple, 1.0);
    EXPECT_TRUE(observed.summary.swmm_report_path.empty());   // mock engine writes no report
    EXPECT_THROW(static_cast<void>(run(config)), std::invalid_argument);
    std::filesystem::remove(output);
    std::filesystem::remove(manifest_path);
    // Preflight: a pre-existing MANIFEST alone must refuse the run before any
    // engine advances (previously only the .nc/.partial were checked).
    { std::ofstream(manifest_path) << "{}"; }
    EXPECT_THROW(static_cast<void>(run(config)), std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(output));
    EXPECT_FALSE(std::filesystem::exists(output.string() + ".partial"));
    std::filesystem::remove(manifest_path);
    config.dt_surface = 1.0;
    const auto rejected = run(config);
    EXPECT_EQ(rejected.committed_epochs, 0U);
    EXPECT_FALSE(std::filesystem::exists(output));
    const auto partial = output.string() + ".partial";
    ASSERT_EQ(nc_open(partial.c_str(), NC_NOWRITE, &file), NC_NOERR);
    ASSERT_EQ(nc_inq_dimid(file, "time", &dim), NC_NOERR);
    ASSERT_EQ(nc_inq_dimlen(file, dim, &count), NC_NOERR);
    EXPECT_EQ(count, 1U);  // only the initial frame, never the rejected epoch
    EXPECT_EQ(nc_close(file), NC_NOERR);
    std::filesystem::remove(partial);
}

TEST(SimDriverRunLoop, DrainageOnlyCflRollbackDoesNotAdvanceSwmm) {
    auto config = run_loop_config();
    config.enable_dflowfm = false;
    config.surface_river.clear();
    config.dt_surface = 1.0;
    sim::SimDriver driver;
    driver.configure(config);
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    const auto result = sim::run_simulation(driver, swmm, dflowfm);
    EXPECT_EQ(result.final_state, sim::SimDriverState::review_required);
    EXPECT_EQ(result.committed_epochs, 0U);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 0.0);
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
