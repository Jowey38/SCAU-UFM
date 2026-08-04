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
            << ", \"wet_cell_count\": " << record.wet_cell_count << "}";
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
