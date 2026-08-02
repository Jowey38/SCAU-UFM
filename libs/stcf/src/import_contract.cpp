#include "stcf/import_contract.hpp"

#include <set>
#include <stdexcept>
#include <string>

namespace scau::stcf {

namespace {

const std::set<std::string> kCanonicalTargets{
    "node_x", "node_y", "face_nodes", "edge_nodes", "phi_t", "phi_xx",
    "phi_xy", "phi_yy", "manning_n", "z_b", "soil_type", "omega_edge",
    "phi_e_n", "phi_et",
};

}  // namespace

void validate_external_import_contract(const ExternalImportContract& contract) {
    const auto& metadata = contract.metadata;
    if (!metadata.authorized_for_project_use) {
        throw std::invalid_argument("external dataset is not authorized for project use");
    }
    if (metadata.source_uri.empty()) {
        throw std::invalid_argument("external dataset source_uri must not be empty");
    }
    if (metadata.source_crs.empty() || metadata.target_crs.empty()) {
        throw std::invalid_argument("external dataset source and target CRS are required");
    }
    if (metadata.horizontal_units.empty() || metadata.vertical_units.empty()) {
        throw std::invalid_argument("external dataset horizontal and vertical units are required");
    }
    if (contract.field_mappings.empty()) {
        throw std::invalid_argument("external dataset field mappings must not be empty");
    }

    std::set<std::string> targets;
    for (const auto& mapping : contract.field_mappings) {
        if (mapping.source_field.empty() || mapping.target_field.empty()) {
            throw std::invalid_argument("external field mapping names must not be empty");
        }
        if (!kCanonicalTargets.contains(mapping.target_field)) {
            throw std::invalid_argument(
                "external field mapping targets unknown STCF field " + mapping.target_field);
        }
        if (!targets.insert(mapping.target_field).second) {
            throw std::invalid_argument(
                "external field mapping duplicates target " + mapping.target_field);
        }
        if (mapping.source_units.empty() || mapping.target_units.empty()) {
            throw std::invalid_argument("external field mapping units must not be empty");
        }
    }
}

}  // namespace scau::stcf
