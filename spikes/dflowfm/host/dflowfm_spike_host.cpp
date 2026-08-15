// D-Flow FM BMI 1.0 spike host.
//
// Purpose: exercise the real BMI C API against the spike-spec assumptions and,
// when available, a versioned native water-balance ABI. NOT a coupling
// implementation.

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
#include "bmi.h"
}

#include "dflowfm_water_balance_v1.h"

namespace {

constexpr int kStepsToRun = 100;
constexpr double kDtSeconds = 60.0;
constexpr const char *kBoundaryDischargeVar = "boundary_discharge";
constexpr const char *kStageVar = "stage_at_section";
constexpr std::uint64_t kAllWaterBalanceComponents =
    SCAU_DFLOWFM_WB_BOUNDARY |
    SCAU_DFLOWFM_WB_LATERAL |
    SCAU_DFLOWFM_WB_SOURCE |
    SCAU_DFLOWFM_WB_QEXT |
    SCAU_DFLOWFM_WB_RAIN_EVAP |
    SCAU_DFLOWFM_WB_GROUNDWATER |
    SCAU_DFLOWFM_WB_STORAGE |
    SCAU_DFLOWFM_WB_VOLUME_ERROR;

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
    const char *verify_lateral_id{nullptr};
    bool probe_native_water_balance{false};
    std::vector<std::string> probe_sum_vars;
    std::vector<std::string> probe_values_vars;
    std::vector<std::string> probe_int_values_vars;
    std::vector<std::string> probe_int_matrix_vars;
    const char *inject_lateral_id{nullptr};
    double inject_lateral_q{0.0};
    bool inject_lateral{false};
};

using WaterBalanceFn = int (*)(scau_dflowfm_water_balance_v1 *, std::uint32_t);

struct WaterBalanceProbeState {
    bool symbol_available{false};
    bool abi_valid{false};
    bool components_complete{false};
    bool finite{false};
    bool monotonic{true};
    bool time_valid{true};
    bool last_seen_initialized{false};
    double last_time{0.0};
    scau_dflowfm_water_balance_v1 last{};
};

void BMI_CALLCONV spike_logger(Level level, const char *msg) {
    std::printf("[bmi log lvl=%d] %s\n", static_cast<int>(level), msg);
}

#if defined(_WIN32)
using LibraryHandle = HMODULE;

struct ModuleRef {
    LibraryHandle handle{nullptr};
    bool owned{false};
};

ModuleRef acquire_dflowfm_module() {
    ModuleRef ref{};
    ref.handle = ::GetModuleHandleA("dflowfm.dll");
    if (ref.handle != nullptr) {
        return ref;
    }
    ref.handle = ::LoadLibraryA("dflowfm.dll");
    ref.owned = ref.handle != nullptr;
    return ref;
}

void release_module(const ModuleRef &ref) noexcept {
    if (ref.owned && ref.handle != nullptr) {
        ::FreeLibrary(ref.handle);
    }
}

void *resolve_symbol(LibraryHandle handle, const char *name) {
    return reinterpret_cast<void *>(::GetProcAddress(handle, name));
}
#else
using LibraryHandle = void *;

struct ModuleRef {
    LibraryHandle handle{nullptr};
    bool owned{false};
};

ModuleRef acquire_dflowfm_module() {
    ModuleRef ref{};
    ref.handle = ::dlopen(nullptr, RTLD_NOW);
    ref.owned = ref.handle != nullptr;
    return ref;
}

void release_module(const ModuleRef &ref) noexcept {
    if (ref.owned && ref.handle != nullptr) {
        ::dlclose(ref.handle);
    }
}

