#include "stcf/topology.hpp"

#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include "core/error.hpp"

namespace scau::stcf {
namespace {

[[noreturn]] void fail(const char* stage, std::size_t index, const std::string& detail) {
    std::ostringstream message;
    message << "STCF topology validation failed at stage '" << stage
            << "', index " << index << ": " << detail;
    throw core::ScauError(message.str());
}

bool valid_index(int index, std::size_t size) {
    return index >= 0 && static_cast<std::size_t>(index) < size;
}

std::pair<int, int> undirected_edge(int first, int second) {
    return first < second ? std::pair{first, second} : std::pair{second, first};
}

}  // namespace

std::size_t face_node_count(const std::array<int, kMaxFaceNodes>& nodes) {
    std::size_t count = 0;
    bool saw_fill = false;
    for (const int node : nodes) {
        if (node == kConnectivityFillValue) {
            saw_fill = true;
            continue;
        }
        if (saw_fill) {
            throw core::ScauError(
                "STCF topology validation failed: face nodes must precede fill values");
        }
        ++count;
    }
    return count;
}

void validate_stcf_case(const StcfCase& stcf_case, const StcfValidationConfig& config) {
    const auto& topology = stcf_case.topology;
    const std::size_t node_count = topology.node_x.size();
    const std::size_t face_count = topology.face_nodes.size();
    const std::size_t edge_count = topology.edge_nodes.size();

    if (node_count == 0U || face_count == 0U || edge_count == 0U) {
        fail("shape", 0, "node, face, and edge counts must be positive");
    }
    if (topology.node_y.size() != node_count) {
        fail("shape", 0, "node_x and node_y counts must match");
    }
    if (topology.edge_faces.size() != edge_count) {
        fail("shape", 0, "edge_faces count must match edge_nodes count");
    }
    if (topology.face_edges.size() != face_count) {
        fail("shape", 0, "face_edges count must match face_nodes count");
    }

    validate_stcf_dataset(stcf_case.fields, face_count, edge_count, config);

    for (std::size_t node = 0; node < node_count; ++node) {
        if (!std::isfinite(topology.node_x[node]) || !std::isfinite(topology.node_y[node])) {
            fail("nodes", node, "coordinates must be finite");
        }
    }

    std::vector<std::size_t> face_sizes(face_count);
    for (std::size_t face = 0; face < face_count; ++face) {
        const auto& nodes = topology.face_nodes[face];
        const std::size_t count = face_node_count(nodes);
        if (count != 3U && count != 4U) {
            fail("face_nodes", face, "face must contain exactly 3 or 4 nodes");
        }
        face_sizes[face] = count;
        std::set<int> distinct;
        for (std::size_t slot = 0; slot < count; ++slot) {
            if (!valid_index(nodes[slot], node_count)) {
                fail("face_nodes", face, "node index is out of range");
            }
            if (!distinct.insert(nodes[slot]).second) {
                fail("face_nodes", face, "face contains a duplicate node");
            }
        }
    }

    std::set<std::pair<int, int>> distinct_topology_edges;
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
        const auto& nodes = topology.edge_nodes[edge];
        if (!valid_index(nodes[0], node_count) || !valid_index(nodes[1], node_count)) {
            fail("edge_nodes", edge, "node index is out of range");
        }
        if (nodes[0] == nodes[1]) {
            fail("edge_nodes", edge, "edge endpoints must be distinct");
        }
        if (!distinct_topology_edges.insert(undirected_edge(nodes[0], nodes[1])).second) {
            fail("edge_nodes", edge, "topology contains a duplicate edge");
        }

        const auto& faces = topology.edge_faces[edge];
        if (!valid_index(faces[0], face_count)) {
            fail("edge_faces", edge, "first adjacent face must be valid");
        }
        if (faces[1] != kConnectivityFillValue && !valid_index(faces[1], face_count)) {
            fail("edge_faces", edge, "second adjacent face is out of range");
        }
        if (faces[1] == faces[0]) {
            fail("edge_faces", edge, "adjacent faces must be distinct");
        }
    }

    std::vector<std::size_t> edge_reference_counts(edge_count, 0U);
    for (std::size_t face = 0; face < face_count; ++face) {
        const auto& edges = topology.face_edges[face];
        const auto& nodes = topology.face_nodes[face];
        const std::size_t count = face_sizes[face];
        std::set<int> distinct_edges;
        for (std::size_t slot = 0; slot < kMaxFaceNodes; ++slot) {
            const int edge_index = edges[slot];
            if (slot >= count) {
                if (edge_index != kConnectivityFillValue) {
                    fail("face_edges", face, "unused edge slots must use fill value");
                }
                continue;
            }
            if (!valid_index(edge_index, edge_count)) {
                fail("face_edges", face, "edge index is out of range");
            }
            if (!distinct_edges.insert(edge_index).second) {
                fail("face_edges", face, "face contains a duplicate edge");
            }
            ++edge_reference_counts[static_cast<std::size_t>(edge_index)];
            const auto& adjacent_faces = topology.edge_faces[static_cast<std::size_t>(edge_index)];
            if (adjacent_faces[0] != static_cast<int>(face)
                && adjacent_faces[1] != static_cast<int>(face)) {
                fail("cross_consistency", face, "face edge does not reference the face");
            }
            const auto expected = undirected_edge(nodes[slot], nodes[(slot + 1U) % count]);
            const auto actual_nodes = topology.edge_nodes[static_cast<std::size_t>(edge_index)];
            const auto actual = undirected_edge(actual_nodes[0], actual_nodes[1]);
            if (actual != expected) {
                fail("cross_consistency", face, "face edge endpoints do not match the face ring");
            }
        }
    }

    for (std::size_t edge = 0; edge < edge_count; ++edge) {
        const std::size_t expected_references =
            topology.edge_faces[edge][1] == kConnectivityFillValue ? 1U : 2U;
        if (edge_reference_counts[edge] != expected_references) {
            fail(
                "cross_consistency",
                edge,
                "edge reference count does not match edge-face adjacency count");
        }
    }
}

}  // namespace scau::stcf
