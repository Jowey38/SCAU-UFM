#include <gtest/gtest.h>

#include "stcf/schema.hpp"
#include "stcf/validate.hpp"

namespace {

using scau::stcf::kMaxSoilTypes;
using scau::stcf::kSchemaVersion;
using scau::stcf::make_uniform_dataset;
using scau::stcf::validate_stcf_dataset;

TEST(StcfSchema, SchemaVersionIsFive) {
    EXPECT_EQ(kSchemaVersion, 5);
    EXPECT_EQ(kMaxSoilTypes, 16u);
}

TEST(StcfSchema, UniformDatasetHasRequestedShape) {
    const auto dataset = make_uniform_dataset(6, 9);
    EXPECT_EQ(dataset.schema_version, kSchemaVersion);
    EXPECT_EQ(dataset.cells.phi_t.size(), 6u);
    EXPECT_EQ(dataset.cells.phi_xx.size(), 6u);
    EXPECT_EQ(dataset.cells.phi_xy.size(), 6u);
    EXPECT_EQ(dataset.cells.phi_yy.size(), 6u);
    EXPECT_EQ(dataset.cells.manning_n.size(), 6u);
    EXPECT_EQ(dataset.cells.z_b.size(), 6u);
    EXPECT_EQ(dataset.cells.soil_type.size(), 6u);
    EXPECT_EQ(dataset.edges.omega_edge.size(), 9u);
    EXPECT_EQ(dataset.edges.phi_e_n.size(), 9u);
    EXPECT_EQ(dataset.edges.phi_et.size(), 9u);
    EXPECT_EQ(dataset.soil_params.size(), 1u);
}

TEST(StcfSchema, UniformDatasetUsesOpenIdentityFields) {
    const auto dataset = make_uniform_dataset(2, 3);
    EXPECT_DOUBLE_EQ(dataset.cells.phi_t[0], 1.0);
    EXPECT_DOUBLE_EQ(dataset.cells.phi_xx[1], 1.0);
    EXPECT_DOUBLE_EQ(dataset.cells.phi_xy[1], 0.0);
    EXPECT_DOUBLE_EQ(dataset.cells.phi_yy[1], 1.0);
    EXPECT_DOUBLE_EQ(dataset.cells.z_b[0], 0.0);
    EXPECT_EQ(dataset.cells.soil_type[0], 0);
    EXPECT_DOUBLE_EQ(dataset.edges.omega_edge[2], 1.0);
    EXPECT_DOUBLE_EQ(dataset.edges.phi_e_n[2], 1.0);
    EXPECT_DOUBLE_EQ(dataset.edges.phi_et[2], 1.0);
}

TEST(StcfSchema, UniformDatasetPassesValidation) {
    const auto dataset = make_uniform_dataset(4, 5);
    EXPECT_NO_THROW(validate_stcf_dataset(dataset, 4, 5));
    EXPECT_NO_THROW(validate_stcf_dataset(dataset));
}

}  // namespace
