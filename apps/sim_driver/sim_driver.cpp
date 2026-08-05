#include "sim_driver.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace scau::apps::sim_driver {

namespace {

void require_positive_finite(double value, const char* field) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(field) + " must be finite and positive");
    }
}

void require_integral_ratio(double whole, double part, const char* message) {
    const double ratio = whole / part;
    const double rounded = std::round(ratio);
    if (rounded < 1.0 || std::abs(ratio - rounded) > 1.0e-9 * ratio) {
        throw std::invalid_argument(message);
    }
}

void require_equal_dt(double dt_engine, double dt_couple, const char* field) {
    if (std::abs(dt_engine - dt_couple) > 1.0e-12 * dt_couple) {
        throw std::invalid_argument(
            std::string(field) +
            " must equal dt_couple (engine sub-stepping is not supported)");
    }
}

void require_state(SimDriverState actual, SimDriverState expected, const char* action) {
    if (actual != expected) {
        throw std::logic_error(std::string(action) + " is invalid in the current SimDriver state");
    }
}

void validate_exchange_structure(double crest_level,
                                 double exchange_width,
                                 double priority_weight,
                                 const char* link_kind) {
    if (!std::isfinite(crest_level)) {
        throw std::invalid_argument(std::string(link_kind) + " crest_level must be finite");
    }
    if (!std::isfinite(exchange_width) || exchange_width <= 0.0) {
        throw std::invalid_argument(
            std::string(link_kind) + " exchange_width must be finite and positive");
    }
    if (!std::isfinite(priority_weight) || priority_weight <= 0.0) {
        throw std::invalid_argument(
            std::string(link_kind) + " priority_weight must be finite and positive");
    }
}

}  // namespace

