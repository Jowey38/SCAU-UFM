#include "mesh/quality.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace scau::mesh {

namespace {

constexpr double kPi = 3.14159265358979323846;

double distance(const Node& a, const Node& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

}  // namespace

MeshQualityReport evaluate_mesh_quality(
    const Mesh& mesh,
    const MeshQualityConfig& config) {
    if (!std::isfinite(config.min_angle_degrees) || config.min_angle_degrees <= 0.0
        || config.min_angle_degrees >= 90.0) {
        throw std::invalid_argument("min_angle_degrees must be in (0, 90)");
    }
    if (!std::isfinite(config.max_aspect_ratio) || config.max_aspect_ratio <= 1.0) {
        throw std::invalid_argument("max_aspect_ratio must be greater than 1");
    }

    MeshQualityReport report;
    const auto nodes = node_lookup(mesh.nodes);
    report.reviewed_cells = mesh.cells.size();
    for (const auto& cell : mesh.cells) {
        double min_length = std::numeric_limits<double>::infinity();
        double max_length = 0.0;
        double min_angle = 180.0;
        for (std::size_t i = 0; i < cell.node_ids.size(); ++i) {
            const auto& previous = nodes.at(
                cell.node_ids[(i + cell.node_ids.size() - 1U) % cell.node_ids.size()]);
            const auto& current = nodes.at(cell.node_ids[i]);
            const auto& next = nodes.at(cell.node_ids[(i + 1U) % cell.node_ids.size()]);
            const double a = distance(current, previous);
            const double b = distance(current, next);
            min_length = std::min({min_length, a, b});
            max_length = std::max({max_length, a, b});
            const double denominator = a * b;
            if (denominator <= 0.0) {
                report.issues.push_back({MeshQualitySeverity::fatal, cell.id, "zero_edge", 0.0});
                report.fatal = true;
                continue;
            }
            const double dot = (previous.x - current.x) * (next.x - current.x)
                + (previous.y - current.y) * (next.y - current.y);
            const double cosine = std::clamp(dot / denominator, -1.0, 1.0);
            min_angle = std::min(min_angle, std::acos(cosine) * 180.0 / kPi);
        }
        const double aspect_ratio = max_length / min_length;
        if (min_angle < config.min_angle_degrees) {
            report.issues.push_back(
                {MeshQualitySeverity::review, cell.id, "min_angle", min_angle});
        }
        if (aspect_ratio > config.max_aspect_ratio) {
            report.issues.push_back(
                {MeshQualitySeverity::review, cell.id, "aspect_ratio", aspect_ratio});
        }
    }
    return report;
}

}  // namespace scau::mesh
