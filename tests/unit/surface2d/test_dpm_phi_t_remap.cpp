#include <cmath>
#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/phi_t_remap.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

constexpr double kTol = 1.0e-12;

void seed_moving_state(scau::surface2d::SurfaceState& state) {
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const double h = 1.0 + 0.25 * static_cast<double>(i);
        state.cells[i].conserved.h = h;
        state.cells[i].conserved.hu = h * (0.5 + 0.1 * static_cast<double>(i));
        state.cells[i].conserved.hv = h * (-0.25 + 0.05 * static_cast<double>(i));
        state.cells[i].eta = h + 0.2 * static_cast<double>(i);
    }
}

std::vector<double> bed_elevations(const scau::surface2d::SurfaceState& state) {
    std::vector<double> beds;
    beds.reserve(state.cells.size());
    for (const auto& cell : state.cells) {
        beds.push_back(cell.eta - cell.conserved.h);
    }
    return beds;
}

}  // namespace

TEST(DpmPhiTRemap, NoOpWhenPhiTUnchanged) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    seed_moving_state(state);
    const auto original = state;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const auto diagnostics = scau::surface2d::remap_state_for_phi_t_change(mesh, state, dpm_fields, dpm_fields, geometry);

    ASSERT_EQ(state.cells.size(), original.cells.size());
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.h, original.cells[i].conserved.h);
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.hu, original.cells[i].conserved.hu);
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.hv, original.cells[i].conserved.hv);
        EXPECT_DOUBLE_EQ(state.cells[i].eta, original.cells[i].eta);
    }
    EXPECT_NEAR(diagnostics.volume_after, diagnostics.volume_before, kTol);
    EXPECT_NEAR(diagnostics.momentum_x_after, diagnostics.momentum_x_before, kTol);
    EXPECT_NEAR(diagnostics.momentum_y_after, diagnostics.momentum_y_before, kTol);
}

TEST(DpmPhiTRemap, ConservesCellStorageWhenPhiTJumps) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    seed_moving_state(state);
    auto old_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    auto new_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        old_dpm.cells[i].phi_t = 0.8 + 0.1 * static_cast<double>(i);
        new_dpm.cells[i].phi_t = 1.4 - 0.05 * static_cast<double>(i);
    }
    std::vector<double> stored_before;
    stored_before.reserve(mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        stored_before.push_back(old_dpm.cells[i].phi_t * state.cells[i].conserved.h * geometry.cell_areas[i]);
    }

    const auto diagnostics = scau::surface2d::remap_state_for_phi_t_change(mesh, state, old_dpm, new_dpm, geometry);

    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        const double stored_after = new_dpm.cells[i].phi_t * state.cells[i].conserved.h * geometry.cell_areas[i];
        EXPECT_NEAR(stored_after, stored_before[i], kTol);
    }
    EXPECT_NEAR(diagnostics.volume_after, diagnostics.volume_before, 1.0e-9);
}

TEST(DpmPhiTRemap, PreservesVelocityAndStoredMomentum) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    seed_moving_state(state);
    auto old_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    auto new_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        old_dpm.cells[i].phi_t = 1.0;
        new_dpm.cells[i].phi_t = (i % 2 == 0) ? 0.75 : 1.25;
    }
    std::vector<double> old_u;
    std::vector<double> old_v;
    old_u.reserve(mesh.cells.size());
    old_v.reserve(mesh.cells.size());
    for (const auto& cell : state.cells) {
        old_u.push_back(cell.u());
        old_v.push_back(cell.v());
    }

    const auto diagnostics = scau::surface2d::remap_state_for_phi_t_change(mesh, state, old_dpm, new_dpm, geometry);

    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_NEAR(state.cells[i].u(), old_u[i], kTol);
        EXPECT_NEAR(state.cells[i].v(), old_v[i], kTol);
    }
    EXPECT_NEAR(diagnostics.momentum_x_after, diagnostics.momentum_x_before, 1.0e-9);
    EXPECT_NEAR(diagnostics.momentum_y_after, diagnostics.momentum_y_before, 1.0e-9);
}

TEST(DpmPhiTRemap, PreservesBedElevationViaEtaMinusH) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    seed_moving_state(state);
    const auto original_beds = bed_elevations(state);
    auto old_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    auto new_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    for (auto& cell : new_dpm.cells) {
        cell.phi_t = 0.5;
    }

    static_cast<void>(scau::surface2d::remap_state_for_phi_t_change(mesh, state, old_dpm, new_dpm, geometry));

    const auto remapped_beds = bed_elevations(state);
    ASSERT_EQ(remapped_beds.size(), original_beds.size());
    for (std::size_t i = 0; i < remapped_beds.size(); ++i) {
        EXPECT_NEAR(remapped_beds[i], original_beds[i], kTol);
    }
}

TEST(DpmPhiTRemap, RejectsInvalidPhiTWithoutMutation) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    seed_moving_state(state);
    const auto original = state;
    auto old_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    auto new_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    new_dpm.cells[0].phi_t = 0.0;

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::remap_state_for_phi_t_change(mesh, state, old_dpm, new_dpm, geometry)),
        std::invalid_argument);
    ASSERT_EQ(state.cells.size(), original.cells.size());
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.h, original.cells[i].conserved.h);
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.hu, original.cells[i].conserved.hu);
        EXPECT_DOUBLE_EQ(state.cells[i].conserved.hv, original.cells[i].conserved.hv);
        EXPECT_DOUBLE_EQ(state.cells[i].eta, original.cells[i].eta);
    }
}

TEST(DpmPhiTRemap, UniformDynamicPhiTJumpKeepsLakeAtRestAfterStep) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto old_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    auto new_dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    for (auto& cell : new_dpm.cells) {
        cell.phi_t = 0.5;
    }

    static_cast<void>(scau::surface2d::remap_state_for_phi_t_change(mesh, state, old_dpm, new_dpm, geometry));
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, new_dpm);

    ASSERT_FALSE(diagnostics.rollback_required);
    for (const auto& cell : state.cells) {
        EXPECT_NEAR(cell.conserved.hu, 0.0, kTol);
        EXPECT_NEAR(cell.conserved.hv, 0.0, kTol);
    }
}
