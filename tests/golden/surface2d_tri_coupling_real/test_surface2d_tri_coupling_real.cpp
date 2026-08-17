#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_external_net_provider.hpp"
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
    // M272 (SWMM) + M276 (D-Flow FM) external-net providers complete the
    // real external flux scope: the run-loop audit gates every committed
    // epoch with the documented real-engine tolerance (M270 defaults;
    // strict epsilon_deficit still applies as the floor).
    config.enable_whole_system_mass_audit = true;
    // Documented real-SWMM internal continuity gap bound (M277 evidence):
    // the wet-start fixture measured -0.030 m3 of engine-internal recession
    // error over two epochs (standalone probe reproduces it without any
    // coupling); 0.05 m3 bounds it with margin. The COUPLING residual is
    // still held to the strict M270 tolerance and measured fp-exact.
    config.mass_audit_engine_internal_gap_absolute = 0.05;
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

    // The SWMM->river interface leg is intentionally NOT part of the G19
    // conservation fixture. Fresh real-audit evidence (M277) exposed two
    // ungoverned mass paths in the current explicit interface design:
    //   bug-208: stage-driven outfall backwater imports untracked mass
    //     (+37.79 m3/epoch) with no routing-totals class and no CouplingLib
    //     debit from D-Flow;
    //   bug-210: rate-sampled outfall->river injection is not volume
    //     conservative (injected 0.6207 m3 vs emitted 0.5243 m3 over two
    //     epochs; the seam creates the difference).
    // Until M278 lands a volume-conservative, ledger-backed 1D-1D interface,
    // G19 audits the tri-model system without that leg; O1 outflow is a
    // genuinely external, massbal-tracked loss. The audit stays armed: any
    // config re-enabling the interface trips REVIEW_REQUIRED on drift.
    // (G17 keeps behavioral coverage of the interface exchange itself.)

    sim::SimDriver driver;
    driver.configure(config);

    scau::coupling::drainage::SwmmEngine swmm;
    scau::coupling::river::DFlowFMEngine dflowfm(library);
    swmm.initialize(config.swmm_inp_path);
    dflowfm.initialize(config.dflowfm_mdu_path);

    const double swmm_storage_initial = swmm.total_stored_volume();
    const auto dflow_native_initial =
        scau::coupling::driver::observe_dflowfm_external_net(dflowfm);
    ASSERT_TRUE(dflow_native_initial.scope_complete);
    const double dflow_storage_initial = dflow_native_initial.storage_m3;
    const double swmm_external_initial =
        swmm.observe_external_net_volume().external_net_volume_m3;
    const double dflow_external_initial =
        dflow_native_initial.external_net_volume_m3;

    sim::RunLoopHooks hooks{};
    hooks.resolve_swmm_node = [&swmm](const std::string& node_name) {
        return swmm.node_index(node_name);
    };
    hooks.swmm_elapsed_time = [&swmm]() { return swmm.elapsed_time(); };
    hooks.dflowfm_elapsed_time = [&dflowfm]() { return dflowfm.elapsed_time(); };
    hooks.swmm_storage_volume = [&swmm]() { return swmm.total_stored_volume(); };
    hooks.swmm_external_net_volume = [&swmm]() {
        const auto observation = swmm.observe_external_net_volume();
        if (!observation.scope_complete) {
            throw std::runtime_error("SWMM external-net observation is incomplete");
        }
        return observation.external_net_volume_m3;
    };
    hooks.dflowfm_external_net_volume = [&dflowfm]() {
        const auto observation =
            scau::coupling::driver::observe_dflowfm_external_net(dflowfm);
        if (!observation.scope_complete) {
            throw std::runtime_error("D-Flow FM external-net observation is incomplete");
        }
        return observation.external_net_volume_m3;
    };
    hooks.dflowfm_storage_volume = [&dflowfm]() {
        const auto observation =
            scau::coupling::driver::observe_dflowfm_external_net(dflowfm);
        if (!observation.scope_complete) {
            throw std::runtime_error("D-Flow FM storage observation is incomplete");
        }
        return observation.storage_m3;
    };

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);

    EXPECT_EQ(result.final_state, sim::SimDriverState::completed)
        << "outcome=" << result.summary.outcome
        << " reason=" << result.summary.reason
        << " final_residual="
        << result.summary.final_whole_system_mass_residual
        << " max_abs_residual="
        << result.summary.max_abs_whole_system_mass_residual;
    EXPECT_EQ(result.committed_epochs, 3U);
    EXPECT_EQ(result.summary.epochs.size(), 3U);

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

    // M272 + M276 completion: both engine external-net scopes are bound, so
    // the whole-system audit is scope-complete and must CONSERVE within the
    // documented real-engine tolerance (M270 defaults; NOT widened here).
    // The run loop already gated every committed epoch on this audit; the
    // explicit final audit below re-derives the verdict as recorded evidence.
    const double swmm_storage_final = swmm.total_stored_volume();
    const auto dflow_native_final =
        scau::coupling::driver::observe_dflowfm_external_net(dflowfm);
    ASSERT_TRUE(dflow_native_final.scope_complete);
    const double swmm_external_final =
        swmm.observe_external_net_volume().external_net_volume_m3;
    scau::coupling::driver::WholeSystemMassSample mass_baseline{};
    mass_baseline.epoch = 0U;
    mass_baseline.logical_time = 0.0;
    mass_baseline.surface_volume = initial_volume;
    mass_baseline.surface_reference_volume = initial_volume;
    mass_baseline.swmm_storage_volume = swmm_storage_initial;
    mass_baseline.dflowfm_volume = dflow_storage_initial;
    mass_baseline.swmm_external_net_volume = swmm_external_initial;
    mass_baseline.dflowfm_external_net_volume = dflow_external_initial;
    mass_baseline.swmm_coupling_lateral_volume = 0.0;
    mass_baseline.dflowfm_coupling_lateral_volume = 0.0;
    scau::coupling::driver::WholeSystemMassSample mass_current{};
    mass_current.epoch = 3U;
    mass_current.logical_time = 180.0;
    mass_current.surface_volume = result.summary.final_surface_physical_volume;
    mass_current.surface_reference_volume = result.summary.final_surface_physical_volume;
    mass_current.coupling_deficit_volume =
        result.summary.final_coupling_deficit_volume;
    mass_current.swmm_storage_volume = swmm_storage_final;
    mass_current.dflowfm_volume = dflow_native_final.storage_m3;
    mass_current.swmm_external_net_volume = swmm_external_final;
    mass_current.dflowfm_external_net_volume =
        dflow_native_final.external_net_volume_m3;
    // M277 in-system return correction from the CouplingLib driver ledger.
    mass_current.cumulative_engine_internal_return_volume =
        result.summary.total_engine_internal_return_volume;
    mass_current.swmm_coupling_lateral_volume =
        result.summary.total_swmm_lateral_volume;
    mass_current.dflowfm_coupling_lateral_volume =
        result.summary.total_dflowfm_lateral_volume;
    scau::coupling::driver::WholeSystemMassTolerance mass_tolerance{};
    mass_tolerance.strict = false;
    mass_tolerance.engine_residual_absolute =
        config.mass_audit_engine_residual_absolute;
    mass_tolerance.engine_residual_relative =
        config.mass_audit_engine_residual_relative;
    mass_tolerance.engine_internal_gap_absolute =
        config.mass_audit_engine_internal_gap_absolute;
    const auto mass_report = scau::coupling::driver::audit_whole_system_mass(
        mass_baseline, mass_current, mass_tolerance);
    std::cout << std::setprecision(15)
              << "[g19-audit] surface0=" << initial_volume
              << " surfaceF=" << result.summary.final_surface_physical_volume
              << " swmm0=" << swmm_storage_initial
              << " swmmF=" << swmm_storage_final
              << " dflow0=" << dflow_storage_initial
              << " dflowF=" << dflow_native_final.storage_m3
              << " swmm_ext_delta=" << (swmm_external_final - swmm_external_initial)
              << " dflow_ext_delta="
              << (dflow_native_final.external_net_volume_m3 - dflow_external_initial)
              << " internal_return="
              << result.summary.total_engine_internal_return_volume
              << " drained=" << result.summary.total_drained_volume
              << " returned=" << result.summary.total_returned_volume
              << " residual=" << mass_report.residual
              << " swmm_elapsed=" << swmm.elapsed_time()
              << " dflow_elapsed=" << dflowfm.elapsed_time()
              << " ledger_swmm_lateral=" << result.summary.total_swmm_lateral_volume
              << " deficit=" << result.summary.final_coupling_deficit_volume << "\n";
    {
        const auto swmm_obs = swmm.observe_external_net_volume();
        std::cout << std::setprecision(15)
                  << "[g19-swmm] dw=" << swmm_obs.dw_inflow_m3
                  << " ww=" << swmm_obs.ww_inflow_m3
                  << " gw=" << swmm_obs.gw_inflow_m3
                  << " ii=" << swmm_obs.ii_inflow_m3
                  << " ex=" << swmm_obs.ex_inflow_m3
                  << " flooding=" << swmm_obs.flooding_m3
                  << " outflow=" << swmm_obs.outflow_m3
                  << " evap=" << swmm_obs.evap_loss_m3
                  << " seep=" << swmm_obs.seep_loss_m3
                  << " init_storage=" << swmm_obs.initial_storage_m3
                  << " final_storage=" << swmm_obs.final_storage_m3
                  << " routing_net=" << swmm_obs.routing_external_net_volume_m3
                  << " api_lateral=" << swmm_obs.api_lateral_inflow_m3
                  << " external_net=" << swmm_obs.external_net_volume_m3 << "\n";
    }
    EXPECT_TRUE(mass_report.scope_complete);
    EXPECT_TRUE(mass_report.conserved);
    EXPECT_EQ(mass_report.verdict,
              scau::coupling::driver::WholeSystemMassVerdict::conserved);
    // The COUPLING residual (engine-internal gaps decomposed out) is held to
    // the strict documented tolerance and is expected fp-exact in practice.
    EXPECT_LE(std::abs(mass_report.coupling_residual),
              mass_report.applied_tolerance);
    EXPECT_LT(std::abs(mass_report.coupling_residual), 1.0e-6);
    // Engine-internal gaps stay inside their documented bounds; D-Flow's is
    // at native volume-error scale, SWMM's at its measured recession error.
    ASSERT_TRUE(mass_report.swmm_internal_gap_volume.has_value());
    ASSERT_TRUE(mass_report.dflowfm_internal_gap_volume.has_value());
    EXPECT_LE(std::abs(*mass_report.swmm_internal_gap_volume),
              config.mass_audit_engine_internal_gap_absolute);
    EXPECT_LT(std::abs(*mass_report.dflowfm_internal_gap_volume), 1.0e-6);
    // Total residual (including engine gaps) stays far below the historical
    // 141.8 m3 missing-scope signal.
    EXPECT_LT(std::abs(mass_report.residual), 1.0e-1);
    EXPECT_LT(std::abs(result.summary.max_abs_whole_system_mass_residual), 1.0e-1);

    swmm.finalize();
    dflowfm.finalize();
}
