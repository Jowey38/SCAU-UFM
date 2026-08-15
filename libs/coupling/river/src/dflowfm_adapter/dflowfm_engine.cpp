#include "coupling/river/dflowfm_engine.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace scau::coupling::river {
namespace {

constexpr int kMaxBmiStringLength = 1024;

std::atomic<bool> g_dflowfm_project_open{false};

std::string default_library_path() {
#ifdef _WIN32
    constexpr const char* kDefaultName = "dflowfm.dll";
    char* env_path = nullptr;
    std::size_t env_size = 0;
    if (::_dupenv_s(&env_path, &env_size, "SCAU_DFLOWFM_LIBRARY") == 0 && env_path != nullptr) {
        std::string path;
        if (env_path[0] != '\0') {
            path = env_path;
        }
        std::free(env_path);
        if (!path.empty()) {
            return path;
        }
    }
#else
    constexpr const char* kDefaultName = "libdflowfm.so";
    const char* env_path = std::getenv("SCAU_DFLOWFM_LIBRARY");
    if (env_path != nullptr && env_path[0] != '\0') {
        return std::string(env_path);
    }
#endif
    return kDefaultName;
}

#ifdef _WIN32

std::string last_library_error() {
    const DWORD code = GetLastError();
    if (code == 0) {
        return "unknown loader error";
    }
    LPSTR buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);
    std::string message = size == 0 ? "unknown loader error" : std::string(buffer, size);
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message;
}

void* open_library(const std::string& path) {
    return reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
}

void close_library(void* handle) noexcept {
    if (handle != nullptr) {
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
    }
}

template <typename Fn>
Fn load_symbol(void* handle, const char* name) {
    const auto symbol = GetProcAddress(reinterpret_cast<HMODULE>(handle), name);
    if (symbol == nullptr) {
        throw DFlowFMEngineError(
            std::string("missing D-Flow FM BMI symbol '") + name + "': " + last_library_error(),
            "DFlowFM",
            "dflowfm_missing_symbol");
    }
    return reinterpret_cast<Fn>(symbol);
}

#else

std::string last_library_error() {
    const char* message = dlerror();
    return message == nullptr ? std::string("unknown loader error") : std::string(message);
}

void* open_library(const std::string& path) {
    dlerror();
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void close_library(void* handle) noexcept {
    if (handle != nullptr) {
        dlclose(handle);
    }
}

template <typename Fn>
Fn load_symbol(void* handle, const char* name) {
    dlerror();
    void* symbol = dlsym(handle, name);
    const char* error = dlerror();
    if (error != nullptr || symbol == nullptr) {
        throw DFlowFMEngineError(
            std::string("missing D-Flow FM BMI symbol '") + name + "': " + last_library_error(),
            "DFlowFM",
            "dflowfm_missing_symbol");
    }
    return reinterpret_cast<Fn>(symbol);
}

#endif

}  // namespace

// Contract linkage: these function-pointer typedefs mirror the BMI 1.0 C
// declarations in the vendored interface contract snapshot at
// extern/dflowfm/include/bmi.h (see extern/dflowfm/README.md for the mapping
// table). bmi.h is intentionally NOT included here: the adapter resolves the
// symbols by name at runtime and no third-party header may leak into the
// main graph. If the vendored header is refreshed, re-verify these typedefs.
struct DFlowFMEngine::BmiApi {
    using InitializeFn = int (*)(const char*);
    using UpdateFn = int (*)(double);
    using FinalizeFn = int (*)();
    using GetStartTimeFn = void (*)(double*);
    using GetCurrentTimeFn = void (*)(double*);
    using GetTimeStepFn = void (*)(double*);
    using GetVarFn = void (*)(const char*, void**);
    using SetVarFn = void (*)(const char*, const void*);
    using GetVarCountFn = void (*)(int*);
    using GetVarNameFn = void (*)(int, char*);
    using GetVarTypeFn = void (*)(const char*, char*);
    using GetVarRankFn = void (*)(const char*, int*);
    using GetVarShapeFn = void (*)(const char*, int*);

