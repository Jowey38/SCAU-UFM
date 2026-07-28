#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/driver/tri_coupling.hpp"
#include "coupling/river/dflowfm_engine.hpp"

namespace {

std::string environment_value(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

scau::coupling::core::CouplingState make_state() {
    return scau::coupling::core::CouplingState{{
        {.volume = 10.0,
         .mass_deficit_account = {.volume = 0.0},
         .phi_t = 1.0,
         .h = 1.0,
         .area = 10.0},
    }};
}

}  // namespace

TEST(GoldenDFlowFMRiverSteady, RealAuthoredCaseAdvancesAndAcceptsMappedLateral) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    if (library.empty() || mdu.empty()) {
        GTEST_SKIP() << "real G11 requires SCAU_DFLOWFM_LIBRARY and SCAU_DFLOWFM_G11_MDU";
    }
    ASSERT_TRUE(std::filesystem::is_regular_file(library));
    ASSERT_TRUE(std::filesystem::is_regular_file(mdu));

    scau::coupling::river::DFlowFMEngine dflowfm(library);
    scau::coupling::drainage::MockSwmmEngine swmm;
    swmm.initialize("mock.inp");
    dflowfm.initialize(mdu);

    ASSERT_GE(dflowfm.variable_count(), 1);
    const double initial_stage = dflowfm.get_value("s1", 0);
    ASSERT_TRUE(std::isfinite(initial_stage));
    const auto initial_volume = scau::coupling::driver::observe_dflowfm_volume(dflowfm);
    ASSERT_TRUE(initial_volume.scope_complete);
    ASSERT_GT(initial_volume.sample_count, 0U);
    ASSERT_TRUE(std::isfinite(initial_volume.volume));
    ASSERT_GE(initial_volume.volume, 0.0);

    auto state = make_state();
    scau::coupling::driver::TriCouplingStepConfig config{
        .surface_river = {{.cell_index = 0U,
                           .location_id = 7,
                           .q_request = 0.0,
                           .priority_weight = 1.0}},
        .river_lateral_ids = {{.location_id = 7, .native_lateral_id = "lat1"}},
        .river_water_level_variable = "s1",
        .step_engines = true,
    };

    for (int step = 0; step < 100; ++step) {
        const auto report = scau::coupling::driver::advance_tri_coupling_step(
            state, swmm, dflowfm, config, 60.0);
        ASSERT_EQ(report.surface_decisions.size(), 1U);
        EXPECT_DOUBLE_EQ(report.surface_decisions[0].exchange.q_granted, 0.0);
        EXPECT_DOUBLE_EQ(report.surface_mass_after.surface_mass, report.surface_mass_before.surface_mass);
        ASSERT_TRUE(std::isfinite(dflowfm.get_value("s1", 0)));
        ASSERT_TRUE(std::isfinite(dflowfm.get_value("q1", 0)));
        const auto volume = scau::coupling::driver::observe_dflowfm_volume(dflowfm);
        ASSERT_EQ(volume.sample_count, initial_volume.sample_count);
        ASSERT_TRUE(std::isfinite(volume.volume));
        ASSERT_GE(volume.volume, 0.0);
    }

    const auto final_volume = scau::coupling::driver::observe_dflowfm_volume(dflowfm);
    EXPECT_NEAR(final_volume.volume, initial_volume.volume, 1.0e-9);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 6000.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("laterals/lat1/water_discharge", 0), 0.0);
    dflowfm.finalize();
}
