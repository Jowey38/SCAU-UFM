"""Mesh-control contract for scau_preproc (M287-B4).

A mesh-controls GeoJSON (schema v1) carries the operator-drawn constraints of
the RAS-Mapper-style mesh workbench:

  control_kind = "breakline"          LineString; embedded in the surface so
                                      that every segment is preserved as a
                                      chain of mesh edges; optional size_m +
                                      dist_max_m add a linear Threshold size
                                      field around it.
  control_kind = "refinement_region"  Polygon; becomes its own gmsh plane
                                      sub-surface with a Constant size field
                                      (size_m inside), so its ring is also
                                      preserved as mesh edges.

This module holds the pure-geometry side (loading, fail-closed validation,
breakline-preservation assertion, per-cell diagnostics). It imports no gmsh
and never repairs anything: every violation is reported with the offending
control_id and the generator aborts. Certification of the produced case still
belongs exclusively to the authoritative C++ validator.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

MESH_CONTROLS_SCHEMA_VERSION = 1
CONTROL_KINDS = ("breakline", "refinement_region")


class MeshControlError(ValueError):
    """Fail-closed rejection; `violations` lists dicts with control_id/violation."""

    def __init__(self, violations: list[dict]) -> None:
        self.violations = violations
        ids = ", ".join(f"{v['control_id']}:{v['violation']}" for v in violations)
        super().__init__(f"mesh controls rejected fail-closed ({ids})")


# --- geometry helpers ---------------------------------------------------------


def _orient(a, b, c) -> int:
    v = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    return 0 if v == 0 else (1 if v > 0 else -1)


def segments_properly_intersect(p1, p2, p3, p4) -> bool:
    o1, o2 = _orient(p1, p2, p3), _orient(p1, p2, p4)
    o3, o4 = _orient(p3, p4, p1), _orient(p3, p4, p2)
    return o1 != o2 and o3 != o4 and 0 not in (o1, o2, o3, o4)


def point_in_ring(x: float, y: float, ring: list[tuple[float, float]]) -> bool:
    inside = False
    for i in range(len(ring) - 1):
        x1, y1 = ring[i]
        x2, y2 = ring[i + 1]
        if (y1 > y) != (y2 > y):
            x_cross = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x_cross > x:
                inside = not inside
    return inside


def point_on_ring(x: float, y: float, ring: list[tuple[float, float]], tol: float) -> bool:
    return any(
        point_segment_distance((x, y), ring[i], ring[i + 1]) <= tol
        for i in range(len(ring) - 1)
    )


def point_segment_distance(p, a, b) -> float:
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length2 = dx * dx + dy * dy
    if length2 == 0.0:
        return math.hypot(p[0] - ax, p[1] - ay)
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / length2))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def polyline_self_intersects(points: list[tuple[float, float]], closed: bool) -> bool:
    n = len(points) - 1
    for i in range(n):
        for j in range(i + 2, n):
            if closed and i == 0 and j == n - 1:
                continue
            if segments_properly_intersect(points[i], points[i + 1], points[j], points[j + 1]):
                return True
    return False


def polyline_crosses_ring(points, ring) -> bool:
    return any(
        segments_properly_intersect(points[i], points[i + 1], ring[j], ring[j + 1])
        for i in range(len(points) - 1)
        for j in range(len(ring) - 1)
    )


# --- loading ------------------------------------------------------------------


def load_mesh_controls(path: Path, default_size_m: float | None,
                       default_dist_max_m: float | None) -> list[dict]:
    """Parses the controls GeoJSON into normalized dicts (no geometry checks)."""
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    violations: list[dict] = []
    if data.get("mesh_controls_schema_version") != MESH_CONTROLS_SCHEMA_VERSION:
        violations.append({
            "control_id": "<file>",
            "violation": "unsupported_mesh_controls_schema_version",
            "detail": f"expected {MESH_CONTROLS_SCHEMA_VERSION}, "
                      f"got {data.get('mesh_controls_schema_version')!r}",
        })
        raise MeshControlError(violations)

    controls: list[dict] = []
    seen: set[str] = set()
    for index, feature in enumerate(data.get("features", [])):
        properties = feature.get("properties") or {}
        geometry = feature.get("geometry") or {}
        control_id = str(properties.get("control_id") or f"control_{index}")
        kind = properties.get("control_kind")
        if control_id in seen:
            violations.append({"control_id": control_id, "violation": "duplicate_control_id"})
            continue
        seen.add(control_id)
        if kind not in CONTROL_KINDS:
            violations.append({"control_id": control_id, "violation": "unknown_control_kind",
                               "detail": repr(kind)})
            continue

        size_m = properties.get("size_m", default_size_m)
        dist_max_m = properties.get("dist_max_m", default_dist_max_m)
        try:
            size_m = None if size_m is None else float(size_m)
            dist_max_m = None if dist_max_m is None else float(dist_max_m)
        except (TypeError, ValueError):
            violations.append({"control_id": control_id, "violation": "non_numeric_size"})
            continue

        expected_geometry = "LineString" if kind == "breakline" else "Polygon"
        if geometry.get("type") != expected_geometry:
            violations.append({"control_id": control_id, "violation": "geometry_type_mismatch",
                               "detail": f"{kind} requires {expected_geometry}, "
                                         f"got {geometry.get('type')!r}"})
            continue
        raw = geometry["coordinates"]
        points = [tuple(map(float, xy[:2])) for xy in (raw[0] if kind == "refinement_region" else raw)]
        controls.append({
            "control_id": control_id,
            "control_kind": kind,
            "points": points,
            "size_m": size_m,
            "dist_max_m": dist_max_m,
            "properties": properties,
        })
    if violations:
        raise MeshControlError(violations)
    return controls


# --- validation (reject only, never repair) -----------------------------------


def validate_mesh_controls(controls: list[dict], boundary_ring, hole_rings: list[list],
                           lc: float, tol: float = 1.0e-9) -> dict:
    """Checks every control against the meshing domain.

    Returns {"hole_region": [region index or None per hole],
             "breakline_region": [region index or None per breakline]} so the
    generator can attach each hole / breakline to the surface that owns it.
    Raises MeshControlError listing every violation found.
    """
    violations: list[dict] = []
    regions = [c for c in controls if c["control_kind"] == "refinement_region"]
    breaklines = [c for c in controls if c["control_kind"] == "breakline"]

    def strictly_inside_domain(points, control_id) -> bool:
        ok = True
        for x, y in points:
            if not point_in_ring(x, y, boundary_ring) or point_on_ring(x, y, boundary_ring, tol):
                violations.append({"control_id": control_id,
                                   "violation": "vertex_not_strictly_inside_boundary",
                                   "detail": f"({x}, {y})"})
                ok = False
                break
            for hole in hole_rings:
                if point_in_ring(x, y, hole) or point_on_ring(x, y, hole, tol):
                    violations.append({"control_id": control_id,
                                       "violation": "vertex_inside_or_on_building_hole",
                                       "detail": f"({x}, {y})"})
                    ok = False
                    break
            if not ok:
                break
        return ok

    def crosses_domain_rings(points, control_id) -> None:
        if polyline_crosses_ring(points, boundary_ring):
            violations.append({"control_id": control_id, "violation": "crosses_boundary"})
        for hole_index, hole in enumerate(hole_rings):
            if polyline_crosses_ring(points, hole):
                violations.append({"control_id": control_id, "violation": "crosses_building_hole",
                                   "detail": f"hole {hole_index}"})

    for region in regions:
        ring = region["points"]
        cid = region["control_id"]
        if len(ring) < 4 or ring[0] != ring[-1]:
            violations.append({"control_id": cid, "violation": "unclosed_ring"})
            continue
        if polyline_self_intersects(ring, closed=True):
            violations.append({"control_id": cid, "violation": "self_intersection"})
            continue
        if region["size_m"] is None or not (0.0 < region["size_m"] <= lc):
            violations.append({"control_id": cid, "violation": "size_m_out_of_range",
                               "detail": f"size_m={region['size_m']} must be in (0, lc={lc}]"})
        if strictly_inside_domain(ring[:-1], cid):
            crosses_domain_rings(ring, cid)

    for i, a in enumerate(regions):
        for b in regions[i + 1:]:
            if (polyline_crosses_ring(a["points"], b["points"])
                    or any(point_in_ring(x, y, b["points"]) for x, y in a["points"][:-1])
                    or any(point_in_ring(x, y, a["points"]) for x, y in b["points"][:-1])):
                violations.append({"control_id": a["control_id"],
                                   "violation": "refinement_regions_overlap",
                                   "detail": b["control_id"]})

    hole_region: list[int | None] = []
    for hole_index, hole in enumerate(hole_rings):
        owner = None
        for region_index, region in enumerate(regions):
            inside = [point_in_ring(x, y, region["points"]) for x, y in hole[:-1]]
            if all(inside) and not polyline_crosses_ring(hole, region["points"]):
                owner = region_index
            elif any(inside) or polyline_crosses_ring(hole, region["points"]):
                violations.append({"control_id": region["control_id"],
                                   "violation": "building_straddles_region_boundary",
                                   "detail": f"hole {hole_index}"})
        hole_region.append(owner)

    breakline_region: list[int | None] = []
    for line in breaklines:
        points = line["points"]
        cid = line["control_id"]
        owner = None
        if len(points) < 2 or any(points[i] == points[i + 1] for i in range(len(points) - 1)):
            violations.append({"control_id": cid, "violation": "degenerate_polyline"})
        elif polyline_self_intersects(points, closed=False):
            violations.append({"control_id": cid, "violation": "self_intersection"})
        elif strictly_inside_domain(points, cid):
            crosses_domain_rings(points, cid)
            memberships = set()
            for x, y in points:
                member = None
                for region_index, region in enumerate(regions):
                    if point_in_ring(x, y, region["points"]) or point_on_ring(x, y, region["points"], tol):
                        member = region_index
                memberships.add(member)
            if len(memberships) != 1 or any(
                polyline_crosses_ring(points, r["points"]) for r in regions
            ):
                violations.append({"control_id": cid,
                                   "violation": "breakline_crosses_region_boundary"})
            else:
                owner = memberships.pop()
        if line["size_m"] is not None:
            if not (0.0 < line["size_m"] <= lc):
                violations.append({"control_id": cid, "violation": "size_m_out_of_range",
                                   "detail": f"size_m={line['size_m']} must be in (0, lc={lc}]"})
            if line["dist_max_m"] is None or line["dist_max_m"] <= 0.0:
                violations.append({"control_id": cid, "violation": "dist_max_m_required_positive"})
        breakline_region.append(owner)

    if violations:
        raise MeshControlError(violations)
    return {"hole_region": hole_region, "breakline_region": breakline_region}


# --- post-mesh assertions -----------------------------------------------------


def assert_breaklines_preserved(controls: list[dict], node_x, node_y, edge_nodes,
                                tol: float = 1.0e-6) -> list[dict]:
    """Every breakline segment must be covered by a chain of mesh edges whose
    nodes lie on the segment, starting and ending at the segment vertices.
    Returns per-breakline reports; raises MeshControlError on the first gap."""
    edge_set = {(min(a, b), max(a, b)) for a, b in edge_nodes}
    reports = []
    violations = []
    for control in controls:
        if control["control_kind"] != "breakline":
            continue
        points = control["points"]
        edges_on_line = 0
        for s in range(len(points) - 1):
            a, b = points[s], points[s + 1]
            dx, dy = b[0] - a[0], b[1] - a[1]
            length2 = dx * dx + dy * dy
            on_segment = []
            for node in range(len(node_x)):
                p = (node_x[node], node_y[node])
                if point_segment_distance(p, a, b) <= tol:
                    t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2
                    on_segment.append((t, node))
            on_segment.sort()
            chain_ok = (
                len(on_segment) >= 2
                and math.hypot(node_x[on_segment[0][1]] - a[0], node_y[on_segment[0][1]] - a[1]) <= tol
                and math.hypot(node_x[on_segment[-1][1]] - b[0], node_y[on_segment[-1][1]] - b[1]) <= tol
                and all(
                    (min(on_segment[k][1], on_segment[k + 1][1]),
                     max(on_segment[k][1], on_segment[k + 1][1])) in edge_set
                    for k in range(len(on_segment) - 1)
                )
            )
            if not chain_ok:
                violations.append({"control_id": control["control_id"],
                                   "violation": "breakline_not_preserved_as_mesh_edges",
                                   "detail": f"segment {s}: {len(on_segment)} nodes on segment"})
                break
            edges_on_line += len(on_segment) - 1
        else:
            reports.append({"control_id": control["control_id"], "preserved": True,
                            "mesh_edges_on_breakline": edges_on_line})
    if violations:
        raise MeshControlError(violations)
    return reports


def region_membership(controls: list[dict], centroids) -> list[str | None]:
    regions = [c for c in controls if c["control_kind"] == "refinement_region"]
    membership: list[str | None] = []
    for x, y in centroids:
        owner = None
        for region in regions:
            if point_in_ring(x, y, region["points"]):
                owner = region["control_id"]
                break
        membership.append(owner)
    return membership


# --- per-cell diagnostics (display only; the C++ quality module is authority) -


def cell_diagnostics(node_x, node_y, faces, edge_nodes, edge_faces, membership=None) -> list[dict]:
    centroids = [
        (sum(node_x[i] for i in face) / len(face), sum(node_y[i] for i in face) / len(face))
        for face in faces
    ]
    nonorthogonality = [0.0] * len(faces)
    for (a, b), (left, right) in zip(edge_nodes, edge_faces):
        if right < 0:
            continue
        ex, ey = node_x[b] - node_x[a], node_y[b] - node_y[a]
        cx, cy = centroids[right][0] - centroids[left][0], centroids[right][1] - centroids[left][1]
        edge_len = math.hypot(ex, ey)
        cc_len = math.hypot(cx, cy)
        if edge_len == 0.0 or cc_len == 0.0:
            continue
        # angle between the edge normal and the centroid-centroid vector
        cos_theta = abs(ex * cy - ey * cx) / (edge_len * cc_len)
        theta = math.degrees(math.acos(max(-1.0, min(1.0, cos_theta))))
        nonorthogonality[left] = max(nonorthogonality[left], theta)
        nonorthogonality[right] = max(nonorthogonality[right], theta)

    rows = []
    for cell_id, face in enumerate(faces):
        points = [(node_x[i], node_y[i]) for i in face]
        n = len(points)
        angles, lengths = [], []
        area2 = 0.0
        for i in range(n):
            ax, ay = points[i]
            bx, by = points[(i + 1) % n]
            cx, cy = points[(i - 1) % n]
            lengths.append(math.hypot(bx - ax, by - ay))
            area2 += ax * by - bx * ay
            v1, v2 = (bx - ax, by - ay), (cx - ax, cy - ay)
            norm = math.hypot(*v1) * math.hypot(*v2)
            if norm > 0.0:
                dot = v1[0] * v2[0] + v1[1] * v2[1]
                angles.append(math.degrees(math.acos(max(-1.0, min(1.0, dot / norm)))))
        ideal = 180.0 * (n - 2) / n
        skew = max(
            (max(angles) - ideal) / (180.0 - ideal), (ideal - min(angles)) / ideal
        ) if angles else 1.0
        rows.append({
            "cell_id": cell_id,
            "vertices": n,
            "area_m2": round(abs(area2) / 2.0, 6),
            "min_angle_deg": round(min(angles), 4) if angles else 0.0,
            "edge_length_ratio": round(max(lengths) / min(lengths), 4) if min(lengths) > 0 else None,
            "equiangle_skewness": round(max(0.0, min(1.0, skew)), 4),
            "nonorthogonality_deg": round(nonorthogonality[cell_id], 4),
            "refinement_region": membership[cell_id] if membership else None,
            "ring": points + [points[0]],
        })
    return rows


def write_cell_diagnostics_geojson(path: Path, rows: list[dict]) -> None:
    features = [
        {
            "type": "Feature",
            "properties": {k: v for k, v in row.items() if k != "ring"},
            "geometry": {"type": "Polygon",
                         "coordinates": [[[round(x, 6), round(y, 6)] for x, y in row["ring"]]]},
        }
        for row in rows
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({
            "type": "FeatureCollection",
            "diagnostic_only": True,
            "note": "display metrics for the QGIS mesh workbench heat map; "
                    "fatal/review certification is libs/mesh quality + scau_preproc validate",
            "features": features,
        }, separators=(",", ":")),
        encoding="utf-8",
    )
