#include "coupling/drainage/swmm_engine.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <string>

// Third-party firewall: the vendored SWMM public API is consumed only inside
// this translation unit (project-layout-design.md section 4 ABI firewall).
#include "swmm5.h"
#include "swmm5_massbal_bridge.h"

// Governed SWMM 5.2.4 internal ABI bridge. massbal_getStorage(FALSE) is the
// upstream node+link storage calculation used by SWMM's own flow balance. It
// returns internal ft3; swmm_NODE_VOLUME exposes the project-unit conversion.
extern "C" double massbal_getStorage(char is_final_storage);

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
    throw SwmmEngineError(text, "SWMM", "swmm_error_" + std::to_string(error_code));
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
            "another SWMM engine instance already owns the process-wide solver state",
            "SWMM",
            "swmm_project_already_open");
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

void SwmmEngine::step(double dt_swmm) {
    require_initialized();
    if (!std::isfinite(dt_swmm) || dt_swmm <= 0.0) {
        throw SwmmEngineError("dt_swmm must be finite and positive");
    }
    if (simulation_ended_) {
        throw SwmmEngineError(
            "SWMM simulation period has already ended", "SWMM", "swmm_simulation_ended");
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

double SwmmEngine::get_node_head(int node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_HEAD, node_id);
}

double SwmmEngine::get_node_lateral_inflow(int node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_LATFLOW, node_id);
}

void SwmmEngine::set_node_lateral_inflow(int node_id, double q) {
    require_valid_node(node_id);
    if (!std::isfinite(q)) {
        throw SwmmEngineError("node lateral inflow must be finite");
    }
    swmm_setValue(swmm_NODE_LATFLOW, node_id, q);
}

double SwmmEngine::get_node_inflow(int node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_INFLOW, node_id);
}

double SwmmEngine::get_node_overflow(int node_id) const {
    require_valid_node(node_id);
    return swmm_getValue(swmm_NODE_OVERFLOW, node_id);
}

void SwmmEngine::set_outfall_stage(int node_id, double stage) {
    require_valid_node(node_id);
    if (!std::isfinite(stage)) {
        throw SwmmEngineError("outfall stage must be finite");
    }
    const auto node_type = static_cast<int>(swmm_getValue(swmm_NODE_TYPE, node_id));
    if (node_type != swmm_OUTFALL) {
        throw SwmmEngineError(
            "set_outfall_stage target node is not an outfall",
            "SWMM",
            "swmm_node_not_outfall");
    }
    swmm_setValue(swmm_NODE_HEAD, node_id, stage);
}

double SwmmEngine::get_link_flow(int link_id) const {
    require_valid_link(link_id);
    return swmm_getValue(swmm_LINK_FLOW, link_id);
}

bool SwmmEngine::is_surcharged(int node_id) const {
    require_valid_node(node_id);
    const double depth = swmm_getValue(swmm_NODE_DEPTH, node_id);
    const double max_depth = swmm_getValue(swmm_NODE_MAXDEPTH, node_id);
    if (max_depth <= 0.0) {
        return false;
    }
    return depth >= max_depth - kSurchargeDepthTolerance;
}

bool SwmmEngine::initialized() const noexcept {
    return initialized_;
}

double SwmmEngine::elapsed_time() const noexcept {
    return elapsed_days_ * kSecondsPerDay;
}

int SwmmEngine::node_count() const {
    require_initialized();
    return swmm_getCount(swmm_NODE);
}

int SwmmEngine::node_index(const std::string& node_name) const {
    require_initialized();
    const int index = swmm_getIndex(swmm_NODE, node_name.c_str());
    if (index < 0) {
        throw SwmmEngineError(
            "SWMM node '" + node_name + "' was not found", "SWMM", "swmm_node_not_found");
    }
    return index;
}

int SwmmEngine::link_index(const std::string& link_name) const {
    require_initialized();
    const int index = swmm_getIndex(swmm_LINK, link_name.c_str());
    if (index < 0) {
        throw SwmmEngineError(
            "SWMM link '" + link_name + "' was not found", "SWMM", "swmm_link_not_found");
    }
    return index;
}

