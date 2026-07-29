#include "stcf/io_netcdf.hpp"

#include <netcdf.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "stcf/topology.hpp"

namespace scau::stcf {
namespace {

void check(int status, const std::string& context) {
    if (status != NC_NOERR) {
        throw core::ScauError("STCF UGRID I/O failed (" + context + "): " + nc_strerror(status));
    }
}

class NcFileGuard {
public:
    explicit NcFileGuard(int ncid) : ncid_(ncid) {}
    NcFileGuard(const NcFileGuard&) = delete;
    NcFileGuard& operator=(const NcFileGuard&) = delete;
    ~NcFileGuard() {
        if (ncid_ >= 0) {
            nc_close(ncid_);
        }
    }
    void release() { ncid_ = -1; }

private:
    int ncid_;
};

void put_text_attribute(int ncid, int var_id, const char* name, const char* value) {
    check(nc_put_att_text(ncid, var_id, name, std::char_traits<char>::length(value), value),
          std::string("write attribute '") + name + "'");
}

void put_int_attribute(int ncid, int var_id, const char* name, int value) {
    check(nc_put_att_int(ncid, var_id, name, NC_INT, 1, &value),
          std::string("write attribute '") + name + "'");
}

int require_dimension(int ncid, const char* name, std::size_t expected = 0U) {
    int dim_id = -1;
    check(nc_inq_dimid(ncid, name, &dim_id), std::string("locate dimension '") + name + "'");
    if (expected != 0U) {
        std::size_t actual = 0;
        check(nc_inq_dimlen(ncid, dim_id, &actual), std::string("read dimension '") + name + "'");
        if (actual != expected) {
            throw core::ScauError(
                std::string("STCF UGRID I/O failed: dimension '") + name
                + "' has length " + std::to_string(actual)
                + ", expected " + std::to_string(expected));
        }
    }
    return dim_id;
}

int define_int_matrix(
    int ncid,
    const char* name,
    int first_dim,
    int second_dim,
    bool uses_fill) {
    const int dims[2] = {first_dim, second_dim};
    int var_id = -1;
    check(nc_def_var(ncid, name, NC_INT, 2, dims, &var_id),
          std::string("define variable '") + name + "'");
    put_int_attribute(ncid, var_id, "start_index", 0);
    if (uses_fill) {
        put_int_attribute(ncid, var_id, "_FillValue", kConnectivityFillValue);
    }
    return var_id;
}

template <std::size_t Width>
std::vector<int> flatten(const std::vector<std::array<int, Width>>& values) {
    std::vector<int> result;
    result.reserve(values.size() * Width);
    for (const auto& row : values) {
        result.insert(result.end(), row.begin(), row.end());
    }
    return result;
}

void add_location_attributes(int ncid, const char* name, const char* location) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate field '") + name + "'");
    put_text_attribute(ncid, var_id, "mesh", "mesh2");
    put_text_attribute(ncid, var_id, "location", location);
}

std::string read_text_attribute(int ncid, int var_id, const char* name) {
    std::size_t length = 0;
    check(nc_inq_attlen(ncid, var_id, name, &length), std::string("inspect attribute '") + name + "'");
    std::string value(length, '\0');
    check(nc_get_att_text(ncid, var_id, name, value.data()), std::string("read attribute '") + name + "'");
    return value;
}

void require_text_attribute(
    int ncid,
    int var_id,
    const char* name,
    const char* expected) {
    const std::string value = read_text_attribute(ncid, var_id, name);
    if (value != expected) {
        throw core::ScauError(
            std::string("STCF UGRID I/O failed: attribute '") + name
            + "' is '" + value + "', expected '" + expected + "'");
    }
}

void require_int_attribute(int ncid, int var_id, const char* name, int expected) {
    int value = 0;
    check(nc_get_att_int(ncid, var_id, name, &value), std::string("read attribute '") + name + "'");
    if (value != expected) {
        throw core::ScauError(
            std::string("STCF UGRID I/O failed: attribute '") + name
            + " has value " + std::to_string(value)
            + ", expected " + std::to_string(expected));
    }
}

void require_location_attributes(int ncid, const char* name, const char* location) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate field '") + name + "'");
    require_text_attribute(ncid, var_id, "mesh", "mesh2");
    require_text_attribute(ncid, var_id, "location", location);
}

