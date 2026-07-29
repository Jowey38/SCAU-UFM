#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

struct PhysicalTotals {
    double volume{0.0};
    double momentum_x{0.0};
    double momentum_y{0.0};
};

struct RunResult {
    scau::surface2d::SurfaceState state;
    PhysicalTotals before;
    PhysicalTotals after;
    std::size_t event_count{0U};
};

std::size_t first_internal_edge(const scau::mesh::Mesh& mesh) {
    for (std::size_t edge = 0; edge < mesh.edges.size(); ++edge) {
        if (mesh.edges[edge].left_cell.has_value() && mesh.edges[edge].right_cell.has_value()) {
            return edge;
        }
    }
    throw std::runtime_error("mesh has no internal edge");
}

std::size_t cell_index(const scau::mesh::Mesh& mesh, const std::string& id) {
    for (std::size_t cell = 0; cell < mesh.cells.size(); ++cell) {
        if (mesh.cells[cell].id == id) {
            return cell;
        }
    }
    throw std::runtime_error("cell not found");
}

PhysicalTotals physical_totals(
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::DpmFields& fields,
    const scau::surface2d::GeometryCache& geometry) {
    PhysicalTotals totals;
    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        const double weight = fields.cells[cell].phi_t * geometry.cell_areas[cell];
        totals.volume += weight * state.cells[cell].conserved.h;
        totals.momentum_x += weight * state.cells[cell].conserved.hu;
        totals.momentum_y += weight * state.cells[cell].conserved.hv;
    }
    return totals;
}

RunResult run(int steps, bool enable_cvc) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    const std::size_t edge = first_internal_edge(mesh);
    const std::size_t left = cell_index(mesh, *mesh.edges[edge].left_cell);
    const std::size_t right = cell_index(mesh, *mesh.edges[edge].right_cell);

    auto fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (auto& edge_fields : fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    fields.edges[edge].phi_e_n = 1.0;
    fields.edges[edge].omega_edge = 1.0;
    fields.cells[left].phi_t = 1.0;
    fields.cells[right].phi_t = 0.4;

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left].conserved.hu = 0.35;
    state.cells[right].conserved.hu = -0.10;
    const auto before = physical_totals(state, fields, geometry);
    const scau::surface2d::StepConfig config{
        .dt = 0.001,
        .cfl_safety = 0.45,
        .c_rollback = 100.0,
        .h_min = 1.0e-8,
        .enable_cvc_spatial_phi_t_correction = enable_cvc,
    };
    std::size_t event_count = 0U;
    for (int step = 0; step < steps; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, fields);
        EXPECT_FALSE(diagnostics.rollback_required);
        event_count += diagnostics.count_phi_t_jump_events;
    }
    return RunResult{
        .state = state,
        .before = before,
        .after = physical_totals(state, fields, geometry),
        .event_count = event_count,
    };
}

TEST(GoldenCvcSpatialPhiTDynamic, BaselineRevealsAndOptInClosesStorageResidual) {
    const auto baseline = run(1, false);
    const auto corrected = run(1, true);

    EXPECT_GT(std::abs(baseline.after.volume - baseline.before.volume), 1.0e-6);
    EXPECT_NEAR(corrected.after.volume, corrected.before.volume, 1.0e-12);
    EXPECT_EQ(corrected.event_count, 1U);
}

TEST(GoldenCvcSpatialPhiTDynamic, CorrectedHundredStepRunConservesPhysicalStorage) {
    const auto corrected = run(100, true);

    EXPECT_NEAR(corrected.after.volume, corrected.before.volume, 1.0e-12);
    EXPECT_EQ(corrected.event_count, 100U);
}

TEST(GoldenCvcSpatialPhiTDynamic, CorrectedReplayIsBitwiseDeterministic) {
    const auto first = run(100, true);
    const auto replay = run(100, true);

    ASSERT_EQ(first.state.cells.size(), replay.state.cells.size());
    for (std::size_t cell = 0; cell < first.state.cells.size(); ++cell) {
        EXPECT_EQ(first.state.cells[cell].conserved.h, replay.state.cells[cell].conserved.h);
        EXPECT_EQ(first.state.cells[cell].conserved.hu, replay.state.cells[cell].conserved.hu);
        EXPECT_EQ(first.state.cells[cell].conserved.hv, replay.state.cells[cell].conserved.hv);
        EXPECT_EQ(first.state.cells[cell].eta, replay.state.cells[cell].eta);
    }
    EXPECT_EQ(first.after.volume, replay.after.volume);
    EXPECT_EQ(first.after.momentum_x, replay.after.momentum_x);
    EXPECT_EQ(first.after.momentum_y, replay.after.momentum_y);
    EXPECT_EQ(first.event_count, replay.event_count);
}

}  // namespace
