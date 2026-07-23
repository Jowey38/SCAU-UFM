// D-Flow FM BMI 1.0 spike host.
//
// Purpose: exercise the real BMI C API against spike-spec sections 3 and 6.
//   assumptions. NOT a coupling implementation. Aligned to the actual BMI
//   signatures verified in
//   Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h.
//
// Gaps already visible from header inspection (see
//   spikes/dflowfm/evidence/interface_gap_matrix.md):
//   D1: no instance handle; API is process-global free C functions.
//   D2: update(double dt) takes external dt; verify the engine respects it.
//   D3: set_var/get_var use string-named variables; caller buffer for set,
//       engine-owned pointer for get (caller does NOT free).
//   D4: no save_state in BMI 1.0; investigate engine-specific extension.

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "bmi.h"
}

namespace {

constexpr int kStepsToRun = 100;
constexpr double kDtSeconds = 60.0;
constexpr const char *kBoundaryDischargeVar = "boundary_discharge";
constexpr const char *kStageVar = "stage_at_section";

struct SpikeOptions {
    const char *config_path{nullptr};
    int steps{kStepsToRun};
    double dt_seconds{kDtSeconds};
    const char *boundary_discharge_var{kBoundaryDischargeVar};
    const char *stage_var{kStageVar};
    const char *inventory_out_path{nullptr};
    const char *trace_out_path{nullptr};
    bool inventory_only{false};
    bool skip_boundary_write{false};
};

void BMI_CALLCONV spike_logger(Level level, const char *msg) {
    std::printf("[bmi log lvl=%d] %s\n", static_cast<int>(level), msg);
}

void write_line(std::FILE *file, const char *text) {
    std::fputs(text, file);
    std::fputc('\n', file);
}

void write_line_if_open(std::FILE *file, const char *text) {
    if (file != nullptr) {
        write_line(file, text);
    }
}

bool parse_int_arg(const char *text, int *value) {
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == nullptr || *end != '\0' || parsed <= 0 || parsed > 1000000L) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool parse_double_arg(const char *text, double *value) {
    char *end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(parsed) || parsed <= 0.0) {
        return false;
    }
    *value = parsed;
    return true;
}

