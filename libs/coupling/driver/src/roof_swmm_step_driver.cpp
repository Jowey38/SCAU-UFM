#include "coupling/driver/roof_swmm_step_driver.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace scau::coupling::driver {

RoofSwmmStepDriver::RoofSwmmStepDriver(
    drainage::ISwmmEngine& engine,
    ExchangeEndpointStateFn endpoint_state_fn,
    Real dt_sub)
    : engine_(&engine),
      adapter_(engine, dt_sub),
      gate_(
          std::move(endpoint_state_fn),
          [this](const surface2d::RoofDrainageIntent& intent) { return adapter_(intent); },
          dt_sub),
      dt_sub_(dt_sub) {
    // dt_sub validity is enforced by the owned adapter and gate constructors.
}

void RoofSwmmStepDriver::begin_substep() {
    adapter_.begin_step();
    gate_.begin_substep();
    ++substep_count_;
}

surface2d::RoofDrainageAcceptanceFn RoofSwmmStepDriver::acceptance_fn() {
    return [this](const surface2d::RoofDrainageIntent& intent) { return gate_(intent); };
}

void RoofSwmmStepDriver::advance_engine() {
    engine_->step(dt_sub_);
}

void RoofSwmmStepDriver::rollback_substep() {
    adapter_.rollback_step();
    gate_.begin_substep();
}

Real RoofSwmmStepDriver::dt_sub() const noexcept {
    return dt_sub_;
}

std::size_t RoofSwmmStepDriver::substep_count() const noexcept {
    return substep_count_;
}

}  // namespace scau::coupling::driver
