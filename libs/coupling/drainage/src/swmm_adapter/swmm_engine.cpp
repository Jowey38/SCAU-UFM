#include "coupling/drainage/swmm_engine.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <string>

// Third-party firewall: the vendored SWMM public API is consumed only inside
// this translation unit (project-layout-design.md section 4 ABI firewall).
#include "swmm5.h"

namespace scau::coupling::drainage {
namespace {

constexpr double kSecondsPerDay = 86400.0;
constexpr double kStepTargetToleranceDays = 1.0e-9;
constexpr double kSurchargeDepthTolerance = 1.0e-9;

// The SWMM solver mutates process-wide global state; only one open project is
// allowed per process.
std::atomic<bool> g_swmm_project_open{false};

[[noreturn]] void throw_swmm_error(int error_code, const std::string& context) {
    std::array<char, 512> message{};
    swmm_getError(message.data(), static_cast<int>(message.size() - 1));
    std::string text = context;
    if (message[0] != '\0') {
        text += ": ";
        text += message.data();
    }
    text += " [swmm_error_" + std::to_string(error_code) + "]";
    throw SwmmEngineError(text);
}

std::string sibling_path_with_extension(const std::string& inp_path, const std::string& extension) {
    const auto dot = inp_path.find_last_of('.');
    const auto separator = inp_path.find_last_of("/\\");
    if (dot == std::string::npos || (separator != std::string::npos && dot < separator)) {
        return inp_path + extension;
    }
    return inp_path.substr(0, dot) + extension;
}

}  // namespace

SwmmEngine::~SwmmEngine() {
    if (!initialized_) {
        return;
    }
    // Best-effort shutdown; destructors must not throw.
    swmm_end();
    swmm_close();
    g_swmm_project_open.store(false);
}

void SwmmEngine::initialize(const std::string& inp_path) {
    if (initialized_) {
        throw SwmmEngineError("SWMM engine is already initialized");
    }
    if (inp_path.empty()) {
        throw SwmmEngineError("inp_path must not be empty");
    }
    bool expected = false;
    if (!g_swmm_project_open.compare_exchange_strong(expected, true)) {
        throw SwmmEngineError(
            "another SWMM engine instance already owns the process-wide solver state");
    }

    const std::string report_path = sibling_path_with_extension(inp_path, ".rpt");
    const std::string output_path = sibling_path_with_extension(inp_path, ".out");
    int error_code = swmm_open(inp_path.c_str(), report_path.c_str(), output_path.c_str());
    if (error_code != 0) {
        swmm_close();
        g_swmm_project_open.store(false);
        throw_swmm_error(error_code, "swmm_open failed for '" + inp_path + "'");
    }
    error_code = swmm_start(0);
    if (error_code != 0) {
        swmm_close();
        g_swmm_project_open.store(false);
        throw_swmm_error(error_code, "swmm_start failed for '" + inp_path + "'");
    }
    initialized_ = true;
    elapsed_days_ = 0.0;
    simulation_ended_ = false;
}

void SwmmEngine::step(core::Real dt_swmm) {
    require_initialized();
    if (!std::isfinite(dt_swmm) || dt_swmm <= 0.0) {
        throw SwmmEngineError("dt_swmm must be finite and positive");
    }
    if (simulation_ended_) {
        throw SwmmEngineError("SWMM simulation period has already ended");
    }
    const double target_days = elapsed_days_ + dt_swmm / kSecondsPerDay;
    while (elapsed_days_ + kStepTargetToleranceDays < target_days) {
        double elapsed = 0.0;
        const int error_code = swmm_step(&elapsed);
        if (error_code != 0) {
            throw_swmm_error(error_code, "swmm_step failed");
        }
        if (elapsed <= 0.0) {
            // SWMM signals end-of-simulation by resetting elapsed time to 0.
            simulation_ended_ = true;
            break;
        }
        elapsed_days_ = elapsed;
    }
}

void SwmmEngine::finalize() {
    if (!initialized_) {
        return;
    }
    swmm_end();
    swmm_close();
    initialized_ = false;
    simulation_ended_ = false;
    elapsed_days_ = 0.0;
    g_swmm_project_open.store(false);
}

core::Real SwmmEngine::get_node_head(std::size_t node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_HEAD, static_cast<int>(node_id));
}