    InitializeFn initialize{nullptr};
    UpdateFn update{nullptr};
    FinalizeFn finalize{nullptr};
    GetStartTimeFn get_start_time{nullptr};
    GetCurrentTimeFn get_current_time{nullptr};
    GetTimeStepFn get_time_step{nullptr};
    GetVarFn get_var{nullptr};
    SetVarFn set_var{nullptr};
    GetVarCountFn get_var_count{nullptr};
    GetVarNameFn get_var_name{nullptr};
    GetVarTypeFn get_var_type{nullptr};
    GetVarRankFn get_var_rank{nullptr};
    GetVarShapeFn get_var_shape{nullptr};
};

DFlowFMEngine::DFlowFMEngine(std::string library_path) : library_path_(std::move(library_path)) {}

DFlowFMEngine::~DFlowFMEngine() {
    if (initialized_ && api_ != nullptr && api_->finalize != nullptr) {
        static_cast<void>(api_->finalize());
        g_dflowfm_project_open.store(false);
    }
    unload_library();
}

void DFlowFMEngine::initialize(const std::string& mdu_path) {
    if (initialized_) {
        throw DFlowFMEngineError("D-Flow FM engine is already initialized");
    }
    if (mdu_path.empty()) {
        throw DFlowFMEngineError("mdu_path must not be empty");
    }
    bool expected = false;
    if (!g_dflowfm_project_open.compare_exchange_strong(expected, true)) {
        throw DFlowFMEngineError(
            "another D-Flow FM engine instance already owns the process-wide BMI state",
            "DFlowFM",
            "dflowfm_project_already_open");
    }

    try {
        load_library();
        const int rc = api_->initialize(mdu_path.c_str());
        if (rc != 0) {
            g_dflowfm_project_open.store(false);
            throw DFlowFMEngineError(
                "D-Flow FM initialize failed with code " + std::to_string(rc),
                "DFlowFM",
                "dflowfm_initialize_failed");
        }
        initialized_ = true;
        api_->get_start_time(&start_time_);
        current_time_ = start_time_;
    } catch (...) {
        if (!initialized_) {
            unload_library();
            g_dflowfm_project_open.store(false);
        }
        throw;
    }
}

void DFlowFMEngine::update(double dt_dfm) {
    require_initialized();
    if (!std::isfinite(dt_dfm) || dt_dfm <= 0.0) {
        throw DFlowFMEngineError("dt_dfm must be finite and positive");
    }
    double engine_dt = 0.0;
    api_->get_time_step(&engine_dt);
    if (!std::isfinite(engine_dt) || engine_dt <= 0.0) {
        throw DFlowFMEngineError(
            "D-Flow FM reported an invalid internal time step",
            "DFlowFM",
            "dflowfm_invalid_time_step");
    }
    constexpr double kTimeStepTolerance = 1.0e-9;
    if (std::abs(dt_dfm - engine_dt) > kTimeStepTolerance) {
        throw DFlowFMEngineError(
            "requested D-Flow FM dt does not match the engine time step",
            "DFlowFM",
            "dflowfm_time_step_mismatch");
    }
    const int rc = api_->update(dt_dfm);
    if (rc != 0) {
        throw DFlowFMEngineError(
            "D-Flow FM update failed with code " + std::to_string(rc),
            "DFlowFM",
            "dflowfm_update_failed");
    }
    api_->get_current_time(&current_time_);
}

void DFlowFMEngine::finalize() {
    if (!initialized_) {
        unload_library();
        return;
    }
    const int rc = api_->finalize();
    initialized_ = false;
    start_time_ = 0.0;
    current_time_ = 0.0;
    g_dflowfm_project_open.store(false);
    unload_library();
    if (rc != 0) {
        throw DFlowFMEngineError(
            "D-Flow FM finalize failed with code " + std::to_string(rc),
            "DFlowFM",
            "dflowfm_finalize_failed");
    }
}

double DFlowFMEngine::get_value(const std::string& var_name, int location_id) const {
    require_valid_location(location_id);
    if (var_name.empty()) {
        throw DFlowFMEngineError("D-Flow FM variable name must not be empty");
    }

    void* engine_ptr = nullptr;
    api_->get_var(var_name.c_str(), &engine_ptr);
    if (engine_ptr == nullptr) {
        throw DFlowFMEngineError(
            "D-Flow FM variable is unavailable: " + var_name,
            "DFlowFM",
            "dflowfm_variable_unavailable");
    }
    const auto* values = static_cast<const double*>(engine_ptr);
    return values[location_id];
}

