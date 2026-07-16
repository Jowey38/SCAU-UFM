#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/roof_drainage_adapter.hpp"
#include "coupling/drainage/swmm_engine.hpp"

namespace {

using scau::coupling::drainage::SwmmEngine;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
}

std::string manhole_overflow_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_manhole_overflow.inp";
}

}  // namespace

// The real roof->SWMM path: a roof drainage intent accepted by the adapter
// becomes node lateral inflow inside the real embedded SWMM solver and is
// routed toward the outfall.
TEST(RoofToRealSwmm, AcceptedRoofVolumeIsRoutedByRealEngine) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const int j1 = engine.node_index("J1");
    const int o1 = engine.node_index("O1");

    constexpr double dt_sub = 600.0;
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, dt_sub);

    adapter.begin_step();
    const RoofDrainageIntent intent{
        .source_cell_index = 0,
        .target_swmm_node_index = j1,
        .requested_volume = 30.0,  // m3 over dt_sub -> q = 0.05 m3/s
        .source_roof_area = 120.0,
    };
    const auto acceptance = adapter(intent);

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(acceptance.accepted_volume, 30.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 0.0, 1.0e-12);

    engine.step(dt_sub);

    // The accepted roof volume must appear as routed lateral inflow at the
    // target node and reach the outfall.
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.05, 1.0e-6);
    EXPECT_GT(engine.get_node_head(j1), 10.0);
    EXPECT_GT(engine.get_node_inflow(o1), 0.0);

    engine.finalize();
}

TEST(RoofToRealSwmm, MultipleRoofIntentsAccumulateOnRealEngineNode) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const int j1 = engine.node_index("J1");

    constexpr double dt_sub = 600.0;
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, dt_sub);

    adapter.begin_step();
    const auto first = adapter(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = j1,
        .requested_volume = 18.0,
        .source_roof_area = 60.0,
    });
    const auto second = adapter(RoofDrainageIntent{
        .source_cell_index = 1,
        .target_swmm_node_index = j1,
        .requested_volume = 12.0,
        .source_roof_area = 40.0,
    });

    EXPECT_EQ(first.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_EQ(second.rejection_reason, RoofDrainageRejectionReason::None);

    engine.step(dt_sub);

    // 18 + 12 = 30 m3 over 600 s -> 0.05 m3/s total at the shared node.
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.05, 1.0e-6);

    engine.finalize();
}

TEST(RoofToRealSwmm, SurchargedRealNodeRejectsRoofIntentFailClosed) {
    SwmmEngine engine;
    engine.initialize(manhole_overflow_case_path());

    const int j1 = engine.node_index("J1");

    // Drive the overloaded case until the manhole is genuinely surcharged.
    bool surcharged = false;
    for (int step = 0; step < 100 && !surcharged; ++step) {
        engine.step(10.0);
        surcharged = engine.is_surcharged(j1);
    }
    ASSERT_TRUE(surcharged);

    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 10.0);
    adapter.begin_step();
    const auto acceptance = adapter(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = j1,
        .requested_volume = 5.0,
        .source_roof_area = 50.0,
    });

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::NodeSurcharged);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 5.0, 1.0e-12);

    engine.finalize();
}

TEST(RoofToRealSwmm, OutOfRangeNodeOnRealEngineRejectsFailClosed) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 60.0);
    adapter.begin_step();
    const auto acceptance = adapter(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = 99,
        .requested_volume = 1.0,
        .source_roof_area = 10.0,
    });

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::InvalidTargetNode);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 1.0, 1.0e-12);

    engine.finalize();
}
