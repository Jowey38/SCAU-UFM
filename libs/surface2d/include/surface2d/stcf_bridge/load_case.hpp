#pragma once

#include <filesystem>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "stcf/topology.hpp"
#include "stcf/validate.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/source_terms/fields.hpp"

namespace scau::surface2d {

struct LoadedSurface2DCase {
    mesh::Mesh mesh;
    DpmFields dpm_fields;
    SourceTermFields source_fields;
    std::vector<core::Real> bed_elevations;
};

// Builds a solver-ready case from a validated, self-contained STCF UGRID
// case. NetCDF edge order is preserved exactly in mesh.edges and DpmFields.
[[nodiscard]] LoadedSurface2DCase load_surface2d_case(
    const stcf::StcfCase& stcf_case,
    const stcf::StcfValidationConfig& config = {});

// Strict file entry point: read_stcf_case (no field-only fallback), then load.
[[nodiscard]] LoadedSurface2DCase load_surface2d_case(
    const std::filesystem::path& path,
    const stcf::StcfValidationConfig& config = {});

}  // namespace scau::surface2d
