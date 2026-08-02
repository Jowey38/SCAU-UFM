#pragma once

#include <cstddef>
#include <string>

namespace scau::apps::sim_driver {

inline constexpr int kRuntimeConfigVersion = 1;

enum class SimDriverState {
    created,
    configured,
    initialized,
    running,
    review_required,
    aborted,
    completed,
};

struct RuntimeConfig {
    int version{kRuntimeConfigVersion};
    double start_time{0.0};
    double end_time{0.0};
    double dt_couple{0.0};
    double dt_surface{0.0};
    double dt_swmm{0.0};
    double dt_dflowfm{0.0};
    bool enable_swmm{false};
    bool enable_dflowfm{false};
    std::string stcf_case_path;
    std::string swmm_inp_path;
    std::string dflowfm_mdu_path;
};

void validate_runtime_config(const RuntimeConfig& config);

class SimDriver {
public:
    SimDriver() = default;

    void configure(RuntimeConfig config);
    void initialize();
    void start();
    void complete();
    void require_review();
    void abort();

    [[nodiscard]] SimDriverState state() const noexcept;
    [[nodiscard]] const RuntimeConfig& config() const;
    [[nodiscard]] std::size_t completed_coupling_steps() const noexcept;

    // Records a committed orchestration boundary. Numerical advancement remains
    // owned by Surface2D/CouplingDriver; this class does not duplicate physics.
    void record_committed_coupling_step();

private:
    RuntimeConfig config_{};
    SimDriverState state_{SimDriverState::created};
    bool configured_{false};
    std::size_t completed_coupling_steps_{0U};
};

}  // namespace scau::apps::sim_driver