void validate_runtime_config(const RuntimeConfig& config) {
    if (config.version != kRuntimeConfigVersion) {
        throw std::invalid_argument("unsupported runtime config version");
    }
    if (!std::isfinite(config.start_time) || !std::isfinite(config.end_time)
        || config.end_time <= config.start_time) {
        throw std::invalid_argument("end_time must be finite and greater than start_time");
    }
    require_positive_finite(config.dt_couple, "dt_couple");
    require_positive_finite(config.dt_surface, "dt_surface");
    if (config.dt_surface > config.dt_couple) {
        throw std::invalid_argument("dt_surface must not exceed dt_couple");
    }
    require_integral_ratio(config.dt_couple, config.dt_surface,
                           "dt_couple must be an integer multiple of dt_surface");
    require_integral_ratio(config.end_time - config.start_time, config.dt_couple,
                           "end_time - start_time must be an integer multiple of dt_couple");
    if (config.stcf_case_path.empty()) {
        throw std::invalid_argument("stcf_case_path must not be empty");
    }
    if (!std::isfinite(config.initial_eta)) {
        throw std::invalid_argument(
            "initial_eta is required (STCF carries no hydrodynamic initial state)");
    }
    require_positive_finite(config.h_wet, "h_wet");
    require_positive_finite(config.cfl_safety, "cfl_safety");
    if (config.cfl_safety > 1.0) {
        throw std::invalid_argument("cfl_safety must not exceed 1.0");
    }
    require_positive_finite(config.c_rollback, "c_rollback");
    if (!std::isfinite(config.mass_audit_engine_residual_absolute) ||
        config.mass_audit_engine_residual_absolute < 0.0 ||
        !std::isfinite(config.mass_audit_engine_residual_relative) ||
        config.mass_audit_engine_residual_relative < 0.0) {
        throw std::invalid_argument(
            "mass audit engine tolerances must be finite and non-negative");
    }

    if (config.enable_swmm) {
        require_positive_finite(config.dt_swmm, "dt_swmm");
        require_equal_dt(config.dt_swmm, config.dt_couple, "dt_swmm");
        if (config.swmm_inp_path.empty()) {
            throw std::invalid_argument("enabled SWMM requires swmm_inp_path");
        }
        if (config.surface_drainage.empty() && config.drainage_river.empty()) {
            throw std::invalid_argument("enabled SWMM requires at least one drainage link");
        }
    } else if (!config.surface_drainage.empty() || !config.drainage_river.empty()) {
        throw std::invalid_argument("drainage links require enable_swmm");
    }
    if (config.enable_dflowfm) {
        require_positive_finite(config.dt_dflowfm, "dt_dflowfm");
        require_equal_dt(config.dt_dflowfm, config.dt_couple, "dt_dflowfm");
        if (config.dflowfm_mdu_path.empty()) {
            throw std::invalid_argument("enabled D-Flow FM requires dflowfm_mdu_path");
        }
        if (config.surface_river.empty() && config.drainage_river.empty()) {
            throw std::invalid_argument("enabled D-Flow FM requires at least one river link");
        }
    } else if (!config.surface_river.empty() || !config.drainage_river.empty()) {
        throw std::invalid_argument("river links require enable_dflowfm");
    }

    std::unordered_set<std::size_t> coupled_cells;
    for (const auto& link : config.surface_drainage) {
        if (link.node_name.empty()) {
            throw std::invalid_argument("surface_drainage_link node must not be empty");
        }
        validate_exchange_structure(link.crest_level, link.exchange_width,
                                    link.priority_weight, "surface_drainage_link");
        if (!coupled_cells.insert(link.cell).second) {
            throw std::invalid_argument(
                "surface cell coupled more than once across links (single-writer rule)");
        }
    }
    for (const auto& link : config.surface_river) {
        if (link.native_lateral_id.empty()) {
            throw std::invalid_argument("surface_river_link lateral_id must not be empty");
        }
        validate_exchange_structure(link.crest_level, link.exchange_width,
                                    link.priority_weight, "surface_river_link");
        if (!coupled_cells.insert(link.cell).second) {
            throw std::invalid_argument(
                "surface cell coupled more than once across links (single-writer rule)");
        }
    }
    for (const auto& link : config.drainage_river) {
        if (link.outfall_name.empty()) {
            throw std::invalid_argument("drainage_river_link outfall must not be empty");
        }
        if (!std::isfinite(link.q_capacity) || link.q_capacity < 0.0) {
            throw std::invalid_argument(
                "drainage_river_link q_capacity must be finite and non-negative");
        }
    }
}

void SimDriver::configure(RuntimeConfig config) {
    require_state(state_, SimDriverState::created, "configure");
    validate_runtime_config(config);
    config_ = std::move(config);
    configured_ = true;
    state_ = SimDriverState::configured;
}

void SimDriver::initialize() {
    require_state(state_, SimDriverState::configured, "initialize");
    state_ = SimDriverState::initialized;
}

void SimDriver::start() {
    require_state(state_, SimDriverState::initialized, "start");
    state_ = SimDriverState::running;
}

void SimDriver::complete() {
    require_state(state_, SimDriverState::running, "complete");
    state_ = SimDriverState::completed;
}

void SimDriver::require_review() {
    require_state(state_, SimDriverState::running, "require_review");
    state_ = SimDriverState::review_required;
}

void SimDriver::abort() {
    if (state_ == SimDriverState::completed || state_ == SimDriverState::aborted) {
        throw std::logic_error("abort is invalid in a terminal SimDriver state");
    }
    state_ = SimDriverState::aborted;
}

SimDriverState SimDriver::state() const noexcept {
    return state_;
}

const RuntimeConfig& SimDriver::config() const {
    if (!configured_) {
        throw std::logic_error("SimDriver has no configured RuntimeConfig");
    }
    return config_;
}

std::size_t SimDriver::completed_coupling_steps() const noexcept {
    return completed_coupling_steps_;
}

void SimDriver::record_committed_coupling_step() {
    require_state(state_, SimDriverState::running, "record_committed_coupling_step");
    ++completed_coupling_steps_;
}

}  // namespace scau::apps::sim_driver
