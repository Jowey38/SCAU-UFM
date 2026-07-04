#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/river/dflowfm_engine.hpp"

namespace {

#ifdef _WIN32
constexpr const char* kMissingLibraryName = "definitely_missing_dflowfm_for_scau_test.dll";
#else
constexpr const char* kMissingLibraryName = "./definitely_missing_dflowfm_for_scau_test.so";
#endif

std::string fake_library_path() {
    return SCAU_FAKE_DFLOWFM_BMI_LIBRARY;
}

}  // namespace

TEST(CouplingDFlowFMEngine, MissingRuntimeLibraryFailsClosed) {
    scau::coupling::river::DFlowFMEngine engine{kMissingLibraryName};

    EXPECT_FALSE(engine.initialized());
    EXPECT_THROW(engine.initialize("case.mdu"), scau::coupling::river::DFlowFMEngineError);
    EXPECT_FALSE(engine.initialized());

    const auto report = scau::coupling::river::make_dflowfm_engine_report(engine);
    EXPECT_FALSE(report.healthy);
    EXPECT_EQ(report.engine_id, "DFlowFM");
    EXPECT_EQ(report.error_code, "dflowfm_engine_not_initialized");
    EXPECT_DOUBLE_EQ(report.elapsed_time, 0.0);
}

TEST(CouplingDFlowFMEngine, InvalidUseFailsClosedWithoutRuntimeLibrary) {
    scau::coupling::river::DFlowFMEngine engine{kMissingLibraryName};

    EXPECT_THROW(engine.initialize(""), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.update(1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.current_time()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.variable_count()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.variable_names()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_value("water_level", 0)), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("lateral_discharge", 0, 1.0), scau::coupling::river::DFlowFMEngineError);

    EXPECT_NO_THROW(engine.finalize());
}

TEST(CouplingDFlowFMEngine, BoundaryInterfaceStillExcludesCoreSemantics) {
    scau::coupling::river::DFlowFMEngine engine{kMissingLibraryName};

    EXPECT_FALSE(engine.initialized());
    EXPECT_DOUBLE_EQ(engine.elapsed_time(), 0.0);

    // Real DFlowFMEngine inherits only lifecycle and state read/write from
    // IDFlowFMEngine; Q_limit, deficit, rollback, replay, and arbitration remain
    // CouplingLib-owned by construction and are not mirrored here.
    EXPECT_THROW(engine.initialize("case.mdu"), scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingDFlowFMEngine, FakeBmiRuntimeLoadsAndAdvancesLifecycle) {
    scau::coupling::river::DFlowFMEngine engine{fake_library_path()};

    engine.initialize("fake_case.mdu");
    EXPECT_TRUE(engine.initialized());
    EXPECT_DOUBLE_EQ(engine.current_time(), 100.0);
    EXPECT_DOUBLE_EQ(engine.elapsed_time(), 0.0);
    EXPECT_EQ(engine.variable_count(), 2);

    const std::vector<std::string> names = engine.variable_names();
    ASSERT_EQ(names.size(), 2U);
    EXPECT_EQ(names[0], "water_level");
    EXPECT_EQ(names[1], "lateral_discharge");

    EXPECT_DOUBLE_EQ(engine.get_value("water_level", 0), 2.5);
    EXPECT_DOUBLE_EQ(engine.get_value("water_level", 1), 3.5);

    engine.set_value("lateral_discharge", 0, 1.25);
    EXPECT_DOUBLE_EQ(engine.get_value("lateral_discharge", 0), 1.25);

    engine.update(30.0);
    EXPECT_DOUBLE_EQ(engine.current_time(), 130.0);
    EXPECT_DOUBLE_EQ(engine.elapsed_time(), 30.0);
    EXPECT_DOUBLE_EQ(engine.get_value("water_level", 0), 2.6);
    EXPECT_DOUBLE_EQ(engine.get_value("water_level", 1), 3.7);

    engine.finalize();
    EXPECT_FALSE(engine.initialized());
    EXPECT_DOUBLE_EQ(engine.elapsed_time(), 0.0);
}

TEST(CouplingDFlowFMEngine, FakeBmiRuntimeRejectsInvalidStateAccess) {
    scau::coupling::river::DFlowFMEngine engine{fake_library_path()};
    engine.initialize("fake_case.mdu");

    EXPECT_THROW(static_cast<void>(engine.get_value("missing", 0)), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_value("water_level", -1)), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("lateral_discharge", 1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("lateral_discharge", 0, std::numeric_limits<double>::quiet_NaN()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.update(0.0), scau::coupling::river::DFlowFMEngineError);

    engine.finalize();
}

TEST(CouplingDFlowFMEngine, FakeBmiRuntimeEnforcesSingleOpenProject) {
    scau::coupling::river::DFlowFMEngine first{fake_library_path()};
    first.initialize("fake_case.mdu");

    scau::coupling::river::DFlowFMEngine second{fake_library_path()};
    EXPECT_THROW(second.initialize("fake_case.mdu"), scau::coupling::river::DFlowFMEngineError);
    EXPECT_FALSE(second.initialized());

    first.finalize();

    EXPECT_NO_THROW(second.initialize("fake_case.mdu"));
    second.finalize();
}
