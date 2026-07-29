#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "stcf/schema.hpp"
#include "stcf/validate.hpp"

namespace scau::stcf {

inline constexpr int kConnectivityFillValue = -1;
inline constexpr std::size_t kMaxFaceNodes = 4;

struct MeshTopology {
    std::vector<core::Real> node_x;
    std::vector<core::Real> node_y;
    std::vector<std::array<int, kMaxFaceNodes>> face_nodes;
    std::vector<std::array<int, 2>> edge_nodes;
    std::vector<std::array<int, 2>> edge_faces;
    std::vector<std::array<int, kMaxFaceNodes>> face_edges;
};

struct StcfCase {
    MeshTopology topology;
    StcfDataset fields;
};

[[nodiscard]] std::size_t face_node_count(
    const std::array<int, kMaxFaceNodes>& face_nodes);

// Validates a self-contained mixed triangle/quadrilateral UGRID topology and
// its one-to-one alignment with STCF face/edge fields. Throws core::ScauError
// naming the failed stage and index.
void validate_stcf_case(
    const StcfCase& stcf_case,
    const StcfValidationConfig& config = {});

}  // namespace scau::stcf
