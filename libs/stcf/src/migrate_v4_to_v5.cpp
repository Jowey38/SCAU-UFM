#include "stcf/migrate_v4_to_v5.hpp"

#include <sstream>
#include <string>

#include "core/error.hpp"
#include "stcf/validate.hpp"

namespace scau::stcf {

namespace {

[[noreturn]] void fail(const std::string& detail) {
    throw core::ScauError("STCF v4 to v5 migration failed: " + detail);
}

void require_size(const char* field, std::size_t actual, std::size_t expected) {
    if (actual != expected) {
        fail(
            std::string(field) + " has size " + std::to_string(actual)
            + ", expected " + std::to_string(expected));
    }
}

const char* disposition_name(StcfMigrationDisposition disposition) {
    switch (disposition) {
        case StcfMigrationDisposition::renamed:
            return "renamed";
        case StcfMigrationDisposition::copied:
            return "copied";
        case StcfMigrationDisposition::defaulted:
            return "defaulted";
    }
    fail("unknown migration disposition");
}

void add_entry(
    StcfMigrationResult& migration,
    const char* source,
    const char* target,
    const char* units,
    StcfMigrationDisposition disposition) {
    migration.entries.push_back(StcfMigrationEntry{
        .source_field = source,
        .target_field = target,
        .units = units,
        .disposition = disposition,
    });
}

}  // namespace

StcfMigrationResult migrate_stcf_v4_to_v5(const LegacyStcfV4Dataset& legacy) {
    if (legacy.schema_version != kLegacySchemaVersion) {
        fail(
            "schema_version is " + std::to_string(legacy.schema_version)
            + ", expected 4");
    }
    if (legacy.phi_s.empty()) {
        fail("phi_s must contain at least one cell");
    }
    if (legacy.edge_count == 0U) {
        fail("edge_count must be positive");
    }

    const std::size_t cell_count = legacy.phi_s.size();
    require_size("psi_xx", legacy.psi_xx.size(), cell_count);
    require_size("psi_xy", legacy.psi_xy.size(), cell_count);
    require_size("psi_yy", legacy.psi_yy.size(), cell_count);

    StcfMigrationResult migration{
        .dataset = make_uniform_dataset(cell_count, legacy.edge_count),
        .entries = {},
    };
    migration.dataset.cells.phi_t = legacy.phi_s;
    migration.dataset.cells.phi_xx = legacy.psi_xx;
    migration.dataset.cells.phi_xy = legacy.psi_xy;
    migration.dataset.cells.phi_yy = legacy.psi_yy;

    add_entry(
        migration, "phi_s", "phi_t", "1", StcfMigrationDisposition::renamed);
    add_entry(
        migration, "psi_tensor.xx", "phi_xx", "1", StcfMigrationDisposition::renamed);
    add_entry(
        migration, "psi_tensor.xy", "phi_xy", "1", StcfMigrationDisposition::renamed);
    add_entry(
        migration, "psi_tensor.yy", "phi_yy", "1", StcfMigrationDisposition::renamed);
    add_entry(
        migration, "<missing>", "manning_n", "s/m^(1/3)",
        StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "z_b", "m", StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "soil_type", "1", StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "soil_params", "SI", StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "omega_edge", "1", StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "phi_e_n", "1", StcfMigrationDisposition::defaulted);
    add_entry(
        migration, "<missing>", "phi_et", "1", StcfMigrationDisposition::defaulted);

    validate_stcf_dataset(migration.dataset, cell_count, legacy.edge_count);
    return migration;
}

std::string format_stcf_migration_report(const StcfMigrationResult& migration) {
    std::ostringstream report;
    report << "source_schema=4\n";
    report << "target_schema=" << migration.dataset.schema_version << '\n';
    report << "cell_count=" << migration.dataset.cells.phi_t.size() << '\n';
    report << "edge_count=" << migration.dataset.edges.omega_edge.size() << '\n';
    for (const auto& entry : migration.entries) {
        report << disposition_name(entry.disposition) << ':'
               << entry.source_field << "->" << entry.target_field
               << ";units=" << entry.units << '\n';
    }
    return report.str();
}

}  // namespace scau::stcf
