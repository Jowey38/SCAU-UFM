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
    // M245: validate only the newly introduced Phi_c tensor field. Other DPM
    // scalars keep their prior (unconstrained-here) handling so existing
    // fixtures that exercise the pressure-pairing math with phi_t > 1 are
    // unaffected; phi_t domain is enforced at the source-term call sites.
    for (const auto& cell : fields.cells) {
        validate_conveyance_tensor(cell.Phi_c);
    }
}

}  // namespace scau::surface2d