void *resolve_symbol(LibraryHandle handle, const char *name) {
    return ::dlsym(handle, name);
}
#endif

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
        if (arg == "--verify-lateral-id" && i + 1 < argc) {
            options->verify_lateral_id = argv[++i];
            continue;
        }
        if (arg == "--probe-native-water-balance") {
            options->probe_native_water_balance = true;
            continue;
        }
        if ((arg == "--probe-sum-vars" || arg == "--probe-values-vars" ||
             arg == "--probe-int-values-vars" || arg == "--probe-int-matrix-vars") &&
            i + 1 < argc) {
            std::vector<std::string> &target =
                arg == "--probe-sum-vars"
                    ? options->probe_sum_vars
                    : arg == "--probe-values-vars"
                          ? options->probe_values_vars
                          : arg == "--probe-int-values-vars"
                                ? options->probe_int_values_vars
                                : options->probe_int_matrix_vars;
            const std::string list = argv[++i];
            std::size_t start = 0;
            while (start <= list.size()) {
                const std::size_t comma = list.find(',', start);
                const std::size_t end = comma == std::string::npos ? list.size() : comma;
                if (end > start) {
                    target.emplace_back(list.substr(start, end - start));
                }
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
            if (target.empty()) {
                return false;
            }
            continue;
        }
        if (arg == "--inject-lateral-id" && i + 1 < argc) {
            options->inject_lateral_id = argv[++i];
            continue;
        }
        if (arg == "--inject-lateral-q" && i + 1 < argc) {
            if (!parse_double_arg(argv[++i], &options->inject_lateral_q)) {
                return false;
            }
            continue;
        }
        return false;
    }
    options->inject_lateral =
        options->inject_lateral_id != nullptr && options->inject_lateral_q > 0.0;
    if (options->inject_lateral_id != nullptr && !options->inject_lateral) {
        return false;
    }
    return true;
}

std::vector<double> copy_rank1_double_var(const char *name) {
    std::array<char, MAXSTRINGLEN> type{};
    int rank = -1;
    get_var_type(name, type.data());
    get_var_rank(name, &rank);
    if (rank != 1 || std::strcmp(type.data(), "double") != 0) {
        std::fprintf(stderr,
                     "[spike] probe var %s skipped: type=%s rank=%d (need rank-1 double)\n",
                     name,
                     type.data(),
                     rank);
        return {};
    }
    std::array<int, MAXDIMS> shape{};
    get_var_shape(name, shape.data());
    if (shape[0] <= 0) {
        std::fprintf(stderr, "[spike] probe var %s skipped: shape[0]=%d\n", name, shape[0]);
        return {};
    }
    void *engine_ptr = nullptr;
    get_var(name, &engine_ptr);
    if (engine_ptr == nullptr) {
        std::fprintf(stderr, "[spike] probe var %s skipped: get_var returned null\n", name);
        return {};
    }
    const double *values = static_cast<const double *>(engine_ptr);
    return std::vector<double>(values, values + static_cast<std::size_t>(shape[0]));
}

std::vector<int> copy_rank2_int_var(const char *name, int *rows, int *cols) {
    std::array<char, MAXSTRINGLEN> type{};
    int rank = -1;
    get_var_type(name, type.data());
    get_var_rank(name, &rank);
    if (rank != 2 || std::strcmp(type.data(), "int") != 0) {
        std::fprintf(stderr,
                     "[spike] probe int matrix %s skipped: type=%s rank=%d (need rank-2 int)\n",
                     name,
                     type.data(),
                     rank);
        return {};
    }
    std::array<int, MAXDIMS> shape{};
    get_var_shape(name, shape.data());
    if (shape[0] <= 0 || shape[1] <= 0) {
        std::fprintf(stderr,
                     "[spike] probe int matrix %s skipped: shape=%d x %d\n",
                     name,
                     shape[0],
                     shape[1]);
        return {};
    }
    void *engine_ptr = nullptr;
    get_var(name, &engine_ptr);
    if (engine_ptr == nullptr) {
        std::fprintf(stderr,
                     "[spike] probe int matrix %s skipped: get_var returned null\n",
                     name);
        return {};
    }
    *rows = shape[0];
    *cols = shape[1];
    const int *values = static_cast<const int *>(engine_ptr);
    return std::vector<int>(values, values + static_cast<std::size_t>(shape[0] * shape[1]));
}

