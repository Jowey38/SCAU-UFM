#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/backend.hpp"
#include "surface2d/state/state.hpp"

TEST(SurfaceBackendContract, CpuReferenceIsAvailableAndDeterministic) {
    const auto capabilities = scau::surface2d::query_backend_capabilities(
        scau::surface2d::BackendKind::cpu_reference);
    EXPECT_TRUE(capabilities.available);
    EXPECT_TRUE(capabilities.deterministic);
    EXPECT_TRUE(capabilities.double_precision);
    EXPECT_TRUE(capabilities.supports_snapshot_restore);
}

TEST(SurfaceBackendContract, CudaPerformanceStaysFailClosed) {
    // M266: cuda_performance can never be the sole correctness path; it stays
    // fail-closed even when the deterministic CUDA backend is compiled in.
    const auto capabilities = scau::surface2d::query_backend_capabilities(
        scau::surface2d::BackendKind::cuda_performance);
    EXPECT_FALSE(capabilities.available);
    auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step(
            scau::surface2d::BackendKind::cuda_performance, mesh, state, {.dt = 0.01})),
        std::runtime_error);
}

TEST(SurfaceBackendContract, CudaDeterministicFailsClosedUnlessCompiledAndDevicePresent) {
    const auto capabilities = scau::surface2d::query_backend_capabilities(
        scau::surface2d::BackendKind::cuda_deterministic);
#if !defined(SCAU_SURFACE2D_HAS_CUDA)
    // Default build: the backend is not compiled, so the M266 fail-closed
    // contract holds exactly as before G9 implementation.
    EXPECT_FALSE(capabilities.available);
    auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step(
            scau::surface2d::BackendKind::cuda_deterministic, mesh, state, {.dt = 0.01})),
        std::runtime_error);
#else
    // CUDA build: availability is a runtime device probe; when available the
    // backend must advertise the full deterministic contract.
    if (capabilities.available) {
        EXPECT_TRUE(capabilities.deterministic);
        EXPECT_TRUE(capabilities.double_precision);
        EXPECT_TRUE(capabilities.supports_snapshot_restore);
    } else {
        auto mesh = scau::mesh::build_mixed_minimal_mesh();
        auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
        EXPECT_THROW(
            static_cast<void>(scau::surface2d::advance_one_step(
                scau::surface2d::BackendKind::cuda_deterministic, mesh, state, {.dt = 0.01})),
            std::runtime_error);
    }
#endif
}

TEST(SurfaceBackendContract, CpuDispatchMatchesReferenceExactly) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto direct_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dispatched_state = direct_state;
    const scau::surface2d::StepConfig config{
        .dt = 0.01,
        .c_rollback = 100.0,
    };

    const auto direct = scau::surface2d::advance_one_step_cpu(mesh, direct_state, config);
    const auto dispatched = scau::surface2d::advance_one_step(
        scau::surface2d::BackendKind::cpu_reference,
        mesh,
        dispatched_state,
        config);

    ASSERT_EQ(direct_state.cells.size(), dispatched_state.cells.size());
    for (std::size_t i = 0; i < direct_state.cells.size(); ++i) {
        EXPECT_DOUBLE_EQ(
            direct_state.cells[i].conserved.h,
            dispatched_state.cells[i].conserved.h);
        EXPECT_DOUBLE_EQ(
            direct_state.cells[i].conserved.hu,
            dispatched_state.cells[i].conserved.hu);
        EXPECT_DOUBLE_EQ(
            direct_state.cells[i].conserved.hv,
            dispatched_state.cells[i].conserved.hv);
    }
    EXPECT_DOUBLE_EQ(direct.max_cell_cfl, dispatched.max_cell_cfl);
}
