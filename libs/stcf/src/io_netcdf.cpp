#include "stcf/io_netcdf.hpp"

#include <netcdf.h>

#include <cstddef>
#include <string>
#include <vector>

#include "core/error.hpp"

namespace scau::stcf {

namespace {

void check(int status, const std::string& context) {
    if (status != NC_NOERR) {
        throw core::ScauError("STCF NetCDF I/O failed (" + context + "): " + nc_strerror(status));
    }
}

// Closes the NetCDF file on scope exit so error paths never leak the handle.
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

int define_double_var(int ncid, const char* name, int dim_id) {
    int var_id = -1;
    check(nc_def_var(ncid, name, NC_DOUBLE, 1, &dim_id, &var_id),
          std::string("define variable '") + name + "'");
    return var_id;
}

void put_double_var(int ncid, int var_id, const std::vector<core::Real>& values, const char* name) {
    check(nc_put_var_double(ncid, var_id, values.data()),
          std::string("write variable '") + name + "'");
}

int require_dim(int ncid, const char* name, std::size_t& length_out) {
    int dim_id = -1;
    check(nc_inq_dimid(ncid, name, &dim_id), std::string("locate dimension '") + name + "'");
    std::size_t length = 0;
    check(nc_inq_dimlen(ncid, dim_id, &length), std::string("read dimension '") + name + "'");
    length_out = length;
    return dim_id;
}

std::vector<core::Real> read_double_var(
    int ncid, const char* name, int expected_dim_id, std::size_t length) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate variable '") + name + "'");
    nc_type var_type = NC_NAT;
    int ndims = 0;
    int dim_ids[NC_MAX_VAR_DIMS] = {};
    check(nc_inq_var(ncid, var_id, nullptr, &var_type, &ndims, dim_ids, nullptr),
          std::string("inspect variable '") + name + "'");
    if (var_type != NC_DOUBLE || ndims != 1 || dim_ids[0] != expected_dim_id) {
        throw core::ScauError(
            std::string("STCF NetCDF I/O failed: variable '") + name
            + "' has an unexpected type or dimension");
    }
    std::vector<core::Real> values(length);
    check(nc_get_var_double(ncid, var_id, values.data()),
          std::string("read variable '") + name + "'");
    return values;
}

std::vector<int> read_int_var(
    int ncid, const char* name, int expected_dim_id, std::size_t length) {
    int var_id = -1;
    check(nc_inq_varid(ncid, name, &var_id), std::string("locate variable '") + name + "'");
    nc_type var_type = NC_NAT;
    int ndims = 0;
    int dim_ids[NC_MAX_VAR_DIMS] = {};
    check(nc_inq_var(ncid, var_id, nullptr, &var_type, &ndims, dim_ids, nullptr),
          std::string("inspect variable '") + name + "'");
    if (var_type != NC_INT || ndims != 1 || dim_ids[0] != expected_dim_id) {
        throw core::ScauError(
            std::string("STCF NetCDF I/O failed: variable '") + name
            + "' has an unexpected type or dimension");
    }
    std::vector<int> values(length);
    check(nc_get_var_int(ncid, var_id, values.data()),
          std::string("read variable '") + name + "'");
    return values;
}

}  // namespace

void write_stcf(
    const std::filesystem::path& path,
    const StcfDataset& dataset,
    const StcfValidationConfig& config) {
    validate_stcf_dataset(dataset, config);

    const std::size_t cell_count = dataset.cells.phi_t.size();
    const std::size_t edge_count = dataset.edges.omega_edge.size();
    const std::size_t soil_count = dataset.soil_params.size();

    int ncid = -1;
    check(nc_create(path.string().c_str(), NC_CLOBBER, &ncid), "create '" + path.string() + "'");
    NcFileGuard guard(ncid);

    int cell_dim = -1;
    int edge_dim = -1;
    int soil_dim = -1;
    check(nc_def_dim(ncid, "cell", cell_count, &cell_dim), "define dimension 'cell'");
    check(nc_def_dim(ncid, "edge", edge_count, &edge_dim), "define dimension 'edge'");
    check(nc_def_dim(ncid, "soil_type_entry", soil_count, &soil_dim),
          "define dimension 'soil_type_entry'");

    const int schema_version = dataset.schema_version;
    check(nc_put_att_int(ncid, NC_GLOBAL, "schema_version", NC_INT, 1, &schema_version),
          "write attribute 'schema_version'");
    const std::string title = "SCAU-UFM STCF v5";
    check(nc_put_att_text(ncid, NC_GLOBAL, "title", title.size(), title.c_str()),
          "write attribute 'title'");

    const int phi_t_var = define_double_var(ncid, "phi_t", cell_dim);
    const int phi_xx_var = define_double_var(ncid, "phi_xx", cell_dim);
    const int phi_xy_var = define_double_var(ncid, "phi_xy", cell_dim);
    const int phi_yy_var = define_double_var(ncid, "phi_yy", cell_dim);
    const int manning_var = define_double_var(ncid, "manning_n", cell_dim);
    const int z_b_var = define_double_var(ncid, "z_b", cell_dim);
    int soil_type_var = -1;
    check(nc_def_var(ncid, "soil_type", NC_INT, 1, &cell_dim, &soil_type_var),
          "define variable 'soil_type'");
    const int omega_var = define_double_var(ncid, "omega_edge", edge_dim);
    const int phi_e_n_var = define_double_var(ncid, "phi_e_n", edge_dim);
    const int phi_et_var = define_double_var(ncid, "phi_et", edge_dim);
    const int k_s_var = define_double_var(ncid, "K_s", soil_dim);
    const int psi_f_var = define_double_var(ncid, "psi_f", soil_dim);
    const int theta_s_var = define_double_var(ncid, "theta_s", soil_dim);
    const int theta_i_var = define_double_var(ncid, "theta_i", soil_dim);

    check(nc_enddef(ncid), "finish definition mode");

    put_double_var(ncid, phi_t_var, dataset.cells.phi_t, "phi_t");
    put_double_var(ncid, phi_xx_var, dataset.cells.phi_xx, "phi_xx");
    put_double_var(ncid, phi_xy_var, dataset.cells.phi_xy, "phi_xy");
    put_double_var(ncid, phi_yy_var, dataset.cells.phi_yy, "phi_yy");
    put_double_var(ncid, manning_var, dataset.cells.manning_n, "manning_n");
    put_double_var(ncid, z_b_var, dataset.cells.z_b, "z_b");

    std::vector<int> soil_type(cell_count);
    for (std::size_t i = 0; i < cell_count; ++i) {
        soil_type[i] = static_cast<int>(dataset.cells.soil_type[i]);
    }
    check(nc_put_var_int(ncid, soil_type_var, soil_type.data()), "write variable 'soil_type'");

    put_double_var(ncid, omega_var, dataset.edges.omega_edge, "omega_edge");
    put_double_var(ncid, phi_e_n_var, dataset.edges.phi_e_n, "phi_e_n");
    put_double_var(ncid, phi_et_var, dataset.edges.phi_et, "phi_et");

    std::vector<core::Real> k_s(soil_count);
    std::vector<core::Real> psi_f(soil_count);
    std::vector<core::Real> theta_s(soil_count);
    std::vector<core::Real> theta_i(soil_count);
    for (std::size_t i = 0; i < soil_count; ++i) {
        k_s[i] = dataset.soil_params[i].K_s;
        psi_f[i] = dataset.soil_params[i].psi_f;
        theta_s[i] = dataset.soil_params[i].theta_s;
        theta_i[i] = dataset.soil_params[i].theta_i;
    }
    put_double_var(ncid, k_s_var, k_s, "K_s");
    put_double_var(ncid, psi_f_var, psi_f, "psi_f");
    put_double_var(ncid, theta_s_var, theta_s, "theta_s");
    put_double_var(ncid, theta_i_var, theta_i, "theta_i");

    guard.release();
    check(nc_close(ncid), "close '" + path.string() + "'");
}

