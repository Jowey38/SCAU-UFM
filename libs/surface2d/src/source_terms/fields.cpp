#include "surface2d/source_terms/fields.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace scau::surface2d {
namespace {

void validate_cell_sized_finite(
    const std::vector<core::Real>& values,
    std::size_t cell_count,
    bool allow_negative,
    const std::string& name) {
    if (values.size() != cell_count) {
        throw std::invalid_argument("source term field " + name + " size must match mesh cell count");
    }
    for (const core::Real value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("source term field " + name + " must be finite");
        }
        if (!allow_negative && value < 0.0) {
            throw std::invalid_argument("source term field " + name + " must be non-negative");
        }
    }
}

}  // namespace

SourceTermFields SourceTermFields::for_mesh(const mesh::Mesh& mesh) {
    SourceTermFields fields;
    fields.manning_n.assign(mesh.cells.size(), 0.0);
    fields.rainfall_rate.assign(mesh.cells.size(), 0.0);
    fields.infiltration_rate.assign(mesh.cells.size(), 0.0);
    fields.exchange_volume.assign(mesh.cells.size(), 0.0);
    return fields;
}

void validate_source_term_fields_match_mesh(const SourceTermFields& fields, const mesh::Mesh& mesh) {
    const std::size_t cell_count = mesh.cells.size();
    validate_cell_sized_finite(fields.manning_n, cell_count, false, "manning_n");
    validate_cell_sized_finite(fields.rainfall_rate, cell_count, false, "rainfall_rate");
    validate_cell_sized_finite(fields.infiltration_rate, cell_count, false, "infiltration_rate");
    validate_cell_sized_finite(fields.exchange_volume, cell_count, true, "exchange_volume");
}

}  // namespace scau::surface2d
