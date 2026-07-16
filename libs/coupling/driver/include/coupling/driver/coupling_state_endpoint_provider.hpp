#pragma once

#include <optional>
#include <unordered_map>

#include "coupling/core/state.hpp"
#include "coupling/driver/roof_exchange_gate.hpp"

namespace scau::coupling::driver {

// Static PreProc-style mapping: roof source cell index -> exchange cell index
// inside the CouplingState. Roof cells absent from the map have no coupling
// endpoint; the gate fail-closes them (HealthBlocked).
using RoofEndpointMap = std::unordered_map<int, std::size_t>;

// Resolves roof drainage intents against LIVE CouplingState cells, so
// arbitration always sees the current post-replay h/phi_t/area and
// mass_deficit_account instead of a caller-frozen copy.
//
// Read-only observer: holds a reference to the state (caller guarantees the
// state outlives the provider) and never mutates it. Satisfies
// ExchangeEndpointStateFn.
class CouplingStateEndpointProvider final {
public:
    CouplingStateEndpointProvider(const core::CouplingState& state, RoofEndpointMap map);

    [[nodiscard]] std::optional<core::ExchangeCellState> operator()(
        const surface2d::RoofDrainageIntent& intent) const;

private:
    const core::CouplingState* state_{nullptr};
    RoofEndpointMap map_;
};

}  // namespace scau::coupling::driver