std::vector<int> copy_rank1_int_var(const char *name) {
    std::array<char, MAXSTRINGLEN> type{};
    int rank = -1;
    get_var_type(name, type.data());
    get_var_rank(name, &rank);
    if (rank != 1 || std::strcmp(type.data(), "int") != 0) {
        std::fprintf(stderr,
                     "[spike] probe int var %s skipped: type=%s rank=%d (need rank-1 int)\n",
                     name,
                     type.data(),
                     rank);
        return {};
    }
    std::array<int, MAXDIMS> shape{};
    get_var_shape(name, shape.data());
    if (shape[0] <= 0) {
        std::fprintf(stderr, "[spike] probe int var %s skipped: shape[0]=%d\n", name, shape[0]);
        return {};
    }
    void *engine_ptr = nullptr;
    get_var(name, &engine_ptr);
    if (engine_ptr == nullptr) {
        std::fprintf(stderr, "[spike] probe int var %s skipped: get_var returned null\n", name);
        return {};
    }
    const int *values = static_cast<const int *>(engine_ptr);
    return std::vector<int>(values, values + static_cast<std::size_t>(shape[0]));
}

bool is_nonnegative_cumulative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool is_snapshot_finite_and_nonnegative(const scau_dflowfm_water_balance_v1 &snapshot) {
    return std::isfinite(snapshot.current_time_seconds) &&
           is_nonnegative_cumulative(snapshot.storage_m3) &&
           std::isfinite(snapshot.volume_error_cumulative_m3) &&
           is_nonnegative_cumulative(snapshot.boundary_in_m3) &&
           is_nonnegative_cumulative(snapshot.boundary_out_m3) &&
           is_nonnegative_cumulative(snapshot.lateral_1d_in_m3) &&
           is_nonnegative_cumulative(snapshot.lateral_1d_out_m3) &&
           is_nonnegative_cumulative(snapshot.lateral_2d_in_m3) &&
           is_nonnegative_cumulative(snapshot.lateral_2d_out_m3) &&
           is_nonnegative_cumulative(snapshot.source_in_m3) &&
           is_nonnegative_cumulative(snapshot.source_out_m3) &&
           is_nonnegative_cumulative(snapshot.qext_1d_in_m3) &&
           is_nonnegative_cumulative(snapshot.qext_1d_out_m3) &&
           is_nonnegative_cumulative(snapshot.qext_2d_in_m3) &&
           is_nonnegative_cumulative(snapshot.qext_2d_out_m3) &&
           is_nonnegative_cumulative(snapshot.rain_in_m3) &&
           is_nonnegative_cumulative(snapshot.evaporation_out_m3) &&
           is_nonnegative_cumulative(snapshot.groundwater_in_m3) &&
           is_nonnegative_cumulative(snapshot.groundwater_out_m3);
}

bool is_monotonic_component(double current, double previous) {
    constexpr double kTolerance = 1.0e-12;
    return current + kTolerance >= previous;
}

bool is_snapshot_monotonic(const scau_dflowfm_water_balance_v1 &current,
                           const scau_dflowfm_water_balance_v1 &previous) {
    return is_monotonic_component(current.current_time_seconds, previous.current_time_seconds) &&
           is_monotonic_component(current.boundary_in_m3, previous.boundary_in_m3) &&
           is_monotonic_component(current.boundary_out_m3, previous.boundary_out_m3) &&
           is_monotonic_component(current.lateral_1d_in_m3, previous.lateral_1d_in_m3) &&
           is_monotonic_component(current.lateral_1d_out_m3, previous.lateral_1d_out_m3) &&
           is_monotonic_component(current.lateral_2d_in_m3, previous.lateral_2d_in_m3) &&
           is_monotonic_component(current.lateral_2d_out_m3, previous.lateral_2d_out_m3) &&
           is_monotonic_component(current.source_in_m3, previous.source_in_m3) &&
           is_monotonic_component(current.source_out_m3, previous.source_out_m3) &&
           is_monotonic_component(current.qext_1d_in_m3, previous.qext_1d_in_m3) &&
           is_monotonic_component(current.qext_1d_out_m3, previous.qext_1d_out_m3) &&
           is_monotonic_component(current.qext_2d_in_m3, previous.qext_2d_in_m3) &&
           is_monotonic_component(current.qext_2d_out_m3, previous.qext_2d_out_m3) &&
           is_monotonic_component(current.rain_in_m3, previous.rain_in_m3) &&
           is_monotonic_component(current.evaporation_out_m3, previous.evaporation_out_m3) &&
           is_monotonic_component(current.groundwater_in_m3, previous.groundwater_in_m3) &&
           is_monotonic_component(current.groundwater_out_m3, previous.groundwater_out_m3);
}

