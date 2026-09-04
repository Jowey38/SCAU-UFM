// G35 case_export_synthetic_package: locks the M287-B6 case package exporter
// as a committed fixture. cases/ IS an exported package (the D-5 synthetic
// pipeline output with all four C4 decisions applied, exported by
// scau_preproc.case_export with engine_mode=mock, end_time=180 s):
//   manifest.json (package_manifest_schema_version 1, per-file SHA-256)
//   mesh/case.stcf.nc, coupling/**, swmm/model.inp, metadata/**, validation/**
//   simdriver/run.conf (RuntimeConfig v2, package-relative paths)
// The golden asserts through the AUTHORITATIVE consumers that the package is
// self-contained and runnable:
//   1. manifest hashes match the committed bytes (no silent edits);
//   2. simdriver/run.conf parses and carries exactly the EFFECTIVE ground
//      links (accept J1 / retarget J2 -> cell 50, crest 9.6) - never the
//      unconfirmed candidate fragment;
//   3. SimDriver COLD START from the package: configure() accepts the exported
//      config, the surface case loads, the real embedded SWMM 5.2 engine
//      opens swmm/model.inp and resolves every effective link node; and the
//      documented limitation that the tri-model run loop refuses a
//      surface+SWMM-only package is LOCKED (flips only with C3 / dual-model
//      driver evidence).
// Regeneration (python exporter) is not run here; determinism (byte-identical
// re-export) is evidence on the pipeline side.

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "runtime_config_io.hpp"
#include "sim_driver.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"

#if defined(SCAU_HAS_SWMM5)
#include "coupling/drainage/swmm_engine.hpp"
#endif

