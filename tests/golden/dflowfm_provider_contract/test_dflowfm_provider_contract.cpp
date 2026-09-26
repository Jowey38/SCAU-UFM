// G39 dflowfm_provider_contract: the C3 provider contract end to end on the
// deterministic mock tri-model loop, THROUGH the artefact both sides share.
//
// The driver freezes boundary identity from the config, binds a native
// water-balance provider (harness-owned here: cumulative api-lateral inflow
// integrated from the compound native lateral the driver writes), validates
// the per-epoch native series inside the commit gate, and writes the
// `dflowfm` provenance block + per-epoch `dflowfm_native` into
// run_summary.json. The Python linker (`scau_results link`) then re-validates
// that SAME file independently and must reach `provenance_validated` with
// NO_GAP accounting (native api-lateral net == CouplingLib river ledger net).
//
// Locks C3-01 identity, C3-02/03 exchange-kind/capability, C3-04 time/series,
// C3-07 MDU byte identity and C3-08 accounting on one artefact, and that a
// tampered MDU is refused by the consumer (LINKAGE_REJECTED). Real-engine
// evidence for the same contract lives in G19 (gateway) and the recorded
// scau_sim + QGIS smoke run; this golden is the CI-gating deterministic lock.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/driver/dflowfm_external_net_provider.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "run_summary.hpp"
#include "sim_driver.hpp"
#include "stcf/case_profiles.hpp"
#include "stcf/io_netcdf.hpp"
#include "surface_timeseries.hpp"

