#pragma once

#include <string>
#include <vector>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::river {

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
    void set_value(const std::string& var_name, int location_id, double value) override;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] double current_time() const;
    [[nodiscard]] double elapsed_time() const noexcept;
    [[nodiscard]] int variable_count() const;
    [[nodiscard]] std::vector<std::string> variable_names() const;

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