void emit_water_balance_snapshot(const WaterBalanceProbeState &state,
                                 const scau_dflowfm_water_balance_v1 &snapshot,
                                 const char *label,
                                 std::FILE *trace_out) {
    std::printf(
        "[wb] %s time=%.6f storage=%.15g volerr=%.15g boundary_in=%.15g boundary_out=%.15g lat1_in=%.15g lat1_out=%.15g lat2_in=%.15g lat2_out=%.15g src_in=%.15g src_out=%.15g qext1_in=%.15g qext1_out=%.15g qext2_in=%.15g qext2_out=%.15g rain=%.15g evap=%.15g gw_in=%.15g gw_out=%.15g symbol=%s abi=%s complete=%s finite=%s monotonic=%s time_valid=%s\n",
        label,
        snapshot.current_time_seconds,
        snapshot.storage_m3,
        snapshot.volume_error_cumulative_m3,
        snapshot.boundary_in_m3,
        snapshot.boundary_out_m3,
        snapshot.lateral_1d_in_m3,
        snapshot.lateral_1d_out_m3,
        snapshot.lateral_2d_in_m3,
        snapshot.lateral_2d_out_m3,
        snapshot.source_in_m3,
        snapshot.source_out_m3,
        snapshot.qext_1d_in_m3,
        snapshot.qext_1d_out_m3,
        snapshot.qext_2d_in_m3,
        snapshot.qext_2d_out_m3,
        snapshot.rain_in_m3,
        snapshot.evaporation_out_m3,
        snapshot.groundwater_in_m3,
        snapshot.groundwater_out_m3,
        state.symbol_available ? "true" : "false",
        state.abi_valid ? "true" : "false",
        state.components_complete ? "true" : "false",
        state.finite ? "true" : "false",
        state.monotonic ? "true" : "false",
        state.time_valid ? "true" : "false");
    if (trace_out != nullptr) {
        std::fprintf(
            trace_out,
            "wb,%s,%.6f,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%s,%s,%s,%s,%s,%s\n",
            label,
            snapshot.current_time_seconds,
            snapshot.storage_m3,
            snapshot.volume_error_cumulative_m3,
            snapshot.boundary_in_m3,
            snapshot.boundary_out_m3,
            snapshot.lateral_1d_in_m3,
            snapshot.lateral_1d_out_m3,
            snapshot.lateral_2d_in_m3,
            snapshot.lateral_2d_out_m3,
            snapshot.source_in_m3,
            snapshot.source_out_m3,
            snapshot.qext_1d_in_m3,
            snapshot.qext_1d_out_m3,
            snapshot.qext_2d_in_m3,
            snapshot.qext_2d_out_m3,
            snapshot.rain_in_m3,
            snapshot.evaporation_out_m3,
            snapshot.groundwater_in_m3,
            snapshot.groundwater_out_m3,
            state.symbol_available ? "true" : "false",
            state.abi_valid ? "true" : "false",
            state.components_complete ? "true" : "false",
            state.finite ? "true" : "false",
            state.monotonic ? "true" : "false",
            state.time_valid ? "true" : "false");
    }
}