StcfDataset read_stcf(const std::filesystem::path& path, const StcfValidationConfig& config) {
    int ncid = -1;
    check(nc_open(path.string().c_str(), NC_NOWRITE, &ncid), "open '" + path.string() + "'");
    NcFileGuard guard(ncid);

    int schema_version = 0;
    check(nc_get_att_int(ncid, NC_GLOBAL, "schema_version", &schema_version),
          "read attribute 'schema_version'");

    std::size_t cell_count = 0;
    std::size_t edge_count = 0;
    std::size_t soil_count = 0;
    const int cell_dim = require_dim(ncid, "cell", cell_count);
    const int edge_dim = require_dim(ncid, "edge", edge_count);
    const int soil_dim = require_dim(ncid, "soil_type_entry", soil_count);

    StcfDataset dataset;
    dataset.schema_version = schema_version;
    dataset.cells.phi_t = read_double_var(ncid, "phi_t", cell_dim, cell_count);
    dataset.cells.phi_xx = read_double_var(ncid, "phi_xx", cell_dim, cell_count);
    dataset.cells.phi_xy = read_double_var(ncid, "phi_xy", cell_dim, cell_count);
    dataset.cells.phi_yy = read_double_var(ncid, "phi_yy", cell_dim, cell_count);
    dataset.cells.manning_n = read_double_var(ncid, "manning_n", cell_dim, cell_count);
    dataset.cells.z_b = read_double_var(ncid, "z_b", cell_dim, cell_count);

    const std::vector<int> soil_type = read_int_var(ncid, "soil_type", cell_dim, cell_count);
    dataset.cells.soil_type.resize(cell_count);
    for (std::size_t i = 0; i < cell_count; ++i) {
        if (soil_type[i] < 0 || soil_type[i] > 255) {
            throw core::ScauError(
                "STCF NetCDF I/O failed: soil_type value " + std::to_string(soil_type[i])
                + " at index " + std::to_string(i) + " outside the uint8 range");
        }
        dataset.cells.soil_type[i] = static_cast<std::uint8_t>(soil_type[i]);
    }

    dataset.edges.omega_edge = read_double_var(ncid, "omega_edge", edge_dim, edge_count);
    dataset.edges.phi_e_n = read_double_var(ncid, "phi_e_n", edge_dim, edge_count);
    dataset.edges.phi_et = read_double_var(ncid, "phi_et", edge_dim, edge_count);

    const std::vector<core::Real> k_s = read_double_var(ncid, "K_s", soil_dim, soil_count);
    const std::vector<core::Real> psi_f = read_double_var(ncid, "psi_f", soil_dim, soil_count);
    const std::vector<core::Real> theta_s = read_double_var(ncid, "theta_s", soil_dim, soil_count);
    const std::vector<core::Real> theta_i = read_double_var(ncid, "theta_i", soil_dim, soil_count);
    dataset.soil_params.resize(soil_count);
    for (std::size_t i = 0; i < soil_count; ++i) {
        dataset.soil_params[i] = SoilParamsEntry{
            .K_s = k_s[i],
            .psi_f = psi_f[i],
            .theta_s = theta_s[i],
            .theta_i = theta_i[i],
        };
    }

    validate_stcf_dataset(dataset, cell_count, edge_count, config);
    return dataset;
}

}  // namespace scau::stcf