std::vector<double> DFlowFMEngine::get_rank1_double_values(
    const std::string& var_name) const {
    require_initialized();
    if (var_name.empty()) {
        throw DFlowFMEngineError("D-Flow FM variable name must not be empty");
    }

    std::array<char, kMaxBmiStringLength> type{};
    api_->get_var_type(var_name.c_str(), type.data());
    if (std::string(type.data()) != "double") {
        throw DFlowFMEngineError(
            "D-Flow FM variable is not double: " + var_name,
            "DFlowFM",
            "dflowfm_variable_type_mismatch");
    }

    int rank = 0;
    api_->get_var_rank(var_name.c_str(), &rank);
    if (rank != 1) {
        throw DFlowFMEngineError(
            "D-Flow FM variable is not rank 1: " + var_name,
            "DFlowFM",
            "dflowfm_variable_rank_mismatch");
    }

    // Real-runtime evidence shows get_var_shape can access-violate for some
    // rank>1 variables. Query it only after requiring rank == 1.
    std::array<int, 6> shape{};
    api_->get_var_shape(var_name.c_str(), shape.data());
    if (shape[0] <= 0) {
        throw DFlowFMEngineError(
            "D-Flow FM variable has invalid rank-1 shape: " + var_name,
            "DFlowFM",
            "dflowfm_variable_shape_invalid");
    }

    void* engine_ptr = nullptr;
    api_->get_var(var_name.c_str(), &engine_ptr);
    if (engine_ptr == nullptr) {
        throw DFlowFMEngineError(
            "D-Flow FM variable is unavailable: " + var_name,
            "DFlowFM",
            "dflowfm_variable_unavailable");
    }
    const auto* values = static_cast<const double*>(engine_ptr);
    return std::vector<double>(values, values + shape[0]);
}

void DFlowFMEngine::set_value(const std::string& var_name, int location_id, double value) {
    require_valid_location(location_id);
    if (var_name.empty()) {
        throw DFlowFMEngineError("D-Flow FM variable name must not be empty");
    }
    if (!std::isfinite(value)) {
        throw DFlowFMEngineError("D-Flow FM value must be finite");
    }

    // BMI 1.0 set_var writes a whole variable buffer. Until G11 records a real
    // variable shape inventory and lands set_var_slice evidence, this adapter
    // intentionally supports scalar/first-entry writes only.
    if (location_id != 0) {
        throw DFlowFMEngineError(
            "D-Flow FM runtime adapter currently supports only scalar location 0 writes",
            "DFlowFM",
            "dflowfm_slice_write_not_ready");
    }
    api_->set_var(var_name.c_str(), static_cast<const void*>(&value));
}

bool DFlowFMEngine::initialized() const noexcept {
    return initialized_;
}

double DFlowFMEngine::current_time() const {
    require_initialized();
    return current_time_;
}

double DFlowFMEngine::elapsed_time() const noexcept {
    return initialized_ ? current_time_ - start_time_ : 0.0;
}

int DFlowFMEngine::variable_count() const {
    require_initialized();
    int count = 0;
    api_->get_var_count(&count);
    return count;
}

std::vector<std::string> DFlowFMEngine::variable_names() const {
    require_initialized();
    const int count = variable_count();
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        std::array<char, kMaxBmiStringLength> name{};
        api_->get_var_name(i, name.data());
        names.emplace_back(name.data());
    }
    return names;
}

namespace {

// Private mirror of the project-authored bridge ABI contract snapshot at
// extern/dflowfm/include/scau_dflowfm_water_balance_v1.h. The header is
// intentionally NOT included here, matching the bmi.h typedef policy above:
// the adapter resolves the symbol by name at runtime and keeps every external
// layout mirrored in this translation unit. Re-verify on contract refresh.
struct NativeWaterBalanceV1 {
    std::uint32_t abi_version;
    std::uint32_t struct_size;
    std::uint64_t valid_components;
    double current_time_seconds;
    double storage_m3;
    double volume_error_cumulative_m3;
    double boundary_in_m3;
    double boundary_out_m3;
    double lateral_1d_in_m3;
    double lateral_1d_out_m3;
    double lateral_2d_in_m3;
    double lateral_2d_out_m3;
    double source_in_m3;
    double source_out_m3;
    double qext_1d_in_m3;
    double qext_1d_out_m3;
    double qext_2d_in_m3;
    double qext_2d_out_m3;
    double rain_in_m3;
    double evaporation_out_m3;
    double groundwater_in_m3;
    double groundwater_out_m3;
};

using GetWaterBalanceV1Fn = int (*)(NativeWaterBalanceV1*, std::uint32_t);

constexpr std::uint32_t kWaterBalanceAbiVersion = 1U;
constexpr std::uint64_t kWaterBalanceAllComponents = 0xFFULL;

bool is_valid_cumulative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

}  // namespace