bool read_water_balance(WaterBalanceFn fn,
                        WaterBalanceProbeState *state,
                        const char *label,
                        std::FILE *trace_out,
                        double observed_time) {
    scau_dflowfm_water_balance_v1 snapshot{};
    const int rc = fn(&snapshot, static_cast<std::uint32_t>(sizeof(snapshot)));
    if (rc != 0) {
        std::fprintf(stderr, "[wb] %s read failed rc=%d\n", label, rc);
        return false;
    }
    state->abi_valid = snapshot.abi_version == SCAU_DFLOWFM_WATER_BALANCE_ABI_VERSION &&
                       snapshot.struct_size == sizeof(snapshot) &&
                       snapshot.valid_components == kAllWaterBalanceComponents;
    state->components_complete = state->abi_valid;
    state->finite = is_snapshot_finite_and_nonnegative(snapshot);
    if (!state->finite) {
        std::fprintf(stderr, "[wb] %s contains non-finite or negative cumulative values\n", label);
        return false;
    }
    if (snapshot.current_time_seconds + 1.0e-9 < observed_time) {
        state->time_valid = false;
        std::fprintf(stderr,
                     "[wb] %s snapshot time %.6f is behind observed time %.6f\n",
                     label,
                     snapshot.current_time_seconds,
                     observed_time);
        return false;
    }
    if (state->last_seen_initialized && !is_snapshot_monotonic(snapshot, state->last)) {
        state->monotonic = false;
        std::fprintf(stderr, "[wb] %s cumulative values regressed\n", label);
        return false;
    }
    state->last_seen_initialized = true;
    state->last_time = snapshot.current_time_seconds;
    state->last = snapshot;
    emit_water_balance_snapshot(*state, snapshot, label, trace_out);
    return state->abi_valid && state->components_complete;
}

