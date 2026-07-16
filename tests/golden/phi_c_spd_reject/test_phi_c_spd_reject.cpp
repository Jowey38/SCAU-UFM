#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/edge_conveyance.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"

TEST(PhiCSpdRejectGolden, RejectsNonPositiveDefiniteTensor) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[0].Phi_c = scau::surface2d::Tensor2Symmetric{.xx = 0.0, .xy = 0.0, .yy = 0.0};

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields)),
        std::invalid_argument);
}
