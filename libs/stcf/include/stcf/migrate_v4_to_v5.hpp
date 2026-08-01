#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/types.hpp"
#include "stcf/schema.hpp"

namespace scau::stcf {

inline constexpr int kLegacySchemaVersion = 4;

// Authored representation of the v4 fields whose mapping is explicitly locked
// by main spec legacy.15.7. Fields not represented by the current v5 slice are
// not guessed here; later schema slices must extend this DTO and its report.
struct LegacyStcfV4Dataset {
    int schema_version{kLegacySchemaVersion};
    std::vector<core::Real> phi_s;
    std::vector<core::Real> psi_xx;
    std::vector<core::Real> psi_xy;
    std::vector<core::Real> psi_yy;
    std::size_t edge_count{0U};
};

enum class StcfMigrationDisposition {
    renamed,
    copied,
    defaulted,
};

struct StcfMigrationEntry {
    std::string source_field;
    std::string target_field;
    std::string units;
    StcfMigrationDisposition disposition{StcfMigrationDisposition::copied};
};

struct StcfMigrationResult {
    StcfDataset dataset;
    std::vector<StcfMigrationEntry> entries;
};

// Migrates the current, evidence-backed v4 subset into a fully validated v5
// dataset. Every v5-only required field is populated by an explicit fixture
// default and recorded in entries; migration never silently invents a value.
[[nodiscard]] StcfMigrationResult migrate_stcf_v4_to_v5(
    const LegacyStcfV4Dataset& legacy);

// Stable machine-readable text used by G7 evidence and CLI/report adapters.
[[nodiscard]] std::string format_stcf_migration_report(
    const StcfMigrationResult& migration);

}  // namespace scau::stcf
