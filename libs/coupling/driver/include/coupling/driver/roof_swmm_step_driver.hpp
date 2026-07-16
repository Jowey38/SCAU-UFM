#pragma once

#include <cstddef>

#include "coupling/drainage/roof_drainage_adapter.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/driver/roof_exchange_gate.hpp"

namespace scau::coupling::driver {

// dt_sub scheduling seam for the roof -> SWMM path. Owns the acceptance
// adapter and the arbitration gate; sees only ISwmmEngine and DTOs
// (project-layout-design: the driver never touches engine native objects).
//
// Per surface substep:
//   driver.begin_substep();                 // reset per-substep ledgers
//   auto accept = driver.acceptance_fn();   // pass to advance_one_step_cpu
//   ... surface2d emits roof intents through `accept` ...
//   driver.advance_engine();                // SWMM advances by the same dt_sub
class RoofSwmmStepDriver final {
public:
    RoofSwmmStepDriver(
        drainage::ISwmmEngine& engine,
        ExchangeEndpointStateFn endpoint_state_fn,
        Real dt_sub);

    void begin_substep();
    [[nodiscard]] surface2d::RoofDrainageAcceptanceFn acceptance_fn();
    void advance_engine();

    [[nodiscard]] Real dt_sub() const noexcept;
    [[nodiscard]] std::size_t substep_count() const noexcept;

private:
    drainage::ISwmmEngine* engine_{nullptr};
    drainage::SwmmRoofDrainageAcceptanceAdapter adapter_;
    RoofExchangeGate gate_;
    Real dt_sub_{0.0};
    std::size_t substep_count_{0U};
};

}  // namespace scau::coupling::driver
