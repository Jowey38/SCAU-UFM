#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "coupling/driver/dflowfm_external_net_provider.hpp"
#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/river/dflowfm_engine.hpp"

// G20 dflowfm_longrun_10000: real-engine long-run policy golden.
//
// Leg A drives the real D-Flow FM runtime for 10,000 x 60 s (600,000 s of
// model time) on the authored open-boundary case with a per-step actual-dt
// audit, native water-balance monotonicity + closure sampling every 1,000
// steps, a bounded cumulative volume error, and a wall-clock budget.
// Leg B restarts from the native 300,000 s checkpoint written by leg A and
// replays 1,000 steps; hydraulic state at matching times must agree at
// floating-point scale and the native cumulative counters must have reset
// (M274 per-initialize re-baseline contract).
//
// Requires the governed runtime (bridged dflowfm.dll):
//   SCAU_DFLOWFM_LIBRARY        real bridged dflowfm.dll
//   SCAU_DFLOWFM_G20_CASE_DIR   dir of the long-run MDUs
//   SCAU_DFLOWFM_G20_MDU        leg-A MDU (relative to that dir)
//   SCAU_DFLOWFM_G20_RESTART_MDU leg-B restart MDU (relative to that dir)
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

constexpr int kTotalSteps = 10000;
constexpr int kSampleStride = 1000;
constexpr int kRestartStep = 5000;      // t = 300,000 s
constexpr int kRestartSteps = 1000;     // leg B replays to t = 360,000 s
constexpr double kDtSeconds = 60.0;
// Native closure |delta storage - (boundary_in - boundary_out)| accumulates
// the engine's own volume error; M274 evidence is 1e-12/step scale, so 1e-6
// over 10,000 steps keeps three orders of margin.
constexpr double kClosureToleranceM3 = 1.0e-6;
// Restart replay agreement at matching times (M274: bit-identical on the
// 60 s grid; 1e-9 avoids over-locking a third-party rst round-trip).
constexpr double kReplayToleranceM3 = 1.0e-9;
// Resource/time policy: the long-run leg must finish inside the documented
// gateway budget. Measured locally around 300-600 s; 1,800 s is the hard
// budget so CI queue noise cannot flake the gate.
constexpr double kWallClockBudgetSeconds = 1800.0;

}  // namespace

