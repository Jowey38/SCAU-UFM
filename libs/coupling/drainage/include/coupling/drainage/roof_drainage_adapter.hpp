#pragma once

#include <unordered_map>

#include "coupling/drainage/swmm_boundary.hpp"
#include "surface2d/source_terms/runoff/result.hpp"

namespace scau::coupling::drainage {

class SwmmRoofDrainageAcceptanceAdapter final {
public:
    SwmmRoofDrainageAcceptanceAdapter(ISwmmEngine& engine, scau::core::Real dt_sub);

    [[nodiscard]] surface2d::RoofDrainageAcceptance operator()(const surface2d::RoofDrainageIntent& intent);

    void begin_step();

private:
    [[nodiscard]] surface2d::RoofDrainageAcceptance reject(
        const surface2d::RoofDrainageIntent& intent,
        surface2d::RoofDrainageRejectionReason reason) const;

    ISwmmEngine* engine_{nullptr};
    scau::core::Real dt_sub_{0.0};
    std::unordered_map<int, scau::core::Real> accumulated_node_flows_;
};

}  // namespace scau::coupling::drainage
