#pragma once

#include <string>
#include <vector>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::river {

// Engine-native cumulative water-balance snapshot read through the
// project-authored bridge ABI `dflowfm_get_water_balance_v1` compiled into
// the governed external dflowfm.dll (contract snapshot:
// extern/dflowfm/include/scau_dflowfm_water_balance_v1.h).
//
// Cumulative semantics (M274 contract evidence): every *_m3 component is a
// cumulative volume since the LAST engine initialize and resets to zero on
// every initialize, including a restart reload. Consumers must re-baseline
// across an initialize boundary and must never compare raw counters across
// one. This is a raw observation DTO; audit/dedup policy is driver-owned.
struct DFlowFMNativeWaterBalance {
    double current_time_seconds{0.0};
    double storage_m3{0.0};
    double volume_error_cumulative_m3{0.0};
    double boundary_in_m3{0.0};
    double boundary_out_m3{0.0};
    double lateral_1d_in_m3{0.0};
    double lateral_1d_out_m3{0.0};
    double lateral_2d_in_m3{0.0};
    double lateral_2d_out_m3{0.0};
    double source_in_m3{0.0};
    double source_out_m3{0.0};
    double qext_1d_in_m3{0.0};
    double qext_1d_out_m3{0.0};
    double qext_2d_in_m3{0.0};
    double qext_2d_out_m3{0.0};
    double rain_in_m3{0.0};
    double evaporation_out_m3{0.0};
    double groundwater_in_m3{0.0};
    double groundwater_out_m3{0.0};
    bool scope_complete{false};
};

// Runtime-loaded D-Flow FM BMI 1.0 engine.
//
// Boundary contract (architecture spec + project-layout-design.md firewall):
// - Lifecycle and state read/write only. No Q_limit / V_limit / deficit /
//   rollback / replay / arbitration semantics live here; those are core-owned.
// - No D-Flow FM header or third-party type leaks through this header or any DTO.
// - The BMI 1.0 API is a process-global free-function ABI, so at most one
//   DFlowFMEngine may be initialized in a process at a time.
//
// Units: values pass through in the unit system of the loaded D-Flow FM model.
// SCAU-UFM expects SI project configuration at this boundary.
class DFlowFMEngine final : public IDFlowFMEngine {
public:
    DFlowFMEngine() = default;
    explicit DFlowFMEngine(std::string library_path);
    ~DFlowFMEngine() override;

    DFlowFMEngine(const DFlowFMEngine&) = delete;
    DFlowFMEngine& operator=(const DFlowFMEngine&) = delete;
    DFlowFMEngine(DFlowFMEngine&&) = delete;
    DFlowFMEngine& operator=(DFlowFMEngine&&) = delete;

    void initialize(const std::string& mdu_path) override;
    void update(double dt_dfm) override;
    void finalize() override;

    [[nodiscard]] double get_value(const std::string& var_name, int location_id) const override;
    [[nodiscard]] std::vector<double> get_rank1_double_values(
        const std::string& var_name) const override;
    void set_value(const std::string& var_name, int location_id, double value) override;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] double current_time() const;
    [[nodiscard]] double elapsed_time() const noexcept;
    [[nodiscard]] int variable_count() const;
    [[nodiscard]] std::vector<std::string> variable_names() const;

    // Concrete-engine-only native observation (NOT on IDFlowFMEngine, which
    // wraps lifecycle and state read/write only): reads the cumulative
    // water-balance snapshot through the bridge ABI. Fails closed when the
    // symbol is missing (un-bridged DLL), the ABI version/size/component set
    // mismatches, or the read reports an error. Value-level validity
    // (finite, non-negative cumulatives, time consistency) is reported via
    // scope_complete rather than thrown, mirroring
    // SwmmEngine::observe_external_net_volume.
    [[nodiscard]] DFlowFMNativeWaterBalance observe_native_water_balance() const;

    // Concrete-engine-only: number of internal flow nodes (BMI scalar `ndxi`).
    // BMI rank-1 arrays over flow nodes store internal cells first; entries
    // beyond ndxi are boundary ghost nodes outside the physical domain
    // (bug-207: summing full `vol1` overcounts storage on open-boundary
    // models). Fails closed on missing/mistyped/non-positive values.
    [[nodiscard]] int internal_cell_count() const;

private:
    struct BmiApi;

    void load_library();
    void unload_library() noexcept;
    void require_loaded() const;
    void require_initialized() const;
    void require_valid_location(int location_id) const;
    [[nodiscard]] const std::string& library_path() const;

    std::string library_path_{};
    void* library_handle_{nullptr};
    BmiApi* api_{nullptr};
    bool initialized_{false};
    double start_time_{0.0};
    double current_time_{0.0};
};

[[nodiscard]] core::EngineReport make_dflowfm_engine_report(const DFlowFMEngine& engine);

}  // namespace scau::coupling::river