std::vector<core::Real> read_double_vector(
    int ncid,
    const char* name,
    int expected_dim,
    std::size_t length) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate variable '") + name + "'");
    nc_type type = NC_NAT;
    int rank = 0;
    int dims[NC_MAX_VAR_DIMS] = {};
    check(nc_inq_var(ncid, var_id, nullptr, &type, &rank, dims, nullptr),
          std::string("inspect variable '") + name + "'");
    if (type != NC_DOUBLE || rank != 1 || dims[0] != expected_dim) {
        throw core::ScauError(
            std::string("STCF UGRID I/O failed: variable '") + name
            + "' has an unexpected type or dimension");
    }
    std::vector<core::Real> values(length);
    check(nc_get_var_double(ncid, var_id, values.data()), std::string("read variable '") + name + "'");
    return values;
}

template <std::size_t Width>
std::vector<std::array<int, Width>> read_int_matrix(
    int ncid,
    const char* name,
    int first_dim,
    int second_dim,
    std::size_t row_count,
    bool uses_fill) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate variable '") + name + "'");
    nc_type type = NC_NAT;
    int rank = 0;
    int dims[NC_MAX_VAR_DIMS] = {};
    check(nc_inq_var(ncid, var_id, nullptr, &type, &rank, dims, nullptr),
          std::string("inspect variable '") + name + "'");
    if (type != NC_INT || rank != 2 || dims[0] != first_dim || dims[1] != second_dim) {
        throw core::ScauError(
            std::string("STCF UGRID I/O failed: variable '") + name
            + "' has an unexpected type or dimension order");
    }
    require_int_attribute(ncid, var_id, "start_index", 0);
    if (uses_fill) {
        require_int_attribute(ncid, var_id, "_FillValue", kConnectivityFillValue);
    }
    std::vector<int> flat(row_count * Width);
    check(nc_get_var_int(ncid, var_id, flat.data()), std::string("read variable '") + name + "'");
    std::vector<std::array<int, Width>> values(row_count);
    for (std::size_t row = 0; row < row_count; ++row) {
        for (std::size_t column = 0; column < Width; ++column) {
            values[row][column] = flat[row * Width + column];
        }
    }
    return values;
}

}  // namespace