void probe_sum_vars(const SpikeOptions &options, int step, std::FILE *trace_out) {
    std::vector<double> hs_copy;
    std::vector<double> ba_copy;
    for (const std::string &var : options.probe_sum_vars) {
        const std::vector<double> values = copy_rank1_double_var(var.c_str());
        if (values.empty()) {
            std::printf("probe,%d,%s,unavailable\n", step, var.c_str());
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "probe,%d,%s,unavailable\n", step, var.c_str());
            }
            continue;
        }
        double sum = 0.0;
        double min_value = 0.0;
        double max_value = 0.0;
        std::size_t finite_count = 0;
        for (const double value : values) {
            if (!std::isfinite(value)) {
                continue;
            }
            if (finite_count == 0) {
                min_value = value;
                max_value = value;
            } else {
                if (value < min_value) {
                    min_value = value;
                }
                if (value > max_value) {
                    max_value = value;
                }
            }
            sum += value;
            ++finite_count;
        }
        std::printf("probe,%d,%s,%.15g,%.15g,%.15g,%zu/%zu\n",
                    step,
                    var.c_str(),
                    sum,
                    min_value,
                    max_value,
                    finite_count,
                    values.size());
        if (trace_out != nullptr) {
            std::fprintf(trace_out,
                         "probe,%d,%s,%.15g,%.15g,%.15g,%zu/%zu\n",
                         step,
                         var.c_str(),
                         sum,
                         min_value,
                         max_value,
                         finite_count,
                         values.size());
        }
        if (var == "hs") {
            hs_copy = values;
        } else if (var == "ba") {
            ba_copy = values;
        }
    }
    if (!hs_copy.empty() && !ba_copy.empty()) {
        const std::size_t n = hs_copy.size() < ba_copy.size() ? hs_copy.size() : ba_copy.size();
        double sum_hs_ba = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::isfinite(hs_copy[i]) && std::isfinite(ba_copy[i])) {
                sum_hs_ba += hs_copy[i] * ba_copy[i];
            }
        }
        std::printf("probe,%d,sum_hs_ba,%.15g\n", step, sum_hs_ba);
        if (trace_out != nullptr) {
            std::fprintf(trace_out, "probe,%d,sum_hs_ba,%.15g\n", step, sum_hs_ba);
        }
    }
    for (const std::string &var : options.probe_values_vars) {
        const std::vector<double> values = copy_rank1_double_var(var.c_str());
        if (values.empty()) {
            std::printf("values,%d,%s,unavailable\n", step, var.c_str());
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "values,%d,%s,unavailable\n", step, var.c_str());
            }
            continue;
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            std::printf("value,%d,%s,%zu,%.15g\n", step, var.c_str(), index, values[index]);
            if (trace_out != nullptr) {
                std::fprintf(trace_out,
                             "value,%d,%s,%zu,%.15g\n",
                             step,
                             var.c_str(),
                             index,
                             values[index]);
            }
        }
    }
    for (const std::string &var : options.probe_int_values_vars) {
        const std::vector<int> values = copy_rank1_int_var(var.c_str());
        if (values.empty()) {
            std::printf("int_values,%d,%s,unavailable\n", step, var.c_str());
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "int_values,%d,%s,unavailable\n", step, var.c_str());
            }
            continue;
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            std::printf("int_value,%d,%s,%zu,%d\n", step, var.c_str(), index, values[index]);
            if (trace_out != nullptr) {
                std::fprintf(trace_out,
                             "int_value,%d,%s,%zu,%d\n",
                             step,
                             var.c_str(),
                             index,
                             values[index]);
            }
        }
    }
    for (const std::string &var : options.probe_int_matrix_vars) {
        int rows = 0;
        int cols = 0;
        const std::vector<int> values = copy_rank2_int_var(var.c_str(), &rows, &cols);
        if (values.empty()) {
            std::printf("int_matrix,%d,%s,unavailable\n", step, var.c_str());
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "int_matrix,%d,%s,unavailable\n", step, var.c_str());
            }
            continue;
        }
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const std::size_t index = static_cast<std::size_t>(row * cols + col);
                std::printf("int_matrix_value,%d,%s,%d,%d,%d\n",
                            step,
                            var.c_str(),
                            row,
                            col,
                            values[index]);
                if (trace_out != nullptr) {
                    std::fprintf(trace_out,
                                 "int_matrix_value,%d,%s,%d,%d,%d\n",
                                 step,
                                 var.c_str(),
                                 row,
                                 col,
                                 values[index]);
                }
            }
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    SpikeOptions options{};
    if (!parse_options(argc, argv, &options)) {
        std::fprintf(stderr,
                     "usage: %s <config.mdu> [--steps N] [--dt seconds] "
                     "[--boundary-var name] [--stage-var name] "
                     "[--inventory-out file] [--trace-out file] [--inventory-only] "
                     "[--skip-boundary-write] [--verify-lateral-id id] "
                     "[--probe-native-water-balance] "
                     "[--probe-sum-vars v1,v2,...] [--probe-values-vars v1,v2,...] "
                     "[--probe-int-values-vars v1,v2,...] "
                     "[--probe-int-matrix-vars v1,v2,...] "
                     "[--inject-lateral-id id --inject-lateral-q q]\n",
                     argv[0]);
        return 2;
    }

    std::FILE *inventory_out = nullptr;
    std::FILE *trace_out = nullptr;
    if (options.inventory_out_path != nullptr) {
        inventory_out = std::fopen(options.inventory_out_path, "w");
        if (inventory_out == nullptr) {
            std::fprintf(stderr,
                         "[spike] failed to open inventory output: %s\n",
                         options.inventory_out_path);
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
        if (rank <= 1) {
            get_var_shape(name.data(), shape.data());
        }
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

    if (options.verify_lateral_id != nullptr) {
        const std::string lateral_var =
            std::string("laterals/") + options.verify_lateral_id + "/water_discharge";
        void *before_ptr = nullptr;
        get_var(lateral_var.c_str(), &before_ptr);
        if (before_ptr == nullptr) {
            std::fprintf(stderr,
                         "[spike] lateral verification read failed: %s\n",
                         lateral_var.c_str());
            finalize();
            return 1;
        }
        const double before = *static_cast<const double *>(before_ptr);
        constexpr double probe = 0.125;
        set_var(lateral_var.c_str(), static_cast<const void *>(&probe));
        void *probe_ptr = nullptr;
        get_var(lateral_var.c_str(), &probe_ptr);
        const bool probe_valid = probe_ptr != nullptr &&
                                 std::isfinite(*static_cast<const double *>(probe_ptr)) &&
                                 *static_cast<const double *>(probe_ptr) == probe;
        set_var(lateral_var.c_str(), static_cast<const void *>(&before));
        void *restored_ptr = nullptr;
        get_var(lateral_var.c_str(), &restored_ptr);
        const bool restore_valid = restored_ptr != nullptr &&
                                   std::isfinite(*static_cast<const double *>(restored_ptr)) &&
                                   *static_cast<const double *>(restored_ptr) == before;
        if (!probe_valid || !restore_valid) {
            std::fprintf(stderr,
                         "[spike] lateral write/restore verification failed: %s\n",
                         lateral_var.c_str());
            finalize();
            return 1;
        }
        std::printf(
            "[spike] lateral_write_restore_valid=true variable=%s before=%.12g probe=%.12g restored=%.12g\n",
            lateral_var.c_str(),
            before,
            probe,
            *static_cast<const double *>(restored_ptr));
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

    ModuleRef module = {};
    WaterBalanceFn water_balance_fn = nullptr;
    WaterBalanceProbeState water_balance_state{};
    if (options.probe_native_water_balance) {
        module = acquire_dflowfm_module();
        if (module.handle == nullptr) {
            std::fprintf(stderr, "[wb] failed to acquire dflowfm module handle\n");
            finalize();
            if (inventory_out != nullptr) {
                std::fclose(inventory_out);
            }
            if (trace_out != nullptr) {
                std::fclose(trace_out);
            }
            return 1;
        }
        water_balance_fn = reinterpret_cast<WaterBalanceFn>(
            resolve_symbol(module.handle, "dflowfm_get_water_balance_v1"));
        water_balance_state.symbol_available = water_balance_fn != nullptr;
        if (water_balance_fn == nullptr) {
            std::fprintf(stderr, "[wb] missing symbol dflowfm_get_water_balance_v1\n");
            release_module(module);
            finalize();
            if (inventory_out != nullptr) {
                std::fclose(inventory_out);
            }
            if (trace_out != nullptr) {
                std::fclose(trace_out);
            }
            return 1;
        }
    }

    double t0 = 0.0;
    double t1 = 0.0;
    double dt_internal = 0.0;
    get_start_time(&t0);
    get_end_time(&t1);
    get_time_step(&dt_internal);
    std::printf("[spike] t0=%.6f t1=%.6f engine_dt=%.6f\n", t0, t1, dt_internal);
    if (trace_out != nullptr) {
        std::fprintf(trace_out,
                     "# t0=%.6f t1=%.6f engine_dt=%.6f steps=%d dt=%.6f boundary_var=%s stage_var=%s native_probe=%s\n",
                     t0,
                     t1,
                     dt_internal,
                     options.steps,
                     options.dt_seconds,
                     options.boundary_discharge_var,
                     options.stage_var,
                     options.probe_native_water_balance ? "true" : "false");
        write_line(trace_out, "# columns: step current_time stage0");
        if (!options.probe_sum_vars.empty()) {
            write_line(trace_out,
                       "# probe columns: probe,step,varname,sum,min,max,finite_count/total (step 0 = after initialize, k = after k-th update)");
        }
        if (options.probe_native_water_balance) {
            write_line(trace_out,
                       "# wb columns: wb,label,time,storage,volerr,boundary_in,boundary_out,lat1_in,lat1_out,lat2_in,lat2_out,src_in,src_out,qext1_in,qext1_out,qext2_in,qext2_out,rain,evap,gw_in,gw_out,symbol,abi,complete,finite,monotonic,time_valid");
        }
    }

    if (!options.probe_sum_vars.empty() || !options.probe_values_vars.empty() ||
        !options.probe_int_values_vars.empty() || !options.probe_int_matrix_vars.empty()) {
        probe_sum_vars(options, 0, trace_out);
    }
    if (options.probe_native_water_balance &&
        !read_water_balance(water_balance_fn, &water_balance_state, "after-initialize", trace_out, t0)) {
        release_module(module);
        finalize();
        if (inventory_out != nullptr) {
            std::fclose(inventory_out);
        }
        if (trace_out != nullptr) {
            std::fclose(trace_out);
        }
        return 1;
    }

    int run_rc = 0;
    int completed_steps = 0;
    double previous_time = t0;
    double last_time = t0;
    double max_dt_abs_error = 0.0;
    bool time_trace_valid = true;
    const std::string inject_lateral_var =
        options.inject_lateral
            ? std::string("laterals/") + options.inject_lateral_id + "/water_discharge"
            : std::string{};
    for (int step = 0; step < options.steps; ++step) {
        const double q_inject = 1.5;
        if (!options.skip_boundary_write) {
            set_var(options.boundary_discharge_var, static_cast<const void *>(&q_inject));
        }
        if (options.inject_lateral) {
            set_var(inject_lateral_var.c_str(), static_cast<const void *>(&options.inject_lateral_q));
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
            std::fprintf(stderr,
                         "[spike] get_current_time returned non-finite value at step %d\n",
                         step);
            run_rc = 1;
            time_trace_valid = false;
            break;
        }

        const double observed_dt = t_now - previous_time;
        const double dt_abs_error = std::fabs(observed_dt - options.dt_seconds);
        if (dt_abs_error > max_dt_abs_error) {
            max_dt_abs_error = dt_abs_error;
        }
        constexpr double kTimeStepTolerance = 1.0e-9;
        if (dt_abs_error > kTimeStepTolerance) {
            time_trace_valid = false;
        }
        previous_time = t_now;
        last_time = t_now;
        ++completed_steps;

        void *engine_ptr = nullptr;
        get_var(options.stage_var, &engine_ptr);
        if (engine_ptr != nullptr) {
            const double *stage = static_cast<const double *>(engine_ptr);
            std::printf("[spike] step=%d t=%.6f stage[0]=%.6f\n", step, t_now, stage[0]);
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "%d %.6f %.6f\n", step, t_now, stage[0]);
            }
        } else {
            std::printf("[spike] step=%d t=%.6f stage var unavailable\n", step, t_now);
            if (trace_out != nullptr) {
                std::fprintf(trace_out, "%d %.6f unavailable\n", step, t_now);
            }
        }

        if (!options.probe_sum_vars.empty() || !options.probe_values_vars.empty() ||
            !options.probe_int_values_vars.empty() || !options.probe_int_matrix_vars.empty()) {
            probe_sum_vars(options, step + 1, trace_out);
        }
        if (options.probe_native_water_balance &&
            !read_water_balance(water_balance_fn, &water_balance_state, "after-update", trace_out, t_now)) {
            run_rc = 1;
            break;
        }
    }

    const double expected_last_time = t0 + static_cast<double>(completed_steps) * options.dt_seconds;
    std::printf(
        "[spike] summary completed_steps=%d requested_steps=%d last_time=%.6f expected_last_time=%.6f max_dt_abs_error=%.12g time_trace_valid=%s\n",
        completed_steps,
        options.steps,
        last_time,
        expected_last_time,
        max_dt_abs_error,
        time_trace_valid ? "true" : "false");
    if (trace_out != nullptr) {
        std::fprintf(
            trace_out,
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
    release_module(module);
    rc = run_rc != 0 ? run_rc : finalize_rc;
    if (!time_trace_valid && rc == 0) {
        rc = 1;
    }
    if (inventory_out != nullptr) {
        std::fclose(inventory_out);
    }
    if (trace_out != nullptr) {
        std::fclose(trace_out);
    }
    return rc == 0 ? 0 : 1;
}
