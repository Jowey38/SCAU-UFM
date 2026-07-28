#include "stcf/validate.hpp"

#include <cmath>
#include <sstream>
#include <string>

#include "core/error.hpp"

namespace scau::stcf {

namespace {

[[noreturn]] void fail(const char* stage, std::size_t index, const std::string& detail) {
    std::ostringstream message;
    message << "STCF validation failed at stage '" << stage << "', index " << index << ": " << detail;
    throw core::ScauError(message.str());
}

[[noreturn]] void fail_shape(const char* field, std::size_t actual, std::size_t expected) {
    std::ostringstream message;
    message << "STCF validation failed at stage 'shape': field '" << field << "' has size "
            << actual << ", expected " << expected;
    throw core::ScauError(message.str());
}

void require_size(const char* field, std::size_t actual, std::size_t expected) {
    if (actual != expected) {
        fail_shape(field, actual, expected);
    }
}

void require_finite(const char* stage, std::size_t index, const char* field, core::Real value) {
    if (!std::isfinite(value)) {
        fail(stage, index, std::string(field) + " is not finite");
    }
}

}  // namespace

void validate_stcf_dataset(
    const StcfDataset& dataset,
    std::size_t expected_cell_count,
    std::size_t expected_edge_count,
    const StcfValidationConfig& config) {
    if (dataset.schema_version != kSchemaVersion) {
        fail("schema_version", 0,
             "schema_version is " + std::to_string(dataset.schema_version) + ", expected "
                 + std::to_string(kSchemaVersion));
    }
    if (expected_cell_count == 0) {
        fail("shape", 0, "cell count must be positive");
    }
    if (expected_edge_count == 0) {
        fail("shape", 0, "edge count must be positive");
    }

    require_size("phi_t", dataset.cells.phi_t.size(), expected_cell_count);
    require_size("phi_xx", dataset.cells.phi_xx.size(), expected_cell_count);
    require_size("phi_xy", dataset.cells.phi_xy.size(), expected_cell_count);
    require_size("phi_yy", dataset.cells.phi_yy.size(), expected_cell_count);
    require_size("manning_n", dataset.cells.manning_n.size(), expected_cell_count);
    require_size("z_b", dataset.cells.z_b.size(), expected_cell_count);
    require_size("soil_type", dataset.cells.soil_type.size(), expected_cell_count);
    require_size("omega_edge", dataset.edges.omega_edge.size(), expected_edge_count);
    require_size("phi_e_n", dataset.edges.phi_e_n.size(), expected_edge_count);
    require_size("phi_et", dataset.edges.phi_et.size(), expected_edge_count);

    if (dataset.soil_params.empty()) {
        fail("soil", 0, "soil_params must contain at least one entry");
    }
    if (dataset.soil_params.size() > kMaxSoilTypes) {
        fail("soil", 0,
             "soil_params has " + std::to_string(dataset.soil_params.size())
                 + " entries, exceeding MAX_SOIL_TYPES = " + std::to_string(kMaxSoilTypes));
    }

    for (std::size_t i = 0; i < expected_cell_count; ++i) {
        const core::Real phi_t = dataset.cells.phi_t[i];
        require_finite("phi_t", i, "phi_t", phi_t);
        if (phi_t <= 0.0 || phi_t > 1.0) {
            fail("phi_t", i, "phi_t = " + std::to_string(phi_t) + " outside (0, 1]");
        }
    }

    for (std::size_t i = 0; i < expected_cell_count; ++i) {
        const core::Real xx = dataset.cells.phi_xx[i];
        const core::Real xy = dataset.cells.phi_xy[i];
        const core::Real yy = dataset.cells.phi_yy[i];
        require_finite("Phi_c", i, "phi_xx", xx);
        require_finite("Phi_c", i, "phi_xy", xy);
        require_finite("Phi_c", i, "phi_yy", yy);
        if (xx <= 0.0 || yy <= 0.0) {
            fail("Phi_c", i, "diagonal entries must be positive");
        }
        const core::Real det = xx * yy - xy * xy;
        if (det <= config.epsilon_det) {
            fail("Phi_c", i, "det = " + std::to_string(det) + " not above epsilon_det");
        }
        const core::Real trace = xx + yy;
        const core::Real discriminant_sq = trace * trace - 4.0 * det;
        const core::Real discriminant = std::sqrt(discriminant_sq > 0.0 ? discriminant_sq : 0.0);
        const core::Real lambda_min = 0.5 * (trace - discriminant);
        const core::Real lambda_max = 0.5 * (trace + discriminant);
        if (lambda_min < config.epsilon_phi) {
            fail("Phi_c", i, "lambda_min = " + std::to_string(lambda_min) + " below epsilon_phi");
        }
        if (lambda_max > 1.0) {
            fail("Phi_c", i, "lambda_max = " + std::to_string(lambda_max) + " above 1");
        }
        if (lambda_max / lambda_min > config.cond_max) {
            fail("Phi_c", i, "condition number above cond_max");
        }
        const core::Real max_diag = xx > yy ? xx : yy;
        if (dataset.cells.phi_t[i] < max_diag) {
            fail("Phi_c", i, "phi_t below max diagonal of Phi_c");
        }
    }

    for (std::size_t i = 0; i < expected_cell_count; ++i) {
        require_finite("cell_scalars", i, "manning_n", dataset.cells.manning_n[i]);
        if (dataset.cells.manning_n[i] < 0.0) {
            fail("cell_scalars", i, "manning_n must be non-negative");
        }
        require_finite("cell_scalars", i, "z_b", dataset.cells.z_b[i]);
        if (dataset.cells.soil_type[i] >= dataset.soil_params.size()) {
            fail("cell_scalars", i,
                 "soil_type = " + std::to_string(dataset.cells.soil_type[i])
                     + " out of range for soil_params size "
                     + std::to_string(dataset.soil_params.size()));
        }
    }

    for (std::size_t i = 0; i < expected_edge_count; ++i) {
        const core::Real omega = dataset.edges.omega_edge[i];
        const core::Real phi_e_n = dataset.edges.phi_e_n[i];
        const core::Real phi_et = dataset.edges.phi_et[i];
        require_finite("edges", i, "omega_edge", omega);
        require_finite("edges", i, "phi_e_n", phi_e_n);
        require_finite("edges", i, "phi_et", phi_et);
        if (omega < 0.0 || omega > 1.0) {
            fail("edges", i, "omega_edge outside [0, 1]");
        }
        if (phi_e_n < 0.0 || phi_e_n > 1.0) {
            fail("edges", i, "phi_e_n outside [0, 1]");
        }
        if (phi_et < 0.0 || phi_et > 1.0) {
            fail("edges", i, "phi_et outside [0, 1]");
        }
    }

    for (std::size_t i = 0; i < dataset.soil_params.size(); ++i) {
        const auto& entry = dataset.soil_params[i];
        require_finite("soil", i, "K_s", entry.K_s);
        require_finite("soil", i, "psi_f", entry.psi_f);
        require_finite("soil", i, "theta_s", entry.theta_s);
        require_finite("soil", i, "theta_i", entry.theta_i);
        if (entry.K_s < 0.0) {
            fail("soil", i, "K_s must be non-negative");
        }
        if (entry.psi_f <= 0.0) {
            fail("soil", i, "psi_f must be strictly positive");
        }
        if (entry.theta_i < 0.0) {
            fail("soil", i, "theta_i must be non-negative");
        }
        if (entry.theta_s <= entry.theta_i || entry.theta_s > 1.0) {
            fail("soil", i, "theta_s must satisfy theta_i < theta_s <= 1");
        }
    }
}

void validate_stcf_dataset(const StcfDataset& dataset, const StcfValidationConfig& config) {
    validate_stcf_dataset(
        dataset, dataset.cells.phi_t.size(), dataset.edges.omega_edge.size(), config);
}

}  // namespace scau::stcf