void write_stcf_case(
    const std::filesystem::path& path,
    const StcfCase& stcf_case,
    const StcfValidationConfig& config) {
    validate_stcf_case(stcf_case, config);
    write_stcf(path, stcf_case.fields, config);

    int ncid = -1;
    check(nc_open(path.string().c_str(), NC_WRITE, &ncid), "open for topology append");
    NcFileGuard guard(ncid);
    check(nc_redef(ncid), "enter definition mode");

    put_text_attribute(ncid, NC_GLOBAL, "Conventions", "CF-1.8 UGRID-1.0");

    const int face_dim = require_dimension(ncid, "cell", stcf_case.topology.face_nodes.size());
    const int edge_dim = require_dimension(ncid, "edge", stcf_case.topology.edge_nodes.size());
    int node_dim = -1;
    int max_face_nodes_dim = -1;
    int two_dim = -1;
    check(nc_def_dim(ncid, "nMesh2_node", stcf_case.topology.node_x.size(), &node_dim),
          "define dimension 'nMesh2_node'");
    check(nc_def_dim(ncid, "nMesh2_max_face_nodes", kMaxFaceNodes, &max_face_nodes_dim),
          "define dimension 'nMesh2_max_face_nodes'");
    check(nc_def_dim(ncid, "Two", 2, &two_dim), "define dimension 'Two'");

    int mesh_var = -1;
    check(nc_def_var(ncid, "mesh2", NC_INT, 0, nullptr, &mesh_var), "define mesh topology variable");
    put_text_attribute(ncid, mesh_var, "cf_role", "mesh_topology");
    put_int_attribute(ncid, mesh_var, "topology_dimension", 2);
    put_text_attribute(ncid, mesh_var, "node_coordinates", "mesh2_node_x mesh2_node_y");
    put_text_attribute(ncid, mesh_var, "face_node_connectivity", "mesh2_face_nodes");
    put_text_attribute(ncid, mesh_var, "edge_node_connectivity", "mesh2_edge_nodes");
    put_text_attribute(ncid, mesh_var, "edge_face_connectivity", "mesh2_edge_faces");
    put_text_attribute(ncid, mesh_var, "face_edge_connectivity", "mesh2_face_edges");
    put_text_attribute(ncid, mesh_var, "face_dimension", "cell");
    put_text_attribute(ncid, mesh_var, "edge_dimension", "edge");

    int node_x_var = -1;
    int node_y_var = -1;
    check(nc_def_var(ncid, "mesh2_node_x", NC_DOUBLE, 1, &node_dim, &node_x_var),
          "define variable 'mesh2_node_x'");
    check(nc_def_var(ncid, "mesh2_node_y", NC_DOUBLE, 1, &node_dim, &node_y_var),
          "define variable 'mesh2_node_y'");
    put_text_attribute(ncid, node_x_var, "standard_name", "projection_x_coordinate");
    put_text_attribute(ncid, node_y_var, "standard_name", "projection_y_coordinate");
    put_text_attribute(ncid, node_x_var, "units", "m");
    put_text_attribute(ncid, node_y_var, "units", "m");

    const int face_nodes_var = define_int_matrix(
        ncid, "mesh2_face_nodes", face_dim, max_face_nodes_dim, true);
    const int edge_nodes_var = define_int_matrix(
        ncid, "mesh2_edge_nodes", edge_dim, two_dim, false);
    const int edge_faces_var = define_int_matrix(
        ncid, "mesh2_edge_faces", edge_dim, two_dim, true);
    const int face_edges_var = define_int_matrix(
        ncid, "mesh2_face_edges", face_dim, max_face_nodes_dim, true);

    constexpr const char* kFaceFields[] = {
        "phi_t", "phi_xx", "phi_xy", "phi_yy", "manning_n", "z_b", "soil_type"};
    constexpr const char* kEdgeFields[] = {"omega_edge", "phi_e_n", "phi_et"};
    for (const char* name : kFaceFields) {
        add_location_attributes(ncid, name, "face");
    }
    for (const char* name : kEdgeFields) {
        add_location_attributes(ncid, name, "edge");
    }

    check(nc_enddef(ncid), "finish topology definition mode");
    check(nc_put_var_double(ncid, node_x_var, stcf_case.topology.node_x.data()),
          "write mesh2_node_x");
    check(nc_put_var_double(ncid, node_y_var, stcf_case.topology.node_y.data()),
          "write mesh2_node_y");
    const std::vector<int> face_nodes = flatten(stcf_case.topology.face_nodes);
    const std::vector<int> edge_nodes = flatten(stcf_case.topology.edge_nodes);
    const std::vector<int> edge_faces = flatten(stcf_case.topology.edge_faces);
    const std::vector<int> face_edges = flatten(stcf_case.topology.face_edges);
    check(nc_put_var_int(ncid, face_nodes_var, face_nodes.data()), "write mesh2_face_nodes");
    check(nc_put_var_int(ncid, edge_nodes_var, edge_nodes.data()), "write mesh2_edge_nodes");
    check(nc_put_var_int(ncid, edge_faces_var, edge_faces.data()), "write mesh2_edge_faces");
    check(nc_put_var_int(ncid, face_edges_var, face_edges.data()), "write mesh2_face_edges");

    guard.release();
    check(nc_close(ncid), "close complete STCF case");
}

