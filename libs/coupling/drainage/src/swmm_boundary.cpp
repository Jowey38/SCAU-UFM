#include "coupling/drainage/swmm_boundary.hpp"

#include <cmath>
#include <limits>

namespace scau::coupling::drainage {
namespace {

constexpr double kFlowVolumeConsistencyTolerance = 1.0e-12;

bool is_flow_volume_consistent(double q, double v, double dt_sub) {
    return std::abs(v - q * dt_sub) <= kFlowVolumeConsistencyTolerance;
}

}  // namespace

void MockSwmmEngine::initialize(const std::string& inp_path) {
    static_cast<void>(inp_path);
    initialized_ = true;
    elapsed_time_ = 0.0;
    node_heads_.clear();
    node_lateral_inflows_.clear();
    node_inflows_.clear();
    node_overflows_.clear();
    outfall_stages_.clear();
    link_flows_.clear();
    node_surcharge_flags_.clear();
}

void MockSwmmEngine::step(double dt_swmm) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (!std::isfinite(dt_swmm) || dt_swmm <= 0.0) {
        throw SwmmEngineError("dt_swmm must be finite and positive");
    }
    elapsed_time_ += dt_swmm;
}

void MockSwmmEngine::finalize() {
    initialized_ = false;
    node_heads_.clear();
    node_lateral_inflows_.clear();
    node_inflows_.clear();
    node_overflows_.clear();
    outfall_stages_.clear();
    link_flows_.clear();
    node_surcharge_flags_.clear();
}

double MockSwmmEngine::get_node_head(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = node_heads_.find(node_id);
    if (iter == node_heads_.end()) {
        return 0.0;
    }
    return iter->second;
}

double MockSwmmEngine::get_node_lateral_inflow(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = node_lateral_inflows_.find(node_id);
    if (iter == node_lateral_inflows_.end()) {
        return 0.0;
    }
    return iter->second;
}

void MockSwmmEngine::set_node_lateral_inflow(int node_id, double q) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(q)) {
        throw SwmmEngineError("node lateral inflow must be finite");
    }
    node_lateral_inflows_[node_id] = q;
}

void MockSwmmEngine::set_node_head_fixture(int node_id, double head) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(head)) {
        throw SwmmEngineError("node head fixture must be finite");
    }
    node_heads_[node_id] = head;
}

double MockSwmmEngine::get_node_inflow(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = node_inflows_.find(node_id);
    if (iter == node_inflows_.end()) {
        return 0.0;
    }
    return iter->second;
}

void MockSwmmEngine::set_node_inflow_fixture(int node_id, double q) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(q)) {
        throw SwmmEngineError("node inflow fixture must be finite");
    }
    node_inflows_[node_id] = q;
}

double MockSwmmEngine::get_node_overflow(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = node_overflows_.find(node_id);
    if (iter == node_overflows_.end()) {
        return 0.0;
    }
    return iter->second;
}

void MockSwmmEngine::set_node_overflow_fixture(int node_id, double q) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(q) || q < 0.0) {
        throw SwmmEngineError("node overflow fixture must be finite and non-negative");
    }
    node_overflows_[node_id] = q;
}

void MockSwmmEngine::set_outfall_stage(int node_id, double stage) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(stage)) {
        throw SwmmEngineError("outfall stage must be finite");
    }
    outfall_stages_[node_id] = stage;
}

double MockSwmmEngine::outfall_stage(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = outfall_stages_.find(node_id);
    if (iter == outfall_stages_.end()) {
        return 0.0;
    }
    return iter->second;
}

double MockSwmmEngine::get_link_flow(int link_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (link_id < 0) {
        throw SwmmEngineError("link_id must be non-negative");
    }
    const auto iter = link_flows_.find(link_id);
    if (iter == link_flows_.end()) {
        return 0.0;
    }
    return iter->second;
}

void MockSwmmEngine::set_link_flow_fixture(int link_id, double q) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (link_id < 0) {
        throw SwmmEngineError("link_id must be non-negative");
    }
    if (!std::isfinite(q)) {
        throw SwmmEngineError("link flow fixture must be finite");
    }
    link_flows_[link_id] = q;
}

