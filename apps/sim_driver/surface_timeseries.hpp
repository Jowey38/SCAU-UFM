#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "sim_driver.hpp"
#include "surface2d/state/state.hpp"

namespace scau::apps::sim_driver {

// Repo-convention content hash ("fnv1a64:<16 hex>") over raw file bytes.
[[nodiscard]] std::string hash_file_bytes(const std::filesystem::path& path);

// Output-only observer. The caller supplies accepted states at epoch boundaries.
// complete() publishes <path> plus a sidecar <path>.manifest.json binding the
// output bytes, the source STCF bytes and the final surface state hash.
class SurfaceTimeseries {
public:
    explicit SurfaceTimeseries(const RuntimeConfig& config);
    ~SurfaceTimeseries();
    SurfaceTimeseries(const SurfaceTimeseries&) = delete;
    SurfaceTimeseries& operator=(const SurfaceTimeseries&) = delete;
    void append(double time, const surface2d::SurfaceState& state);
    void complete(const surface2d::SurfaceState& final_state, std::size_t committed_epochs);
private:
    int file_{-1};
    int time_var_{-1};
    std::array<int, 5> vars_{};
    std::size_t cells_{0};
    std::size_t frames_{0};
    double previous_time_{0.0};
    double h_wet_{0.0};
    std::string source_stcf_;
    std::string source_stcf_hash_;
    std::filesystem::path output_;
    std::filesystem::path partial_;
};

}  // namespace scau::apps::sim_driver
