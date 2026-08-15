#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "coupling/driver/dflowfm_external_net_provider.hpp"
#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/river/dflowfm_engine.hpp"

// G27 dflowfm_external_net: real-engine external-flux contract golden.
//
// Verifies the driver-owned D-Flow FM external-net provider against the
// bridged native water-balance ABI on the authored single-reach cases:
//   Leg 1 (mixed boundary): upstream discharge + downstream stage; the audit
//         external net must independently close against delta sum(vol1).
//   Leg 2 (lateral dedup): closed reach + API lateral injection; the audit
//         external net must stay zero while delta sum(vol1) equals the
//         injected lateral volume.
//   Leg 3 (re-baseline): re-initializing the engine must reset every native
//         cumulative counter to zero (per-initialize contract, M274).
//
// Requires the governed runtime (bridged dflowfm.dll) and authored cases:
//   SCAU_DFLOWFM_LIBRARY            real bridged dflowfm.dll
//   SCAU_DFLOWFM_G27_OPEN_CASE_DIR  dir of single_reach_open.mdu
//   SCAU_DFLOWFM_G27_OPEN_MDU       open-boundary MDU (relative to that dir)
//   SCAU_DFLOWFM_G27_CLOSED_CASE_DIR dir of single_reach.mdu (lat1 registered)
//   SCAU_DFLOWFM_G27_CLOSED_MDU     closed MDU (relative to that dir)
// Skips without them (hosted CI lanes).

namespace driver = scau::coupling::driver;
namespace river = scau::coupling::river;

