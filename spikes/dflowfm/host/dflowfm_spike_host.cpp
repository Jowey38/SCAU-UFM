// D-Flow FM BMI 1.0 spike host.
//
// Purpose: exercise the real BMI C API against the spike-spec §3 / §6
//   assumptions. NOT a coupling implementation. Aligned to the actual BMI
//   signatures verified in
//   Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h.
//
// Gaps already visible from header inspection (see
//   spikes/dflowfm/evidence/interface_gap_matrix.md):
//   D1: no instance handle; API is process-global free C functions.
//   D2: update(double dt) takes external dt — must verify engine respects it.
//   D3: set_var/get_var use string-named variables; caller buffer for set,
//       engine-owned pointer for get (caller does NOT free).
//   D4: no save_state in BMI 1.0; investigate engine-specific extension.

#include <array>
#include <cstdio>
#include <cstring>

extern "C" {
#include "bmi.h"
}

namespace {

constexpr int kStepsToRun = 100;
constexpr double kDtSeconds = 60.0;
constexpr const char *kBoundaryDischargeVar = "boundary_discharge";
constexpr const char *kStageVar = "stage_at_section";

void BMI_CALLCONV spike_logger(Level level, const char *msg) {
    std::printf("[bmi log lvl=%d] %s\n", static_cast<int>(level), msg);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <config.mdu>\n", argv[0]);
        return 2;
    }

    set_logger(&spike_logger);

    int rc = initialize(argv[1]);
    if (rc != 0) {
        std::fprintf(stderr, "[spike] initialize returned %d\n", rc);
        return 1;
    }

    int var_count = 0;
    get_var_count(&var_count);
    std::printf("[spike] get_var_count = %d\n", var_count);
    for (int i = 0; i < var_count && i < 32; ++i) {
        std::array<char, MAXSTRINGLEN> name{};
        get_var_name(i, name.data());
        std::printf("[spike]   var[%d] = %s\n", i, name.data());
    }

    double t0 = 0.0;
    double t1 = 0.0;
    double dt_internal = 0.0;
    get_start_time(&t0);
    get_end_time(&t1);
    get_time_step(&dt_internal);
    std::printf("[spike] t0=%.6f t1=%.6f engine_dt=%.6f\n",
                t0, t1, dt_internal);

    for (int step = 0; step < kStepsToRun; ++step) {
        const double q_inject = 1.5;  // SI m^3/s
        // GAP D3: caller provides pointer to caller-owned buffer.
        set_var(kBoundaryDischargeVar, static_cast<const void *>(&q_inject));

        rc = update(kDtSeconds);
        if (rc != 0) {
            std::fprintf(stderr, "[spike] update returned %d at step %d\n", rc, step);
            break;
        }

        double t_now = 0.0;
        get_current_time(&t_now);

        // GAP D3: get_var fills a pointer-to-pointer into engine memory.
        // Caller does NOT free.
        void *engine_ptr = nullptr;
        get_var(kStageVar, &engine_ptr);
        if (engine_ptr != nullptr) {
            const double *stage = static_cast<const double *>(engine_ptr);
            std::printf("[spike] step=%d t=%.6f stage[0]=%.6f\n",
                        step, t_now, stage[0]);
        } else {
            std::printf("[spike] step=%d t=%.6f stage var unavailable\n",
                        step, t_now);
        }
    }

    rc = finalize();
    if (rc != 0) {
        std::fprintf(stderr, "[spike] finalize returned %d\n", rc);
    }
    return 0;
}