int DFlowFMEngine::internal_cell_count() const {
    require_initialized();
    constexpr const char* kVariable = "ndxi";
    std::array<char, kMaxBmiStringLength> type{};
    api_->get_var_type(kVariable, type.data());
    int rank = -1;
    api_->get_var_rank(kVariable, &rank);
    if (rank != 0 || std::strcmp(type.data(), "int") != 0) {
        throw DFlowFMEngineError(
            "D-Flow FM ndxi must be a scalar int variable",
            "DFlowFM",
            "dflowfm_ndxi_contract_mismatch");
    }
    void* engine_ptr = nullptr;
    api_->get_var(kVariable, &engine_ptr);
    if (engine_ptr == nullptr) {
        throw DFlowFMEngineError(
            "D-Flow FM ndxi is unavailable through BMI",
            "DFlowFM",
            "dflowfm_ndxi_unavailable");
    }
    const int count = *static_cast<const int*>(engine_ptr);
    if (count <= 0) {
        throw DFlowFMEngineError(
            "D-Flow FM ndxi must be positive",
            "DFlowFM",
            "dflowfm_ndxi_invalid");
    }
    return count;
}

DFlowFMNativeWaterBalance DFlowFMEngine::observe_native_water_balance() const {
    require_initialized();
    const auto fn = load_symbol<GetWaterBalanceV1Fn>(
        library_handle_, "dflowfm_get_water_balance_v1");

    NativeWaterBalanceV1 raw{};
    const int rc = fn(&raw, static_cast<std::uint32_t>(sizeof(raw)));
    if (rc != 0) {
        throw DFlowFMEngineError(
            "D-Flow FM native water-balance read failed with code " + std::to_string(rc),
            "DFlowFM",
            "dflowfm_water_balance_read_failed");
    }
    if (raw.abi_version != kWaterBalanceAbiVersion ||
        raw.struct_size != static_cast<std::uint32_t>(sizeof(raw)) ||
        raw.valid_components != kWaterBalanceAllComponents) {
        throw DFlowFMEngineError(
            "D-Flow FM native water-balance ABI mismatch",
            "DFlowFM",
            "dflowfm_water_balance_abi_mismatch");
    }

    DFlowFMNativeWaterBalance observation{};
    observation.current_time_seconds = raw.current_time_seconds;
    observation.storage_m3 = raw.storage_m3;
    observation.volume_error_cumulative_m3 = raw.volume_error_cumulative_m3;
    observation.boundary_in_m3 = raw.boundary_in_m3;
    observation.boundary_out_m3 = raw.boundary_out_m3;
    observation.lateral_1d_in_m3 = raw.lateral_1d_in_m3;
    observation.lateral_1d_out_m3 = raw.lateral_1d_out_m3;
    observation.lateral_2d_in_m3 = raw.lateral_2d_in_m3;
    observation.lateral_2d_out_m3 = raw.lateral_2d_out_m3;
    observation.source_in_m3 = raw.source_in_m3;
    observation.source_out_m3 = raw.source_out_m3;
    observation.qext_1d_in_m3 = raw.qext_1d_in_m3;
    observation.qext_1d_out_m3 = raw.qext_1d_out_m3;
    observation.qext_2d_in_m3 = raw.qext_2d_in_m3;
    observation.qext_2d_out_m3 = raw.qext_2d_out_m3;
    observation.rain_in_m3 = raw.rain_in_m3;
    observation.evaporation_out_m3 = raw.evaporation_out_m3;
    observation.groundwater_in_m3 = raw.groundwater_in_m3;
    observation.groundwater_out_m3 = raw.groundwater_out_m3;

    constexpr double kTimeToleranceSeconds = 1.0e-6;
    observation.scope_complete =
        std::isfinite(raw.current_time_seconds) &&
        std::abs(raw.current_time_seconds - current_time_) <= kTimeToleranceSeconds &&
        is_valid_cumulative(raw.storage_m3) &&
        std::isfinite(raw.volume_error_cumulative_m3) &&
        is_valid_cumulative(raw.boundary_in_m3) &&
        is_valid_cumulative(raw.boundary_out_m3) &&
        is_valid_cumulative(raw.lateral_1d_in_m3) &&
        is_valid_cumulative(raw.lateral_1d_out_m3) &&
        is_valid_cumulative(raw.lateral_2d_in_m3) &&
        is_valid_cumulative(raw.lateral_2d_out_m3) &&
        is_valid_cumulative(raw.source_in_m3) &&
        is_valid_cumulative(raw.source_out_m3) &&
        is_valid_cumulative(raw.qext_1d_in_m3) &&
        is_valid_cumulative(raw.qext_1d_out_m3) &&
        is_valid_cumulative(raw.qext_2d_in_m3) &&
        is_valid_cumulative(raw.qext_2d_out_m3) &&
        is_valid_cumulative(raw.rain_in_m3) &&
        is_valid_cumulative(raw.evaporation_out_m3) &&
        is_valid_cumulative(raw.groundwater_in_m3) &&
        is_valid_cumulative(raw.groundwater_out_m3);
    return observation;
}