bool parse_options(int argc, char **argv, SpikeOptions *options) {
    if (argc < 2) {
        return false;
    }
    options->config_path = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--steps" && i + 1 < argc) {
            if (!parse_int_arg(argv[++i], &options->steps)) {
                return false;
            }
            continue;
        }
        if (arg == "--dt" && i + 1 < argc) {
            if (!parse_double_arg(argv[++i], &options->dt_seconds)) {
                return false;
            }
            continue;
        }
        if (arg == "--boundary-var" && i + 1 < argc) {
            options->boundary_discharge_var = argv[++i];
            continue;
        }
        if (arg == "--stage-var" && i + 1 < argc) {
            options->stage_var = argv[++i];
            continue;
        }
        if (arg == "--inventory-out" && i + 1 < argc) {
            options->inventory_out_path = argv[++i];
            continue;
        }
        if (arg == "--trace-out" && i + 1 < argc) {
            options->trace_out_path = argv[++i];
            continue;
        }
        if (arg == "--inventory-only") {
            options->inventory_only = true;
            continue;
        }
        if (arg == "--skip-boundary-write") {
            options->skip_boundary_write = true;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    SpikeOptions options{};
    if (!parse_options(argc, argv, &options)) {
        std::fprintf(stderr,
                     "usage: %s <config.mdu> [--steps N] [--dt seconds] "
                     "[--boundary-var name] [--stage-var name] "
                     "[--inventory-out file] [--trace-out file] [--inventory-only] "
                     "[--skip-boundary-write]\n",
                     argv[0]);
        return 2;
    }

    std::FILE *inventory_out = nullptr;
    std::FILE *trace_out = nullptr;
    if (options.inventory_out_path != nullptr) {
        inventory_out = std::fopen(options.inventory_out_path, "w");
        if (inventory_out == nullptr) {
            std::fprintf(stderr, "[spike] failed to open inventory output: %s\n", options.inventory_out_path);
            return 1;
        }
    }
    if (options.trace_out_path != nullptr) {
        trace_out = std::fopen(options.trace_out_path, "w");
        if (trace_out == nullptr) {
            std::fprintf(stderr, "[spike] failed to open trace output: %s\n", options.trace_out_path);
            if (inventory_out != nullptr) {
                std::fclose(inventory_out);
            }
            return 1;
        }
    }

    set_logger(&spike_logger);

    int rc = initialize(options.config_path);
    if (rc != 0) {
        std::fprintf(stderr, "[spike] initialize returned %d\n", rc);
        if (inventory_out != nullptr) {
            std::fclose(inventory_out);
        }
        if (trace_out != nullptr) {
            std::fclose(trace_out);
        }
        return 1;
    }

    int var_count = 0;
    get_var_count(&var_count);
    std::printf("[spike] get_var_count = %d\n", var_count);
    write_line_if_open(inventory_out, "| index | name | type | rank | shape | units | read/write role | notes |");
    write_line_if_open(inventory_out, "|---|---|---|---|---|---|---|---|");
    std::printf("[spike] variable inventory markdown follows\n");
    std::printf("| index | name | type | rank | shape | units | read/write role | notes |\n");
    std::printf("|---|---|---|---|---|---|---|---|\n");
    for (int i = 0; i < var_count; ++i) {
        std::array<char, MAXSTRINGLEN> name{};
        std::array<char, MAXSTRINGLEN> type{};
        std::array<int, MAXDIMS> shape{};
        int rank = 0;
        get_var_name(i, name.data());
        get_var_type(name.data(), type.data());
        get_var_rank(name.data(), &rank);
        // The 2026-07-19 D-Flow FM release DLL crashes in get_var_shape for
        // some rank>1 variables (first observed: bodsed, rank 2). Query shape
        // only for scalar/vector variables so inventory capture stays robust;
        // record high-rank shape as unavailable pending an upstream ABI fix.
        if (rank <= 1) {
            get_var_shape(name.data(), shape.data());
        }
        // BMI 1.0 exposes no get_var_units symbol. Preserve the inventory
        // column explicitly as unavailable rather than calling a non-ABI API.
        constexpr const char *units = "N/A (BMI 1.0)";

        std::string shape_text;
        if (rank > 1) {
            shape_text = "unavailable (runtime get_var_shape crash)";
        } else {
            for (int dim = 0; dim < rank; ++dim) {
                if (!shape_text.empty()) {
                    shape_text += " x ";
                }
                shape_text += std::to_string(shape[dim]);
            }
            if (shape_text.empty()) {
                shape_text = "scalar";
            }
        }

        std::printf("| %d | %s | %s | %d | %s | %s | TBD | captured by spike host |\n",
                    i,
                    name.data(),
                    type.data(),
                    rank,
                    shape_text.c_str(),
                    units);
        if (inventory_out != nullptr) {
            std::fprintf(inventory_out,
                         "| %d | %s | %s | %d | %s | %s | TBD | captured by spike host |\n",
                         i,
                         name.data(),
                         type.data(),
                         rank,
                         shape_text.c_str(),
                         units);
        }
    }

    if (options.inventory_only) {
        const int finalize_rc = finalize();
        if (inventory_out != nullptr) {
            std::fclose(inventory_out);
        }
        if (trace_out != nullptr) {
            std::fclose(trace_out);
        }
        return finalize_rc == 0 ? 0 : 1;
    }

    double t0 = 0.0;
    double t1 = 0.0;
    double dt_internal = 0.0;
    get_start_time(&t0);
    get_end_time(&t1);
    get_time_step(&dt_internal);
    std::printf("[spike] t0=%.6f t1=%.6f engine_dt=%.6f\n",
                t0, t1, dt_internal);
    if (trace_out != nullptr) {
        std::fprintf(trace_out,
                     "# t0=%.6f t1=%.6f engine_dt=%.6f steps=%d dt=%.6f boundary_var=%s stage_var=%s\n",
                     t0,
                     t1,
                     dt_internal,
                     options.steps,
                     options.dt_seconds,
                     options.boundary_discharge_var,
                     options.stage_var);
        write_line(trace_out, "# columns: step current_time stage0");
    }

    int run_rc = 0;
    int completed_steps = 0;
    double previous_time = t0;
    double last_time = t0;
    double max_dt_abs_error = 0.0;
    bool time_trace_valid = true;
    for (int step = 0; step < options.steps; ++step) {
        const double q_inject = 1.5;  // SI m^3/s
        if (!options.skip_boundary_write) {
            // GAP D3: caller provides pointer to caller-owned buffer.
            set_var(options.boundary_discharge_var, static_cast<const void *>(&q_inject));
        }

        rc = update(options.dt_seconds);
        if (rc != 0) {
            std::fprintf(stderr, "[spike] update returned %d at step %d\n", rc, step);
            run_rc = rc;
            break;
        }

        double t_now = 0.0;
        get_current_time(&t_now);
        if (!std::isfinite(t_now)) {
            std::fprintf(stderr, "[spike] get_current_time returned non-finite value at step %d\n", step);
            run_rc = 1;
            time_trace_valid = false;
            break;
        }

        const double observed_dt = t_now - previous_time;
        const double dt_abs_error = std::fabs(observed_dt - options.dt_seconds);
        if (dt_abs_error > max_dt_abs_error) {
            max_dt_abs_error = dt_abs_error;
        }
        previous_time = t_now;
        last_time = t_now;
        ++completed_steps;

        // GAP D3: get_var fills a pointer-to-pointer into engine memory.
        // Caller does NOT free.
        void *engine_ptr = nullptr;
        get_var(options.stage_var, &engine_ptr);
        if (engine_ptr != nullptr) {
            const double *stage = static_cast<const double *>(engine_ptr);
            std::printf("[spike] step=%d t=%.6f stage[0]=%.6f\n",
                        step, t_now, stage[0]);
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "%d %.6f %.6f\n", step, t_now, stage[0]);
            }
        } else {
            std::printf("[spike] step=%d t=%.6f stage var unavailable\n",
                        step, t_now);
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "%d %.6f unavailable\n", step, t_now);
            }
        }
    }

    const double expected_last_time = t0 + static_cast<double>(completed_steps) * options.dt_seconds;
    std::printf("[spike] summary completed_steps=%d requested_steps=%d last_time=%.6f expected_last_time=%.6f max_dt_abs_error=%.12g time_trace_valid=%s\n",
                completed_steps,
                options.steps,
                last_time,
                expected_last_time,
                max_dt_abs_error,
                time_trace_valid ? "true" : "false");
    if (trace_out != nullptr) {
        std::fprintf(trace_out,
                     "# summary completed_steps=%d requested_steps=%d last_time=%.6f expected_last_time=%.6f max_dt_abs_error=%.12g time_trace_valid=%s\n",
                     completed_steps,
                     options.steps,
                     last_time,
                     expected_last_time,
                     max_dt_abs_error,
                     time_trace_valid ? "true" : "false");
    }

    const int finalize_rc = finalize();
    if (finalize_rc != 0) {
        std::fprintf(stderr, "[spike] finalize returned %d\n", finalize_rc);
    }
    rc = run_rc != 0 ? run_rc : finalize_rc;
    if (inventory_out != nullptr) {
        std::fclose(inventory_out);
    }
    if (trace_out != nullptr) {
        std::fclose(trace_out);
    }
    return rc == 0 ? 0 : 1;
}
