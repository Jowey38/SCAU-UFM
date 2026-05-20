#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

struct CellSnapshot {
    double h{0.0};
    double hu{0.0};
    double hv{0.0};
    double eta{0.0};
};

std::vector<CellSnapshot> snapshot_cells(const scau::surface2d::SurfaceState& state) {
    std::vector<CellSnapshot> snapshots;
    snapshots.reserve(state.cells.size());
    for (const auto& cell : state.cells) {
        snapshots.push_back(CellSnapshot{
            .h = cell.conserved.h,
            .hu = cell.conserved.hu,
            .hv = cell.conserved.hv,
            .eta = cell.eta,
        });
    }
    return snapshots;
}

void expect_cells_unchanged(
    const scau::surface2d::SurfaceState& state,
    const std::vector<CellSnapshot>& before) {
    ASSERT_EQ(state.cells.size(), before.size());
    for (std::size_t index = 0; index < state.cells.size(); ++index) {
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.h, before[index].h);
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.hu, before[index].hu);
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.hv, before[index].hv);
        EXPECT_DOUBLE_EQ(state.cells[index].eta, before[index].eta);
    }
}

}  // namespace

TEST(SurfaceStepRollbackRejection, RejectedStepReportsRollbackAndDoesNotMutateState) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    state.cells[0].conserved.hv = 0.5;
    const auto before = snapshot_cells(state);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 0.01};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_TRUE(diagnostics.rollback_required);
    EXPECT_GT(diagnostics.max_cell_cfl, config.c_rollback);
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());
    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    expect_cells_unchanged(state, before);
}

TEST(SurfaceStepRollbackRejection, AcceptedStepKeepsExistingStateMutationPath) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    state.cells[0].conserved.hv = 0.5;
    const auto before = snapshot_cells(state);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_FALSE(diagnostics.rollback_required);
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());

    bool any_cell_changed = false;
    for (std::size_t index = 0; index < state.cells.size(); ++index) {
        any_cell_changed = any_cell_changed || state.cells[index].conserved.h != before[index].h;
        any_cell_changed = any_cell_changed || state.cells[index].conserved.hu != before[index].hu;
        any_cell_changed = any_cell_changed || state.cells[index].conserved.hv != before[index].hv;
    }
    EXPECT_TRUE(any_cell_changed);
}