void DFlowFMEngine::load_library() {
    if (library_handle_ != nullptr) {
        return;
    }
    library_handle_ = open_library(library_path());
    if (library_handle_ == nullptr) {
        throw DFlowFMEngineError(
            "failed to load D-Flow FM BMI library '" + library_path() + "': " + last_library_error(),
            "DFlowFM",
            "dflowfm_library_load_failed");
    }

    auto api = std::make_unique<BmiApi>();
    try {
        api->initialize = load_symbol<BmiApi::InitializeFn>(library_handle_, "initialize");
        api->update = load_symbol<BmiApi::UpdateFn>(library_handle_, "update");
        api->finalize = load_symbol<BmiApi::FinalizeFn>(library_handle_, "finalize");
        api->get_start_time = load_symbol<BmiApi::GetStartTimeFn>(library_handle_, "get_start_time");
        api->get_current_time = load_symbol<BmiApi::GetCurrentTimeFn>(library_handle_, "get_current_time");
        api->get_time_step = load_symbol<BmiApi::GetTimeStepFn>(library_handle_, "get_time_step");
        api->get_var = load_symbol<BmiApi::GetVarFn>(library_handle_, "get_var");
        api->set_var = load_symbol<BmiApi::SetVarFn>(library_handle_, "set_var");
        api->get_var_count = load_symbol<BmiApi::GetVarCountFn>(library_handle_, "get_var_count");
        api->get_var_name = load_symbol<BmiApi::GetVarNameFn>(library_handle_, "get_var_name");
        api->get_var_type = load_symbol<BmiApi::GetVarTypeFn>(library_handle_, "get_var_type");
        api->get_var_rank = load_symbol<BmiApi::GetVarRankFn>(library_handle_, "get_var_rank");
        api->get_var_shape = load_symbol<BmiApi::GetVarShapeFn>(library_handle_, "get_var_shape");
    } catch (...) {
        close_library(library_handle_);
        library_handle_ = nullptr;
        throw;
    }
    api_ = api.release();
}

void DFlowFMEngine::unload_library() noexcept {
    delete api_;
    api_ = nullptr;
    close_library(library_handle_);
    library_handle_ = nullptr;
}

void DFlowFMEngine::require_loaded() const {
    if (api_ == nullptr || library_handle_ == nullptr) {
        throw DFlowFMEngineError("D-Flow FM BMI library is not loaded");
    }
}

void DFlowFMEngine::require_initialized() const {
    require_loaded();
    if (!initialized_) {
        throw DFlowFMEngineError("D-Flow FM engine is not initialized");
    }
}

void DFlowFMEngine::require_valid_location(int location_id) const {
    require_initialized();
    if (location_id < 0) {
        throw DFlowFMEngineError("location_id must be non-negative");
    }
}

const std::string& DFlowFMEngine::library_path() const {
    if (library_path_.empty()) {
        static const std::string path = default_library_path();
        return path;
    }
    return library_path_;
}

core::EngineReport make_dflowfm_engine_report(const DFlowFMEngine& engine) {
    return core::EngineReport{
        .healthy = engine.initialized(),
        .engine_id = "DFlowFM",
        .error_code = engine.initialized() ? "" : "dflowfm_engine_not_initialized",
        .elapsed_time = engine.elapsed_time(),
    };
}

}  // namespace scau::coupling::river
