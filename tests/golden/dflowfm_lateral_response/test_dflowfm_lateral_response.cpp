#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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

struct RiverState {
    std::vector<double> stage;
    std::vector<double> flow;
};

// Runs the authored single-reach case for 20 aligned 60 s steps with a
// constant native lateral discharge, then samples every mesh stage and link
// flow. Engines are strictly finalized between runs (process-global BMI).
RiverState run_case(const std::string& library, const std::string& mdu, double lateral_q) {
    scau::coupling::river::DFlowFMEngine engine(library);
    engine.initialize(mdu);
    for (int step = 0; step < 20; ++step) {
        engine.set_value("laterals/lat1/water_discharge", 0, lateral_q);
        engine.update(60.0);
    }

    RiverState result;
    result.stage.reserve(11U);
    result.flow.reserve(10U);
    for (int i = 0; i < 11; ++i) {
        result.stage.push_back(engine.get_value("s1", i));
    }
    for (int i = 0; i < 10; ++i) {
        result.flow.push_back(engine.get_value("q1", i));
    }
    engine.finalize();
    return result;
}

}  // namespace

// Controlled experiment: identical case and schedule, baseline lateral 0
// versus forced lateral 0.01 m3/s. The forced run must produce a measurable
// hydraulic response somewhere in s1/q1, and every value must stay finite.
TEST(GoldenDFlowFMLateralResponse, NonzeroNativeLateralChangesRealHydraulicState) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    if (library.empty() || mdu.empty()) {
        GTEST_SKIP() << "real D-Flow FM runtime env required";
    }

    const RiverState baseline = run_case(library, mdu, 0.0);
    const RiverState forced = run_case(library, mdu, 0.01);
    ASSERT_EQ(baseline.stage.size(), forced.stage.size());
    ASSERT_EQ(baseline.flow.size(), forced.flow.size());

    double max_stage_delta = 0.0;
    double max_flow_delta = 0.0;
    for (std::size_t i = 0; i < baseline.stage.size(); ++i) {
        ASSERT_TRUE(std::isfinite(baseline.stage[i]));
        ASSERT_TRUE(std::isfinite(forced.stage[i]));
        max_stage_delta = std::max(max_stage_delta, std::abs(forced.stage[i] - baseline.stage[i]));
    }
    for (std::size_t i = 0; i < baseline.flow.size(); ++i) {
        ASSERT_TRUE(std::isfinite(baseline.flow[i]));
        ASSERT_TRUE(std::isfinite(forced.flow[i]));
        max_flow_delta = std::max(max_flow_delta, std::abs(forced.flow[i] - baseline.flow[i]));
    }

    // 0.01 m3/s over 1200 s into a 1000 m x 5 m reach adds 12 m3, i.e. an
    // average stage rise of ~2.4 mm; require a clearly measurable response
    // in stage or flow, well above numerical noise.
    EXPECT_GT(std::max(max_stage_delta, max_flow_delta), 1.0e-6);
}
