// SWMM 5.2.4 spike host.
//
// Purpose: exercise the real swmm5.h public C API against the spike-spec
//   §3.1-§3.6 assumptions. NOT a coupling implementation. Aligned to the
//   actual swmm_*() function signatures verified in
//   Stormwater-Management-Model-develop/src/solver/include/swmm5.h.
//
// Gaps already visible from header inspection (see
//   spikes/swmm/evidence/interface_gap_matrix.md):
//   S1: no swmm_setNodeInflow; use swmm_setValue(swmm_NODE_LATFLOW, idx, q).
//   S2: swmm_step(double*) writes elapsedTime — engine drives dt, not caller.
//       This contradicts spike-spec §3.2 "外部控制 dt".
//   S3: no swmm_saveHotStart in public API; investigate .hsf path separately.

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "swmm5.h"
}

namespace {

constexpr int kStepsToRun = 100;
constexpr const char *kInjectNodeName = "J1";   // adjust to chosen case
constexpr double kInjectFlow = 0.05;            // CMS or current flow units

void report_error_if_any(int rc, const char *where) {
    if (rc == 0) {
        return;
    }
    std::array<char, 512> msg{};
    swmm_getError(msg.data(), static_cast<int>(msg.size()));
    std::fprintf(stderr, "[spike] %s returned %d: %s\n", where, rc, msg.data());
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "usage: %s <input.inp> <report.rpt> <output.out>\n",
            argv[0]);
        return 2;
    }

    std::printf("[spike] SWMM version = %d\n", swmm_getVersion());

    int rc = swmm_open(argv[1], argv[2], argv[3]);
    report_error_if_any(rc, "swmm_open");
    if (rc != 0) {
        return 1;
    }

    rc = swmm_start(/*saveFlag=*/1);
    report_error_if_any(rc, "swmm_start");

    const int node_index = swmm_getIndex(swmm_NODE, kInjectNodeName);
    std::printf("[spike] swmm_getIndex(NODE, \"%s\") = %d\n",
                kInjectNodeName, node_index);

    for (int step = 0; step < kStepsToRun; ++step) {
        // GAP S1 alignment: lateral inflow via setValue + NODE_LATFLOW enum.
        if (node_index >= 0) {
            swmm_setValue(swmm_NODE_LATFLOW, node_index, kInjectFlow);
        }

        double elapsedDays = 0.0;
        rc = swmm_step(&elapsedDays);
        report_error_if_any(rc, "swmm_step");
        if (rc != 0) {
            break;
        }

        if (node_index >= 0) {
            const double head     = swmm_getValue(swmm_NODE_HEAD,     node_index);
            const double overflow = swmm_getValue(swmm_NODE_OVERFLOW, node_index);
            std::printf("[spike] step=%d elapsedDays=%.6f head=%.6f overflow=%.6f\n",
                        step, elapsedDays, head, overflow);
        }
    }

    float runoffErr = 0.0F;
    float flowErr   = 0.0F;
    float qualErr   = 0.0F;
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    std::printf("[spike] mass balance error: runoff=%.4f%% flow=%.4f%% qual=%.4f%%\n",
                static_cast<double>(runoffErr),
                static_cast<double>(flowErr),
                static_cast<double>(qualErr));

    rc = swmm_end();
    report_error_if_any(rc, "swmm_end");

    rc = swmm_close();
    report_error_if_any(rc, "swmm_close");

    return 0;
}
