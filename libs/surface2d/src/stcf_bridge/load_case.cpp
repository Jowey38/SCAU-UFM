#include "surface2d/stcf_bridge/load_case.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "stcf/io_netcdf.hpp"
#include "surface2d/stcf_bridge/assemble.hpp"

namespace scau::surface2d {
namespace {

std::string node_id(int index) {
    return "N" + std::to_string(index);
}

std::string cell_id(int index) {
    return "C" + std::to_string(index);
}

std::string edge_id(int index) {
    return "E" + std::to_string(index);
}

core::Real signed_area_twice(
    const std::vector<int>& face_nodes,
    const stcf::MeshTopology& topology) {
    core::Real result = 0.0;
    for (std::size_t index = 0; index < face_nodes.size(); ++index) {
        const std::size_t current = static_cast<std::size_t>(face_nodes[index]);
        const std::size_t next = static_cast<std::size_t>(
            face_nodes[(index + 1U) % face_nodes.size()]);
        result += topology.node_x[current] * topology.node_y[next]
            - topology.node_x[next] * topology.node_y[current];
    }
    return result;
}

std::vector<std::vector<int>> normalized_face_nodes(const stcf::StcfCase& stcf_case) {
    std::vector<std::vector<int>> result;
    result.reserve(stcf_case.topology.face_nodes.size());
    for (std::size_t face = 0; face < stcf_case.topology.face_nodes.size(); ++face) {
        const auto& stored = stcf_case.topology.face_nodes[face];
        const std::size_t count = stcf::face_node_count(stored);
        std::vector<int> nodes(stored.begin(), stored.begin() + static_cast<std::ptrdiff_t>(count));
        const core::Real area_twice = signed_area_twice(nodes, stcf_case.topology);
        if (area_twice == 0.0) {
            throw std::invalid_argument(
                "STCF face " + std::to_string(face) + " has zero signed area");
        }
        if (area_twice < 0.0) {
            std::reverse(nodes.begin(), nodes.end());
        }
        result.push_back(std::move(nodes));
    }
    return result;
}

bool face_has_directed_edge(
    const std::vector<int>& face_nodes,
    int first,
    int second) {
    for (std::size_t index = 0; index < face_nodes.size(); ++index) {
        if (face_nodes[index] == first
            && face_nodes[(index + 1U) % face_nodes.size()] == second) {
            return true;
        }
    }
    return false;
}

}  // namespace

LoadedSurface2DCase load_surface2d_case(
    const stcf::StcfCase& stcf_case,
    const stcf::StcfValidationConfig& config) {
    stcf::validate_stcf_case(stcf_case, config);
    const auto normalized_faces = normalized_face_nodes(stcf_case);

    std::vector<mesh::Node> nodes;
    nodes.reserve(stcf_case.topology.node_x.size());
    for (std::size_t index = 0; index < stcf_case.topology.node_x.size(); ++index) {
        nodes.push_back(mesh::Node{
            .id = node_id(static_cast<int>(index)),
            .x = stcf_case.topology.node_x[index],
            .y = stcf_case.topology.node_y[index],
        });
    }

    std::vector<mesh::Cell> cells;
    cells.reserve(normalized_faces.size());
    for (std::size_t face = 0; face < normalized_faces.size(); ++face) {
        const auto& face_nodes = normalized_faces[face];
        std::vector<std::string> ids;
        ids.reserve(face_nodes.size());
        for (const int index : face_nodes) {
            ids.push_back(node_id(index));
        }
        cells.push_back(mesh::Cell{
            .id = cell_id(static_cast<int>(face)),
            .cell_type = face_nodes.size() == 3U
                ? mesh::CellType::Triangle
                : mesh::CellType::Quadrilateral,
            .node_ids = std::move(ids),
        });
    }

    std::vector<mesh::EdgeSpec> edge_specs;
    edge_specs.reserve(stcf_case.topology.edge_nodes.size());
    for (std::size_t edge = 0; edge < stcf_case.topology.edge_nodes.size(); ++edge) {
        const auto stored_nodes = stcf_case.topology.edge_nodes[edge];
        const auto adjacent_faces = stcf_case.topology.edge_faces[edge];
        const int left_face = adjacent_faces[0];
        int first = stored_nodes[0];
        int second = stored_nodes[1];
        const auto& left_ring = normalized_faces[static_cast<std::size_t>(left_face)];
        if (!face_has_directed_edge(left_ring, first, second)) {
            if (!face_has_directed_edge(left_ring, second, first)) {
                throw std::invalid_argument(
                    "STCF edge " + std::to_string(edge)
                    + " endpoints are not consecutive in its left face");
            }
            std::swap(first, second);
        }
        edge_specs.push_back(mesh::EdgeSpec{
            .id = edge_id(static_cast<int>(edge)),
            .node_ids = {node_id(first), node_id(second)},
            .left_cell = cell_id(left_face),
            .right_cell = adjacent_faces[1] == stcf::kConnectivityFillValue
                ? std::nullopt
                : std::optional<std::string>{cell_id(adjacent_faces[1])},
        });
    }

    mesh::Mesh loaded_mesh = mesh::build_mesh(
        std::move(nodes), std::move(cells), std::move(edge_specs));
    if (loaded_mesh.edges.size() != stcf_case.topology.edge_nodes.size()) {
        throw std::runtime_error("loaded mesh edge count changed from STCF topology");
    }
    for (std::size_t edge = 0; edge < loaded_mesh.edges.size(); ++edge) {
        if (loaded_mesh.edges[edge].id != edge_id(static_cast<int>(edge))) {
            throw std::runtime_error("loaded mesh edge order changed from STCF topology");
        }
    }

    LoadedSurface2DCase result;
    result.mesh = std::move(loaded_mesh);
    result.dpm_fields = assemble_dpm_fields(result.mesh, stcf_case.fields);
    result.source_fields = assemble_source_term_fields(result.mesh, stcf_case.fields);
    result.bed_elevations = assemble_bed_elevations(result.mesh, stcf_case.fields);
    return result;
}

LoadedSurface2DCase load_surface2d_case(
    const std::filesystem::path& path,
    const stcf::StcfValidationConfig& config) {
    return load_surface2d_case(stcf::read_stcf_case(path, config), config);
}

}  // namespace scau::surface2d
