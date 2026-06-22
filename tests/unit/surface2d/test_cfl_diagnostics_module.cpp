#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/cfl/diagnostics.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"

TEST(CflDiagnosticsModule, MatchesStepDiagnosticCflValue) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.h = 1.5;
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);

    const auto cfl = scau::surface2d::evaluate_cfl_diagnostics(
        mesh,
        state,
        boundary,
        geometry,
        0.1,
        100.0);

    EXPECT_GT(cfl.max_cell_cfl, 0.0);
    EXPECT_FALSE(cfl.rollback_required);
}

TEST(CflDiagnosticsModule, RollbackUsesRawCflNotSafetyScaledCfl) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.h = 2.0;
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    const double raw = scau::surface2d::raw_cell_cfl(mesh, state, boundary, geometry, 0.2);

    const auto accepted = scau::surface2d::evaluate_cfl_diagnostics(mesh, state, boundary, geometry, 0.2, raw + 1.0e-9);
    const auto rejected = scau::surface2d::evaluate_cfl_diagnostics(mesh, state, boundary, geometry, 0.2, raw - 1.0e-9);

    EXPECT_FALSE(accepted.rollback_required);
    EXPECT_TRUE(rejected.rollback_required);
}

TEST(CflDiagnosticsModule, InvalidInputsFailClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::raw_cell_cfl(mesh, state, boundary, geometry, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::evaluate_cfl_diagnostics(mesh, state, boundary, geometry, 0.1, 0.0)),
        std::invalid_argument);
}