TEST(GoldenDFlowFMLongrun10000, TenThousandStepPolicyWithRestartReplay) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string case_dir = environment_value("SCAU_DFLOWFM_G20_CASE_DIR");
    const std::string mdu = environment_value("SCAU_DFLOWFM_G20_MDU");
    const std::string restart_mdu = environment_value("SCAU_DFLOWFM_G20_RESTART_MDU");
    if (library.empty() || case_dir.empty() || mdu.empty() || restart_mdu.empty()) {
        GTEST_SKIP() << "real D-Flow FM runtime env (SCAU_DFLOWFM_LIBRARY + "
                        "SCAU_DFLOWFM_G20_* vars) required";
    }

    const std::filesystem::path original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(case_dir);

    std::vector<double> leg_a_storage_samples;  // every kSampleStride steps

    // --- Leg A: 10,000 x 60 s continuous with per-step dt audit. ---
    {
        river::DFlowFMEngine engine(library);
        engine.initialize(mdu);

        const auto baseline = driver::observe_dflowfm_external_net(engine);
        ASSERT_TRUE(baseline.scope_complete);
        EXPECT_DOUBLE_EQ(baseline.boundary_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(baseline.boundary_out_m3, 0.0);

        const auto wall_start = std::chrono::steady_clock::now();
        double previous_time = engine.current_time();
        double max_dt_abs_error = 0.0;
        double previous_boundary_in = 0.0;
        double previous_boundary_out = 0.0;
        for (int step = 1; step <= kTotalSteps; ++step) {
            engine.update(kDtSeconds);
            const double now = engine.current_time();
            max_dt_abs_error =
                std::max(max_dt_abs_error, std::abs((now - previous_time) - kDtSeconds));
            previous_time = now;

            if (step % kSampleStride == 0) {
                const auto sample = driver::observe_dflowfm_external_net(engine);
                ASSERT_TRUE(sample.scope_complete);
                // Cumulative external classes stay monotonic across the run.
                EXPECT_GE(sample.boundary_in_m3, previous_boundary_in);
                EXPECT_GE(sample.boundary_out_m3, previous_boundary_out);
                previous_boundary_in = sample.boundary_in_m3;
                previous_boundary_out = sample.boundary_out_m3;
                // Native closure at every sample: storage change equals the
                // boundary net within the accumulated volume-error budget.
                EXPECT_NEAR(sample.storage_m3 - baseline.storage_m3,
                            sample.boundary_in_m3 - sample.boundary_out_m3,
                            kClosureToleranceM3);
                EXPECT_LE(std::abs(sample.volume_error_cumulative_m3),
                          kClosureToleranceM3);
                leg_a_storage_samples.push_back(sample.storage_m3);
            }
        }
        const double wall_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start)
                .count();

        // Per-step actual-dt audit: the engine advanced exactly 60 s per
        // update over the whole run (no silent dt substitution).
        EXPECT_DOUBLE_EQ(max_dt_abs_error, 0.0);
        EXPECT_DOUBLE_EQ(engine.current_time(), 600000.0);

        // Independent storage cross-check through the BMI vol1 path.
        const auto final_vol = driver::observe_dflowfm_internal_volume(engine);
        const auto final_net = driver::observe_dflowfm_external_net(engine);
        EXPECT_NEAR(final_net.storage_m3, final_vol.volume, kClosureToleranceM3);

        // Resource/time policy evidence.
        std::cout << std::setprecision(15)
                  << "[g20] steps=" << kTotalSteps
                  << " wall_seconds=" << wall_seconds
                  << " steps_per_second=" << (kTotalSteps / wall_seconds)
                  << " final_storage=" << final_net.storage_m3
                  << " boundary_in=" << final_net.boundary_in_m3
                  << " boundary_out=" << final_net.boundary_out_m3
                  << " volerr=" << final_net.volume_error_cumulative_m3 << "\n";
        EXPECT_LT(wall_seconds, kWallClockBudgetSeconds);

        engine.finalize();
    }

    ASSERT_EQ(leg_a_storage_samples.size(),
              static_cast<std::size_t>(kTotalSteps / kSampleStride));

    // --- Leg B: restart from the native 300,000 s checkpoint. ---
    {
        river::DFlowFMEngine engine(library);
        engine.initialize(restart_mdu);

        const auto rebaselined = driver::observe_dflowfm_external_net(engine);
        ASSERT_TRUE(rebaselined.scope_complete);
        // M274 per-initialize contract: every cumulative counter reset.
        EXPECT_DOUBLE_EQ(rebaselined.boundary_in_m3, 0.0);
        EXPECT_DOUBLE_EQ(rebaselined.boundary_out_m3, 0.0);
        EXPECT_DOUBLE_EQ(engine.current_time(), 300000.0);
        // Hydraulic state restored from the leg-A checkpoint.
        EXPECT_NEAR(rebaselined.storage_m3,
                    leg_a_storage_samples[kRestartStep / kSampleStride - 1],
                    kReplayToleranceM3);

        double previous_time = engine.current_time();
        double max_dt_abs_error = 0.0;
        for (int step = 1; step <= kRestartSteps; ++step) {
            engine.update(kDtSeconds);
            const double now = engine.current_time();
            max_dt_abs_error =
                std::max(max_dt_abs_error, std::abs((now - previous_time) - kDtSeconds));
            previous_time = now;
        }
        EXPECT_DOUBLE_EQ(max_dt_abs_error, 0.0);
        EXPECT_DOUBLE_EQ(engine.current_time(), 360000.0);

        // Replay agreement: storage at t=360,000 matches leg A's sample.
        const auto replayed = driver::observe_dflowfm_external_net(engine);
        EXPECT_NEAR(replayed.storage_m3,
                    leg_a_storage_samples[(kRestartStep + kRestartSteps) /
                                              kSampleStride - 1],
                    kReplayToleranceM3);

        engine.finalize();
    }

    std::filesystem::current_path(original_cwd);
}
