#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "stcf/schema.hpp"
#include "stcf/validate.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/stcf_bridge/assemble.hpp"

namespace {

using scau::mesh::build_mixed_minimal_mesh;
using scau::stcf::make_uniform_dataset;
using scau::stcf::StcfDataset;
using scau::stcf::validate_stcf_dataset;
using scau::surface2d::assemble_bed_elevations;
using scau::surface2d::assemble_dpm_fields;
using scau::surface2d::assemble_source_term_fields;
using scau::surface2d::validate_dpm_fields_match_mesh;
using scau::surface2d::validate_source_term_fields_match_mesh;

StcfDataset varied_dataset_for(const scau::mesh::Mesh& mesh) {
    auto dataset = make_uniform_dataset(mesh.cells.size(), mesh.edges.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        const double f = static_cast<double>(i);
        dataset.cells.phi_t[i] = 0.9 - 0.05 * f;
        dataset.cells.phi_xx[i] = 0.8 - 0.05 * f;
        dataset.cells.phi_xy[i] = 0.01 * f;
        dataset.cells.phi_yy[i] = 0.7 - 0.03 * f;
        dataset.cells.manning_n[i] = 0.013 + 0.002 * f;
        dataset.cells.z_b[i] = 0.25 * f;
    }
    for (std::size_t e = 0; e < mesh.edges.size(); ++e) {
        dataset.edges.omega_edge[e] = 1.0;
        dataset.edges.phi_e_n[e] = 0.9;
        dataset.edges.phi_et[e] = 0.85;
    }
    return dataset;
}

TEST(StcfBridgeAssemble, DpmFieldsMirrorDatasetAndMatchMesh) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto dataset = varied_dataset_for(mesh);
    ASSERT_NO_THROW(validate_stcf_dataset(dataset, mesh.cells.size(), mesh.edges.size()));

    const auto fields = assemble_dpm_fields(mesh, dataset);
    ASSERT_EQ(fields.cells.size(), mesh.cells.size());
    ASSERT_EQ(fields.edges.size(), mesh.edges.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(fields.cells[i].phi_t, dataset.cells.phi_t[i]);
        EXPECT_EQ(fields.cells[i].Phi_c.xx, dataset.cells.phi_xx[i]);
        EXPECT_EQ(fields.cells[i].Phi_c.xy, dataset.cells.phi_xy[i]);
        EXPECT_EQ(fields.cells[i].Phi_c.yy, dataset.cells.phi_yy[i]);
    }
    for (std::size_t e = 0; e < mesh.edges.size(); ++e) {
        EXPECT_EQ(fields.edges[e].phi_e_n, dataset.edges.phi_e_n[e]);
        EXPECT_EQ(fields.edges[e].omega_edge, dataset.edges.omega_edge[e]);
    }
    EXPECT_NO_THROW(validate_dpm_fields_match_mesh(fields, mesh));
}

TEST(StcfBridgeAssemble, SourceTermFieldsCarryManningAndZeroExchange) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto dataset = varied_dataset_for(mesh);

    const auto sources = assemble_source_term_fields(mesh, dataset);
    ASSERT_EQ(sources.manning_n.size(), mesh.cells.size());
    ASSERT_EQ(sources.exchange_volume.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(sources.manning_n[i], dataset.cells.manning_n[i]);
        EXPECT_EQ(sources.exchange_volume[i], 0.0);
    }
    EXPECT_NO_THROW(validate_source_term_fields_match_mesh(sources, mesh));
}

TEST(StcfBridgeAssemble, BedElevationsMirrorDataset) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto dataset = varied_dataset_for(mesh);

    const auto z_b = assemble_bed_elevations(mesh, dataset);
    ASSERT_EQ(z_b.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(z_b[i], dataset.cells.z_b[i]);
    }
}

TEST(StcfBridgeAssemble, RejectsCellCountMismatch) {
    const auto mesh = build_mixed_minimal_mesh();
    auto dataset = make_uniform_dataset(mesh.cells.size() + 1, mesh.edges.size());
    EXPECT_THROW(static_cast<void>(assemble_dpm_fields(mesh, dataset)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(assemble_source_term_fields(mesh, dataset)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(assemble_bed_elevations(mesh, dataset)), std::invalid_argument);
}

TEST(StcfBridgeAssemble, RejectsEdgeCountMismatch) {
    const auto mesh = build_mixed_minimal_mesh();
    auto dataset = make_uniform_dataset(mesh.cells.size(), mesh.edges.size() + 1);
    EXPECT_THROW(static_cast<void>(assemble_dpm_fields(mesh, dataset)), std::invalid_argument);
}

}  // namespace
