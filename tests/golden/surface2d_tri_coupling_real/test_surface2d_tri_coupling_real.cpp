#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/driver/whole_system_mass_audit.hpp"
#include "coupling/drainage/swmm_engine.hpp"
#include "coupling/river/dflowfm_engine.hpp"
#include "run_loop.hpp"
#include "sim_driver.hpp"
#include "stcf/case_profiles.hpp"
#include "stcf/io_netcdf.hpp"

// G19 surface2d_tri_coupling_real: the SimDriver run loop drives the REAL
// Surface2D CPU solver, the REAL embedded SWMM engine, and the REAL D-Flow FM
// BMI runtime in one loop over a strict STCF case. This is the first golden
// where surface2d::SurfaceState is the actual 2D state inside the coupled
// loop (G17 exercises the same engines against a bare CouplingState).
//
// Env gating follows G17: skipped unless SCAU_DFLOWFM_LIBRARY and
// SCAU_DFLOWFM_G11_MDU are set. Under tools/dflowfm/run_real_goldens.sh the
// working directory is the single_reach_1d case directory, so the relative
// MDU path resolves and native output stays alive.

namespace {

namespace sim = scau::apps::sim_driver;

std::string environment_value(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name) != 0 || value == nullptr) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

// The strict STCF case is authored in-process from the locked mixed-minimal
// profile (one quad, one triangle; phi_t = {1.0, 0.8}, z_b = {0.0, 0.1}).
std::filesystem::path write_case_file() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "scau_g19_mixed_minimal.stcf.nc";
    std::filesystem::remove(path);
    scau::stcf::write_stcf_case(path, scau::stcf::make_mixed_minimal_case());
    return path;
}

}  // namespace

