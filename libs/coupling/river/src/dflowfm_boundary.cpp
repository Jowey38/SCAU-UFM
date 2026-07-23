#include "coupling/river/dflowfm_boundary.hpp"

#include <cmath>
#include <limits>

namespace scau::coupling::river {
namespace {

bool is_compound_lateral_discharge(const std::string& var_name) {
    constexpr const char* kPrefix = "laterals/";
    constexpr const char* kSuffix = "/water_discharge";
    if (!var_name.starts_with(kPrefix) || !var_name.ends_with(kSuffix)) {
        return false;
    }
    const std::size_t id_begin = std::char_traits<char>::length(kPrefix);
    const std::size_t id_length = var_name.size() - id_begin - std::char_traits<char>::length(kSuffix);
    return id_length > 0U && var_name.find('/', id_begin) == id_begin + id_length;
}

bool is_supported_dflowfm_variable(const std::string& var_name) {
    return var_name == "water_level" || var_name == "discharge" ||
        var_name == "lateral_discharge" || var_name == "gate_opening" ||
        is_compound_lateral_discharge(var_name);
}

bool is_supported_exchange_type(const std::string& exchange_type) {
    return exchange_type == "bank_overtop" || exchange_type == "lateral_weir" ||
        exchange_type == "gate_outlet" || exchange_type == "culvert_link";
}

bool has_only_nonnegative_edges(const std::vector<int>& edges) {
    for (const int edge : edges) {
        if (edge < 0) {
            return false;
        }
    }
    return true;
}

constexpr double kFlowVolumeConsistencyTolerance = 1.0e-12;

bool is_flow_volume_consistent(double q, double v, double dt_sub) {
    return std::abs(v - q * dt_sub) <= kFlowVolumeConsistencyTolerance;
}

}  // namespace

bool is_valid_river_exchange_point(const RiverExchangePoint& point) {
    return point.branch_id >= 0 && point.link_id >= 0 &&
        is_supported_exchange_type(point.exchange_type) && std::isfinite(point.crest_level) &&
        std::isfinite(point.priority_weight) && point.priority_weight > 0.0 &&
        std::isfinite(point.exchange_width) && point.exchange_width >= 0.0 &&
        !point.neighbor_2d_edges.empty() && has_only_nonnegative_edges(point.neighbor_2d_edges);
}

void MockDFlowFMEngine::initialize(const std::string& mdu_path) {
    static_cast<void>(mdu_path);
    initialized_ = true;
    elapsed_time_ = 0.0;
    values_.clear();
}

void MockDFlowFMEngine::update(double dt_dfm) {
    if (!initialized_) {
        throw DFlowFMEngineError("D-Flow FM mock engine is not initialized");
    }
    if (!std::isfinite(dt_dfm) || dt_dfm <= 0.0) {
        throw DFlowFMEngineError("dt_dfm must be finite and positive");
    }
    elapsed_time_ += dt_dfm;
}

void MockDFlowFMEngine::finalize() {
    initialized_ = false;
    values_.clear();
}

double MockDFlowFMEngine::get_value(const std::string& var_name, int location_id) const {
    if (!initialized_) {
        throw DFlowFMEngineError("D-Flow FM mock engine is not initialized");
    }
    if (location_id < 0) {
        throw DFlowFMEngineError("location_id must be non-negative");
    }
    if (!is_supported_dflowfm_variable(var_name)) {
        throw DFlowFMEngineError("unsupported D-Flow FM variable");
    }
    if (is_compound_lateral_discharge(var_name) && location_id != 0) {
        throw DFlowFMEngineError("compound D-Flow FM lateral variable requires location 0");
    }
    const auto iter = values_.find(ValueKey{.var_name = var_name, .location_id = location_id});
    if (iter == values_.end()) {
        return 0.0;
    }
    return iter->second;
}

void MockDFlowFMEngine::set_value(const std::string& var_name, int location_id, double value) {
    if (!initialized_) {
        throw DFlowFMEngineError("D-Flow FM mock engine is not initialized");
    }
    if (location_id < 0) {
        throw DFlowFMEngineError("location_id must be non-negative");
    }
    if (!is_supported_dflowfm_variable(var_name)) {
        throw DFlowFMEngineError("unsupported D-Flow FM variable");
    }
    if (is_compound_lateral_discharge(var_name) && location_id != 0) {
        throw DFlowFMEngineError("compound D-Flow FM lateral variable requires location 0");
    }
    if (!std::isfinite(value)) {
        throw DFlowFMEngineError("D-Flow FM value must be finite");
    }
    if (var_name == "gate_opening" && (value < 0.0 || value > 1.0)) {
        throw DFlowFMEngineError("gate_opening must be in [0, 1]");
    }
    values_[ValueKey{.var_name = var_name, .location_id = location_id}] = value;
}

void MockDFlowFMEngine::set_water_level_fixture(int location_id, double water_level) {
    set_value("water_level", location_id, water_level);
}

void MockDFlowFMEngine::set_discharge_fixture(int location_id, double discharge) {
    set_value("discharge", location_id, discharge);
}

void MockDFlowFMEngine::set_lateral_discharge_fixture(int location_id, double lateral_discharge) {
    set_value("lateral_discharge", location_id, lateral_discharge);
}

void MockDFlowFMEngine::set_gate_opening_fixture(int location_id, double gate_opening) {
    set_value("gate_opening", location_id, gate_opening);
}

bool MockDFlowFMEngine::initialized() const noexcept {
    return initialized_;
}

double MockDFlowFMEngine::elapsed_time() const noexcept {
    return elapsed_time_;
}

std::size_t MockDFlowFMEngine::ValueKeyHash::operator()(const ValueKey& key) const noexcept {
    return std::hash<std::string>{}(key.var_name) ^ (std::hash<int>{}(key.location_id) << 1U);
}

core::ExchangeRequest make_dflowfm_exchange_request(double q_request, double dt_sub) {
    if (!std::isfinite(q_request) || q_request < 0.0) {
        throw DFlowFMEngineError("q_request must be finite and non-negative");
    }
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw DFlowFMEngineError("dt_sub must be finite and positive");
    }
    return core::ExchangeRequest{
        .q_request = q_request,
        .dt_sub = dt_sub,
    };
}

