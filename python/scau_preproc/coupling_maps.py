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

Outputs (schema coupling_mapping_schema_version = 1):
  coupling/surface_swmm_mapping.json
  coupling/roof_drain_mapping.json
  coupling/surface_dflowfm_mapping.json
  coupling/mapping_report.json
  coupling/simdriver_links.conf   # version = 2 + surface_drainage_link lines
                                  # (parsed by apps/sim_driver runtime config)

The generator emits data and candidate relations only: no Q_limit, V_limit,
deficit, rollback/replay, or arbitration semantics (CouplingLib territory).

Usage: py -3 -m scau_preproc.coupling_maps <package_dir> <case.stcf.nc> <out_dir>
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


class MappingError(SystemExit):
    def __init__(self, message: str) -> None:
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
        faces = []
        for row in face_nodes:
            ring = [int(v) for v in row.compressed()] if hasattr(row, "compressed") else [
                int(v) for v in row if int(v) != -1
            ]
            faces.append(ring)
    finally:
        ds.close()
    return {"node_x": node_x, "node_y": node_y, "faces": faces}


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
    coordinates: dict[str, tuple[float, float]] = {}
    section = None
    for raw in inp_path.read_text(encoding="utf-8").splitlines():
        line = raw.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("["):
            section = line.upper()
            continue
        if section == "[COORDINATES]":
            parts = line.split()
            if len(parts) >= 3:
                coordinates[parts[0]] = (float(parts[1]), float(parts[2]))
    if not coordinates:
        raise MappingError(f"SWMM input has no [COORDINATES] section: {inp_path}")
    return coordinates


def read_csv_rows(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


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


def generate(package: Path, case_path: Path, out_dir: Path) -> dict:
    mesh = load_mesh(case_path)
    coordinates = read_swmm_coordinates(package / "swmm/model.inp")
    out_dir.mkdir(parents=True, exist_ok=True)

    surface_rows = read_csv_rows(package / "swmm/node_surface_mapping.csv")
    surface_relations = []
    simdriver_lines = ["version = 2"]
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

    roof_rows = read_csv_rows(package / "swmm/roof_drain_mapping.csv")
    centroids = building_centroids(package / "buildings/buildings.geojson")
    roof_relations = []
    for row in roof_rows:
        building = row["building_id"]
        if building not in centroids:
            raise MappingError(f"roof mapping references unknown building: {building}")
        bx, by = centroids[building]
        cell, distance = nearest_cell_by_centroid(mesh, bx, by)
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
        "chains": {
            "surface_to_swmm": {
                "relations": len(surface_relations),
                "methods": sorted({r["method"] for r in surface_relations}),
                "needs_confirmation": sum(
                    1 for r in surface_relations if r["review_status"] != "confirmed"
                ),
            },
            "roof_to_swmm": {
                "relations": len(roof_relations),
                "methods": sorted({r["method"] for r in roof_relations}),
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
    if len(sys.argv) != 4:
        raise MappingError(
            "usage: py -3 -m scau_preproc.coupling_maps <package_dir> <case.stcf.nc> <out_dir>"
        )
    report = generate(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]))
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
