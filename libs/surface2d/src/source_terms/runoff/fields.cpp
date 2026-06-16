#include "surface2d/source_terms/runoff/fields.hpp"

#include <stdexcept>

namespace scau::surface2d {

RunoffFields RunoffFields::for_mesh(const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    RunoffFields fields;
    fields.pervious_fraction.assign(n, 1.0);
    fields.impervious_fraction.assign(n, 0.0);
    fields.roof_fraction.assign(n, 0.0);
    fields.initial_abstraction_capacity.assign(n, 0.0);
    fields.depression_storage_capacity.assign(n, 0.0);
    fields.roof_abstraction_capacity.assign(n, 0.0);
    fields.roof_storage_capacity.assign(n, 0.0);
    fields.soil_type.assign(n, 0U);
    return fields;
}

RunoffState RunoffState::for_mesh(const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    RunoffState state;
    state.cumulative_infiltration.assign(n, 0.0);
    state.ponding_time.assign(n, -1.0);
    state.abstraction_filled.assign(n, 0.0);
    state.depression_storage_filled.assign(n, 0.0);
    state.roof_abstraction_filled.assign(n, 0.0);
    state.roof_storage_filled.assign(n, 0.0);
    state.roof_pending_volume.assign(n, 0.0);
    return state;
}

RoofDrainageMap RoofDrainageMap::for_mesh(const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    RoofDrainageMap map;
    map.roof_drain_capacity.assign(n, 0.0);
    map.swmm_node_index.assign(n, -1);
    map.roof_overflow_to_surface_cell_index.assign(n, -1);
    map.roof_to_surface_fraction.assign(n, 0.0);
    return map;
}

void validate_runoff_fields_match_mesh(const RunoffFields& fields, const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    if (fields.pervious_fraction.size() != n ||
        fields.impervious_fraction.size() != n ||
        fields.roof_fraction.size() != n ||
        fields.initial_abstraction_capacity.size() != n ||
        fields.depression_storage_capacity.size() != n ||
        fields.roof_abstraction_capacity.size() != n ||
        fields.roof_storage_capacity.size() != n ||
        fields.soil_type.size() != n) {
        throw std::invalid_argument("runoff fields array sizes must match mesh cell count");
    }
}

void validate_runoff_state_match_mesh(const RunoffState& state, const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    if (state.cumulative_infiltration.size() != n ||
        state.ponding_time.size() != n ||
        state.abstraction_filled.size() != n ||
        state.depression_storage_filled.size() != n ||
        state.roof_abstraction_filled.size() != n ||
        state.roof_storage_filled.size() != n ||
        state.roof_pending_volume.size() != n) {
        throw std::invalid_argument("runoff state array sizes must match mesh cell count");
    }
}

void validate_roof_drainage_map_match_mesh(const RoofDrainageMap& map, const mesh::Mesh& mesh) {
    const std::size_t n = mesh.cells.size();
    if (map.roof_drain_capacity.size() != n ||
        map.swmm_node_index.size() != n ||
        map.roof_overflow_to_surface_cell_index.size() != n ||
        map.roof_to_surface_fraction.size() != n) {
        throw std::invalid_argument("roof drainage map array sizes must match mesh cell count");
    }
}

}  // namespace scau::surface2d
