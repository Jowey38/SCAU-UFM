#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace scau::apps::sim_driver {

inline constexpr int kRuntimeConfigVersion = 2;

enum class SimDriverState {
    created,
    configured,
    initialized,
    running,
    review_required,
    aborted,
    completed,
};

enum class EngineMode {
    mock,
    real,
};

// One 2D surface cell draining into one SWMM node through a head-driven
// exchange structure. `node_name` resolves through SwmmEngine::node_index in
// real mode and through a strict non-negative integer parse in mock mode.
struct SurfaceDrainageLinkConfig {
    std::size_t cell{0U};
    std::string node_name{};
    double crest_level{0.0};
    double exchange_width{0.0};
    double priority_weight{1.0};
};

// One 2D surface cell coupled to one D-Flow FM location through a head-driven
// exchange structure. `native_lateral_id` is the engine-native compound
// lateral identifier (case-owned; stays outside core DTOs).
struct SurfaceRiverLinkConfig {
    std::size_t cell{0U};
    int location_id{0};
    std::string native_lateral_id{};
    double crest_level{0.0};
    double exchange_width{0.0};
    double priority_weight{1.0};
};

// SWMM outfall discharge routed into a D-Flow FM lateral.
struct DrainageRiverLinkConfig {
    std::string outfall_name{};
    int river_location_id{0};
    double q_capacity{0.0};
    bool drive_outfall_stage{true};
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
    // v2 run-loop fields. STCF carries no hydrodynamic initial state, so the
    // initial lake-at-rest water surface elevation is a required run input.
    double initial_eta{std::numeric_limits<double>::quiet_NaN()};
    double h_wet{1.0e-6};
    double cfl_safety{0.45};
    double c_rollback{1.0};
    // Opt-in CVC side-specific fluctuations (G23). Required for exact
    // phi_t*h*A closure when the case has spatial phi_t jumps under nonzero
    // velocity; stays default-off per the project-wide CVC decision.
    bool enable_cvc_spatial_phi_t_correction{false};
    // M270 whole-system audit. Disabled by default for backward compatibility;
    // when enabled the run requires complete SWMM and D-Flow storage providers.
    bool enable_whole_system_mass_audit{false};
    // Main Spec default N_writeoff_steps = 3 committed coupling epochs.
    std::size_t n_writeoff_steps{3U};
    // Real third-party path tolerance. Strict mock/correctness mode ignores
    // these and uses epsilon_deficit only.
    double mass_audit_engine_residual_absolute{1.0e-4};
    double mass_audit_engine_residual_relative{1.0e-6};
    // Documented bound for a real engine's own internal continuity gap
    // (M277 audit decomposition); 0 disables any allowance.
    double mass_audit_engine_internal_gap_absolute{0.0};
    EngineMode engine_mode{EngineMode::mock};
    // Project-standard variable name; real BMI kernels expose "s1".
    std::string river_water_level_variable{"water_level"};
    std::string output_summary_path;
    std::vector<SurfaceDrainageLinkConfig> surface_drainage{};
    std::vector<SurfaceRiverLinkConfig> surface_river{};
    std::vector<DrainageRiverLinkConfig> drainage_river{};
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
