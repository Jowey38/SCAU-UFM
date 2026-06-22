#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/edge_conveyance.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(DpmEdgeConveyanceAssembly, DefaultTensorsLeavePhiEnAtBaseline) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const auto report = scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields);

    EXPECT_EQ(report.weak_dpm_guarantee_edge_count, 0U);
    for (const auto& edge : dpm_fields.edges) {
        EXPECT_NEAR(edge.phi_e_n, 1.0, 1.0e-15);
    }
}

TEST(DpmEdgeConveyanceAssembly, OmegaScalesDerivedPhiEn) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (auto& edge : dpm_fields.edges) {
        edge.omega_edge = 0.5;
    }

    static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields));

    for (const auto& edge : dpm_fields.edges) {
        EXPECT_NEAR(edge.phi_e_n, 0.5, 1.0e-15);
    }
}

TEST(DpmEdgeConveyanceAssembly, AnisotropicTensorsCountWeakGuarantee) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    // diag(1, 9): edge-normal anisotropy metric 4/9 > 0.3 on axis-aligned edges.
    for (auto& cell : dpm_fields.cells) {
        cell.Phi_c = scau::surface2d::Tensor2Symmetric{.xx = 1.0, .xy = 0.0, .yy = 9.0};
    }

    const auto report = scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields);

    EXPECT_GT(report.weak_dpm_guarantee_edge_count, 0U);
    for (const auto& edge : dpm_fields.edges) {
        EXPECT_GE(edge.phi_e_n, 0.0);
    }
}

TEST(DpmEdgeConveyanceAssembly, DerivedPhiEnDrivesHllcStep) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.01, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    auto make_state = [&]() {
        auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
        state.cells[0].conserved.h = 1.5;
        return state;
    };

    // Baseline: identity tensors -> derived phi_e_n == 1.0.
    auto dpm_identity = scau::surface2d::DpmFields::for_mesh(mesh);
    static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_identity));
    auto state_identity = make_state();
    static_cast<void>(scau::surface2d::advance_one_step_cpu(
        mesh, state_identity, config, dpm_identity, boundary));

    // Reduced conveyance via omega -> smaller phi_e_n -> different fluxes.
    auto dpm_reduced = scau::surface2d::DpmFields::for_mesh(mesh);
    for (auto& edge : dpm_reduced.edges) {
        edge.omega_edge = 0.25;
    }
    static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_reduced));
    auto state_reduced = make_state();
    static_cast<void>(scau::surface2d::advance_one_step_cpu(
        mesh, state_reduced, config, dpm_reduced, boundary));

    bool any_difference = false;
    for (std::size_t index = 0; index < state_identity.cells.size(); ++index) {
        if (state_identity.cells[index].conserved.h != state_reduced.cells[index].conserved.h) {
            any_difference = true;
        }
    }
    EXPECT_TRUE(any_difference);
}

TEST(DpmEdgeConveyanceAssembly, InvalidTensorFieldFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[0].Phi_c = scau::surface2d::Tensor2Symmetric{.xx = 0.0, .xy = 0.0, .yy = 0.0};

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields)),
        std::invalid_argument);
}
