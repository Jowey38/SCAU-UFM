#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_engine.hpp"

namespace {

std::string case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_external_minimal.inp";
}

}  // namespace

TEST(GoldenSwmmExternalNet, ClosesRoutingContinuityAndSeparatesApiLateral) {
    scau::coupling::drainage::SwmmEngine engine;
    engine.initialize(case_path());
    const auto before = engine.observe_external_net_volume();
    ASSERT_TRUE(before.scope_complete);

    const int node = engine.node_index("J1");
    engine.set_node_lateral_inflow(node, 0.05);
    engine.step(60.0);
    const auto after = engine.observe_external_net_volume();
    ASSERT_TRUE(after.scope_complete);
    ASSERT_GT(after.api_lateral_inflow_m3, 0.0);

    const double total_inflow = after.dw_inflow_m3 + after.ww_inflow_m3 +
        after.gw_inflow_m3 + after.ii_inflow_m3 + after.ex_inflow_m3;
    const double total_outflow = after.flooding_m3 + after.outflow_m3 +
        after.evap_loss_m3 + after.seep_loss_m3;
    const double continuity_residual = after.final_storage_m3 -
        after.initial_storage_m3 - (total_inflow - total_outflow);
    // SWMM 5.2.4 KINWAVE reports a small native routing residual for this
    // authored one-minute case. Lock that engineering envelope separately from
    // the exact component/de-duplication identities below.
    EXPECT_NEAR(continuity_residual, 0.0, 6.0e-2);

    const double expected_routing_external = total_inflow - total_outflow;
    EXPECT_NEAR(after.routing_external_net_volume_m3,
                expected_routing_external, 1.0e-12);
    EXPECT_NEAR(after.external_net_volume_m3,
                expected_routing_external - after.api_lateral_inflow_m3, 1.0e-12);
    EXPECT_LT(after.external_net_volume_m3,
              after.routing_external_net_volume_m3);
    engine.finalize();
}
