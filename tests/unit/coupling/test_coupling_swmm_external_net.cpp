#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_engine.hpp"

namespace {

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
}

}  // namespace

TEST(CouplingSwmmExternalNet, ReportsGovernedRoutingComponents) {
    scau::coupling::drainage::SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const auto initial = engine.observe_external_net_volume();
    ASSERT_TRUE(initial.scope_complete);
    EXPECT_TRUE(std::isfinite(initial.external_net_volume_m3));
    EXPECT_DOUBLE_EQ(initial.api_lateral_inflow_m3, 0.0);

    const int node = engine.node_index("J1");
    engine.set_node_lateral_inflow(node, 0.05);
    engine.step(60.0);
    const auto current = engine.observe_external_net_volume();
    ASSERT_TRUE(current.scope_complete);
    EXPECT_GT(current.api_lateral_inflow_m3, 0.0);
    EXPECT_TRUE(std::isfinite(current.ex_inflow_m3));
    EXPECT_TRUE(std::isfinite(current.final_storage_m3));
    EXPECT_TRUE(std::isfinite(current.external_net_volume_m3));

    // Preserve the routed SWMM identity, then remove the API-owned lateral
    // component exactly once for the whole-system audit scope.
    const double expected_routing_external =
        current.dw_inflow_m3 + current.ww_inflow_m3 + current.gw_inflow_m3 +
        current.ii_inflow_m3 + current.ex_inflow_m3 - current.outflow_m3 -
        current.flooding_m3 - current.evap_loss_m3 - current.seep_loss_m3;
    EXPECT_DOUBLE_EQ(current.routing_external_net_volume_m3,
                     expected_routing_external);
    EXPECT_DOUBLE_EQ(current.external_net_volume_m3,
                     expected_routing_external - current.api_lateral_inflow_m3);

    engine.finalize();
}