StcfCase read_stcf_case(
    const std::filesystem::path& path,
    const StcfValidationConfig& config) {
    StcfCase stcf_case;
    stcf_case.fields = read_stcf(path, config);

    int ncid = -1;
    check(nc_open(path.string().c_str(), NC_NOWRITE, &ncid), "open complete STCF case");
    NcFileGuard guard(ncid);

    require_text_attribute(ncid, NC_GLOBAL, "Conventions", "CF-1.8 UGRID-1.0");
    int mesh_var = -1;
    check(nc_inq_varid(ncid, "mesh2", &mesh_var), "locate mesh topology variable");
    require_text_attribute(ncid, mesh_var, "cf_role", "mesh_topology");
    require_int_attribute(ncid, mesh_var, "topology_dimension", 2);
    require_text_attribute(ncid, mesh_var, "node_coordinates", "mesh2_node_x mesh2_node_y");
    require_text_attribute(ncid, mesh_var, "face_node_connectivity", "mesh2_face_nodes");
    require_text_attribute(ncid, mesh_var, "edge_node_connectivity", "mesh2_edge_nodes");
    require_text_attribute(ncid, mesh_var, "edge_face_connectivity", "mesh2_edge_faces");
    require_text_attribute(ncid, mesh_var, "face_edge_connectivity", "mesh2_face_edges");
    require_text_attribute(ncid, mesh_var, "face_dimension", "cell");
    require_text_attribute(ncid, mesh_var, "edge_dimension", "edge");
    constexpr const char* kFaceFields[] = {
        "phi_t", "phi_xx", "phi_xy", "phi_yy", "manning_n", "z_b", "soil_type"};
    constexpr const char* kEdgeFields[] = {"omega_edge", "phi_e_n", "phi_et"};
    for (const char* name : kFaceFields) {
        require_location_attributes(ncid, name, "face");
    }
    for (const char* name : kEdgeFields) {
        require_location_attributes(ncid, name, "edge");
    }

    const std::size_t face_count = stcf_case.fields.cells.phi_t.size();
    const std::size_t edge_count = stcf_case.fields.edges.omega_edge.size();
    const int face_dim = require_dimension(ncid, "cell", face_count);
    const int edge_dim = require_dimension(ncid, "edge", edge_count);
    std::size_t node_count = 0;
    int node_dim = -1;
    check(nc_inq_dimid(ncid, "nMesh2_node", &node_dim), "locate dimension 'nMesh2_node'");
    check(nc_inq_dimlen(ncid, node_dim, &node_count), "read dimension 'nMesh2_node'");
    const int max_face_nodes_dim = require_dimension(ncid, "nMesh2_max_face_nodes", kMaxFaceNodes);
    const int two_dim = require_dimension(ncid, "Two", 2);

    stcf_case.topology.node_x = read_double_vector(
        ncid, "mesh2_node_x", node_dim, node_count);
    stcf_case.topology.node_y = read_double_vector(
        ncid, "mesh2_node_y", node_dim, node_count);
    stcf_case.topology.face_nodes = read_int_matrix<kMaxFaceNodes>(
        ncid, "mesh2_face_nodes", face_dim, max_face_nodes_dim, face_count, true);
    stcf_case.topology.edge_nodes = read_int_matrix<2>(
        ncid, "mesh2_edge_nodes", edge_dim, two_dim, edge_count, false);
    stcf_case.topology.edge_faces = read_int_matrix<2>(
        ncid, "mesh2_edge_faces", edge_dim, two_dim, edge_count, true);
    stcf_case.topology.face_edges = read_int_matrix<kMaxFaceNodes>(
        ncid, "mesh2_face_edges", face_dim, max_face_nodes_dim, face_count, true);

    validate_stcf_case(stcf_case, config);
    return stcf_case;
}

}  // namespace scau::stcf