namespace {

std::string environment_value(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

struct G27Environment {
    std::string library;
    std::filesystem::path open_case_dir;
    std::string open_mdu;
    std::filesystem::path closed_case_dir;
    std::string closed_mdu;
    bool complete{false};
};

G27Environment read_environment() {
    G27Environment env{};
    env.library = environment_value("SCAU_DFLOWFM_LIBRARY");
    env.open_case_dir = environment_value("SCAU_DFLOWFM_G27_OPEN_CASE_DIR");
    env.open_mdu = environment_value("SCAU_DFLOWFM_G27_OPEN_MDU");
    env.closed_case_dir = environment_value("SCAU_DFLOWFM_G27_CLOSED_CASE_DIR");
    env.closed_mdu = environment_value("SCAU_DFLOWFM_G27_CLOSED_MDU");
    env.complete = !env.library.empty() && !env.open_case_dir.empty() &&
                   !env.open_mdu.empty() && !env.closed_case_dir.empty() &&
                   !env.closed_mdu.empty();
    return env;
}

constexpr int kSteps = 10;
constexpr double kDtSeconds = 60.0;
constexpr double kLateralDischarge = 0.125;
// Independent-closure tolerance: the M274 traces close at the 1e-11 m3
// volume-error scale; 1e-6 m3 keeps a wide safety margin while remaining
// far below any physically meaningful residual (missing-scope signals are
// O(10-100) m3).
constexpr double kClosureToleranceM3 = 1.0e-6;

}  // namespace

TEST(DFlowFMExternalNetGolden, MixedBoundaryClosesAgainstVol1AndRebaselines) {
    const G27Environment env = read_environment();
    if (!env.complete) {
        GTEST_SKIP() << "real D-Flow FM runtime env (SCAU_DFLOWFM_LIBRARY + "
                        "SCAU_DFLOWFM_G27_* case vars) required";
    }

    const std::filesystem::path original_cwd = std::filesystem::current_path();

    // --- Leg 1: mixed boundary (upstream discharge + downstream stage). ---
    std::filesystem::current_path(env.open_case_dir);
    {
        river::DFlowFMEngine engine(env.library);
        engine.initialize(env.open_mdu);

        const auto baseline_net = driver::observe_dflowfm_external_net(engine);
        const auto baseline_vol = driver::observe_dflowfm_internal_volume(engine);
        ASSERT_TRUE(baseline_net.scope_complete);
        ASSERT_TRUE(baseline_vol.scope_complete);
        // Fresh initialize: native cumulative counters start at zero.
        EXPECT_DOUBLE_EQ(baseline_net.boundary_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(baseline_net.boundary_out_m3, 0.0);
        EXPECT_DOUBLE_EQ(baseline_net.api_lateral_net_volume_m3, 0.0);
        // bug-207 discovery lock: on this open-boundary case the FULL vol1
        // sum includes boundary ghost-node volumes and materially overcounts
        // the physical domain; internal truncation is what closes.
        const auto baseline_full_vol = driver::observe_dflowfm_volume(engine);
        EXPECT_GT(baseline_full_vol.volume, baseline_vol.volume + 100.0);
        // Native internal-domain storage and sum(vol1[0:ndxi]) are the same
        // physical quantity observed through two independent paths.
        EXPECT_NEAR(baseline_net.storage_m3, baseline_vol.volume, kClosureToleranceM3);

        double previous_boundary_in = 0.0;
        double previous_boundary_out = 0.0;
        for (int step = 0; step < kSteps; ++step) {
            engine.update(kDtSeconds);
            const auto net = driver::observe_dflowfm_external_net(engine);
            ASSERT_TRUE(net.scope_complete);
            // Cumulative components must be monotonic across updates.
            EXPECT_GE(net.boundary_in_m3, previous_boundary_in);
            EXPECT_GE(net.boundary_out_m3, previous_boundary_out);
            previous_boundary_in = net.boundary_in_m3;
            previous_boundary_out = net.boundary_out_m3;
        }

        const auto final_net = driver::observe_dflowfm_external_net(engine);
        const auto final_vol = driver::observe_dflowfm_internal_volume(engine);

        // Both boundary directions must be exercised by this case.
        EXPECT_GT(final_net.boundary_in_m3, 1.0);
        EXPECT_GT(final_net.boundary_out_m3, 1.0);
        // No API laterals were written in this leg.
        EXPECT_DOUBLE_EQ(final_net.api_lateral_net_volume_m3, 0.0);

        // Independent closure: storage response equals the audit external
        // net observed through the native cumulative boundary classes.
        const double storage_delta = final_vol.volume - baseline_vol.volume;
        const double external_net_delta =
            final_net.external_net_volume_m3 - baseline_net.external_net_volume_m3;
        EXPECT_NEAR(storage_delta, external_net_delta, kClosureToleranceM3);
        EXPECT_NEAR(final_net.storage_m3, final_vol.volume, kClosureToleranceM3);

        engine.finalize();
    }

    // --- Leg 2: closed reach + API lateral (dedup contract). ---
    std::filesystem::current_path(env.closed_case_dir);
    {
        river::DFlowFMEngine engine(env.library);
        engine.initialize(env.closed_mdu);

        const auto baseline_net = driver::observe_dflowfm_external_net(engine);
        const auto baseline_vol = driver::observe_dflowfm_internal_volume(engine);
        ASSERT_TRUE(baseline_net.scope_complete);
        // Closed model: no boundary ghost nodes, so internal and full sums
        // agree (ndxi covers every vol1 entry).
        const auto baseline_full_vol = driver::observe_dflowfm_volume(engine);
        EXPECT_NEAR(baseline_full_vol.volume, baseline_vol.volume, kClosureToleranceM3);

        for (int step = 0; step < kSteps; ++step) {
            engine.set_value("laterals/lat1/water_discharge", 0, kLateralDischarge);
            engine.update(kDtSeconds);
        }

        const auto final_net = driver::observe_dflowfm_external_net(engine);
        const auto final_vol = driver::observe_dflowfm_internal_volume(engine);

        const double expected_lateral_volume =
            kLateralDischarge * kDtSeconds * static_cast<double>(kSteps);
        // The native lateral class integrates the API-injected discharge.
        EXPECT_NEAR(final_net.api_lateral_net_volume_m3 -
                        baseline_net.api_lateral_net_volume_m3,
                    expected_lateral_volume, kClosureToleranceM3);
        // The audit external net removes the CouplingLib-owned lateral: a
        // closed reach has no external exchange.
        EXPECT_NEAR(final_net.external_net_volume_m3 -
                        baseline_net.external_net_volume_m3,
                    0.0, kClosureToleranceM3);
        // Storage grows by exactly the injected lateral volume.
        EXPECT_NEAR(final_vol.volume - baseline_vol.volume,
                    expected_lateral_volume, kClosureToleranceM3);
        EXPECT_DOUBLE_EQ(final_net.boundary_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(final_net.boundary_out_m3, 0.0);

        engine.finalize();
    }

    // --- Leg 3: re-initialize resets every native cumulative counter. ---
    std::filesystem::current_path(env.open_case_dir);
    {
        river::DFlowFMEngine engine(env.library);
        engine.initialize(env.open_mdu);

        const auto rebaselined = driver::observe_dflowfm_external_net(engine);
        ASSERT_TRUE(rebaselined.scope_complete);
        EXPECT_DOUBLE_EQ(rebaselined.boundary_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(rebaselined.boundary_out_m3, 0.0);
        EXPECT_DOUBLE_EQ(rebaselined.api_lateral_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(rebaselined.api_lateral_out_m3, 0.0);
        EXPECT_DOUBLE_EQ(rebaselined.external_net_volume_m3, 0.0);

        engine.update(kDtSeconds);
        const auto after_step = driver::observe_dflowfm_external_net(engine);
        EXPECT_GT(after_step.boundary_in_m3, 0.0);

        engine.finalize();
    }

    std::filesystem::current_path(original_cwd);
}
