#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "mesh/mesh.hpp"

namespace scau::mesh {

enum class MeshQualitySeverity {
    review,
    fatal,
};

struct MeshQualityIssue {
    MeshQualitySeverity severity{MeshQualitySeverity::review};
    std::string entity_id;
    std::string code;
    double value{0.0};
};

struct MeshQualityConfig {
    double min_angle_degrees{10.0};
    double max_aspect_ratio{20.0};
};

struct MeshQualityReport {
    std::vector<MeshQualityIssue> issues;
    bool fatal{false};
    std::size_t reviewed_cells{0U};
};

// Reports quality without repairing topology. build_mesh owns fatal topological
// validation; this layer adds auditable geometric review thresholds.
[[nodiscard]] MeshQualityReport evaluate_mesh_quality(
    const Mesh& mesh,
    const MeshQualityConfig& config = {});

}  // namespace scau::mesh
