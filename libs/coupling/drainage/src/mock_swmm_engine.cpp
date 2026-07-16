#include "coupling/drainage/mock_swmm_engine.hpp"

#include <cmath>

namespace scau::coupling::drainage {
namespace {

void validate_finite(scau::core::Real value, const char* name) {
    if (!std::isfinite(value)) {
        throw SwmmEngineError(name);
    }
}

}  // namespace

MockSwmmEngine::MockSwmmEngine(std::size_t node_count, std::size_t link_count)
    : node_heads_(node_count, 0.0),
      node_lateral_inflows_(node_count, 0.0),
      node_surcharged_(node_count, false),
      link_flows_(link_count, 0.0) {}

void MockSwmmEngine::initialize(const std::string& /*inp_path*/) {
    initialized_ = true;
    elapsed_time_ = 0.0;
}

void MockSwmmEngine::step(scau::core::Real dt_swmm) {
    if (!std::isfinite(dt_swmm) || dt_swmm <= 0.0) {
        throw SwmmEngineError("SWMM step dt must be finite and positive");
    }
    elapsed_time_ += dt_swmm;
}

void MockSwmmEngine::finalize() {
    initialized_ = false;
    elapsed_time_ = 0.0;
    for (auto& q : node_lateral_inflows_) {
        q = 0.0;
    }
}

scau::core::Real MockSwmmEngine::get_node_head(std::size_t node_id) const {
    validate_node(node_id);
    return node_heads_[node_id];
}

scau::core::Real MockSwmmEngine::get_node_lateral_inflow(std::size_t node_id) const {
    validate_node(node_id);
    return node_lateral_inflows_[node_id];
}

void MockSwmmEngine::set_node_lateral_inflow(std::size_t node_id, scau::core::Real q) {
    validate_node(node_id);
    validate_finite(q, "SWMM lateral inflow must be finite");
    node_lateral_inflows_[node_id] = q;
}

scau::core::Real MockSwmmEngine::get_link_flow(std::size_t link_id) const {
    validate_link(link_id);
    return link_flows_[link_id];
}

bool MockSwmmEngine::is_surcharged(std::size_t node_id) const {
    validate_node(node_id);
    return node_surcharged_[node_id];
}

void MockSwmmEngine::set_node_head(std::size_t node_id, scau::core::Real head) {
    validate_node(node_id);
    validate_finite(head, "SWMM node head must be finite");
    node_heads_[node_id] = head;
}

void MockSwmmEngine::set_surcharged(std::size_t node_id, bool surcharged) {
    validate_node(node_id);
    node_surcharged_[node_id] = surcharged;
}

void MockSwmmEngine::set_link_flow(std::size_t link_id, scau::core::Real flow) {
    validate_link(link_id);
    validate_finite(flow, "SWMM link flow must be finite");
    link_flows_[link_id] = flow;
}

scau::core::Real MockSwmmEngine::elapsed_time() const {
    return elapsed_time_;
}

bool MockSwmmEngine::initialized() const {
    return initialized_;
}

void MockSwmmEngine::validate_node(std::size_t node_id) const {
    if (node_id >= node_heads_.size()) {
        throw SwmmEngineError("invalid SWMM node id");
    }
}

void MockSwmmEngine::validate_link(std::size_t link_id) const {
    if (link_id >= link_flows_.size()) {
        throw SwmmEngineError("invalid SWMM link id");
    }
}

}  // namespace scau::coupling::drainage
