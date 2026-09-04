"""scau_preproc coupling-map generator v1 (M287-C).

Generates the three candidate coupling-relationship files from a synthetic
D-5 package plus the pipeline-generated STCF case, and a SimDriver-native
config fragment for the ground chain:

  surface -> SWMM      explicit CSV IDs are authoritative; the generator only
                       resolves the SWMM node coordinate to its containing
                       mesh cell (ray casting). A node outside every cell is
                       FATAL (fail-closed), never nearest-matched silently.
  roof    -> SWMM      explicit drain/node IDs kept; the overflow target cell
                       is a NEAREST-CENTROID CANDIDATE (buildings are mesh
                       holes), always marked review_status=needs_confirmation.
  surface -> D-Flow FM emitted only as provider_required with zero relations
                       when the package mapping is TODO_PROVIDER (no data is
                       invented).

Mapping modes (M287-C5; job_config "coupling_maps": {"mode": ...}):
  explicit_ids        (default, v1 behaviour) every relation comes from the
                      package CSV tables; a missing table is fatal.
  spatial_candidates  no CSV tables are read: every SWMM JUNCTION whose
                      coordinate lies in a mesh cell becomes a surface
                      candidate (node in a building hole is still FATAL;
                      node outside the boundary is a review finding, not a
                      relation); every building becomes a roof candidate to
                      the nearest junction within roof_node_max_distance_m.
                      exchange_elevation_m is the cell z_b, tagged as a
                      placeholder. ALL candidates are confidence=review and
                      need C4 confirmation; nothing spatial is authoritative.
  mixed               CSV rows are authoritative (high) where present (a table
                      may be absent entirely); nodes / buildings absent from
                      the tables get spatial candidates (review). Counts are
                      reported per method and confidence.

Outputs (schema coupling_mapping_schema_version = 1):
  coupling/surface_swmm_mapping.json
  coupling/roof_drain_mapping.json
  coupling/surface_dflowfm_mapping.json
  coupling/mapping_report.json
  coupling/simdriver_links.conf   # version = 2 + surface_drainage_link lines
                                  # (parsed by apps/sim_driver runtime config)

The generator emits data and candidate relations only: no Q_limit, V_limit,
deficit, rollback/replay, or arbitration semantics (CouplingLib territory).

Usage: py -3 -m scau_preproc.coupling_maps <package_dir> <case.stcf.nc> <out_dir> [mode]
Exit codes: 0 ok; 2 fail-closed.
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

SCHEMA_VERSION = 1
PLACEHOLDER_EXCHANGE_WIDTH_M = 1.0  # recorded v1 placeholder, provider-confirmable
PLACEHOLDER_PRIORITY_WEIGHT = 1.0
MODES = ("explicit_ids", "spatial_candidates", "mixed")
DEFAULT_ROOF_NODE_MAX_DISTANCE_M = 50.0  # versioned C5 default; job_config may override


class MappingError(SystemExit):
    def __init__(self, message: str) -> None:
        self.message = message
        print(f"error: {message}", file=sys.stderr)
        super().__init__(2)


# --- geometry ----------------------------------------------------------------


def load_mesh(case_path: Path) -> dict:
    from netCDF4 import Dataset

    ds = Dataset(case_path, "r")
    try:
        node_x = ds.variables["mesh2_node_x"][:].tolist()
        node_y = ds.variables["mesh2_node_y"][:].tolist()
        face_nodes = ds.variables["mesh2_face_nodes"][:]
        z_b = ds.variables["z_b"][:].tolist() if "z_b" in ds.variables else None
        faces = []
        for row in face_nodes:
            ring = [int(v) for v in row.compressed()] if hasattr(row, "compressed") else [
                int(v) for v in row if int(v) != -1
            ]
            faces.append(ring)
    finally:
        ds.close()
    return {"node_x": node_x, "node_y": node_y, "faces": faces, "z_b": z_b}


def point_in_face(mesh: dict, face: list[int], x: float, y: float) -> bool:
    inside = False
    n = len(face)
    for i in range(n):
        x1, y1 = mesh["node_x"][face[i]], mesh["node_y"][face[i]]
        x2, y2 = mesh["node_x"][face[(i + 1) % n]], mesh["node_y"][face[(i + 1) % n]]
        if (y1 > y) != (y2 > y):
            if x1 + (y - y1) * (x2 - x1) / (y2 - y1) > x:
                inside = not inside
    return inside


def containing_cell(mesh: dict, x: float, y: float) -> int | None:
    for index, face in enumerate(mesh["faces"]):
        if point_in_face(mesh, face, x, y):
            return index
    return None


def nearest_cell_by_centroid(mesh: dict, x: float, y: float) -> tuple[int, float]:
    best_index, best_d2 = -1, float("inf")
    for index, face in enumerate(mesh["faces"]):
        cx = sum(mesh["node_x"][i] for i in face) / len(face)
        cy = sum(mesh["node_y"][i] for i in face) / len(face)
        d2 = (cx - x) ** 2 + (cy - y) ** 2
        if d2 < best_d2:
            best_index, best_d2 = index, d2
    return best_index, best_d2 ** 0.5


# --- inputs ------------------------------------------------------------------


def read_swmm_coordinates(inp_path: Path) -> dict[str, tuple[float, float]]:
    return read_swmm_nodes(inp_path)["coordinates"]


def read_swmm_nodes(inp_path: Path) -> dict:
    """[COORDINATES] plus node kinds from [JUNCTIONS]/[OUTFALLS]/[STORAGE]/
    [DIVIDERS]. Spatial candidates are generated for junctions only: outfalls
    are river-interface candidates (engine-interface semantics), never street
    inlets."""
    coordinates: dict[str, tuple[float, float]] = {}
    kinds: dict[str, str] = {}
    kind_sections = {"[JUNCTIONS]": "junction", "[OUTFALLS]": "outfall",
                     "[STORAGE]": "storage", "[DIVIDERS]": "divider"}
    section = None
    for raw in inp_path.read_text(encoding="utf-8").splitlines():
        line = raw.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("["):
            section = line.upper()
            continue
        parts = line.split()
        if section == "[COORDINATES]" and len(parts) >= 3:
            coordinates[parts[0]] = (float(parts[1]), float(parts[2]))
        elif section in kind_sections and parts:
            kinds[parts[0]] = kind_sections[section]
    if not coordinates:
        raise MappingError(f"SWMM input has no [COORDINATES] section: {inp_path}")
    return {"coordinates": coordinates, "kinds": kinds}


def read_csv_rows_optional(path: Path, required: bool) -> list[dict]:
    if not path.is_file():
        if required:
            raise MappingError(f"mapping table required in explicit_ids mode: {path}")
        return []
    return read_csv_rows(path)


def read_csv_rows(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def point_in_ring(x: float, y: float, ring: list) -> bool:
    inside = False
    for i in range(len(ring) - 1):
        (x1, y1), (x2, y2) = ring[i], ring[i + 1]
        if (y1 > y) != (y2 > y) and x1 + (y - y1) * (x2 - x1) / (y2 - y1) > x:
            inside = not inside
    return inside


def boundary_polygon(path: Path) -> list:
    data = json.loads(path.read_text(encoding="utf-8"))
    rings = [f["geometry"]["coordinates"][0] for f in data.get("features", [])
             if (f.get("geometry") or {}).get("type") == "Polygon"]
    if len(rings) != 1:
        raise MappingError("exactly one computational boundary polygon required")
    return [tuple(map(float, xy)) for xy in rings[0]]


def building_rings(path: Path) -> dict[str, list]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return {
        str(f["properties"]["building_id"]): [tuple(map(float, xy)) for xy in f["geometry"]["coordinates"][0]]
        for f in data.get("features", []) if (f.get("geometry") or {}).get("type") == "Polygon"
    }


def building_areas(path: Path) -> dict[str, float]:
    areas = {}
    for building, ring in building_rings(path).items():
        area2 = sum(ring[i][0] * ring[i + 1][1] - ring[i + 1][0] * ring[i][1] for i in range(len(ring) - 1))
        areas[building] = abs(area2) / 2.0
    return areas


def building_centroids(buildings_path: Path) -> dict[str, tuple[float, float]]:
    data = json.loads(buildings_path.read_text(encoding="utf-8"))
    centroids: dict[str, tuple[float, float]] = {}
    for feature in data.get("features", []):
        ring = feature["geometry"]["coordinates"][0][:-1]
        cx = sum(p[0] for p in ring) / len(ring)
        cy = sum(p[1] for p in ring) / len(ring)
        centroids[str(feature["properties"]["building_id"])] = (cx, cy)
    return centroids


# --- generation --------------------------------------------------------------


def generate(package: Path, case_path: Path, out_dir: Path, mode: str = "explicit_ids",
             roof_node_max_distance_m: float = DEFAULT_ROOF_NODE_MAX_DISTANCE_M) -> dict:
    if mode not in MODES:
        raise MappingError(f"unknown coupling mapping mode {mode!r} (expected one of {MODES})")
    if not roof_node_max_distance_m > 0.0:
        raise MappingError("roof_node_max_distance_m must be positive")
    mesh = load_mesh(case_path)
    swmm = read_swmm_nodes(package / "swmm/model.inp")
    coordinates = swmm["coordinates"]
    kinds = swmm["kinds"]
    out_dir.mkdir(parents=True, exist_ok=True)
    explicit = mode in ("explicit_ids", "mixed")
    spatial = mode in ("spatial_candidates", "mixed")
    findings: list[dict] = []

    def cell_z_b(cell: int) -> float:
        if mesh["z_b"] is None:
            raise MappingError("spatial candidates need z_b in the STCF case for the placeholder crest")
        return float(mesh["z_b"][cell])

    # --- surface -> SWMM ------------------------------------------------------
    surface_rows = (
        read_csv_rows_optional(package / "swmm/node_surface_mapping.csv", required=(mode == "explicit_ids"))
        if explicit else []
    )
    surface_relations = []
    simdriver_lines = ["version = 2"]
    explicit_nodes: set[str] = set()
    for row in surface_rows:
        node = row["swmm_node_id"]
        if node not in coordinates:
            raise MappingError(f"mapping references SWMM node without coordinates: {node}")
        x, y = coordinates[node]
        cell = containing_cell(mesh, x, y)
        if cell is None:
            raise MappingError(
                f"SWMM node {node} at ({x}, {y}) is not inside any mesh cell "
                "(fail-closed; no nearest-neighbor fallback for the ground chain)"
            )
        crest = float(row["exchange_elevation_m"])
        explicit_nodes.add(node)
        surface_relations.append({
            "mapping_id": row["mapping_id"],
            "surface_zone_id": row["surface_zone_id"],
            "inlet_id": row["inlet_id"],
            "swmm_node_id": node,
            "cell_index": cell,
            "node_x": x,
            "node_y": y,
            "exchange_elevation_m": crest,
            "method": "explicit_id_plus_point_in_cell",
            "confidence": "high",
            "review_status": row.get("review_status", "needs_confirmation"),
        })
        simdriver_lines.append(
            f"surface_drainage_link = cell={cell},node={node},crest={crest},"
            f"width={PLACEHOLDER_EXCHANGE_WIDTH_M},weight={PLACEHOLDER_PRIORITY_WEIGHT}"
        )

    spatial_surface_nodes: list[str] = []
    if spatial:
        boundary_ring = boundary_polygon(package / "boundary/computational_boundary.geojson")
        holes = building_rings(package / "buildings/buildings.geojson")
        for node in sorted(coordinates):
            if node in explicit_nodes or kinds.get(node, "junction") != "junction":
                continue
            x, y = coordinates[node]
            in_hole = next((hid for hid, ring in holes.items() if point_in_ring(x, y, ring)), None)
            if in_hole is not None:
                # Checked against the footprint itself, not only against mesh
                # coverage: a manhole inside a building can never be a street inlet.
                raise MappingError(
                    f"SWMM node {node} at ({x}, {y}) lies inside building footprint {in_hole} "
                    "(fail-closed: a manhole cannot exchange through a building)"
                )
            cell = containing_cell(mesh, x, y)
            if cell is None:
                inside_boundary = point_in_ring(x, y, boundary_ring)
                findings.append({
                    "severity": "review",
                    "code": "SwmmNodeNotInAnyCell" if inside_boundary else "SwmmNodeOutsideSurfaceDomain",
                    "detail": f"junction {node} at ({x}, {y}) has no containing cell; no candidate emitted",
                    "objects": [{"feature_id": node, "kind": "swmm:junction",
                                 "violation": "no_containing_cell"}],
                })
                continue
            crest = cell_z_b(cell)
            spatial_surface_nodes.append(node)
            surface_relations.append({
                "mapping_id": f"SPATIAL_SURF_{node}",
                "surface_zone_id": None,
                "inlet_id": None,
                "swmm_node_id": node,
                "cell_index": cell,
                "node_x": x,
                "node_y": y,
                "exchange_elevation_m": crest,
                "exchange_elevation_source": "placeholder:cell_z_b",
                "method": "spatial_point_in_cell_candidate",
                "confidence": "review",
                "review_status": "needs_confirmation",
            })
            simdriver_lines.append(
                f"surface_drainage_link = cell={cell},node={node},crest={crest},"
                f"width={PLACEHOLDER_EXCHANGE_WIDTH_M},weight={PLACEHOLDER_PRIORITY_WEIGHT}"
            )

    # --- roof -> SWMM ---------------------------------------------------------
    roof_rows = (
        read_csv_rows_optional(package / "swmm/roof_drain_mapping.csv", required=(mode == "explicit_ids"))
        if explicit else []
    )
    centroids = building_centroids(package / "buildings/buildings.geojson")
    roof_relations = []
    explicit_buildings: set[str] = set()
    for row in roof_rows:
        building = row["building_id"]
        if building not in centroids:
            raise MappingError(f"roof mapping references unknown building: {building}")
        bx, by = centroids[building]
        cell, distance = nearest_cell_by_centroid(mesh, bx, by)
        explicit_buildings.add(building)
        roof_relations.append({
            "mapping_id": row["mapping_id"],
            "building_id": building,
            "roof_drain_id": row["roof_drain_id"],
            "swmm_node_id": row["swmm_node_id"],
            "overflow_target_cell_index": cell,
            "candidate_distance_m": distance,
            "catchment_area_m2": float(row["catchment_area_m2"]),
            "exchange_elevation_m": float(row["exchange_elevation_m"]),
            "method": "explicit_ids_plus_nearest_centroid_candidate",
            "confidence": "review",
            "review_status": "needs_confirmation",
        })

    spatial_roof_buildings: list[str] = []
    if spatial:
        junctions = {n: xy for n, xy in coordinates.items() if kinds.get(n, "junction") == "junction"}
        areas = building_areas(package / "buildings/buildings.geojson")
        for building in sorted(centroids):
            if building in explicit_buildings:
                continue
            bx, by = centroids[building]
            if not junctions:
                findings.append({"severity": "review", "code": "RoofNoJunctionAvailable",
                                 "detail": f"building {building}: SWMM model has no junctions",
                                 "objects": [{"feature_id": building, "kind": "building",
                                              "violation": "no_junction"}]})
                continue
            node, node_distance = min(
                ((n, ((x - bx) ** 2 + (y - by) ** 2) ** 0.5) for n, (x, y) in junctions.items()),
                key=lambda item: (item[1], item[0]),
            )
            if node_distance > roof_node_max_distance_m:
                findings.append({
                    "severity": "review",
                    "code": "RoofNoJunctionWithinDistance",
                    "detail": f"building {building}: nearest junction {node} is {node_distance:.2f} m away "
                              f"(> roof_node_max_distance_m={roof_node_max_distance_m}); no candidate emitted",
                    "objects": [{"feature_id": building, "kind": "building",
                                 "violation": "nearest_junction_too_far"}],
                })
                continue
            cell, distance = nearest_cell_by_centroid(mesh, bx, by)
            spatial_roof_buildings.append(building)
            roof_relations.append({
                "mapping_id": f"SPATIAL_ROOF_{building}",
                "building_id": building,
                "roof_drain_id": None,
                "swmm_node_id": node,
                "node_candidate_distance_m": node_distance,
                "overflow_target_cell_index": cell,
                "candidate_distance_m": distance,
                "catchment_area_m2": areas[building],
                "catchment_area_source": "placeholder:footprint_area",
                "exchange_elevation_m": cell_z_b(cell),
                "exchange_elevation_source": "placeholder:cell_z_b",
                "method": "spatial_nearest_junction_candidate",
                "confidence": "review",
                "review_status": "needs_confirmation",
            })

    dflowfm_rows = read_csv_rows(package / "dflowfm/boundary_mapping.csv")
    unresolved = [row for row in dflowfm_rows if "TODO_PROVIDER" in row.values()]
    dflowfm_payload = {
        "coupling_mapping_schema_version": SCHEMA_VERSION,
        "chain": "surface_to_dflowfm",
        "status": "provider_required",
        "relations": [],
        "unresolved_provider_rows": len(unresolved),
        "note": "No authorized river boundary contract in the synthetic package; "
                "relations are never invented (M281).",
    }

    payloads = {
        "surface_swmm_mapping.json": {
            "coupling_mapping_schema_version": SCHEMA_VERSION,
            "chain": "surface_to_swmm",
            "status": "candidates_ready",
            "relations": surface_relations,
        },
        "roof_drain_mapping.json": {
            "coupling_mapping_schema_version": SCHEMA_VERSION,
            "chain": "roof_to_swmm",
            "status": "candidates_ready",
            "relations": roof_relations,
        },
        "surface_dflowfm_mapping.json": dflowfm_payload,
    }
    for name, payload in payloads.items():
        (out_dir / name).write_text(json.dumps(payload, indent=2), encoding="utf-8")

    conf_text = "\n".join(simdriver_lines) + "\n"
    (out_dir / "simdriver_links.conf").write_text(conf_text, encoding="utf-8")

    report = {
        "coupling_mapping_schema_version": SCHEMA_VERSION,
        "mode": mode,
        "mode_parameters": {"roof_node_max_distance_m": roof_node_max_distance_m} if spatial else {},
        "spatial_candidates": {
            "surface_nodes": spatial_surface_nodes,
            "roof_buildings": spatial_roof_buildings,
        } if spatial else None,
        "findings": findings,
        "chains": {
            "surface_to_swmm": {
                "relations": len(surface_relations),
                "methods": sorted({r["method"] for r in surface_relations}),
                "by_confidence": {
                    level: sum(1 for r in surface_relations if r["confidence"] == level)
                    for level in ("high", "review")
                },
                "needs_confirmation": sum(
                    1 for r in surface_relations if r["review_status"] != "confirmed"
                ),
            },
            "roof_to_swmm": {
                "relations": len(roof_relations),
                "methods": sorted({r["method"] for r in roof_relations}),
                "by_confidence": {
                    level: sum(1 for r in roof_relations if r["confidence"] == level)
                    for level in ("high", "review")
                },
                "needs_confirmation": len(roof_relations),
            },
            "surface_to_dflowfm": {
                "relations": 0,
                "status": "provider_required",
            },
        },
        "simdriver_fragment": "simdriver_links.conf",
        "placeholders": {
            "exchange_width_m": PLACEHOLDER_EXCHANGE_WIDTH_M,
            "priority_weight": PLACEHOLDER_PRIORITY_WEIGHT,
        },
        "runtime_semantics": "none (Q_limit/deficit/arbitration remain CouplingLib-owned)",
    }
    (out_dir / "mapping_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def main() -> int:
    if len(sys.argv) not in (4, 5):
        raise MappingError(
            "usage: py -3 -m scau_preproc.coupling_maps <package_dir> <case.stcf.nc> <out_dir> [mode]"
        )
    mode = sys.argv[4] if len(sys.argv) == 5 else "explicit_ids"
    report = generate(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), mode=mode)
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