namespace {

namespace sim = scau::apps::sim_driver;

std::filesystem::path scratch_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "scau_g39_dflowfm_provider_contract";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::string quote(const std::filesystem::path& p) { return "\"" + p.string() + "\""; }

// Runs `python -m scau_results link ...`; returns (exit code, output json text or stderr).
std::pair<int, std::string> run_linker(const std::filesystem::path& summary,
                                       const std::filesystem::path& mdu,
                                       const std::filesystem::path& out) {
    const std::filesystem::path py(SCAU_PYTHON_EXECUTABLE);
    const std::filesystem::path pkg(SCAU_PYTHON_PACKAGE_DIR);
    const auto err = out.parent_path() / (out.stem().string() + ".stderr.txt");
    std::filesystem::remove(out);
    std::string cmd = quote(py) + " -m scau_results link " + quote(summary) +
                      " --dflowfm-mdu " + quote(mdu) + " --output " + quote(out) +
                      " 2> " + quote(err);
#ifdef _WIN32
    cmd = "cmd /C \"set PYTHONPATH=" + pkg.string() + "&& " + cmd + "\"";
#else
    cmd = "PYTHONPATH=" + quote(pkg) + " " + cmd;
#endif
    const int code = std::system(cmd.c_str());
    return {code, std::filesystem::is_regular_file(out) ? read_text(out) : read_text(err)};
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

TEST(GoldenDFlowFMProviderContract, DriverArtefactIsConsumedByTheLinkerWithNoGap) {
    const auto dir = scratch_dir();
    const auto stcf = dir / "mixed_minimal.stcf.nc";
    scau::stcf::write_stcf_case(stcf, scau::stcf::make_mixed_minimal_case());
    const auto mdu = dir / "single_reach.mdu";
    std::ofstream(mdu, std::ios::binary) << "[model]\nProgram = D-Flow FM\n[geometry]\nNetFile = reach_net.nc\n";
    const auto summary_path = dir / "run_summary.json";

    sim::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = 10.0;
    config.dt_couple = 1.0;
    config.dt_surface = 0.05;
    config.dt_swmm = 1.0;
    config.dt_dflowfm = 1.0;
    config.enable_swmm = true;
    config.enable_dflowfm = true;
    config.stcf_case_path = stcf.string();
    config.swmm_inp_path = "mock.inp";
    config.dflowfm_mdu_path = mdu.string();
    config.initial_eta = 1.0;
    config.enable_cvc_spatial_phi_t_correction = true;
    config.output_summary_path = summary_path.string();
    sim::SurfaceDrainageLinkConfig drainage{};
    drainage.cell = 0U; drainage.node_name = "11"; drainage.crest_level = 0.5; drainage.exchange_width = 1.0;
    config.surface_drainage.push_back(drainage);
    sim::SurfaceRiverLinkConfig river{};
    river.cell = 1U; river.location_id = 5; river.native_lateral_id = "lat1";
    river.crest_level = 0.5; river.exchange_width = 1.0;
    config.surface_river.push_back(river);

    sim::SimDriver driver;
    driver.configure(config);
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize(config.dflowfm_mdu_path);
    swmm.set_node_head_fixture(11, 0.0);
    dflowfm.set_water_level_fixture(5, 0.2);

    // Harness-owned native provider: integrates the compound native lateral the
    // driver wrote (dt_couple = 1 s) into a cumulative api-lateral inflow.
    double cumulative_lateral = 0.0;
    sim::RunLoopHooks hooks{};
    hooks.dflowfm_elapsed_time = [&]() {
        cumulative_lateral += dflowfm.get_value("laterals/lat1/water_discharge", 0) * config.dt_couple;
        return dflowfm.elapsed_time();
    };
    hooks.dflowfm_native_observation = [&]() {
        scau::coupling::driver::DFlowFMExternalNetObservation o{};
        o.scope_complete = true;
        o.storage_m3 = 1000.0 + cumulative_lateral;
        o.api_lateral_in_m3 = cumulative_lateral;
        return o;
    };

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);
    swmm.finalize();
    dflowfm.finalize();
    ASSERT_EQ(result.summary.outcome, "completed") << result.summary.reason;
    ASSERT_EQ(result.committed_epochs, 10U);
    EXPECT_GT(result.summary.total_dflowfm_lateral_volume, 0.0);   // the river link really moved water
    sim::write_summary_json(summary_path, result.summary);

    // Producer side of the contract as written to disk.
    const std::string json = read_text(summary_path);
    EXPECT_TRUE(contains(json, "\"dflowfm\": {\"provider_id\": \"dflowfm\", \"capability\": \"dflowfm.external_net.v1\""));
    EXPECT_TRUE(contains(json, "\"mdu_hash\": \"" + sim::hash_file_bytes(mdu) + "\""));
    EXPECT_TRUE(contains(json, "\"native_observed\": true"));
    EXPECT_TRUE(contains(json, "\"boundaries\": [{\"boundary_id\": \"lat1\", \"provider_object_id\": 5, "
                               "\"surface_cell\": 1, \"exchange_kind\": \"api_lateral\"}]"));
    for (const auto& record : result.summary.epochs) {
        ASSERT_TRUE(record.has_dflowfm_native);
    }

    // Consumer side: the independent Python validator on the SAME artefact.
    const auto [code, linked] = run_linker(summary_path, mdu, dir / "linked.json");
    ASSERT_EQ(code, 0) << linked;
    EXPECT_TRUE(contains(linked, "\"dflowfm_contract_bound\": true")) << linked;
    EXPECT_TRUE(contains(linked, "\"mdu_bound\": true")) << linked;
    EXPECT_TRUE(contains(linked, "\"status\": \"provenance_validated\"")) << linked;
    EXPECT_TRUE(contains(linked, "\"diagnostic_code\": \"NO_GAP\"")) << linked;
    EXPECT_FALSE(contains(linked, "LATERAL_INTEGRATION_GAP")) << linked;

    // C3-07: a byte-tampered MDU is not this run's river input.
    const auto tampered = dir / "tampered.mdu";
    std::ofstream(tampered, std::ios::binary) << read_text(mdu) << "\n# tampered\n";
    const auto [bad_code, rejection] = run_linker(summary_path, tampered, dir / "linked_tampered.json");
    EXPECT_NE(bad_code, 0);
    EXPECT_TRUE(contains(rejection, "LINKAGE_REJECTED")) << rejection;
    EXPECT_TRUE(contains(rejection, "MDU bytes do not match")) << rejection;
}
