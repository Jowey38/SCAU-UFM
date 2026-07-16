#include "coupling/driver/coupling_state_endpoint_provider.hpp"

#include <utility>

namespace scau::coupling::driver {

CouplingStateEndpointProvider::CouplingStateEndpointProvider(
    const core::CouplingState& state,
    RoofEndpointMap map)
    : state_(&state), map_(std::move(map)) {}

std::optional<core::ExchangeCellState> CouplingStateEndpointProvider::operator()(
    const surface2d::RoofDrainageIntent& intent) const {
    const auto mapping = map_.find(intent.source_cell_index);
    if (mapping == map_.end()) {
        return std::nullopt;
    }
    const auto& cells = state_->cells();
    if (mapping->second >= cells.size()) {
        return std::nullopt;
    }
    return cells[mapping->second];
}

}  // namespace scau::coupling::driver
