from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from math import hypot
from typing import Iterable


class CellType(str, Enum):
    Triangle = "triangle"
    Quadrilateral = "quadrilateral"


@dataclass(frozen=True)
class Node:
    id: str
    x: float
    y: float


@dataclass(frozen=True)
class Cell:
    id: str
    cell_type: CellType
    node_ids: tuple[str, ...]

    @property
    def node_count(self) -> int:
        return len(self.node_ids)

    @property
    def edge_count(self) -> int:
        return len(self.node_ids)


@dataclass(frozen=True)
class Edge:
    id: str
    node_ids: tuple[str, str]
    left_cell: str | None
    right_cell: str | None
    normal: tuple[float, float]
    length: float
    midpoint: tuple[float, float]


@dataclass(frozen=True)
class EdgeSpec:
    id: str
    node_ids: tuple[str, str]
    left_cell: str | None
    right_cell: str | None


@dataclass(frozen=True)
class Mesh:
    nodes: tuple[Node, ...]
    cells: tuple[Cell, ...]
    edges: tuple[Edge, ...]


def build_mixed_minimal_mesh() -> Mesh:
    nodes = (
        Node("N0", 0.0, 0.0),
        Node("N1", 1.0, 0.0),
        Node("N2", 2.0, 0.0),
        Node("N3", 0.0, 1.0),
        Node("N4", 1.0, 1.0),
        Node("N5", 2.0, 1.0),
    )
    cells = (
        Cell("C0", CellType.Quadrilateral, ("N0", "N1", "N4", "N3")),
        Cell("C1", CellType.Triangle, ("N1", "N2", "N4")),
        Cell("C2", CellType.Triangle, ("N2", "N5", "N4")),
    )
    edge_specs = (
        EdgeSpec("E0", ("N0", "N1"), None, "C0"),
        EdgeSpec("E1", ("N1", "N4"), "C0", "C1"),
        EdgeSpec("E2", ("N1", "N2"), None, "C1"),
        EdgeSpec("E3", ("N2", "N4"), "C1", "C2"),
        EdgeSpec("E4", ("N4", "N3"), "C0", None),
        EdgeSpec("E5", ("N0", "N3"), None, "C0"),
        EdgeSpec("E6", ("N2", "N5"), None, "C2"),
        EdgeSpec("E7", ("N5", "N4"), "C2", None),
    )
    return build_mesh(nodes, cells, edge_specs)


def build_quad_only_control_mesh() -> Mesh:
    nodes = _control_nodes()
    cells = (
        Cell("C0", CellType.Quadrilateral, ("N0", "N1", "N4", "N3")),
        Cell("C1", CellType.Quadrilateral, ("N1", "N2", "N5", "N4")),
    )
    return build_mesh(nodes, cells)


def build_tri_only_control_mesh() -> Mesh:
    nodes = _control_nodes()
    cells = (
        Cell("C0", CellType.Triangle, ("N0", "N1", "N3")),
        Cell("C1", CellType.Triangle, ("N1", "N4", "N3")),
        Cell("C2", CellType.Triangle, ("N1", "N2", "N4")),
        Cell("C3", CellType.Triangle, ("N2", "N5", "N4")),
    )
    return build_mesh(nodes, cells)


def build_mesh(
    nodes: Iterable[Node],
    cells: Iterable[Cell],
    edge_specs: Iterable[EdgeSpec] | None = None,
) -> Mesh:
    node_tuple = tuple(nodes)
    cell_tuple = tuple(cells)
    node_lookup = {node.id: node for node in node_tuple}
    _validate_cells(cell_tuple, node_lookup)
    edges = _build_edges(cell_tuple, node_lookup, tuple(edge_specs) if edge_specs else None)
    return Mesh(node_tuple, cell_tuple, edges)


def cell_area(cell: Cell, nodes: dict[str, Node]) -> float:
    polygon = [nodes[node_id] for node_id in cell.node_ids]
    twice_area = 0.0
    for index in range(cell.node_count):
        current = polygon[index]
        following = polygon[(index + 1) % cell.node_count]
        twice_area += current.x * following.y - following.x * current.y
    return abs(twice_area) * 0.5