double SwmmEngine::total_stored_volume() const {
    require_initialized();
    const double internal_storage_ft3 = massbal_getStorage(0);
    // All accepted SCAU-UFM projects are metric CMS. Keep the locked upstream
    // 5.2.4 VOLUME conversion explicit instead of exposing private UnitSystem.
    constexpr double kCubicMetresPerCubicFoot = 0.02832;
    const double storage_m3 = internal_storage_ft3 * kCubicMetresPerCubicFoot;
    if (!std::isfinite(storage_m3) || storage_m3 < 0.0) {
        throw SwmmEngineError(
            "SWMM total stored volume is invalid", "SWMM", "swmm_storage_invalid");
    }
    return storage_m3;
}

SwmmExternalNetObservation SwmmEngine::observe_external_net_volume() const {
    require_initialized();
    SwmmRoutingTotalsSnapshot raw{};
    if (massbal_getRoutingTotals(&raw) != 0) {
        throw SwmmEngineError(
            "SWMM routing totals bridge failed", "SWMM", "swmm_routing_totals_unavailable");
    }

    constexpr double kCubicMetresPerCubicFoot = 0.02832;
    const auto volume = [](double value) {
        return value * kCubicMetresPerCubicFoot;
    };
    SwmmExternalNetObservation observation{};
    const double api_lateral_m3 = volume(raw.apiInflow);
    observation.api_lateral_inflow_m3 = api_lateral_m3;
    observation.dw_inflow_m3 = volume(raw.dwInflow);
    observation.ww_inflow_m3 = volume(raw.wwInflow);
    observation.gw_inflow_m3 = volume(raw.gwInflow);
    observation.ii_inflow_m3 = volume(raw.iiInflow);
    observation.ex_inflow_m3 = volume(raw.exInflow);
    observation.flooding_m3 = volume(raw.flooding);
    observation.outflow_m3 = volume(raw.outflow);
    observation.evap_loss_m3 = volume(raw.evapLoss);
    observation.seep_loss_m3 = volume(raw.seepLoss);
    observation.initial_storage_m3 = volume(raw.initStorage);
    observation.final_storage_m3 = volume(raw.finalStorage);
    // Preserve SWMM's raw continuity scope for provenance, then expose the
    // audit scope with the CouplingLib-owned API lateral volume removed once.
    observation.routing_external_net_volume_m3 = observation.dw_inflow_m3 +
        observation.ww_inflow_m3 + observation.gw_inflow_m3 + observation.ii_inflow_m3 +
        observation.ex_inflow_m3 - observation.outflow_m3 - observation.flooding_m3 -
        observation.evap_loss_m3 - observation.seep_loss_m3;
    observation.external_net_volume_m3 =
        observation.routing_external_net_volume_m3 - observation.api_lateral_inflow_m3;
    observation.scope_complete = true;
    for (const double value : {
             observation.dw_inflow_m3, observation.ww_inflow_m3,
             observation.gw_inflow_m3, observation.ii_inflow_m3,
             observation.ex_inflow_m3, observation.flooding_m3,
             observation.outflow_m3, observation.evap_loss_m3,
             observation.seep_loss_m3, observation.initial_storage_m3,
             observation.final_storage_m3, observation.routing_external_net_volume_m3,
             observation.api_lateral_inflow_m3, observation.external_net_volume_m3}) {
        if (!std::isfinite(value)) {
            observation.scope_complete = false;
            break;
        }
    }
    return observation;
}

void SwmmEngine::require_initialized() const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM engine is not initialized");
    }
}

void SwmmEngine::require_valid_node(int node_id) const {
    require_initialized();
    if (node_id < 0 || node_id >= swmm_getCount(swmm_NODE)) {
        throw SwmmEngineError("node_id is out of range", "SWMM", "swmm_node_out_of_range");
    }
}

void SwmmEngine::require_valid_link(int link_id) const {
    require_initialized();
    if (link_id < 0 || link_id >= swmm_getCount(swmm_LINK)) {
        throw SwmmEngineError("link_id is out of range", "SWMM", "swmm_link_out_of_range");
    }
}

core::EngineReport make_swmm_engine_report(const SwmmEngine& engine) {
    return core::EngineReport{
        .healthy = engine.initialized(),
        .engine_id = "SWMM",
        .error_code = engine.initialized() ? "" : "swmm_engine_not_initialized",
        .elapsed_time = engine.elapsed_time(),
    };
}

}  // namespace scau::coupling::drainage
