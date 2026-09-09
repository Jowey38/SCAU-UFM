"""scau_preproc mesh generator (production port of the M287-A spike).

Runs as an ISOLATED SUBPROCESS driven by a JSON config file: the process
boundary is both the fault-isolation boundary (the pipeline kills it on the
geometry_clean_policy timeout; output is atomic-rename so a kill never leaves
a partial case) and the GPL license boundary (gmsh, GPL-2.0+, is imported
only inside this process; see third_party/licenses/gmsh-LICENSE-NOTE.md).

Geometry pre-checks implement geometry_clean_policy REJECTION only (no
repairs): self-intersecting or unclosed rings abort with exit 2 and a
diagnostic GeoJSON naming the offending features. Certification of the
generated case remains exclusively `scau_preproc validate` (the authoritative
C++ read path); nothing here re-implements the validator.

M287-B4 mesh controls (config key "mesh_controls"): operator-drawn breaklines
and refinement regions (mesh_controls.py contract) drive gmsh embedded curves
and size fields; breaklines are asserted post-mesh to survive as mesh-edge
chains; a per-cell diagnostic GeoJSON feeds the QGIS heat map. Without the
key, generation is byte-identical to pipeline v1.

M287-B5 field derivation (config key "field_derivation": {"dpm_rule_table",
"soil_zones"?}): phi_t / Phi_c per landcover class from an approved rule table,
omega_edge / phi_e_n / phi_et by the spec 5.3 rule-2 edge projection, soil_type
from soil_zones; without the key the v1 unit placeholders are written.

Run as `py -3 -m scau_preproc.meshgen <generator.config.json>`.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import os
import sys
from pathlib import Path

from scau_preproc import field_derivation, mesh_controls

FILL = -1


# --- input loading -----------------------------------------------------------


def load_geojson_polygons(path: Path, id_field: str) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    polygons = []
    for feature in data.get("features", []):
        geometry = feature.get("geometry") or {}
        if geometry.get("type") != "Polygon":
            continue
        rings = geometry["coordinates"]
        properties = feature.get("properties") or {}
        polygons.append(
            {
                "id": str(properties.get(id_field, f"feature_{len(polygons)}")),
                "outer": [tuple(map(float, xy)) for xy in rings[0]],
                "properties": properties,
            }
        )
    return polygons


def load_ascii_grid(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8").split("\n")
    header: dict[str, float] = {}
    row_start = 0
    for index, line in enumerate(lines):
        parts = line.split()
        if len(parts) == 2 and parts[0].lower() in {
            "ncols", "nrows", "xllcorner", "yllcorner", "cellsize", "nodata_value",
        }:
            header[parts[0].lower()] = float(parts[1])
            row_start = index + 1
        elif parts:
            break
    ncols = int(header["ncols"])
    nrows = int(header["nrows"])
    values: list[list[float]] = []
    for line in lines[row_start:]:
        parts = line.split()
        if parts:
            values.append([float(v) for v in parts])
    if len(values) != nrows or any(len(row) != ncols for row in values):
        raise ValueError(f"ASCII grid shape mismatch in {path}")
    return {**header, "ncols": ncols, "nrows": nrows, "values": values}


def sample_dem_nearest(grid: dict, x: float, y: float) -> float:
    col = int((x - grid["xllcorner"]) / grid["cellsize"])
    row_from_bottom = int((y - grid["yllcorner"]) / grid["cellsize"])
    col = min(max(col, 0), grid["ncols"] - 1)
    row_from_bottom = min(max(row_from_bottom, 0), grid["nrows"] - 1)
    value = grid["values"][grid["nrows"] - 1 - row_from_bottom][col]
    if value == grid.get("nodata_value"):
        raise ValueError(f"DEM NoData at sample point ({x}, {y})")
    return value


# --- geometry pre-checks (geometry_clean_policy: reject, never silently fix) --


def _segments_properly_intersect(p1, p2, p3, p4) -> bool:
    def orient(a, b, c):
        v = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        return 0 if v == 0 else (1 if v > 0 else -1)

    o1, o2 = orient(p1, p2, p3), orient(p1, p2, p4)
    o3, o4 = orient(p3, p4, p1), orient(p3, p4, p2)
    return o1 != o2 and o3 != o4 and 0 not in (o1, o2, o3, o4)


def find_self_intersection(ring: list[tuple[float, float]]) -> bool:
    n = len(ring) - 1  # closed ring repeats the first vertex
    for i in range(n):
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue  # adjacent through the closure
            if _segments_properly_intersect(ring[i], ring[i + 1], ring[j], ring[j + 1]):
                return True
    return False


def reject_invalid_polygons(polygons: list[dict], kind: str, diagnostics_path: Path) -> None:
    offenders = []
    for polygon in polygons:
        ring = polygon["outer"]
        if len(ring) < 4 or ring[0] != ring[-1]:
            offenders.append((polygon, "unclosed_ring"))
        elif find_self_intersection(ring):
            offenders.append((polygon, "self_intersection"))
    if offenders:
        write_diagnostic_geojson(
            diagnostics_path,
            [
                {"feature_id": polygon["id"], "kind": kind, "violation": violation,
                 "ring": polygon["outer"]}
                for polygon, violation in offenders
            ],
            reason="invalid_input_geometry",
        )
        ids = ", ".join(p["id"] for p, _ in offenders)
        print(
            f"MeshGenerationFailed: invalid {kind} geometry rejected fail-closed "
            f"({ids}); diagnostic written to {diagnostics_path}",
            file=sys.stderr,
        )
        raise SystemExit(2)


def control_ring(control: dict) -> list[tuple[float, float]]:
    points = control["points"]
    return points if control["control_kind"] == "refinement_region" else points + points[::-1]


def reject_mesh_controls(error, controls: list[dict], diagnostics_path: Path) -> None:
    by_id = {c["control_id"]: c for c in controls}
    entries = []
    for violation in error.violations:
        control = by_id.get(violation["control_id"])
        entries.append({
            "feature_id": violation["control_id"],
            "kind": f"mesh_control:{control['control_kind']}" if control else "mesh_control",
            "violation": violation["violation"],
            "detail": violation.get("detail"),
            "ring": control_ring(control) if control else None,
        })
    write_diagnostic_geojson(diagnostics_path, entries, reason="invalid_mesh_controls")
    print(f"MeshControlsRejected: {error}; diagnostic written to {diagnostics_path}",
          file=sys.stderr)
    raise SystemExit(2)


def region_size_report(controls, membership, mesh, topology) -> list[dict]:
    node_x, node_y = mesh["node_x"], mesh["node_y"]
    faces = mesh["faces"]
    edge_length = [
        math.hypot(node_x[b] - node_x[a], node_y[b] - node_y[a]) for a, b in topology["edge_nodes"]
    ]
    reports = []
    for control in controls:
        if control["control_kind"] != "refinement_region":
            continue
        inside = [i for i, owner in enumerate(membership) if owner == control["control_id"]]
        edges = sorted({e for i in inside for e in topology["face_edges"][i] if e >= 0})
        lengths = [edge_length[e] for e in edges]
        reports.append({
            "control_id": control["control_id"],
            "size_m": control["size_m"],
            "cells_inside": len(inside),
            "max_edge_length_inside_m": max(lengths) if lengths else None,
            "mean_edge_length_inside_m": sum(lengths) / len(lengths) if lengths else None,
        })
    return reports


def write_diagnostic_geojson(path: Path, entries: list[dict], reason: str) -> None:
    features = [
        {
            "type": "Feature",
            "properties": {k: v for k, v in entry.items() if k != "ring"},
            "geometry": None if not entry.get("ring") else {
                "type": "Polygon", "coordinates": [[list(xy) for xy in entry["ring"]]]},
        }
        for entry in entries
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"type": "FeatureCollection", "diagnostic_reason": reason,
                    "features": features}, indent=2),
        encoding="utf-8",
    )


# --- meshing -----------------------------------------------------------------


def build_mesh(boundary: dict, holes: list[dict], lc: float, recombine: bool,
               controls: list[dict] | None = None, ownership: dict | None = None) -> dict:
    """Constrained gmsh meshing. With `controls` (validated mesh_controls),
    refinement regions become plane sub-surfaces carrying a Constant size
    field, breaklines are embedded curves (optionally with a linear Threshold
    size field); `ownership` (from validate_mesh_controls) tells which surface
    owns each building hole / breakline. Without controls the call path is
    byte-for-byte the pipeline-v1 behaviour (G30 fixture)."""
    import gmsh

    controls = controls or []
    regions = [c for c in controls if c["control_kind"] == "refinement_region"]
    breaklines = [c for c in controls if c["control_kind"] == "breakline"]

    gmsh.initialize()
    try:
        gmsh.option.setNumber("General.Terminal", 0)
        gmsh.option.setNumber("General.NumThreads", 1)
        gmsh.option.setNumber("Mesh.Algorithm", 6)  # Frontal-Delaunay, deterministic here
        gmsh.option.setNumber("Mesh.RandomSeed", 1)
        gmsh.model.add("m287a")

        def add_polyline(points: list[tuple[float, float]], closed: bool, size: float) -> list[int]:
            point_tags = [gmsh.model.geo.addPoint(x, y, 0.0, size) for x, y in points]
            count = len(point_tags)
            pairs = [(i, (i + 1) % count) for i in range(count)] if closed else [
                (i, i + 1) for i in range(count - 1)
            ]
            return [gmsh.model.geo.addLine(point_tags[i], point_tags[j]) for i, j in pairs]

        def add_loop(ring: list[tuple[float, float]], size: float = lc) -> tuple[int, list[int]]:
            line_tags = add_polyline(ring[:-1], True, size)
            return gmsh.model.geo.addCurveLoop(line_tags), line_tags

        outer_loop, _ = add_loop(boundary["outer"])
        hole_loops = [add_loop(hole["outer"])[0] for hole in holes]
        region_loops = [add_loop(region["points"], region["size_m"])[0] for region in regions]
        hole_owner = ownership["hole_region"] if ownership else [None] * len(holes)
        surfaces = [gmsh.model.geo.addPlaneSurface(
            [outer_loop] + region_loops
            + [hole_loops[i] for i, owner in enumerate(hole_owner) if owner is None]
        )]
        for region_index, region_loop in enumerate(region_loops):
            surfaces.append(gmsh.model.geo.addPlaneSurface(
                [region_loop]
                + [hole_loops[i] for i, owner in enumerate(hole_owner) if owner == region_index]
            ))
        gmsh.model.geo.synchronize()

        breakline_curves: list[list[int]] = []
        breakline_owner = ownership["breakline_region"] if ownership else [None] * len(breaklines)
        for line, owner in zip(breaklines, breakline_owner):
            size = line["size_m"] if line["size_m"] is not None else lc
            curves = add_polyline(line["points"], False, size)
            breakline_curves.append(curves)
            gmsh.model.geo.synchronize()
            target = surfaces[0] if owner is None else surfaces[owner + 1]
            gmsh.model.mesh.embed(1, curves, 2, target)

        size_fields = []
        for region_index, region in enumerate(regions):
            field = gmsh.model.mesh.field.add("Constant")
            gmsh.model.mesh.field.setNumbers(field, "SurfacesList", [surfaces[region_index + 1]])
            gmsh.model.mesh.field.setNumber(field, "VIn", region["size_m"])
            gmsh.model.mesh.field.setNumber(field, "VOut", lc)
            size_fields.append(field)
        for line, curves in zip(breaklines, breakline_curves):
            if line["size_m"] is None:
                continue
            distance = gmsh.model.mesh.field.add("Distance")
            gmsh.model.mesh.field.setNumbers(distance, "CurvesList", curves)
            gmsh.model.mesh.field.setNumber(distance, "Sampling", 200)
            threshold = gmsh.model.mesh.field.add("Threshold")
            gmsh.model.mesh.field.setNumber(threshold, "InField", distance)
            gmsh.model.mesh.field.setNumber(threshold, "SizeMin", line["size_m"])
            gmsh.model.mesh.field.setNumber(threshold, "SizeMax", lc)
            gmsh.model.mesh.field.setNumber(threshold, "DistMin", 0.0)
            gmsh.model.mesh.field.setNumber(threshold, "DistMax", line["dist_max_m"])
            gmsh.model.mesh.field.setNumber(threshold, "Sigmoid", 0)
            size_fields.append(threshold)
        if size_fields:
            combined = gmsh.model.mesh.field.add("Min")
            gmsh.model.mesh.field.setNumbers(combined, "FieldsList", size_fields)
            gmsh.model.mesh.field.setAsBackgroundMesh(combined)
            # Size fields are the single source of element size (gmsh guidance).
            gmsh.option.setNumber("Mesh.MeshSizeExtendFromBoundary", 0)
            gmsh.option.setNumber("Mesh.MeshSizeFromPoints", 0)
            gmsh.option.setNumber("Mesh.MeshSizeFromCurvature", 0)

        if recombine:
            for surface in surfaces:
                gmsh.model.mesh.setRecombine(2, surface)
            gmsh.option.setNumber("Mesh.RecombinationAlgorithm", 1)  # Blossom
        gmsh.model.mesh.generate(2)

        node_tags, coords, _ = gmsh.model.mesh.getNodes()
        tag_to_xy = {
            int(tag): (coords[3 * i], coords[3 * i + 1])
            for i, tag in enumerate(node_tags)
        }
        faces: list[list[int]] = []
        for element_type, count in ((2, 3), (3, 4)):  # triangles, quadrangles
            _, element_nodes = gmsh.model.mesh.getElementsByType(element_type)
            for start in range(0, len(element_nodes), count):
                faces.append([int(t) for t in element_nodes[start:start + count]])
        gmsh_version = gmsh.option.getString("General.Version")
    finally:
        gmsh.finalize()

    if not faces:
        raise SystemExit("MeshGenerationFailed: gmsh produced no 2D elements")

    used = sorted({tag for face in faces for tag in face})
    remap = {tag: index for index, tag in enumerate(used)}
    node_x = [tag_to_xy[tag][0] for tag in used]
    node_y = [tag_to_xy[tag][1] for tag in used]
    faces = [[remap[tag] for tag in face] for face in faces]

    # Orient every ring counter-clockwise (loader precedent; deterministic).
    for face in faces:
        area2 = sum(
            node_x[face[i]] * node_y[face[(i + 1) % len(face)]]
            - node_x[face[(i + 1) % len(face)]] * node_y[face[i]]
            for i in range(len(face))
        )
        if area2 < 0.0:
            face.reverse()

    return {"node_x": node_x, "node_y": node_y, "faces": faces, "gmsh_version": gmsh_version}


def derive_topology(node_x: list[float], node_y: list[float], faces: list[list[int]]) -> dict:
    edge_index: dict[tuple[int, int], int] = {}
    edge_nodes: list[list[int]] = []
    edge_faces: list[list[int]] = []
    face_edges: list[list[int]] = []
    for face_id, face in enumerate(faces):
        slots = []
        for i, node in enumerate(face):
            a, b = node, face[(i + 1) % len(face)]
            key = (min(a, b), max(a, b))
            if key not in edge_index:
                edge_index[key] = len(edge_nodes)
                edge_nodes.append([a, b])
                edge_faces.append([face_id, FILL])
            else:
                slot = edge_index[key]
                if edge_faces[slot][1] != FILL or edge_faces[slot][0] == face_id:
                    raise SystemExit(
                        f"MeshGenerationFailed: non-manifold or duplicate edge at face {face_id}"
                    )
                edge_faces[slot][1] = face_id
            slots.append(edge_index[key])
        face_edges.append(slots + [FILL] * (4 - len(slots)))
    face_nodes = [face + [FILL] * (4 - len(face)) for face in faces]
    return {
        "face_nodes": face_nodes,
        "edge_nodes": edge_nodes,
        "edge_faces": edge_faces,
        "face_edges": face_edges,
    }


# --- fields ------------------------------------------------------------------


def point_in_ring(x: float, y: float, ring: list[tuple[float, float]]) -> bool:
    inside = False
    n = len(ring) - 1
    for i in range(n):
        x1, y1 = ring[i]
        x2, y2 = ring[i + 1]
        if (y1 > y) != (y2 > y):
            x_cross = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x_cross > x:
                inside = not inside
    return inside


def assign_fields(centroids, dem, landcover, soil_table_path: Path) -> dict:
    with soil_table_path.open(encoding="utf-8", newline="") as handle:
        soil_rows = list(csv.DictReader(handle))
    soil_params = [
        {key: float(row[key]) for key in ("K_s", "psi_f", "theta_s", "theta_i")}
        for row in sorted(soil_rows, key=lambda row: int(row["soil_type"]))
    ]

    manning, soil_type, z_b = [], [], []
    for x, y in centroids:
        z_b.append(sample_dem_nearest(dem, x, y))
        chosen = None
        for polygon in landcover:
            if point_in_ring(x, y, polygon["outer"]):
                chosen = polygon["properties"]
                break
        manning.append(float(chosen["manning_n"]) if chosen else 0.030)
        soil_type.append(int(chosen["soil_type"]) if chosen else 1)
    return {"manning_n": manning, "soil_type": soil_type, "z_b": z_b,
            "soil_params": soil_params}


# --- quality -----------------------------------------------------------------


def quality_report(node_x, node_y, faces) -> dict:
    min_angle = 180.0
    max_aspect = 0.0
    for face in faces:
        points = [(node_x[i], node_y[i]) for i in face]
        lengths = []
        for i in range(len(points)):
            ax, ay = points[i]
            bx, by = points[(i + 1) % len(points)]
            lengths.append(math.hypot(bx - ax, by - ay))
            cx, cy = points[(i - 1) % len(points)]
            v1 = (bx - ax, by - ay)
            v2 = (cx - ax, cy - ay)
            dot = v1[0] * v2[0] + v1[1] * v2[1]
            norm = math.hypot(*v1) * math.hypot(*v2)
            if norm > 0.0:
                min_angle = min(min_angle, math.degrees(math.acos(max(-1.0, min(1.0, dot / norm)))))
        if min(lengths) > 0.0:
            max_aspect = max(max_aspect, max(lengths) / min(lengths))
    triangles = sum(1 for face in faces if len(face) == 3)
    return {
        "cells": len(faces),
        "triangles": triangles,
        "quadrilaterals": len(faces) - triangles,
        "min_angle_degrees": min_angle,
        "max_edge_length_ratio": max_aspect,
        "review_thresholds": {"min_angle_degrees": 10.0, "max_aspect_ratio": 20.0},
    }


# --- NetCDF writing (mirrors the authoritative STCF v5 UGRID contract) --------


def write_stcf_case(path: Path, topology: dict, node_x, node_y, fields: dict) -> None:
    import numpy as np
    from netCDF4 import Dataset

    cell_count = len(topology["face_nodes"])
    edge_count = len(topology["edge_nodes"])
    temporary = path.with_suffix(path.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()

    ds = Dataset(temporary, "w", format="NETCDF3_CLASSIC")
    try:
        ds.setncattr("schema_version", np.int32(5))
        ds.setncattr("title", "M287-A gmsh spike synthetic STCF v5 case")
        ds.setncattr("Conventions", "CF-1.8 UGRID-1.0")

        ds.createDimension("cell", cell_count)
        ds.createDimension("edge", edge_count)
        ds.createDimension("soil_type_entry", len(fields["soil_params"]))
        ds.createDimension("nMesh2_node", len(node_x))
        ds.createDimension("nMesh2_max_face_nodes", 4)
        ds.createDimension("Two", 2)

        def field_var(name, dim, values, location, dtype="f8"):
            var = ds.createVariable(name, dtype, (dim,))
            var.setncattr("mesh", "mesh2")
            var.setncattr("location", location)
            var[:] = values

        derived = fields.get("derived")  # B5 rule-table derivation; None = v1 unit placeholders
        field_var("phi_t", "cell", np.array(derived["phi_t"], dtype="f8") if derived else np.full(cell_count, 1.0), "face")
        field_var("phi_xx", "cell", np.array(derived["phi_xx"], dtype="f8") if derived else np.full(cell_count, 1.0), "face")
        field_var("phi_xy", "cell", np.array(derived["phi_xy"], dtype="f8") if derived else np.zeros(cell_count), "face")
        field_var("phi_yy", "cell", np.array(derived["phi_yy"], dtype="f8") if derived else np.full(cell_count, 1.0), "face")
        field_var("manning_n", "cell", np.array(fields["manning_n"], dtype="f8"), "face")
        field_var("z_b", "cell", np.array(fields["z_b"], dtype="f8"), "face")
        field_var("soil_type", "cell", np.array(fields["soil_type"], dtype="i4"), "face", "i4")
        field_var("omega_edge", "edge", np.array(derived["omega_edge"], dtype="f8") if derived else np.full(edge_count, 1.0), "edge")
        field_var("phi_e_n", "edge", np.array(derived["phi_e_n"], dtype="f8") if derived else np.full(edge_count, 1.0), "edge")
        field_var("phi_et", "edge", np.array(derived["phi_et"], dtype="f8") if derived else np.full(edge_count, 1.0), "edge")
        for name in ("K_s", "psi_f", "theta_s", "theta_i"):
            var = ds.createVariable(name, "f8", ("soil_type_entry",))
            var[:] = np.array([entry[name] for entry in fields["soil_params"]], dtype="f8")

        mesh = ds.createVariable("mesh2", "i4", ())
        mesh.setncattr("cf_role", "mesh_topology")
        mesh.setncattr("topology_dimension", np.int32(2))
        mesh.setncattr("node_coordinates", "mesh2_node_x mesh2_node_y")
        mesh.setncattr("face_node_connectivity", "mesh2_face_nodes")
        mesh.setncattr("edge_node_connectivity", "mesh2_edge_nodes")
        mesh.setncattr("edge_face_connectivity", "mesh2_edge_faces")
        mesh.setncattr("face_edge_connectivity", "mesh2_face_edges")
        mesh.setncattr("face_dimension", "cell")
        mesh.setncattr("edge_dimension", "edge")
        mesh.assignValue(0)

        for name, values in (("mesh2_node_x", node_x), ("mesh2_node_y", node_y)):
            var = ds.createVariable(name, "f8", ("nMesh2_node",))
            var.setncattr("standard_name",
                          "projection_x_coordinate" if name.endswith("x") else "projection_y_coordinate")
            var.setncattr("units", "m")
            var[:] = np.array(values, dtype="f8")

        def connectivity(name, dims, values, uses_fill):
            kwargs = {"fill_value": np.int32(FILL)} if uses_fill else {}
            var = ds.createVariable(name, "i4", dims, **kwargs)
            var.setncattr("start_index", np.int32(0))
            var[:] = np.array(values, dtype="i4")

        connectivity("mesh2_face_nodes", ("cell", "nMesh2_max_face_nodes"),
                     topology["face_nodes"], True)
        connectivity("mesh2_edge_nodes", ("edge", "Two"), topology["edge_nodes"], False)
        connectivity("mesh2_edge_faces", ("edge", "Two"), topology["edge_faces"], True)
        connectivity("mesh2_face_edges", ("cell", "nMesh2_max_face_nodes"),
                     topology["face_edges"], True)
    finally:
        ds.close()

    if path.exists():
        path.unlink()
    os.rename(temporary, path)


# --- main --------------------------------------------------------------------


def main() -> int:
    config = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    package = Path(config["package"])
    output = Path(config["output"])
    diagnostics = Path(config["diagnostics"])
    lc = float(config.get("characteristic_length_m", 8.0))
    recombine = bool(config.get("recombine", True))
    boundary_path = Path(config.get("boundary", package / "boundary/computational_boundary.geojson"))
    buildings_path = Path(config.get("buildings", package / "buildings/buildings.geojson"))

    boundary = load_geojson_polygons(boundary_path, "boundary_id")
    buildings = load_geojson_polygons(buildings_path, "building_id")
    if len(boundary) != 1:
        raise SystemExit("MeshGenerationFailed: exactly one computational boundary polygon required")
    reject_invalid_polygons(boundary, "boundary", diagnostics)
    reject_invalid_polygons(buildings, "building", diagnostics)

    controls_config = config.get("mesh_controls") or {}
    controls: list[dict] = []
    ownership = None
    if controls_config.get("geojson"):
        try:
            controls = mesh_controls.load_mesh_controls(
                Path(controls_config["geojson"]),
                controls_config.get("default_size_m"),
                controls_config.get("default_dist_max_m"),
            )
            ownership = mesh_controls.validate_mesh_controls(
                controls, boundary[0]["outer"], [b["outer"] for b in buildings], lc
            )
        except mesh_controls.MeshControlError as error:
            reject_mesh_controls(error, controls, diagnostics)

    try:
        mesh = build_mesh(boundary[0], buildings, lc, recombine, controls, ownership)
    except SystemExit:
        raise
    except Exception as error:  # gmsh failure -> diagnostic, no partial output
        write_diagnostic_geojson(
            diagnostics,
            [{"feature_id": boundary[0]["id"], "kind": "boundary", "ring": boundary[0]["outer"]}]
            + [{"feature_id": b["id"], "kind": "building_constraint", "ring": b["outer"]}
               for b in buildings]
            + [{"feature_id": c["control_id"], "kind": f"mesh_control:{c['control_kind']}",
                "ring": control_ring(c)} for c in controls],
            reason=f"gmsh_failure: {error}",
        )
        print(f"MeshGenerationFailed: gmsh error ({error}); diagnostic at {diagnostics}", file=sys.stderr)
        raise SystemExit(2)

    topology = derive_topology(mesh["node_x"], mesh["node_y"], mesh["faces"])
    centroids = [
        (
            sum(mesh["node_x"][i] for i in face) / len(face),
            sum(mesh["node_y"][i] for i in face) / len(face),
        )
        for face in mesh["faces"]
    ]

    controls_report = None
    membership = None
    if controls:
        try:
            breakline_reports = mesh_controls.assert_breaklines_preserved(
                controls, mesh["node_x"], mesh["node_y"], topology["edge_nodes"]
            )
        except mesh_controls.MeshControlError as error:
            reject_mesh_controls(error, controls, diagnostics)
        membership = mesh_controls.region_membership(controls, centroids)
        controls_report = {
            "geojson": str(controls_config["geojson"]),
            "geojson_sha256": hashlib.sha256(
                Path(controls_config["geojson"]).read_bytes()).hexdigest(),
            "default_size_m": controls_config.get("default_size_m"),
            "default_dist_max_m": controls_config.get("default_dist_max_m"),
            "size_field": "Min(Constant per refinement_region, "
                          "linear Threshold(Distance) per sized breakline); "
                          "MeshSizeFromPoints/ExtendFromBoundary/FromCurvature=0",
            "breaklines": breakline_reports,
            "refinement_regions": region_size_report(controls, membership, mesh, topology),
        }

    # B3: the pipeline points at the conditioned DEM when a terrain policy is
    # enabled; without the key the package DEM is sampled verbatim.
    dem = load_ascii_grid(Path(config.get("dem", package / "terrain/dem.asc")))
    landcover = load_geojson_polygons(package / "landcover/landcover.geojson", "class_code")
    fields = assign_fields(centroids, dem, landcover, package / "soil/soil_parameters.csv")

    derivation_config = config.get("field_derivation") or {}
    derivation_report_payload = None
    if derivation_config.get("dpm_rule_table"):
        package_manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
        try:
            rules = field_derivation.load_rule_table(
                Path(derivation_config["dpm_rule_table"]),
                package_is_synthetic=bool(package_manifest.get("synthetic_data", False)),
            )
            soil_zones = None
            if rules["soil"]["source"] == "soil_zones":
                soil_zones_path = Path(derivation_config.get("soil_zones", package / "soil/soil_zones.geojson"))
                if not soil_zones_path.is_file():
                    raise field_derivation.FieldDerivationError(f"soil zones not found: {soil_zones_path}")
                soil_zones = load_geojson_polygons(soil_zones_path, "soil_type")
            cell_fields = field_derivation.derive_cell_fields(centroids, landcover, rules, soil_zones)
            edge_fields = field_derivation.derive_edge_fields(
                mesh["node_x"], mesh["node_y"], topology["edge_nodes"], topology["edge_faces"],
                cell_fields, rules)
        except field_derivation.FieldDerivationError as error:
            print(f"FieldDerivationFailed: {error}", file=sys.stderr)
            raise SystemExit(2)
        fields["derived"] = {**cell_fields, **edge_fields}
        fields["soil_type"] = cell_fields["soil_type"]
        fields["manning_n"] = cell_fields["manning_n"]
        max_soil = len(fields["soil_params"])
        bad = sorted({s for s in fields["soil_type"] if not 0 <= s < max_soil})
        if bad:
            print(f"FieldDerivationFailed: soil_type {bad} not in soil_parameters.csv (0..{max_soil - 1})",
                  file=sys.stderr)
            raise SystemExit(2)
        derivation_report_payload = field_derivation.derivation_report(rules, cell_fields, edge_fields)
        derivation_report_payload["rule_table_sha256"] = hashlib.sha256(
            Path(derivation_config["dpm_rule_table"]).read_bytes()).hexdigest()

    write_stcf_case(output, topology, mesh["node_x"], mesh["node_y"], fields)
    if derivation_report_payload is not None:
        report_path = Path(config.get("field_report", output.with_suffix(".field_derivation.json")))
        report_path.write_text(json.dumps(derivation_report_payload, indent=2), encoding="utf-8")

    cell_rows = mesh_controls.cell_diagnostics(
        mesh["node_x"], mesh["node_y"], mesh["faces"],
        topology["edge_nodes"], topology["edge_faces"], membership,
    )
    cells_path = Path(config.get("quality_cells", output.with_suffix(".quality_cells.geojson")))
    mesh_controls.write_cell_diagnostics_geojson(cells_path, cell_rows)

    report = {
        "gmsh_version": mesh["gmsh_version"],
        "characteristic_length_m": lc,
        "recombine": recombine,
        "nodes": len(mesh["node_x"]),
        "edges": len(topology["edge_nodes"]),
        "output": str(output),
        "output_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "quality": quality_report(mesh["node_x"], mesh["node_y"], mesh["faces"]),
        "per_cell_diagnostics": {
            "geojson": str(cells_path),
            "diagnostic_only": True,
            "max_equiangle_skewness": max(r["equiangle_skewness"] for r in cell_rows),
            "max_nonorthogonality_deg": max(r["nonorthogonality_deg"] for r in cell_rows),
        },
        "mesh_controls": controls_report,
        "field_derivation": None if derivation_report_payload is None else {
            "report": str(Path(config.get("field_report", output.with_suffix(".field_derivation.json")))),
            "rule_table_sha256": derivation_report_payload["rule_table_sha256"],
            "approval": derivation_report_payload["approval"],
        },
    }
    report_path = Path(config.get("report", output.with_suffix(".quality.json")))
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
