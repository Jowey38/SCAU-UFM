#include "surface2d/dpm/fields.hpp"

#include <stdexcept>

namespace scau::surface2d {

DpmFields DpmFields::for_mesh(const mesh::Mesh& mesh) {
    DpmFields fields;
    fields.cells.resize(mesh.cells.size());
    fields.edges.resize(mesh.edges.size());
    return fields;
}

void validate_dpm_fields_match_mesh(const DpmFields& fields, const mesh::Mesh& mesh) {
    if (fields.cells.size() != mesh.cells.size()) {
        throw std::invalid_argument("DPM field cell count must match mesh cell count");
    }
    if (fields.edges.size() != mesh.edges.size()) {
        throw std::invalid_argument("DPM field edge count must match mesh edge count");
    }
}

}  // namespace scau::surface2d
