#pragma once

#include <array>
#include <cstddef>
#include <filesystem>

#include "sim_driver.hpp"
#include "surface2d/state/state.hpp"

namespace scau::apps::sim_driver {

// Output-only observer. The caller supplies accepted states at epoch boundaries.
class SurfaceTimeseries {
public:
    explicit SurfaceTimeseries(const RuntimeConfig& config);
    ~SurfaceTimeseries();
    SurfaceTimeseries(const SurfaceTimeseries&) = delete;
    SurfaceTimeseries& operator=(const SurfaceTimeseries&) = delete;
    void append(double time, const surface2d::SurfaceState& state);
    void complete();
private:
    int file_{-1};
    int time_var_{-1};
    std::array<int, 5> vars_{};
    std::size_t cells_{0};
    std::size_t frames_{0};
    double previous_time_{0.0};
    double h_wet_{0.0};
    std::filesystem::path output_;
    std::filesystem::path partial_;
};

}  // namespace scau::apps::sim_driver
