#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"

TEST(DpmFields, CreatesMeshAlignedDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto fields = scau::surface2d::DpmFields::for_mesh(mesh);

    ASSERT_EQ(fields.cells.size(), mesh.cells.size());
    ASSERT_EQ(fields.edges.size(), mesh.edges.size());
    for (const auto& cell : fields.cells) {
        EXPECT_EQ(cell.phi_t, 1.0);
        EXPECT_EQ(cell.Phi_c, 1.0);
    }
    for (const auto& edge : fields.edges) {
        EXPECT_EQ(edge.phi_e_n, 1.0);
        EXPECT_EQ(edge.omega_edge, 1.0);
    }
}

TEST(DpmFields, RejectsWrongFieldCounts) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto fields = scau::surface2d::DpmFields::for_mesh(mesh);

    fields.cells.pop_back();
    EXPECT_THROW(scau::surface2d::validate_dpm_fields_match_mesh(fields, mesh), std::invalid_argument);

    fields = scau::surface2d::DpmFields::for_mesh(mesh);
    fields.edges.pop_back();
    EXPECT_THROW(scau::surface2d::validate_dpm_fields_match_mesh(fields, mesh), std::invalid_argument);
}