core::SharedExchangeIntent make_dflowfm_shared_exchange_intent(
    int location_id,
    double q_request,
    double priority_weight) {
    if (location_id < 0) {
        throw DFlowFMEngineError("location_id must be non-negative");
    }
    if (!std::isfinite(q_request) || q_request < 0.0) {
        throw DFlowFMEngineError("q_request must be finite and non-negative");
    }
    if (!std::isfinite(priority_weight) || priority_weight <= 0.0) {
        throw DFlowFMEngineError("priority_weight must be finite and positive");
    }
    return core::SharedExchangeIntent{
        .endpoint = {
            .engine = core::SharedExchangeEngine::river,
            .node_id = static_cast<std::size_t>(location_id),
        },
        .q_request = q_request,
        .priority_weight = priority_weight,
    };
}

void accept_dflowfm_exchange_decision(
    IDFlowFMEngine& engine,
    const std::string& var_name,
    int location_id,
    const core::ExchangeDecision& decision,
    double dt_sub) {
    if (location_id < 0) {
        throw DFlowFMEngineError("location_id must be non-negative");
    }
    if (var_name != "lateral_discharge") {
        throw DFlowFMEngineError("accepted D-Flow FM exchange must target lateral_discharge");
    }
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw DFlowFMEngineError("dt_sub must be finite and positive");
    }
    if (!std::isfinite(decision.q_granted) || !std::isfinite(decision.v_granted) ||
        !std::isfinite(decision.q_repay) || !std::isfinite(decision.v_repay) ||
        !std::isfinite(decision.v_unmet) || decision.q_granted < 0.0 ||
        decision.v_granted < 0.0 || decision.q_repay < 0.0 || decision.v_repay < 0.0 ||
        decision.v_unmet < 0.0) {
        throw DFlowFMEngineError("exchange decision fields must be finite and non-negative");
    }
    if (!is_flow_volume_consistent(decision.q_granted, decision.v_granted, dt_sub) ||
        !is_flow_volume_consistent(decision.q_repay, decision.v_repay, dt_sub)) {
        throw DFlowFMEngineError("exchange decision flow and volume fields must be consistent with dt_sub");
    }
    engine.set_value(var_name, location_id, decision.q_granted + decision.q_repay);
}

void accept_dflowfm_lateral_discharge_exchange_decision(
    IDFlowFMEngine& engine,
    int location_id,
    const core::ExchangeDecision& decision,
    double dt_sub) {
    accept_dflowfm_exchange_decision(
        engine,
        "lateral_discharge",
        location_id,
        decision,
        dt_sub);
}

void accept_dflowfm_shared_exchange_decision(
    IDFlowFMEngine& engine,
    const core::SharedExchangeDecision& decision,
    double dt_sub) {
    if (decision.endpoint.engine != core::SharedExchangeEngine::river) {
        throw DFlowFMEngineError("shared exchange decision must target river engine");
    }
    if (decision.endpoint.node_id > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw DFlowFMEngineError("shared exchange node_id is out of int range");
    }
    accept_dflowfm_lateral_discharge_exchange_decision(
        engine,
        static_cast<int>(decision.endpoint.node_id),
        decision.exchange,
        dt_sub);
}

core::EngineReport make_dflowfm_engine_report(const MockDFlowFMEngine& engine) {
    return core::EngineReport{
        .healthy = engine.initialized(),
        .engine_id = "DFlowFM",
        .error_code = engine.initialized() ? "" : "dflowfm_mock_not_initialized",
        .elapsed_time = engine.elapsed_time(),
    };
}

core::EngineReport make_dflowfm_engine_report(const DFlowFMEngineError& error) {
    return core::EngineReport{
        .healthy = false,
        .engine_id = error.engine_id(),
        .error_code = error.error_code(),
        .elapsed_time = 0.0,
    };
}

}  // namespace scau::coupling::river
