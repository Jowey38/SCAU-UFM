// G36 preproc_field_derivation_case: locks the M287-B5 rule-table field
// derivation as a committed fixture:
//   synthetic D-5 package + cases/dpm_rule_table.json (synthetic_unapproved:
//   road phi_t 0.95 / Phi_c diag(0.95, 0.80), grass & water isotropic unit,
//   kerb interface road|grass omega 0.9, soil from soil_zones with the road
//   strip defaulting to soil_type 0, landcover_overlap smallest_area_wins)
//   -> synthetic_city_block_derived.stcf.nc
// The golden asserts through the AUTHORITATIVE read path and the solver's own
// DPM kernels that:
//   1. every cell's (phi_t, Phi_c, manning_n, soil_type) is exactly one of the
//      rule-table class tuples and the class census matches the derivation
//      report (road 83 / grass 363 / water 23 on the G30 mesh; the pond is
//      resolved by smallest-area, which v1 first-match never assigned);
//   2. every edge's (omega_edge, phi_e_n) equals the solver's
//      project_edge_conveyance (spec 5.3 rule 2: arithmetic-mean tensor,
//      n^T Phi n, omega scaling) recomputed from the cell tensors - i.e. the
//      python writer and the C++ assembler agree to 1e-12 - and kerb edges
//      carry omega 0.9 exactly 30 times;
//   3. every cell passes validate_dpm_cell_consistency (closure laws);
//   4. a wet lake at rest over the sampled DEM stays at rest to 1e-12 despite
//      the spatial phi_t jump (0.95 | 1.0) and anisotropic conveyance.
// Regeneration is pipeline-side; nothing here re-implements the derivation.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>

#include <gtest/gtest.h>

#include "stcf/io_netcdf.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/closure_laws.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::surface2d::BoundaryConditions;
using scau::surface2d::GeometryCache;
using scau::surface2d::load_surface2d_case;
using scau::surface2d::StepConfig;
using scau::surface2d::SurfaceState;

constexpr double kRestTolerance = 1.0e-12;
constexpr double kProjectionTolerance = 1.0e-12;

std::filesystem::path fixture_path() {
    return std::filesystem::path(SCAU_PREPROC_FIELD_CASE_DIR) / "synthetic_city_block_derived.stcf.nc";
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// Rule-table class tuples: phi_t, xx, xy, yy, manning_n (mirrors cases/dpm_rule_table.json
// + the synthetic landcover Manning values locked by G30).
struct ClassTuple {
    const char* name;
    double phi_t, xx, xy, yy, manning;
    std::size_t recorded_cells;
};
const std::array<ClassTuple, 3> kClasses{{
    {"road", 0.95, 0.95, 0.0, 0.80, 0.015, 83U},
    {"grass", 1.0, 1.0, 0.0, 1.0, 0.030, 363U},
    {"water", 1.0, 1.0, 0.0, 1.0, 0.020, 23U},
}};

const ClassTuple* classify(double phi_t, const scau::surface2d::Tensor2Symmetric& t, double manning) {
    for (const auto& c : kClasses) {
        if (phi_t == c.phi_t && t.xx == c.xx && t.xy == c.xy && t.yy == c.yy && manning == c.manning) {
            return &c;
        }
    }
    return nullptr;
}

TEST(GoldenPreprocFieldDerivationCase, CellFieldsAreExactRuleTableTuples) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto stcf = scau::stcf::read_stcf_case(fixture_path());  // soil_type lives on the STCF dataset
    ASSERT_EQ(loaded.mesh.cells.size(), 469U);   // same mesh as G30 (only fields differ)
    ASSERT_EQ(loaded.mesh.edges.size(), 996U);
    ASSERT_EQ(stcf.fields.cells.soil_type.size(), 469U);

    std::map<std::string, std::size_t> census;
    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        const auto& dpm = loaded.dpm_fields.cells[i];
        const auto* cls = classify(dpm.phi_t, dpm.Phi_c, loaded.source_fields.manning_n[i]);
        ASSERT_NE(cls, nullptr) << "cell " << i << " carries a (phi_t, Phi_c, manning) tuple not in the rule table";
        ++census[cls->name];
        // Soil: pond/grass zone -> synthetic sandy loam (1); road strip outside every zone -> default 0.
        const unsigned soil = stcf.fields.cells.soil_type[i];
        EXPECT_EQ(soil, std::string(cls->name) == "road" ? 0U : 1U) << "cell " << i;
        EXPECT_NO_THROW(scau::surface2d::validate_dpm_cell_consistency(dpm.phi_t, dpm.Phi_c)) << "cell " << i;
    }
    for (const auto& c : kClasses) {
        EXPECT_EQ(census[c.name], c.recorded_cells) << c.name;
    }
    // The recorded report agrees with the file (no drift between report and case).
    const auto report = read_text(std::filesystem::path(SCAU_PREPROC_FIELD_CASE_DIR) / "field_derivation.json");
    EXPECT_NE(report.find("\"road\": 83"), std::string::npos);
    EXPECT_NE(report.find("\"grass\": 363"), std::string::npos);
    EXPECT_NE(report.find("\"water\": 23"), std::string::npos);
    EXPECT_NE(report.find("\"status\": \"synthetic_unapproved\""), std::string::npos);
    EXPECT_NE(report.find("\"landcover_overlap_rule\": \"smallest_area_wins\""), std::string::npos);
}

