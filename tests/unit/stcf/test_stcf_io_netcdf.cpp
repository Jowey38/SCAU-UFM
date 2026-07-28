#include <gtest/gtest.h>

#include <netcdf.h>

#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "stcf/io_netcdf.hpp"
#include "stcf/schema.hpp"

namespace {

using scau::core::ScauError;
using scau::stcf::make_uniform_dataset;
using scau::stcf::read_stcf;
using scau::stcf::StcfDataset;
using scau::stcf::write_stcf;

std::filesystem::path temp_file(const std::string& name) {
    return std::filesystem::path(::testing::TempDir()) / name;
}

StcfDataset make_varied_dataset() {
    auto dataset = make_uniform_dataset(3, 4);
    dataset.cells.phi_t = {1.0, 0.8, 0.6};
    dataset.cells.phi_xx = {1.0, 0.75, 0.5};
    dataset.cells.phi_xy = {0.0, 0.05, -0.05};
    dataset.cells.phi_yy = {1.0, 0.8, 0.55};
    dataset.cells.manning_n = {0.013, 0.02, 0.035};
    dataset.cells.z_b = {0.0, 0.25, -0.5};
    dataset.cells.soil_type = {0, 1, 1};
    dataset.edges.omega_edge = {1.0, 0.5, 0.0, 1.0};
    dataset.edges.phi_e_n = {1.0, 0.4, 0.0, 0.9};
    dataset.edges.phi_et = {1.0, 0.3, 0.0, 0.85};
    dataset.soil_params.push_back(scau::stcf::SoilParamsEntry{
        .K_s = 5.0e-6,
        .psi_f = 0.2,
        .theta_s = 0.45,
        .theta_i = 0.05,
    });
    return dataset;
}

TEST(StcfIoNetcdf, RoundTripPreservesEveryFieldBitwise) {
    const auto path = temp_file("stcf_round_trip.stcf.nc");
    const auto written = make_varied_dataset();
    write_stcf(path, written);

    const auto loaded = read_stcf(path);
    EXPECT_EQ(loaded.schema_version, written.schema_version);
    ASSERT_EQ(loaded.cells.phi_t.size(), written.cells.phi_t.size());
    ASSERT_EQ(loaded.edges.omega_edge.size(), written.edges.omega_edge.size());
    ASSERT_EQ(loaded.soil_params.size(), written.soil_params.size());
    for (std::size_t i = 0; i < written.cells.phi_t.size(); ++i) {
        EXPECT_EQ(loaded.cells.phi_t[i], written.cells.phi_t[i]);
        EXPECT_EQ(loaded.cells.phi_xx[i], written.cells.phi_xx[i]);
        EXPECT_EQ(loaded.cells.phi_xy[i], written.cells.phi_xy[i]);
        EXPECT_EQ(loaded.cells.phi_yy[i], written.cells.phi_yy[i]);
        EXPECT_EQ(loaded.cells.manning_n[i], written.cells.manning_n[i]);
        EXPECT_EQ(loaded.cells.z_b[i], written.cells.z_b[i]);
        EXPECT_EQ(loaded.cells.soil_type[i], written.cells.soil_type[i]);
    }
    for (std::size_t i = 0; i < written.edges.omega_edge.size(); ++i) {
        EXPECT_EQ(loaded.edges.omega_edge[i], written.edges.omega_edge[i]);
        EXPECT_EQ(loaded.edges.phi_e_n[i], written.edges.phi_e_n[i]);
        EXPECT_EQ(loaded.edges.phi_et[i], written.edges.phi_et[i]);
    }
    for (std::size_t i = 0; i < written.soil_params.size(); ++i) {
        EXPECT_EQ(loaded.soil_params[i].K_s, written.soil_params[i].K_s);
        EXPECT_EQ(loaded.soil_params[i].psi_f, written.soil_params[i].psi_f);
        EXPECT_EQ(loaded.soil_params[i].theta_s, written.soil_params[i].theta_s);
        EXPECT_EQ(loaded.soil_params[i].theta_i, written.soil_params[i].theta_i);
    }
}

TEST(StcfIoNetcdf, WriteRejectsInvalidDatasetBeforeTouchingDisk) {
    const auto path = temp_file("stcf_never_written.stcf.nc");
    std::filesystem::remove(path);
    auto dataset = make_varied_dataset();
    dataset.cells.phi_t[0] = 2.0;
    EXPECT_THROW(write_stcf(path, dataset), ScauError);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(StcfIoNetcdf, ReadRejectsMissingFile) {
    const auto path = temp_file("stcf_missing.stcf.nc");
    std::filesystem::remove(path);
    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

TEST(StcfIoNetcdf, ReadRejectsWrongSchemaVersionAttribute) {
    const auto path = temp_file("stcf_wrong_version.stcf.nc");
    write_stcf(path, make_varied_dataset());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_redef(ncid), NC_NOERR);
    const int legacy_version = 4;
    ASSERT_EQ(nc_put_att_int(ncid, NC_GLOBAL, "schema_version", NC_INT, 1, &legacy_version),
              NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

TEST(StcfIoNetcdf, ReadRejectsFileWithoutSchemaVersionAttribute) {
    const auto path = temp_file("stcf_no_version.stcf.nc");
    int ncid = -1;
    ASSERT_EQ(nc_create(path.string().c_str(), NC_CLOBBER, &ncid), NC_NOERR);
    int cell_dim = -1;
    ASSERT_EQ(nc_def_dim(ncid, "cell", 2, &cell_dim), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

TEST(StcfIoNetcdf, ReadRejectsMissingVariable) {
    const auto path = temp_file("stcf_missing_var.stcf.nc");
    int ncid = -1;
    ASSERT_EQ(nc_create(path.string().c_str(), NC_CLOBBER, &ncid), NC_NOERR);
    const int schema_version = 5;
    ASSERT_EQ(nc_put_att_int(ncid, NC_GLOBAL, "schema_version", NC_INT, 1, &schema_version),
              NC_NOERR);
    int cell_dim = -1;
    int edge_dim = -1;
    int soil_dim = -1;
    ASSERT_EQ(nc_def_dim(ncid, "cell", 2, &cell_dim), NC_NOERR);
    ASSERT_EQ(nc_def_dim(ncid, "edge", 2, &edge_dim), NC_NOERR);
    ASSERT_EQ(nc_def_dim(ncid, "soil_type_entry", 1, &soil_dim), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

TEST(StcfIoNetcdf, ReadRejectsVariableOnWrongDimension) {
    const auto path = temp_file("stcf_wrong_dim.stcf.nc");
    write_stcf(path, make_varied_dataset());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_redef(ncid), NC_NOERR);
    int phi_t_var = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "phi_t", &phi_t_var), NC_NOERR);
    ASSERT_EQ(nc_rename_var(ncid, phi_t_var, "phi_t_renamed"), NC_NOERR);
    int edge_dim = -1;
    ASSERT_EQ(nc_inq_dimid(ncid, "edge", &edge_dim), NC_NOERR);
    int replacement_var = -1;
    ASSERT_EQ(nc_def_var(ncid, "phi_t", NC_DOUBLE, 1, &edge_dim, &replacement_var), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

TEST(StcfIoNetcdf, ReadRejectsValueTamperedBelowValidationGate) {
    const auto path = temp_file("stcf_tampered_value.stcf.nc");
    write_stcf(path, make_varied_dataset());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    int phi_t_var = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "phi_t", &phi_t_var), NC_NOERR);
    const std::size_t index = 0;
    const double invalid_phi_t = 1.75;
    ASSERT_EQ(nc_put_var1_double(ncid, phi_t_var, &index, &invalid_phi_t), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf(path)), ScauError);
}

}  // namespace