TEST(GoldenSurface2DTriCouplingReal, RunLoopDrivesRealSolverAndBothRealEngines) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    if (library.empty() || mdu.empty()) {
        GTEST_SKIP() << "G19 requires the real D-Flow FM runtime environment";
    }

    const std::filesystem::path case_path = write_case_file();

    sim::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = 180.0;
    config.dt_couple = 60.0;  // matches the authored single_reach BMI time step
    config.dt_surface = 0.05;
    config.dt_swmm = 60.0;
    config.dt_dflowfm = 60.0;
    config.enable_swmm = true;
    config.enable_dflowfm = true;
    config.stcf_case_path = case_path.string();
    config.swmm_inp_path = std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_river_datum.inp";
    config.dflowfm_mdu_path = mdu;
    config.initial_eta = 2.0;  // wets both cells: h = {2.0, 1.9}
    // mixed-minimal carries a spatial phi_t jump (1.0 / 0.8); exact
    // phi_t*h*A closure requires the opt-in G23 CVC correction.
    config.enable_cvc_spatial_phi_t_correction = true;
    // G19 keeps the run-loop gate disabled because the governed real-engine
    // APIs do not yet expose complete external flux scope. The test constructs
    // and asserts the scope-incomplete audit report after the 3-epoch run.
    config.enable_whole_system_mass_audit = false;
    config.engine_mode = sim::EngineMode::real;
    config.river_water_level_variable = "s1";

    // Narrow structures keep head-driven weir intents inside Q_limit for the
    // 1 m2-scale mixed-minimal cells at dt_sub = 60 s.
    sim::SurfaceDrainageLinkConfig drainage{};
    drainage.cell = 0U;
    drainage.node_name = "J1";
    drainage.crest_level = 1.9;
    drainage.exchange_width = 0.1;
    config.surface_drainage.push_back(drainage);

    sim::SurfaceRiverLinkConfig river{};
    river.cell = 1U;
    river.location_id = 0;
    river.native_lateral_id = "lat1";
    river.crest_level = 1.9;
    river.exchange_width = 0.1;
    config.surface_river.push_back(river);

    sim::DrainageRiverLinkConfig interface_link{};
    interface_link.outfall_name = "O1";
    interface_link.river_location_id = 0;
    interface_link.q_capacity = 1.0;
    interface_link.drive_outfall_stage = true;
    config.drainage_river.push_back(interface_link);

    sim::SimDriver driver;
    driver.configure(config);

    scau::coupling::drainage::SwmmEngine swmm;
    scau::coupling::river::DFlowFMEngine dflowfm(library);
    swmm.initialize(config.swmm_inp_path);
    dflowfm.initialize(config.dflowfm_mdu_path);

    const double swmm_storage_initial = swmm.total_stored_volume();
    const double dflow_storage_initial =
        scau::coupling::driver::observe_dflowfm_volume(dflowfm).volume;

    sim::RunLoopHooks hooks{};
    hooks.resolve_swmm_node = [&swmm](const std::string& node_name) {
        return swmm.node_index(node_name);
    };
    hooks.swmm_elapsed_time = [&swmm]() { return swmm.elapsed_time(); };
    hooks.dflowfm_elapsed_time = [&dflowfm]() { return dflowfm.elapsed_time(); };
    hooks.swmm_storage_volume = [&swmm]() { return swmm.total_stored_volume(); };

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);

    EXPECT_EQ(result.final_state, sim::SimDriverState::completed);
    EXPECT_EQ(result.committed_epochs, 3U);
    ASSERT_EQ(result.summary.epochs.size(), 3U);

    // Real engines advanced exactly one dt_couple per committed epoch. The
    // real SWMM engine steps in its own routing increments and may overshoot
    // the requested target by up to one ROUTING_STEP (5 s in this case).
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 180.0);
    EXPECT_GE(swmm.elapsed_time(), 180.0 - 1.0e-6);
    EXPECT_LT(swmm.elapsed_time(), 185.0);

    // Head-driven drains moved real volume off the surface.
    EXPECT_GT(result.summary.total_drained_volume, 0.0);

    // Conservative write-back identity over the whole run: the initial
    // physical storage of the profile is 1.0*2.0*1.0 (quad) plus
    // 0.8*1.9*0.5 (triangle) = 2.76 m3 with wall-only boundaries.
    const double initial_volume = 1.0 * 2.0 * 1.0 + 0.8 * 1.9 * 0.5;
    EXPECT_NEAR(result.summary.final_surface_physical_volume +
                    result.summary.total_drained_volume -
                    result.summary.total_returned_volume,
                initial_volume, 1.0e-9);
    EXPECT_DOUBLE_EQ(result.summary.total_boundary_inflow_volume, 0.0);

    // Deficit ledger is finite and non-negative; real river stage stays finite.
    EXPECT_GE(result.summary.final_coupling_deficit_volume, 0.0);
    EXPECT_TRUE(std::isfinite(result.summary.final_coupling_deficit_volume));
    EXPECT_TRUE(std::isfinite(dflowfm.get_value("s1", 0)));

    // Every committed epoch stayed inside the CFL contract and carries a
    // committed checkpoint record (M269 epoch commit protocol).
    for (const sim::EpochRecord& record : result.summary.epochs) {
        EXPECT_LE(record.max_cell_cfl, 1.0);
        EXPECT_GE(record.coupling_surface_mass_after, 0.0);
        EXPECT_EQ(record.checkpoint_status, "committed");
    }
    EXPECT_TRUE(result.summary.recovery_action.empty());

    // M270 real-scope evidence: storage providers are complete, but the
    // governed wrappers do not yet expose complete cumulative external
    // source/sink terms. A zero-external assumption produces a raw 141.8 m3
    // residual; the honest verdict is scope-incomplete REVIEW_REQUIRED, not a
    // relaxed-tolerance conservation claim.
    const double swmm_storage_final = swmm.total_stored_volume();
    const double dflow_storage_final =
        scau::coupling::driver::observe_dflowfm_volume(dflowfm).volume;
    scau::coupling::driver::WholeSystemMassSample mass_baseline{};
    mass_baseline.epoch = 0U;
    mass_baseline.logical_time = 0.0;
    mass_baseline.surface_volume = initial_volume;
    mass_baseline.surface_reference_volume = initial_volume;
    mass_baseline.swmm_storage_volume = swmm_storage_initial;
    mass_baseline.dflowfm_volume = dflow_storage_initial;
    scau::coupling::driver::WholeSystemMassSample mass_current{};
    mass_current.epoch = 3U;
    mass_current.logical_time = 180.0;
    mass_current.surface_volume = result.summary.final_surface_physical_volume;
    mass_current.surface_reference_volume = result.summary.final_surface_physical_volume;
    mass_current.coupling_deficit_volume =
        result.summary.final_coupling_deficit_volume;
    mass_current.swmm_storage_volume = swmm_storage_final;
    mass_current.dflowfm_volume = dflow_storage_final;
    const auto mass_report = scau::coupling::driver::audit_whole_system_mass(
        mass_baseline, mass_current);
    EXPECT_FALSE(mass_report.scope_complete);
    EXPECT_FALSE(mass_report.conserved);
    EXPECT_EQ(mass_report.verdict,
              scau::coupling::driver::WholeSystemMassVerdict::review_required);
    EXPECT_GT(std::abs(mass_report.residual), 100.0);
    EXPECT_LT(std::abs(mass_report.residual), 200.0);

    swmm.finalize();
    dflowfm.finalize();
}
