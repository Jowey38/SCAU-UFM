#include <cmath>
#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"
#include "coupling/drainage/swmm_engine.hpp"
#include "coupling/river/dflowfm_engine.hpp"

namespace {

std::string environment_value(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name) != 0 || value == nullptr) return {};
    std::string result(value); std::free(value); return result;
#else
    const char* value = std::getenv(name); return value == nullptr ? std::string{} : std::string(value);
#endif
}

scau::coupling::core::CouplingState make_state() {
    return scau::coupling::core::CouplingState{{
        {.volume = 100.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 1.0, .h = 1.0, .area = 100.0},
    }};
}

}  // namespace

TEST(GoldenTriCouplingRealMinimal, SurfacePipeRiverAndBackwaterRunThroughBothRealEngines) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    if (library.empty() || mdu.empty()) {
        GTEST_SKIP() << "real tri-coupling Golden requires D-Flow FM runtime env";
    }

    scau::coupling::drainage::SwmmEngine swmm;
    scau::coupling::river::DFlowFMEngine dflowfm(library);
    swmm.initialize(std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_river_datum.inp");
    dflowfm.initialize(mdu);
    const int j1 = swmm.node_index("J1");
    const int o1 = swmm.node_index("O1");

    auto state = make_state();
    scau::coupling::driver::TriCouplingStepConfig first{
        .surface_drainage = {{.cell_index = 0U, .node_id = j1,
                              .q_request = 0.05, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 0U, .location_id = 0,
                           .q_request = 0.0, .priority_weight = 1.0}},
        .drainage_river = {{.outfall_node_id = o1, .river_location_id = 0,
                            .q_capacity = 1.0, .drive_outfall_stage = true}},
        .river_lateral_ids = {{.location_id = 0, .native_lateral_id = "lat1"}},
        .river_water_level_variable = "s1",
    };

    const auto report1 = scau::coupling::driver::advance_tri_coupling_step(
        state, swmm, dflowfm, first, 60.0);
    ASSERT_EQ(report1.surface_decisions.size(), 2U);
    EXPECT_NEAR(
        report1.surface_mass_before.surface_mass - report1.surface_mass_after.surface_mass,
        3.0,
        1.0e-9);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), 0.05, 1.0e-6);
    EXPECT_GT(swmm.get_node_inflow(o1), 0.0);
    EXPECT_TRUE(std::isfinite(dflowfm.get_value("s1", 0)));

    // The first step routed surface water through the pipe. On the second
    // step, the previous outfall inflow is offered to the real river lateral;
    // new surface requests are zero, so surface mass must remain unchanged.
    const double routed_outfall = swmm.get_node_inflow(o1);
    auto second = first;
    second.surface_drainage[0].q_request = 0.0;
    second.surface_river[0].q_request = 0.0;
    const double river_stage_before = dflowfm.get_value("s1", 0);
    const auto report2 = scau::coupling::driver::advance_tri_coupling_step(
        state, swmm, dflowfm, second, 60.0);

    ASSERT_EQ(report2.interface_decisions.size(), 1U);
    EXPECT_NEAR(report2.interface_decisions[0].q_granted, routed_outfall, 1.0e-9);
    EXPECT_NEAR(report2.interface_decisions[0].v_granted, routed_outfall * 60.0, 1.0e-9);
    EXPECT_DOUBLE_EQ(report2.interface_decisions[0].v_unmet, 0.0);
    EXPECT_NEAR(
        dflowfm.get_value("laterals/lat1/water_discharge", 0),
        routed_outfall,
        1.0e-9);
    EXPECT_DOUBLE_EQ(report2.surface_mass_after.surface_mass, report2.surface_mass_before.surface_mass);
    EXPECT_TRUE(std::isfinite(dflowfm.get_value("s1", 0)));
    EXPECT_NEAR(swmm.get_node_head(o1), river_stage_before, 1.0e-6);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 120.0);

    swmm.finalize();
    dflowfm.finalize();
}