bool MockSwmmEngine::is_surcharged(int node_id) const {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    const auto iter = node_surcharge_flags_.find(node_id);
    if (iter == node_surcharge_flags_.end()) {
        return false;
    }
    return iter->second;
}

void MockSwmmEngine::set_node_surcharge_fixture(int node_id, bool surcharged) {
    if (!initialized_) {
        throw SwmmEngineError("SWMM mock engine is not initialized");
    }
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    node_surcharge_flags_[node_id] = surcharged;
}

bool MockSwmmEngine::initialized() const noexcept {
    return initialized_;
}

double MockSwmmEngine::elapsed_time() const noexcept {
    return elapsed_time_;
}

core::ExchangeRequest make_swmm_exchange_request(double q_request, double dt_sub) {
    if (!std::isfinite(q_request) || q_request < 0.0) {
        throw SwmmEngineError("q_request must be finite and non-negative");
    }
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw SwmmEngineError("dt_sub must be finite and positive");
    }
    return core::ExchangeRequest{
        .q_request = q_request,
        .dt_sub = dt_sub,
    };
}

core::SharedExchangeIntent make_swmm_shared_exchange_intent(
    int node_id,
    double q_request,
    double priority_weight) {
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(q_request) || q_request < 0.0) {
        throw SwmmEngineError("q_request must be finite and non-negative");
    }
    if (!std::isfinite(priority_weight) || priority_weight <= 0.0) {
        throw SwmmEngineError("priority_weight must be finite and positive");
    }
    return core::SharedExchangeIntent{
        .endpoint = {
            .engine = core::SharedExchangeEngine::drainage,
            .node_id = static_cast<std::size_t>(node_id),
        },
        .q_request = q_request,
        .priority_weight = priority_weight,
    };
}

void accept_swmm_exchange_decision(
    ISwmmEngine& engine,
    int node_id,
    const core::ExchangeDecision& decision,
    double dt_sub) {
    if (node_id < 0) {
        throw SwmmEngineError("node_id must be non-negative");
    }
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw SwmmEngineError("dt_sub must be finite and positive");
    }
    if (!std::isfinite(decision.q_granted) || !std::isfinite(decision.v_granted) ||
        !std::isfinite(decision.q_repay) || !std::isfinite(decision.v_repay) ||
        !std::isfinite(decision.v_unmet) || decision.q_granted < 0.0 ||
        decision.v_granted < 0.0 || decision.q_repay < 0.0 || decision.v_repay < 0.0 ||
        decision.v_unmet < 0.0) {
        throw SwmmEngineError("exchange decision fields must be finite and non-negative");
    }
    if (!is_flow_volume_consistent(decision.q_granted, decision.v_granted, dt_sub) ||
        !is_flow_volume_consistent(decision.q_repay, decision.v_repay, dt_sub)) {
        throw SwmmEngineError("exchange decision flow and volume fields must be consistent with dt_sub");
    }
    engine.set_node_lateral_inflow(node_id, decision.q_granted + decision.q_repay);
}

void accept_swmm_shared_exchange_decision(
    ISwmmEngine& engine,
    const core::SharedExchangeDecision& decision,
    double dt_sub) {
    if (decision.endpoint.engine != core::SharedExchangeEngine::drainage) {
        throw SwmmEngineError("shared exchange decision must target drainage engine");
    }
    if (decision.endpoint.node_id > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw SwmmEngineError("shared exchange node_id is out of int range");
    }
    accept_swmm_exchange_decision(
        engine,
        static_cast<int>(decision.endpoint.node_id),
        decision.exchange,
        dt_sub);
}

core::EngineReport make_swmm_engine_report(const MockSwmmEngine& engine) {
    return core::EngineReport{
        .healthy = engine.initialized(),
        .engine_id = "SWMM",
        .error_code = engine.initialized() ? "" : "swmm_mock_not_initialized",
        .elapsed_time = engine.elapsed_time(),
    };
}

core::EngineReport make_swmm_engine_report(const SwmmEngineError& error) {
    return core::EngineReport{
        .healthy = false,
        .engine_id = error.engine_id(),
        .error_code = error.error_code(),
        .elapsed_time = 0.0,
    };
}

}  // namespace scau::coupling::drainage
