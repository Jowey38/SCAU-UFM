#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace scau::coupling::driver {

// One post-replay epoch-boundary observation. Internal transfers between the
// three engines are intentionally absent: surface drainage leaves
// surface_volume and enters an engine storage in the same epoch.
// coupling_deficit_volume is reported as an obligation/ledger diagnostic but
// is NOT physical stored water: v_unmet remains in Surface2D and adding the
// deficit to storage_total would double-count it.
struct WholeSystemMassSample {
    std::uint64_t epoch{0U};
    double logical_time{0.0};
    double surface_volume{0.0};
    // M_ref diagnostic using the configured h_wet threshold. This is separate
    // from physical surface_volume, which counts all h >= 0 water.
    double surface_reference_volume{0.0};
    double coupling_deficit_volume{0.0};
    std::optional<double> swmm_storage_volume{};
    std::optional<double> dflowfm_volume{};
    // Complete cumulative external net-volume observations owned by each 1D
    // engine (inflow positive, outflow/loss negative), since engine init.
    // nullopt means the storage is observable but external flux scope is
    // incomplete; the report MUST be REVIEW_REQUIRED regardless of residual.
    std::optional<double> swmm_external_net_volume{};
    std::optional<double> dflowfm_external_net_volume{};
    // Cumulative Surface2D terms since the baseline epoch.
    double cumulative_boundary_inflow_volume{0.0};
    double cumulative_rainfall_volume{0.0};
    double cumulative_infiltration_volume{0.0};
    double cumulative_abstraction_volume{0.0};
    double cumulative_depression_storage_delta_volume{0.0};
};

enum class WholeSystemMassVerdict {
    conserved,
    review_required,
};

struct WholeSystemMassTolerance {
    // Deterministic/correctness path: epsilon_deficit only.
    bool strict{true};
    // Real third-party engine path: additional documented tolerance, combined
    // as max(epsilon_deficit, absolute + relative * max(1, baseline storage)).
    double engine_residual_absolute{0.0};
    double engine_residual_relative{0.0};
};

struct WholeSystemMassAuditReport {
    WholeSystemMassSample baseline{};
    WholeSystemMassSample current{};
    double baseline_storage_total{0.0};
    double current_storage_total{0.0};
    double external_net_volume{0.0};
    double residual{0.0};
    // Canonical symbols-reference tolerance:
    // max(1e-10, 1e-12 * M_ref), M_ref = baseline surface volume.
    double epsilon_deficit{0.0};
    double applied_tolerance{0.0};
    bool scope_complete{false};
    bool conserved{false};
    WholeSystemMassVerdict verdict{WholeSystemMassVerdict::review_required};
};

// Closure equation:
//   S(t) = surface + swmm + dflowfm + depression_delta
//   External(t) = boundary_inflow + rainfall - infiltration - abstraction
//                 + swmm_external_net + dflowfm_external_net
//   residual = [S(t) - S(t0)] - External(t)
// A residual is verdict-eligible only when both engine external-net terms are
// present at baseline and current; otherwise scope_complete=false and the
// verdict is review_required even if the raw residual happens to be small.
//
// Fail-closed: non-finite/negative storage terms, storage-scope asymmetry
// between baseline/current (one side nullopt), non-monotone epoch/time, or
// invalid tolerance throw std::invalid_argument. Missing external-net terms
// are not an exception: they produce scope_complete=false / review_required
// so the raw residual remains reportable as evidence.
[[nodiscard]] WholeSystemMassAuditReport audit_whole_system_mass(
    const WholeSystemMassSample& baseline,
    const WholeSystemMassSample& current,
    const WholeSystemMassTolerance& tolerance = {});

// Deficit write-off remains M271 scope. M270 makes the trigger observable by
// reporting consecutive non-zero age per account at epoch boundaries.
struct DeficitAgeObservation {
    std::size_t account_index{0U};
    std::size_t deficit_age_steps{0U};
    double volume{0.0};
};

[[nodiscard]] std::vector<DeficitAgeObservation> update_deficit_ages(
    const std::vector<double>& account_volumes,
    const std::vector<DeficitAgeObservation>& previous);

}  // namespace scau::coupling::driver
