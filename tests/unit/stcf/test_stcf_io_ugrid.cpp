#include <gtest/gtest.h>

#include <netcdf.h>

#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "stcf/io_netcdf.hpp"
#include "stcf_case_fixture.hpp"

namespace {

using scau::core::ScauError;
using scau::stcf::read_stcf;
using scau::stcf::read_stcf_case;
using scau::stcf::test::make_mixed_case;
using scau::stcf::write_stcf;
using scau::stcf::write_stcf_case;

std::filesystem::path temp_file(const std::string& name) {
    return std::filesystem::path(::testing::TempDir()) / name;
}

TEST(StcfIoUgrid, CompleteCaseRoundTripPreservesTopologyAndFields) {
    const auto path = temp_file("stcf_ugrid_round_trip.stcf.nc");
    const auto written = make_mixed_case();
    write_stcf_case(path, written);

    const auto loaded = read_stcf_case(path);
    EXPECT_EQ(loaded.topology.node_x, written.topology.node_x);
    EXPECT_EQ(loaded.topology.node_y, written.topology.node_y);
    EXPECT_EQ(loaded.topology.face_nodes, written.topology.face_nodes);
    EXPECT_EQ(loaded.topology.edge_nodes, written.topology.edge_nodes);
    EXPECT_EQ(loaded.topology.edge_faces, written.topology.edge_faces);
    EXPECT_EQ(loaded.topology.face_edges, written.topology.face_edges);
    EXPECT_EQ(loaded.fields.cells.phi_t, written.fields.cells.phi_t);
    EXPECT_EQ(loaded.fields.cells.z_b, written.fields.cells.z_b);
    EXPECT_EQ(loaded.fields.edges.phi_e_n, written.fields.edges.phi_e_n);
}

TEST(StcfIoUgrid, CompleteCaseRemainsReadableAsFieldOnlyDataset) {
    const auto path = temp_file("stcf_ugrid_fields_compatible.stcf.nc");
    const auto written = make_mixed_case();
    write_stcf_case(path, written);

    const auto fields = read_stcf(path);
    EXPECT_EQ(fields.cells.phi_t, written.fields.cells.phi_t);
    EXPECT_EQ(fields.edges.omega_edge, written.fields.edges.omega_edge);
}

TEST(StcfIoUgrid, StrictReaderRejectsFieldOnlyFile) {
    const auto path = temp_file("stcf_field_only_rejected.stcf.nc");
    const auto stcf_case = make_mixed_case();
    write_stcf(path, stcf_case.fields);

    EXPECT_NO_THROW(static_cast<void>(read_stcf(path)));
    EXPECT_THROW(static_cast<void>(read_stcf_case(path)), ScauError);
}

TEST(StcfIoUgrid, WriterRejectsInvalidTopologyBeforeTouchingDisk) {
    const auto path = temp_file("stcf_invalid_topology_never_written.stcf.nc");
    std::filesystem::remove(path);
    auto stcf_case = make_mixed_case();
    stcf_case.topology.face_nodes[0][0] = 99;

    EXPECT_THROW(write_stcf_case(path, stcf_case), ScauError);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(StcfIoUgrid, StrictReaderRejectsTamperedStartIndex) {
    const auto path = temp_file("stcf_bad_start_index.stcf.nc");
    write_stcf_case(path, make_mixed_case());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_redef(ncid), NC_NOERR);
    int var_id = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "mesh2_face_nodes", &var_id), NC_NOERR);
    const int one_based = 1;
    ASSERT_EQ(nc_put_att_int(ncid, var_id, "start_index", NC_INT, 1, &one_based), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf_case(path)), ScauError);
}

TEST(StcfIoUgrid, StrictReaderRejectsTamperedTopologyAttribute) {
    const auto path = temp_file("stcf_bad_topology_attribute.stcf.nc");
    write_stcf_case(path, make_mixed_case());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_redef(ncid), NC_NOERR);
    int mesh_var = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "mesh2", &mesh_var), NC_NOERR);
    const char wrong[] = "wrong_face_nodes";
    ASSERT_EQ(
        nc_put_att_text(ncid, mesh_var, "face_node_connectivity", sizeof(wrong) - 1U, wrong),
        NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf_case(path)), ScauError);
}

TEST(StcfIoUgrid, StrictReaderRejectsTamperedFieldLocationAttribute) {
    const auto path = temp_file("stcf_bad_field_location.stcf.nc");
    write_stcf_case(path, make_mixed_case());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_redef(ncid), NC_NOERR);
    int var_id = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "phi_t", &var_id), NC_NOERR);
    const char wrong[] = "edge";
    ASSERT_EQ(nc_put_att_text(ncid, var_id, "location", sizeof(wrong) - 1U, wrong), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf_case(path)), ScauError);
}

TEST(StcfIoUgrid, StrictReaderRejectsTamperedConnectivityValue) {
    const auto path = temp_file("stcf_bad_connectivity.stcf.nc");
    write_stcf_case(path, make_mixed_case());

    int ncid = -1;
    ASSERT_EQ(nc_open(path.string().c_str(), NC_WRITE, &ncid), NC_NOERR);
    int var_id = -1;
    ASSERT_EQ(nc_inq_varid(ncid, "mesh2_edge_nodes", &var_id), NC_NOERR);
    const std::size_t index[2] = {0U, 0U};
    const int invalid_node = 99;
    ASSERT_EQ(nc_put_var1_int(ncid, var_id, index, &invalid_node), NC_NOERR);
    ASSERT_EQ(nc_close(ncid), NC_NOERR);

    EXPECT_THROW(static_cast<void>(read_stcf_case(path)), ScauError);
}

}  // namespace