def cell_center(cell: Cell, nodes: dict[str, Node]) -> tuple[float, float]:
    polygon = [nodes[node_id] for node_id in cell.node_ids]
    signed_area_twice = 0.0
    centroid_x = 0.0
    centroid_y = 0.0
    for index in range(cell.node_count):
        current = polygon[index]
        following = polygon[(index + 1) % cell.node_count]
        cross = current.x * following.y - following.x * current.y
        signed_area_twice += cross
        centroid_x += (current.x + following.x) * cross
        centroid_y += (current.y + following.y) * cross
    if signed_area_twice == 0.0:
        raise ValueError(f"cell {cell.id} has zero area")
    return (centroid_x / (3.0 * signed_area_twice), centroid_y / (3.0 * signed_area_twice))


def to_reference_dict(mesh: Mesh) -> dict[str, object]:
    node_lookup = {node.id: node for node in mesh.nodes}
    return {
        "nodes": [node.__dict__ for node in mesh.nodes],
        "cells": [
            {
                "id": cell.id,
                "cell_type": cell.cell_type.value,
                "node_ids": list(cell.node_ids),
                "node_count": cell.node_count,
                "edge_count": cell.edge_count,
                "area": cell_area(cell, node_lookup),
                "center": list(cell_center(cell, node_lookup)),
            }
            for cell in mesh.cells
        ],
        "edges": [
            {
                "id": edge.id,
                "node_ids": list(edge.node_ids),
                "left_cell": edge.left_cell,
                "right_cell": edge.right_cell,
                "normal": list(edge.normal),
                "length": edge.length,
                "midpoint": list(edge.midpoint),
            }
            for edge in mesh.edges
        ],
    }


def _control_nodes() -> tuple[Node, ...]:
    return (
        Node("N0", 0.0, 0.0),
        Node("N1", 1.0, 0.0),
        Node("N2", 2.0, 0.0),
        Node("N3", 0.0, 1.0),
        Node("N4", 1.0, 1.0),
        Node("N5", 2.0, 1.0),
    )


def _validate_cells(cells: tuple[Cell, ...], nodes: dict[str, Node]) -> None:
    for cell in cells:
        if cell.cell_type is CellType.Triangle and cell.node_count != 3:
            raise ValueError(f"triangle {cell.id} must have 3 nodes")
        if cell.cell_type is CellType.Quadrilateral and cell.node_count != 4:
            raise ValueError(f"quadrilateral {cell.id} must have 4 nodes")
        if len(set(cell.node_ids)) != cell.node_count:
            raise ValueError(f"cell {cell.id} has duplicate nodes")
        for node_id in cell.node_ids:
            if node_id not in nodes:
                raise ValueError(f"cell {cell.id} references unknown node {node_id}")
        if cell_area(cell, nodes) == 0.0:
            raise ValueError(f"cell {cell.id} has zero area")


def _build_edges(
    cells: tuple[Cell, ...],
    nodes: dict[str, Node],
    edge_specs: tuple[EdgeSpec, ...] | None,
) -> tuple[Edge, ...]:
    edge_cells: dict[frozenset[str], list[tuple[str, tuple[str, str]]]] = {}
    cell_lookup = {cell.id: cell for cell in cells}
    for cell in cells:
        for index in range(cell.edge_count):
            start = cell.node_ids[index]
            end = cell.node_ids[(index + 1) % cell.edge_count]
            key = frozenset((start, end))
            edge_cells.setdefault(key, []).append((cell.id, (start, end)))

    specs = edge_specs if edge_specs is not None else _default_edge_specs(edge_cells)
    _validate_edge_specs(specs, edge_cells)
    edges: list[Edge] = []
    for spec in specs:
        start = nodes[spec.node_ids[0]]
        end = nodes[spec.node_ids[1]]
        dx = end.x - start.x
        dy = end.y - start.y
        length = hypot(dx, dy)
        if length == 0.0:
            raise ValueError(f"edge {spec.id} has zero length")
        midpoint = ((start.x + end.x) * 0.5, (start.y + end.y) * 0.5)
        normal = _oriented_normal(spec, cell_lookup, nodes, length, dx, dy, midpoint)
        edges.append(
            Edge(
                spec.id,
                spec.node_ids,
                spec.left_cell,
                spec.right_cell,
                normal,
                length,
                midpoint,
            )
        )
    return tuple(edges)