TEST(GoldenPreprocFieldDerivationCase, EdgeFieldsEqualSolverSpecRule2Projection) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto geometry = GeometryCache::for_mesh(loaded.mesh);

    std::size_t kerb_edges = 0U;
    std::size_t boundary_edges = 0U;
    for (std::size_t e = 0; e < loaded.mesh.edges.size(); ++e) {
        const auto& adjacency = geometry.edge_cells[e];
        const std::size_t left = adjacency.left.has_value() ? *adjacency.left : *adjacency.right;
        const std::size_t right = adjacency.right.has_value() ? *adjacency.right : *adjacency.left;
        const auto& file_edge = loaded.dpm_fields.edges[e];
        const auto projection = scau::surface2d::project_edge_conveyance(
            loaded.dpm_fields.cells[left].Phi_c, loaded.dpm_fields.cells[right].Phi_c,
            loaded.mesh.edges[e].normal, file_edge.omega_edge);
        EXPECT_NEAR(file_edge.phi_e_n, projection.phi_e_n, kProjectionTolerance) << "edge " << e;
        EXPECT_LE(file_edge.phi_e_n, 1.0) << "edge " << e;   // strict file-validator bound honoured

        const bool boundary = !adjacency.left.has_value() || !adjacency.right.has_value();
        boundary_edges += boundary ? 1U : 0U;
        const double phi_l = loaded.dpm_fields.cells[left].phi_t;
        const double phi_r = loaded.dpm_fields.cells[right].phi_t;
        const bool road_grass_interface = !boundary && ((phi_l == 0.95) != (phi_r == 0.95))
            && loaded.source_fields.manning_n[left] != 0.020 && loaded.source_fields.manning_n[right] != 0.020;
        if (road_grass_interface) {
            EXPECT_EQ(file_edge.omega_edge, 0.9) << "edge " << e;
            ++kerb_edges;
        } else {
            EXPECT_EQ(file_edge.omega_edge, 1.0) << "edge " << e;
        }
    }
    EXPECT_EQ(kerb_edges, 30U);       // recorded interface_edges
    EXPECT_GT(boundary_edges, 0U);
}

TEST(GoldenPreprocFieldDerivationCase, LakeAtRestWithSpatialPhiTJumpStaysAtRest) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto geometry = GeometryCache::for_mesh(loaded.mesh);
    const auto boundary = BoundaryConditions::for_mesh(loaded.mesh);

    const double eta = 12.0;  // above the DEM maximum (10.9): every cell wet
    auto state = SurfaceState::for_mesh(loaded.mesh);
    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        state.cells[i].conserved.h = eta - loaded.bed_elevations[i];
        ASSERT_GT(state.cells[i].conserved.h, 0.0);
        state.cells[i].eta = eta;
    }
    const auto initial = state;

    const StepConfig config{.dt = 0.05, .cfl_safety = 0.45, .c_rollback = 10.0};
    for (int step = 0; step < 20; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh, state, config, loaded.dpm_fields, boundary,
            loaded.source_fields, geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_NEAR(state.cells[i].conserved.h, initial.cells[i].conserved.h, kRestTolerance) << i;
        EXPECT_NEAR(state.cells[i].conserved.hu, 0.0, kRestTolerance) << i;
        EXPECT_NEAR(state.cells[i].conserved.hv, 0.0, kRestTolerance) << i;
    }
}

}  // namespace
