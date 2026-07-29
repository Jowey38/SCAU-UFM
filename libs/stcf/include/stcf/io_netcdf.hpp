#pragma once

#include <filesystem>

#include "stcf/schema.hpp"
#include "stcf/topology.hpp"
#include "stcf/validate.hpp"

namespace scau::stcf {

// Writes a validated STCF v5 dataset to a NetCDF classic file. Validation
// runs before any file is created (main spec legacy.10 export gate): an
// invalid dataset never reaches disk.
void write_stcf(
    const std::filesystem::path& path,
    const StcfDataset& dataset,
    const StcfValidationConfig& config = {});

// Reads an STCF v5 NetCDF file fail-closed: the schema_version attribute,
// every variable, and every dimension must be present and consistent, and
// the loaded dataset must pass the full validation gate.
[[nodiscard]] StcfDataset read_stcf(
    const std::filesystem::path& path,
    const StcfValidationConfig& config = {});

// Writes/reads the strict, self-contained UGRID topology + STCF v5 field
// contract. A field-only file is rejected by read_stcf_case; use read_stcf
// only when the caller intentionally owns a sidecar mesh.
void write_stcf_case(
    const std::filesystem::path& path,
    const StcfCase& stcf_case,
    const StcfValidationConfig& config = {});

[[nodiscard]] StcfCase read_stcf_case(
    const std::filesystem::path& path,
    const StcfValidationConfig& config = {});

}  // namespace scau::stcf
