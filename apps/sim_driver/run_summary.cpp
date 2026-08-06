#include "run_summary.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace scau::apps::sim_driver {

namespace {

std::string escape_json_string(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
        }
    }
    return escaped;
}

}  // namespace

std::string to_json(const RunSummary& summary) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\n";
    out << "  \"outcome\": \"" << escape_json_string(summary.outcome) << "\",\n";
    out << "  \"reason\": \"" << escape_json_string(summary.reason) << "\",\n";
    out << "  \"committed_epochs\": " << summary.committed_epochs << ",\n";
    out << "  \"final_time\": " << summary.final_time << ",\n";
    out << "  \"total_drained_volume\": " << summary.total_drained_volume << ",\n";
    out << "  \"total_returned_volume\": " << summary.total_returned_volume << ",\n";
    out << "  \"total_boundary_inflow_volume\": " << summary.total_boundary_inflow_volume
        << ",\n";
    out << "  \"final_surface_physical_volume\": " << summary.final_surface_physical_volume
        << ",\n";
    out << "  \"final_coupling_deficit_volume\": " << summary.final_coupling_deficit_volume
        << ",\n";
    out << "  \"recovery_action\": \"" << escape_json_string(summary.recovery_action)
        << "\",\n";
    out << "  \"dflowfm_rollback_decision\": \""
        << escape_json_string(summary.dflowfm_rollback_decision) << "\",\n";
    out << "  \"final_surface_state_hash\": \""
        << escape_json_string(summary.final_surface_state_hash) << "\",\n";
    out << "  \"whole_system_mass_audit_enabled\": "
        << (summary.whole_system_mass_audit_enabled ? "true" : "false") << ",\n";
    out << "  \"whole_system_mass_verdict\": \""
        << escape_json_string(summary.whole_system_mass_verdict) << "\",\n";
    out << "  \"final_whole_system_mass_residual\": "
        << summary.final_whole_system_mass_residual << ",\n";
    out << "  \"max_abs_whole_system_mass_residual\": "
        << summary.max_abs_whole_system_mass_residual << ",\n";
    out << "  \"whole_system_mass_tolerance\": "
        << summary.whole_system_mass_tolerance << ",\n";
    out << "  \"epochs\": [";
    for (std::size_t index = 0U; index < summary.epochs.size(); ++index) {
        const EpochRecord& record = summary.epochs[index];
        out << (index == 0U ? "\n" : ",\n");
        out << "    {\"epoch\": " << record.epoch
            << ", \"logical_time\": " << record.logical_time
            << ", \"coupling_surface_mass_before\": " << record.coupling_surface_mass_before
            << ", \"coupling_surface_mass_after\": " << record.coupling_surface_mass_after
            << ", \"coupling_deficit_mass_after\": " << record.coupling_deficit_mass_after
            << ", \"drained_volume\": " << record.drained_volume
            << ", \"returned_volume\": " << record.returned_volume
            << ", \"max_cell_cfl\": " << record.max_cell_cfl
            << ", \"wet_cell_count\": " << record.wet_cell_count
            << ", \"checkpoint_status\": \"" << escape_json_string(record.checkpoint_status)
            << "\", \"surface_content_hash\": \""
            << escape_json_string(record.surface_content_hash)
            << "\", \"coupling_content_hash\": \""
            << escape_json_string(record.coupling_content_hash)
            << "\", \"whole_system_mass_audit_enabled\": "
            << (record.whole_system_mass_audit_enabled ? "true" : "false")
            << ", \"whole_system_storage_total\": "
            << record.whole_system_storage_total
            << ", \"whole_system_mass_residual\": "
            << record.whole_system_mass_residual
            << ", \"whole_system_mass_tolerance\": "
            << record.whole_system_mass_tolerance
            << ", \"whole_system_mass_verdict\": \""
            << escape_json_string(record.whole_system_mass_verdict)
            << "\", \"deficit_age_steps\": [";
        for (std::size_t age_index = 0U; age_index < record.deficit_age_steps.size();
             ++age_index) {
            out << (age_index == 0U ? "" : ", ")
                << record.deficit_age_steps[age_index];
        }
        out << "], \"deficit_account_volumes\": [";
        for (std::size_t volume_index = 0U;
             volume_index < record.deficit_account_volumes.size(); ++volume_index) {
            out << (volume_index == 0U ? "" : ", ")
                << record.deficit_account_volumes[volume_index];
        }
        out << "]}";
    }
    out << (summary.epochs.empty() ? "]\n" : "\n  ]\n");
    out << "}\n";
    return out.str();
}

void write_summary_json(const std::filesystem::path& path, const RunSummary& summary) {
    std::ofstream stream(path);
    if (!stream) {
        throw std::runtime_error("cannot open summary output file '" + path.string() + "'");
    }
    stream << to_json(summary);
    if (!stream) {
        throw std::runtime_error("failed writing summary output file '" + path.string() + "'");
    }
}

}  // namespace scau::apps::sim_driver
