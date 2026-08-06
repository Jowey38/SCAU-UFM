#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace scau::apps::sim_driver {

// One committed coupling epoch as recorded by the run loop. Masses are the
// CouplingState exchange-cell audit values (phi_t * h * A over coupled cells
// plus deficit ledger), not a whole-system audit (M270 scope).
struct EpochRecord {
    std::uint64_t epoch{0U};
    double logical_time{0.0};
    double coupling_surface_mass_before{0.0};
    double coupling_surface_mass_after{0.0};
    double coupling_deficit_mass_after{0.0};
    double drained_volume{0.0};
    double returned_volume{0.0};
    double max_cell_cfl{0.0};
    std::size_t wet_cell_count{0U};
    // M269 epoch commit protocol: only "committed" epochs appear here; the
    // hashes make the commit reproducible in evidence.
    std::string checkpoint_status{};
    std::string surface_content_hash{};
    std::string coupling_content_hash{};
    // M270 whole-system physical-storage audit. Empty runs leave enabled=false.
    bool whole_system_mass_audit_enabled{false};
    double whole_system_storage_total{0.0};
    double whole_system_mass_residual{0.0};
    double whole_system_mass_tolerance{0.0};
    std::string whole_system_mass_verdict{};
    std::vector<std::size_t> deficit_age_steps{};
    std::vector<double> deficit_account_volumes{};
};

struct RunSummary {
    std::string outcome{};  // "completed" | "review_required" | "aborted"
    std::string reason{};
    std::size_t committed_epochs{0U};
    double final_time{0.0};
    double total_drained_volume{0.0};
    double total_returned_volume{0.0};
    double total_boundary_inflow_volume{0.0};
    double final_surface_physical_volume{0.0};
    double final_coupling_deficit_volume{0.0};
    // M269 recovery evidence: empty on a clean run. recovery_action is
    // "restored_to_last_commit" (failure before engines advanced) or
    // "refused_engine_rollback" (failure at/after engine advancement).
    std::string recovery_action{};
    std::string dflowfm_rollback_decision{};
    std::string final_surface_state_hash{};
    bool whole_system_mass_audit_enabled{false};
    std::string whole_system_mass_verdict{};
    double final_whole_system_mass_residual{0.0};
    double max_abs_whole_system_mass_residual{0.0};
    double whole_system_mass_tolerance{0.0};
    std::vector<EpochRecord> epochs{};
};

// Hand-rolled JSON emission (no third-party dependency); doubles are written
// with 17 significant digits so the summary is deterministic and lossless.
[[nodiscard]] std::string to_json(const RunSummary& summary);

void write_summary_json(const std::filesystem::path& path, const RunSummary& summary);

}  // namespace scau::apps::sim_driver
