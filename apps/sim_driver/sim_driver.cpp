#include "sim_driver.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace scau::apps::sim_driver {

namespace {

void require_positive_finite(double value, const char* field) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(field) + " must be finite and positive");
    }
}

void require_state(SimDriverState actual, SimDriverState expected, const char* action) {
    if (actual != expected) {
        throw std::logic_error(std::string(action) + " is invalid in the current SimDriver state");
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
    if (config.stcf_case_path.empty()) {
        throw std::invalid_argument("stcf_case_path must not be empty");
    }
    if (config.enable_swmm) {
        require_positive_finite(config.dt_swmm, "dt_swmm");
        if (config.swmm_inp_path.empty()) {
            throw std::invalid_argument("enabled SWMM requires swmm_inp_path");
        }
    }
    if (config.enable_dflowfm) {
        require_positive_finite(config.dt_dflowfm, "dt_dflowfm");
        if (config.dflowfm_mdu_path.empty()) {
            throw std::invalid_argument("enabled D-Flow FM requires dflowfm_mdu_path");
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