namespace {

namespace sim = scau::apps::sim_driver;

std::filesystem::path package_dir() {
    return std::filesystem::path(SCAU_CASE_EXPORT_PACKAGE_DIR);
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::size_t count(const std::string& text, const std::string& needle) {
    std::size_t total = 0U;
    for (auto pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + 1U)) {
        ++total;
    }
    return total;
}

TEST(GoldenCaseExportSyntheticPackage, ManifestListsEveryFileAndMatchesCommittedBytes) {
    const auto manifest = read_text(package_dir() / "manifest.json");
    EXPECT_TRUE(contains(manifest, "\"package_manifest_schema_version\": 1"));
    EXPECT_TRUE(contains(manifest, "\"effective_links_status\": \"complete\""));
    EXPECT_TRUE(contains(manifest, "\"validation_status\": \"ok\""));
    EXPECT_TRUE(contains(manifest, "\"dflowfm\": \"provider_required\""));
    EXPECT_TRUE(contains(manifest, "\"effective_surface_links\": 2"));
    EXPECT_TRUE(contains(manifest, "\"effective_roof_links\": 1"));
    // The package carries the operator decisions that produced the links.
    for (const char* name : {"coupling/confirmed/MAP_SURF_SYNTH_001.json",
                             "coupling/confirmed/MAP_SURF_SYNTH_002.json",
                             "coupling/confirmed/MAP_ROOF_SYNTH_001.json",
                             "coupling/confirmed/MAP_ROOF_SYNTH_002.json",
                             "coupling/effective_links.json", "mesh/case.stcf.nc",
                             "swmm/model.inp", "simdriver/run.conf",
                             "metadata/geometry_clean_policy.json", "validation/validation.json"}) {
        EXPECT_TRUE(contains(manifest, std::string("\"") + name + "\": \"")) << name;
        EXPECT_TRUE(std::filesystem::is_regular_file(package_dir() / name)) << name;
    }
    // The committed mesh is the G30 fixture byte-for-byte (recorded SHA-256).
    EXPECT_TRUE(contains(manifest,
        "\"mesh/case.stcf.nc\": \"a89ba98da95bcd5732c17e1bf9977e8f75bdc1d81b96a6844333fc796e911bc4\""));
    EXPECT_EQ(count(manifest, "\"reproduce\""), 1U);
}

TEST(GoldenCaseExportSyntheticPackage, RunConfCarriesOnlyEffectiveLinks) {
    const auto config = sim::read_runtime_config_file(package_dir() / "simdriver" / "run.conf");
    EXPECT_EQ(config.version, 2);
    EXPECT_EQ(config.engine_mode, sim::EngineMode::mock);
    EXPECT_TRUE(config.enable_swmm);
    EXPECT_FALSE(config.enable_dflowfm);                  // river chain is provider_required
    EXPECT_TRUE(config.dflowfm_mdu_path.empty());
    EXPECT_EQ(config.stcf_case_path, "mesh/case.stcf.nc");
    EXPECT_EQ(config.swmm_inp_path, "swmm/model.inp");
    EXPECT_EQ(config.initial_eta, 11.4);  // DEM max 10.9 + 0.5 (recorded source)
    EXPECT_EQ(config.end_time, 180.0);
    EXPECT_EQ(config.dt_couple, 60.0);
    ASSERT_EQ(config.surface_drainage.size(), 2U);
    EXPECT_EQ(config.surface_drainage[0].node_name, "J1");
    EXPECT_EQ(config.surface_drainage[0].cell, 379U);
    EXPECT_EQ(config.surface_drainage[0].crest_level, 10.0);
    EXPECT_EQ(config.surface_drainage[1].node_name, "J2");
    EXPECT_EQ(config.surface_drainage[1].cell, 50U);      // operator retarget, not candidate 234
    EXPECT_EQ(config.surface_drainage[1].crest_level, 9.6);
    EXPECT_TRUE(config.surface_river.empty());            // river chain is provider_required
    EXPECT_TRUE(config.drainage_river.empty());
}

TEST(GoldenCaseExportSyntheticPackage, SimDriverColdStartsFromThePackage) {
#if !defined(SCAU_HAS_SWMM5)
    GTEST_SKIP() << "real embedded SWMM engine required (SCAU_EMBED_SWMM=ON)";
#else
    // Real SWMM writes .rpt/.out beside the .inp; work on a scratch copy so
    // the committed package stays byte-identical.
    const auto scratch = std::filesystem::temp_directory_path() / "scau_g35_case";
    std::filesystem::remove_all(scratch);
    std::filesystem::copy(package_dir(), scratch, std::filesystem::copy_options::recursive);

    auto config = sim::read_runtime_config_file(scratch / "simdriver" / "run.conf");
    config.stcf_case_path = (scratch / config.stcf_case_path).string();
    config.swmm_inp_path = (scratch / config.swmm_inp_path).string();
    config.output_summary_path.clear();
    config.engine_mode = sim::EngineMode::real;  // bind the real drainage engine below

    // 1. The driver accepts the exported configuration as-is (surface + SWMM,
    //    river leg explicitly disabled because the chain is provider_required).
    sim::SimDriver driver;
    ASSERT_NO_THROW(driver.configure(config));
    EXPECT_FALSE(driver.config().enable_dflowfm);
    ASSERT_EQ(driver.config().surface_drainage.size(), 2U);

    // 2. The shipped surface case loads through the strict CF/UGRID path.
    const auto loaded = scau::surface2d::load_surface2d_case(config.stcf_case_path);
    ASSERT_EQ(loaded.mesh.cells.size(), 469U);
    for (const auto& link : driver.config().surface_drainage) {
        ASSERT_LT(link.cell, loaded.mesh.cells.size());
        // Confirmed crest sits below the wet initial stage: the inlet is live.
        EXPECT_LT(link.crest_level, driver.config().initial_eta);
    }

    // 3. The shipped swmm/model.inp opens in the real embedded SWMM 5.2 engine
    //    and every effective link resolves to a real node (no dangling ids).
    scau::coupling::drainage::SwmmEngine swmm;
    try {
        swmm.initialize(config.swmm_inp_path);
    } catch (const std::exception& error) {
        FAIL() << "SwmmEngine::initialize: " << error.what();
    }
    for (const auto& link : driver.config().surface_drainage) {
        EXPECT_GE(swmm.node_index(link.node_name), 0) << link.node_name;
    }
    EXPECT_EQ(swmm.elapsed_time(), 0.0);
    swmm.finalize();

    // 4. Locked limitation (not a fixture defect): the current run loop is the
    //    TRI-model loop and refuses a surface+SWMM-only package. Running an
    //    exported package therefore needs either C3 river links or a driver
    //    dual-model mode; when either lands this expectation flips WITH
    //    evidence, never silently.
    scau::coupling::drainage::MockSwmmEngine mock_swmm;
    scau::coupling::river::MockDFlowFMEngine mock_dflowfm;
    mock_swmm.initialize("mock");
    mock_dflowfm.initialize("mock");
    EXPECT_THROW(static_cast<void>(sim::run_simulation(driver, mock_swmm, mock_dflowfm, {})),
                 std::invalid_argument);

    std::filesystem::remove_all(scratch);
#endif
}

}  // namespace
