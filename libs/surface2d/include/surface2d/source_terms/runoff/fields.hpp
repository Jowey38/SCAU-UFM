#pragma once

#include <cstdint>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

// Runoff infiltration method tag. Only GreenAmptMeinLarson is runnable in this
// phase; the reserved values must fail closed at the dispatch site (later task),
// never silently degrade to a constant-rate model.
enum class RunoffMethod {
    GreenAmptMeinLarson,
    HortonReserved,
    SCSCurveNumberReserved,
};

// Per-cell dominant surface class. Mixed cells use the area fractions in
// RunoffFields; this tag records the dominant/primary classification.
enum class SurfaceClass {
    PerviousGround,
    ImperviousGround,
    RoofDirectDrain,
    RoofOverflowToSurface,
};

// Static or quasi-static per-cell land-surface parameters (SoA).
// Area fractions are dimensionless; abstraction/depression capacities are
// depths in metres. soil_type indexes a SoilParamsLUT.
struct RunoffFields {
    RunoffMethod method{RunoffMethod::GreenAmptMeinLarson};
    std::vector<core::Real> pervious_fraction;
    std::vector<core::Real> impervious_fraction;
    std::vector<core::Real> roof_fraction;
    std::vector<core::Real> initial_abstraction_capacity;
    std::vector<core::Real> depression_storage_capacity;
    std::vector<core::Real> roof_abstraction_capacity;
    std::vector<core::Real> roof_storage_capacity;
    std::vector<std::uint8_t> soil_type;

    // Neutral default: fully pervious, no losses, no roof, soil_type 0.
    [[nodiscard]] static RunoffFields for_mesh(const mesh::Mesh& mesh);
};

// Dynamic per-cell runoff state (SoA). cumulative_infiltration is F_inf;
// ponding_time is t_ponding with -1 meaning "not ponded". All others are
// filled depths in metres; roof_pending_volume is a held volume in m^3.
struct RunoffState {
    std::vector<core::Real> cumulative_infiltration;
    std::vector<core::Real> ponding_time;
    std::vector<core::Real> abstraction_filled;
    std::vector<core::Real> depression_storage_filled;
    std::vector<core::Real> roof_abstraction_filled;
    std::vector<core::Real> roof_storage_filled;
    std::vector<core::Real> roof_pending_volume;

    // Neutral default: zero state, ponding_time = -1 (not ponded).
    [[nodiscard]] static RunoffState for_mesh(const mesh::Mesh& mesh);
};

// Per-cell roof-to-pipe-network mapping. swmm_node_index is a stable integer
// index resolved at PreProc time (never a string lookup in the hot path);
// -1 means "no roof drain target". roof_overflow_to_surface_cell_index is the
// 2D cell that receives roof overflow; -1 means "no overflow target".
struct RoofDrainageMap {
    std::vector<core::Real> roof_drain_capacity;
    std::vector<int> swmm_node_index;
    std::vector<int> roof_overflow_to_surface_cell_index;
    std::vector<core::Real> roof_to_surface_fraction;

    // Neutral default: zero capacity, indices -1, zero overflow fraction.
    [[nodiscard]] static RoofDrainageMap for_mesh(const mesh::Mesh& mesh);
};

// Fail-closed size checks against the mesh cell count.
void validate_runoff_fields_match_mesh(const RunoffFields& fields, const mesh::Mesh& mesh);
void validate_runoff_state_match_mesh(const RunoffState& state, const mesh::Mesh& mesh);
void validate_roof_drainage_map_match_mesh(const RoofDrainageMap& map, const mesh::Mesh& mesh);

}  // namespace scau::surface2d
