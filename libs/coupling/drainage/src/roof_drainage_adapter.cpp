#include "coupling/drainage/roof_drainage_adapter.hpp"

#include <cmath>
#include <limits>

namespace scau::coupling::drainage {

SwmmRoofDrainageAcceptanceAdapter::SwmmRoofDrainageAcceptanceAdapter(ISwmmEngine& engine, scau::core::Real dt_sub)
    : engine_(&engine), dt_sub_(dt_sub) {
    if (!std::isfinite(dt_sub_) || dt_sub_ <= 0.0) {
        throw SwmmEngineError("roof drainage adapter dt_sub must be finite and positive");
    }
}

surface2d::RoofDrainageAcceptance SwmmRoofDrainageAcceptanceAdapter::operator()(
    const surface2d::RoofDrainageIntent& intent) {
    if (intent.target_swmm_node_index < 0) {
        return reject(intent, surface2d::RoofDrainageRejectionReason::InvalidTargetNode);
    }
    if (!std::isfinite(intent.requested_volume) || intent.requested_volume < 0.0) {
        return reject(intent, surface2d::RoofDrainageRejectionReason::EngineUnavailable);
    }

    const int node_id = intent.target_swmm_node_index;
    try {
        if (engine_->is_surcharged(node_id)) {
            return reject(intent, surface2d::RoofDrainageRejectionReason::NodeSurcharged);
        }
    } catch (const SwmmEngineError&) {
        return reject(intent, surface2d::RoofDrainageRejectionReason::InvalidTargetNode);
    }

    const scau::core::Real q_increment = intent.requested_volume / dt_sub_;
    const scau::core::Real q_total = accumulated_node_flows_[node_id] + q_increment;
    if (!std::isfinite(q_total)) {
        return reject(intent, surface2d::RoofDrainageRejectionReason::EngineUnavailable);
    }

    try {
        engine_->set_node_lateral_inflow(node_id, q_total);
    } catch (const SwmmEngineError&) {
        return reject(intent, surface2d::RoofDrainageRejectionReason::EngineUnavailable);
    }
    accumulated_node_flows_[node_id] = q_total;

    surface2d::RoofDrainageAcceptance acceptance;
    acceptance.requested_volume = intent.requested_volume;
    acceptance.accepted_volume = intent.requested_volume;
    acceptance.rejected_volume = 0.0;
    acceptance.rejection_reason = surface2d::RoofDrainageRejectionReason::None;
    return acceptance;
}

void SwmmRoofDrainageAcceptanceAdapter::begin_step() {
    accumulated_node_flows_.clear();
}

surface2d::RoofDrainageAcceptance SwmmRoofDrainageAcceptanceAdapter::reject(
    const surface2d::RoofDrainageIntent& intent,
    surface2d::RoofDrainageRejectionReason reason) const {
    surface2d::RoofDrainageAcceptance acceptance;
    const scau::core::Real requested_volume = std::isfinite(intent.requested_volume)
        ? std::abs(intent.requested_volume)
        : 0.0;
    acceptance.requested_volume = requested_volume;
    acceptance.accepted_volume = 0.0;
    acceptance.rejected_volume = requested_volume;
    acceptance.rejection_reason = reason;
    return acceptance;
}

}  // namespace scau::coupling::drainage
