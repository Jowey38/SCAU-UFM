#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "runtime_config_io.hpp"
#include "sim_driver.hpp"

// Fail-closed matrix for the strict key=value runtime config parser. The
// parser owns syntax; cross-field rules stay in validate_runtime_config.

namespace {

using scau::apps::sim_driver::EngineMode;
using scau::apps::sim_driver::parse_runtime_config_text;
using scau::apps::sim_driver::validate_runtime_config;

const char* const kFullConfig = R"(
# demo tri-model run
version = 2
start_time = 0.0
end_time = 600.0
dt_couple = 60.0
dt_surface = 10.0
dt_swmm = 60.0
dt_dflowfm = 60.0
enable_swmm = true
enable_dflowfm = true
stcf_case_path = case.stcf.nc
swmm_inp_path = network.inp
dflowfm_mdu_path = river.mdu
initial_eta = 1.25
h_wet = 1.0e-6
cfl_safety = 0.45
c_rollback = 1.0
enable_whole_system_mass_audit = true
n_writeoff_steps = 3
mass_audit_engine_residual_absolute = 0.0001
mass_audit_engine_residual_relative = 0.000001
engine_mode = mock
river_water_level_variable = water_level
output_summary_path = summary.json
surface_drainage_link = cell=0,node=11,crest=1.05,width=1.5,weight=1.0
surface_river_link = cell=2,location=5,lateral_id=lat1,crest=1.0,width=2.0
drainage_river_link = outfall=21,location=5,q_capacity=0.5,drive_outfall_stage=true
)";

}  // namespace

TEST(RuntimeConfigIo, ParsesFullConfigAndPassesValidation) {
    const auto config = parse_runtime_config_text(kFullConfig);

    EXPECT_EQ(config.version, 2);
    EXPECT_DOUBLE_EQ(config.end_time, 600.0);
    EXPECT_DOUBLE_EQ(config.dt_couple, 60.0);
    EXPECT_TRUE(config.enable_swmm);
    EXPECT_TRUE(config.enable_dflowfm);
    EXPECT_EQ(config.stcf_case_path, "case.stcf.nc");
    EXPECT_DOUBLE_EQ(config.initial_eta, 1.25);
    EXPECT_TRUE(config.enable_whole_system_mass_audit);
    EXPECT_EQ(config.n_writeoff_steps, 3U);
    EXPECT_DOUBLE_EQ(config.mass_audit_engine_residual_absolute, 1.0e-4);
    EXPECT_DOUBLE_EQ(config.mass_audit_engine_residual_relative, 1.0e-6);
    EXPECT_EQ(config.engine_mode, EngineMode::mock);
    EXPECT_EQ(config.output_summary_path, "summary.json");

    ASSERT_EQ(config.surface_drainage.size(), 1U);
    EXPECT_EQ(config.surface_drainage[0].cell, 0U);
    EXPECT_EQ(config.surface_drainage[0].node_name, "11");
    EXPECT_DOUBLE_EQ(config.surface_drainage[0].crest_level, 1.05);
    EXPECT_DOUBLE_EQ(config.surface_drainage[0].exchange_width, 1.5);

    ASSERT_EQ(config.surface_river.size(), 1U);
    EXPECT_EQ(config.surface_river[0].cell, 2U);
    EXPECT_EQ(config.surface_river[0].location_id, 5);
    EXPECT_EQ(config.surface_river[0].native_lateral_id, "lat1");
    EXPECT_DOUBLE_EQ(config.surface_river[0].priority_weight, 1.0);

    ASSERT_EQ(config.drainage_river.size(), 1U);
    EXPECT_EQ(config.drainage_river[0].outfall_name, "21");
    EXPECT_DOUBLE_EQ(config.drainage_river[0].q_capacity, 0.5);
    EXPECT_TRUE(config.drainage_river[0].drive_outfall_stage);

    EXPECT_NO_THROW(validate_runtime_config(config));
}

TEST(RuntimeConfigIo, RejectsUnknownAndDuplicateKeys) {
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\nno_such_key = 1\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text(
            "version = 2\ndt_couple = 60\ndt_couple = 30\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\nversion = 2\n")),
        std::invalid_argument);
}

TEST(RuntimeConfigIo, RequiresVersionFirstAndRejectsMalformedValues) {
    EXPECT_THROW(static_cast<void>(parse_runtime_config_text("dt_couple = 60\n")),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(parse_runtime_config_text("")), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\ndt_couple = sixty\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\ndt_couple = 60x\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\nenable_swmm = yes\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\nengine_mode = fast\n")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text("version = 2\nbroken line\n")),
        std::invalid_argument);
}

TEST(RuntimeConfigIo, RejectsMalformedLinkItems) {
    // Unknown subkey.
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text(
            "version = 2\nsurface_drainage_link = cell=0,node=11,crest=1,width=1,depth=2\n")),
        std::invalid_argument);
    // Missing required subkey.
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text(
            "version = 2\nsurface_drainage_link = cell=0,node=11,crest=1\n")),
        std::invalid_argument);
    // Repeated subkey.
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text(
            "version = 2\nsurface_river_link = cell=0,cell=1,location=5,"
            "lateral_id=lat1,crest=1,width=1\n")),
        std::invalid_argument);
    // Negative cell index.
    EXPECT_THROW(
        static_cast<void>(parse_runtime_config_text(
            "version = 2\nsurface_drainage_link = cell=-1,node=11,crest=1,width=1\n")),
        std::invalid_argument);
}

TEST(RuntimeConfigIo, ParsedUnsupportedVersionFailsValidation) {
    const auto config = parse_runtime_config_text("version = 1\n");
    EXPECT_THROW(validate_runtime_config(config), std::invalid_argument);
}