core::Real SwmmEngine::get_node_lateral_inflow(std::size_t node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_LATFLOW, static_cast<int>(node_id));
}

void SwmmEngine::set_node_lateral_inflow(std::size_t node_id, core::Real q) {
    require_valid_node(node_id);
    if (!std::isfinite(q)) {
        throw SwmmEngineError("node lateral inflow must be finite");
    }
    swmm_setValue(swmm_NODE_LATFLOW, static_cast<int>(node_id), q);
}

core::Real SwmmEngine::get_node_inflow(std::size_t node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_INFLOW, static_cast<int>(node_id));
}

core::Real SwmmEngine::get_node_overflow(std::size_t node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_OVERFLOW, static_cast<int>(node_id));
}

void SwmmEngine::set_outfall_stage(std::size_t node_id, core::Real stage) {
    require_valid_node(node_id);
    if (!std::isfinite(stage)) {
        throw SwmmEngineError("outfall stage must be finite");
    }
    const auto node_type =
        static_cast<int>(swmm_getValue(swmm_NODE_TYPE, static_cast<int>(node_id)));
    if (node_type != swmm_OUTFALL) {
        throw SwmmEngineError("set_outfall_stage target node is not an outfall");
    }
    swmm_setValue(swmm_NODE_HEAD, static_cast<int>(node_id), stage);
}

core::Real SwmmEngine::get_link_flow(std::size_t link_id) const {
    require_valid_link(link_id);
    return swmm_getValue(swmm_LINK_FLOW, static_cast<int>(link_id));
}

bool SwmmEngine::is_surcharged(std::size_t node_id) const {
    require_valid_node(node_id);
    const double depth = swmm_getValue(swmm_NODE_DEPTH, static_cast<int>(node_id));
    const double max_depth = swmm_getValue(swmm_NODE_MAXDEPTH, static_cast<int>(node_id));
    if (max_depth <= 0.0) {
        return false;
    }
    return depth >= max_depth - kSurchargeDepthTolerance;
}

bool SwmmEngine::initialized() const noexcept {
    return initialized_;
}

core::Real SwmmEngine::elapsed_time() const noexcept {
    return elapsed_days_ * kSecondsPerDay;
}

std::size_t SwmmEngine::node_count() const {
    require_initialized();
    return static_cast<std::size_t>(swmm_getCount(swmm_NODE));
}

std::size_t SwmmEngine::node_index(const std::string& node_name) const {
    require_initialized();
    const int index = swmm_getIndex(swmm_NODE, node_name.c_str());
    if (index < 0) {
        throw SwmmEngineError("SWMM node '" + node_name + "' was not found");
    }
    return static_cast<std::size_t>(index);
}

std::size_t SwmmEngine::link_index(const std::string& link_name) const {
    require_initialized();
    const int index = swmm_getIndex(swmm_LINK, link_name.c_str());
    if (index < 0) {
        throw SwmmEngineError("SWMM link '" + link_name + "' was not found");
    }
    return static_cast<std::size_t>(index);
}

void SwmmEngine::require_initialized() const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM engine is not initialized");
    }
}

void SwmmEngine::require_valid_node(std::size_t node_id) const {
    require_initialized();
    if (node_id >= static_cast<std::size_t>(swmm_getCount(swmm_NODE))) {
        throw SwmmEngineError("node_id is out of range");
    }
}

void SwmmEngine::require_valid_link(std::size_t link_id) const {
    require_initialized();
    if (link_id >= static_cast<std::size_t>(swmm_getCount(swmm_LINK))) {
        throw SwmmEngineError("link_id is out of range");
    }
}

}  // namespace scau::coupling::drainage
