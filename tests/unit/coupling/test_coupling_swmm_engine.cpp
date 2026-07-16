#include <algorithm>
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_engine.hpp"

namespace {

using scau::coupling::drainage::SwmmEngine;
using scau::coupling::drainage::SwmmEngineError;

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
}

std::string manhole_overflow_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_manhole_overflow.inp";
}

}  // namespace

TEST(CouplingSwmmEngine, InitializeStepFinalizeLifecycle) {
    SwmmEngine engine;
    EXPECT_FALSE(engine.initialized());

    engine.initialize(minimal_case_path());
    EXPECT_TRUE(engine.initialized());
    EXPECT_EQ(engine.node_count(), 3);
    EXPECT_GE(engine.node_index("J1"), 0);
    EXPECT_GE(engine.node_index("O1"), 0);
    EXPECT_GE(engine.link_index("C1"), 0);

    engine.step(60.0);
    EXPECT_GT(engine.elapsed_time(), 0.0);
    EXPECT_GE(engine.elapsed_time(), 60.0 - 5.0);

    engine.finalize();
    EXPECT_FALSE(engine.initialized());
}

TEST(CouplingSwmmEngine, LateralInflowRoutesTowardOutfall) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const int j1 = engine.node_index("J1");
    const int o1 = engine.node_index("O1");

    engine.set_node_lateral_inflow(j1, 0.05);

    engine.step(600.0);

    // SWMM exposes the routed lateral inflow only after a routing step has
    // consumed the API inflow buffer (upstream getNodeValue semantics).
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.05, 1.0e-6);

    // Sustained lateral inflow must raise water above the J1 invert and reach
    // the outfall as inflow (head is reported in project units: metres).
    EXPECT_GT(engine.get_node_head(j1), 10.0);
    EXPECT_GT(engine.get_node_inflow(o1), 0.0);
    EXPECT_GE(engine.get_node_overflow(j1), 0.0);
    EXPECT_FALSE(engine.is_surcharged(j1));

    engine.finalize();
}

TEST(CouplingSwmmEngine, SetOutfallStageOnlyTargetsOutfalls) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const int j1 = engine.node_index("J1");
    const int o1 = engine.node_index("O1");

    EXPECT_NO_THROW(engine.set_outfall_stage(o1, 9.4));
    EXPECT_THROW(engine.set_outfall_stage(j1, 9.4), SwmmEngineError);
    EXPECT_THROW(engine.set_outfall_stage(o1, std::nan("")), SwmmEngineError);

    engine.finalize();
}

TEST(CouplingSwmmEngine, MainGraphG8EvidenceSeesRealOverflowAndSurcharge) {
    SwmmEngine engine;
    engine.initialize(manhole_overflow_case_path());

    const int j1 = engine.node_index("J1");
    double max_head = 0.0;
    double max_overflow = 0.0;
    bool saw_positive_overflow = false;
    bool saw_surcharge = false;

    for (int step = 0; step < 100; ++step) {
        engine.step(10.0);
        max_head = std::max(max_head, engine.get_node_head(j1));
        max_overflow = std::max(max_overflow, engine.get_node_overflow(j1));
        saw_positive_overflow = saw_positive_overflow || engine.get_node_overflow(j1) > 0.0;
        saw_surcharge = saw_surcharge || engine.is_surcharged(j1);
        if (saw_positive_overflow && saw_surcharge) {
            break;
        }
    }

    EXPECT_TRUE(saw_positive_overflow);
    EXPECT_TRUE(saw_surcharge);
    EXPECT_GE(max_head, 0.45 - 1.0e-9);
    EXPECT_GT(max_overflow, 0.30);

    engine.finalize();
}

TEST(CouplingSwmmEngine, FailsClosedOnInvalidUse) {
    SwmmEngine engine;
    EXPECT_THROW(engine.step(5.0), SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_head(0)), SwmmEngineError);
    EXPECT_THROW(engine.initialize(""), SwmmEngineError);
    EXPECT_THROW(engine.initialize("does_not_exist.inp"), SwmmEngineError);
    EXPECT_FALSE(engine.initialized());

    engine.initialize(minimal_case_path());
    EXPECT_THROW(static_cast<void>(engine.get_node_head(-1)), SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_head(99)), SwmmEngineError);
    EXPECT_THROW(engine.step(0.0), SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.node_index("missing")), SwmmEngineError);
    engine.finalize();
}

TEST(CouplingSwmmEngine, SecondConcurrentEngineFailsClosed) {
    SwmmEngine first;
    first.initialize(minimal_case_path());

    SwmmEngine second;
    EXPECT_THROW(second.initialize(minimal_case_path()), SwmmEngineError);
    EXPECT_FALSE(second.initialized());

    first.finalize();

    // Once the first project is closed the solver becomes available again.
    EXPECT_NO_THROW(second.initialize(minimal_case_path()));
    second.finalize();
}
