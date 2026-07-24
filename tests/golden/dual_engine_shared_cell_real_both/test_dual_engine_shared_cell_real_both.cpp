#include <cmath>
#include <cstdlib>
#include <filesystem>
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
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

const scau::coupling::core::SharedExchangeDecision& decision_for(
    const std::vector<scau::coupling::core::SharedExchangeDecision>& decisions,
    scau::coupling::core::SharedExchangeEngine engine) {
    for (const auto& decision : decisions) if (decision.endpoint.engine == engine) return decision;
    throw std::logic_error("missing shared decision");
}

}  // namespace

TEST(GoldenDualEngineSharedCellRealBoth, BothRealEnginesReceiveCoreArbitratedGrants) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    if (library.empty() || mdu.empty()) {
        GTEST_SKIP() << "real dual-engine Golden requires D-Flow FM runtime env";
    }

    scau::coupling::drainage::SwmmEngine swmm;
    scau::coupling::river::DFlowFMEngine dflowfm(library);
    swmm.initialize(std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp");
    dflowfm.initialize(mdu);
    const int j1 = swmm.node_index("J1");

    auto state = make_state();
    scau::coupling::driver::TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = j1,
                              .q_request = 4.0, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 0U, .location_id = 5,
                           .q_request = 4.0, .priority_weight = 2.0}},
        .river_lateral_ids = {{.location_id = 5, .native_lateral_id = "lat1"}},
        .river_water_level_variable = "s1",
    };

    const auto report = scau::coupling::driver::advance_tri_coupling_step(
        state, swmm, dflowfm, config, 4.0);
    ASSERT_EQ(report.surface_decisions.size(), 2U);
    const auto& drainage = decision_for(
        report.surface_decisions, scau::coupling::core::SharedExchangeEngine::drainage);
    const auto& river = decision_for(
        report.surface_decisions, scau::coupling::core::SharedExchangeEngine::river);

    EXPECT_DOUBLE_EQ(drainage.allocated_limit.v_limit, 12.0);
    EXPECT_DOUBLE_EQ(river.allocated_limit.v_limit, 24.0);
    EXPECT_DOUBLE_EQ(drainage.exchange.v_granted, 12.0);
    EXPECT_DOUBLE_EQ(river.exchange.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(drainage.exchange.v_unmet, 4.0);
    EXPECT_DOUBLE_EQ(river.exchange.v_unmet, 0.0);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), 3.0, 1.0e-6);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("laterals/lat1/water_discharge", 0), 4.0);
    EXPECT_TRUE(std::isfinite(dflowfm.get_value("s1", 0)));
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_NEAR(
        report.surface_mass_before.surface_mass - report.surface_mass_after.surface_mass,
        drainage.exchange.v_granted + river.exchange.v_granted,
        1.0e-9);

    swmm.finalize();
    dflowfm.finalize();
}