def _oriented_normal(
    spec: EdgeSpec,
    cells: dict[str, Cell],
    nodes: dict[str, Node],
    length: float,
    dx: float,
    dy: float,
    midpoint: tuple[float, float],
) -> tuple[float, float]:
    normal = (dy / length, -dx / length)
    if spec.left_cell is None:
        return _normal_toward_cell(spec.right_cell, cells, nodes, midpoint, normal)
    if spec.right_cell is None:
        return _normal_away_from_cell(spec.left_cell, cells, nodes, midpoint, normal)
    return _normal_from_cell_to_cell(spec.left_cell, spec.right_cell, cells, nodes, midpoint, normal)


def _normal_toward_cell(
    cell_id: str | None,
    cells: dict[str, Cell],
    nodes: dict[str, Node],
    midpoint: tuple[float, float],
    normal: tuple[float, float],
) -> tuple[float, float]:
    if cell_id is None:
        raise ValueError("boundary edge must reference one cell")
    if _cell_side(cell_id, cells, nodes, midpoint, normal) < 0.0:
        return (-normal[0], -normal[1])
    return normal


def _normal_away_from_cell(
    cell_id: str,
    cells: dict[str, Cell],
    nodes: dict[str, Node],
    midpoint: tuple[float, float],
    normal: tuple[float, float],
) -> tuple[float, float]:
    if _cell_side(cell_id, cells, nodes, midpoint, normal) > 0.0:
        return (-normal[0], -normal[1])
    return normal


def _normal_from_cell_to_cell(
    left_cell: str,
    right_cell: str,
    cells: dict[str, Cell],
    nodes: dict[str, Node],
    midpoint: tuple[float, float],
    normal: tuple[float, float],
) -> tuple[float, float]:
    if _cell_side(right_cell, cells, nodes, midpoint, normal) < 0.0:
        normal = (-normal[0], -normal[1])
    if _cell_side(left_cell, cells, nodes, midpoint, normal) > 0.0:
        raise ValueError("edge normal does not separate left and right cells")
    return normal


def _cell_side(
    cell_id: str,
    cells: dict[str, Cell],
    nodes: dict[str, Node],
    midpoint: tuple[float, float],
    normal: tuple[float, float],
) -> float:
    center = cell_center(cells[cell_id], nodes)
    return (center[0] - midpoint[0]) * normal[0] + (center[1] - midpoint[1]) * normal[1]


def _validate_edge_specs(
    specs: tuple[EdgeSpec, ...],
    edge_cells: dict[frozenset[str], list[tuple[str, tuple[str, str]]]],
) -> None:
    if any(len(spec.node_ids) != 2 for spec in specs):
        raise ValueError("edge specs must reference exactly two nodes")
    if len({spec.id for spec in specs}) != len(specs):
        raise ValueError("edge specs contain duplicate ids")
    if len({frozenset(spec.node_ids) for spec in specs}) != len(specs):
        raise ValueError("edge specs contain duplicate topological edges")
    if {frozenset(spec.node_ids) for spec in specs} != set(edge_cells):
        raise ValueError("edge specs do not cover the cell topology exactly")
    for spec in specs:
        key = frozenset(spec.node_ids)
        owners = edge_cells[key]
        if len(owners) > 2:
            raise ValueError(f"edge {spec.id} has more than two adjacent cells")
        owner_cells = {cell_id for cell_id, _ in owners}
        expected_cells = {cell for cell in (spec.left_cell, spec.right_cell) if cell is not None}
        if owner_cells != expected_cells:
            raise ValueError(f"edge {spec.id} adjacency does not match cell topology")
        if spec.left_cell is not None:
            left_orientations = [orientation for cell_id, orientation in owners if cell_id == spec.left_cell]
            if left_orientations != [spec.node_ids]:
                raise ValueError(f"edge {spec.id} direction must match the left cell orientation")


def _default_edge_specs(
    edge_cells: dict[frozenset[str], list[tuple[str, tuple[str, str]]]],
) -> tuple[EdgeSpec, ...]:
    specs: list[EdgeSpec] = []
    for index, (node_ids, owners) in enumerate(edge_cells.items()):
        if len(owners) > 2:
            raise ValueError(f"edge {sorted(node_ids)} has more than two adjacent cells")
        left_cell, oriented_node_ids = owners[0]
        right_cell = owners[1][0] if len(owners) == 2 else None
        specs.append(EdgeSpec(f"E{index}", oriented_node_ids, left_cell, right_cell))
    return tuple(specs)
