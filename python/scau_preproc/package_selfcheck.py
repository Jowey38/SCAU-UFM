"""Input-package self-check (A6): cheap, engine-independent lint that catches the
defect classes the synthetic D-5 template shipped with before they hide behind
a green pipeline run:

  * SWMM `.inp` sections outside the SWMM 5.2 section list (the sample carried
    a bogus `[END]` that only the real engine rejected: ERROR 205);
  * overlapping landcover polygons without an explicit `landcover_overlap` rule
    (23 pond cells were silently meshed as grass by the v1 first-match lookup);
  * manifest datasets that are missing / unlisted files present;
  * required policies present and schema-versioned;
  * DEM extent covering the computational boundary; NoData inside the boundary;
  * mapping tables referencing SWMM nodes that the `.inp` does not define;
  * placeholder markers (`TODO_PROVIDER`) left in a package that claims
    `synthetic_data: false`.

Usage: py -3 -m scau_preproc.package_selfcheck <package_dir> [--json out.json]
Exit 0 when no fatal finding, 2 otherwise. Findings use the pipeline's
{severity, code, detail, objects[]} shape so the QGIS report browser can list
them. This is a pre-flight; certification stays with `scau_preproc validate`
and the real engines.
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

SWMM_SECTIONS = {
    "TITLE", "OPTIONS", "REPORT", "FILES", "RAINGAGES", "EVAPORATION", "TEMPERATURE", "ADJUSTMENTS",
    "SUBCATCHMENTS", "SUBAREAS", "INFILTRATION", "LID_CONTROLS", "LID_USAGE", "AQUIFERS", "GROUNDWATER",
    "GWF", "SNOWPACKS", "JUNCTIONS", "OUTFALLS", "DIVIDERS", "STORAGE", "CONDUITS", "PUMPS", "ORIFICES",
    "WEIRS", "OUTLETS", "XSECTIONS", "TRANSECTS", "STREETS", "INLETS", "INLET_USAGE", "LOSSES", "CONTROLS",
    "POLLUTANTS", "LANDUSES", "COVERAGES", "LOADINGS", "BUILDUP", "WASHOFF", "TREATMENT", "INFLOWS",
    "DWF", "PATTERNS", "RDII", "HYDROGRAPHS", "CURVES", "TIMESERIES", "MAP", "COORDINATES", "VERTICES",
    "POLYGONS", "SYMBOLS", "LABELS", "BACKDROP", "PROFILES", "TAGS",
}
NODE_SECTIONS = ("JUNCTIONS", "OUTFALLS", "DIVIDERS", "STORAGE")
POLICIES = {
    "metadata/geometry_clean_policy.json": "policy_version",
    "metadata/crs_policy.json": "crs_policy_schema_version",
    "metadata/terrain_condition_policy.json": "terrain_condition_policy_schema_version",
    "metadata/dpm_rule_table.json": "dpm_rule_table_schema_version",
}
# Files the pipeline / UI create beside the package; not manifest datasets.
IGNORED_PREFIXES = ("coupling/confirmed/", "metadata/field_mapping", "README", "validation/", "inputs/",
                    "reference/", "manifest.json", "metadata/", "swmm/roof_drain_mapping.csv",
                    "swmm/node_surface_mapping.csv", "terrain/dem_metadata.json", "terrain/streams.geojson")


def finding(severity: str, code: str, detail: str, *objects) -> dict:
    return {"severity": severity, "code": code, "detail": detail,
            "objects": [{"feature_id": fid, "kind": kind, "violation": code} for fid, kind in objects]}


# --- helpers --------------------------------------------------------------------


def _ring_area(ring) -> float:
    return 0.5 * sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(ring, ring[1:] + ring[:1]))


def _point_in_ring(x, y, ring) -> bool:
    inside = False
    for (x1, y1), (x2, y2) in zip(ring, ring[1:]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


def _segments_cross(p1, p2, p3, p4) -> bool:
    def orient(a, b, c):
        v = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        return 0 if v == 0 else (1 if v > 0 else -1)
    o1, o2, o3, o4 = orient(p1, p2, p3), orient(p1, p2, p4), orient(p3, p4, p1), orient(p3, p4, p2)
    return o1 != o2 and o3 != o4 and 0 not in (o1, o2, o3, o4)


def polygons_overlap(a, b) -> str | None:
    """'contains' / 'contained' / 'crosses' / None (touching edges are not overlap)."""
    for (p1, p2) in zip(a, a[1:]):
        for (p3, p4) in zip(b, b[1:]):
            if _segments_cross(p1, p2, p3, p4):
                return "crosses"
    if all(_point_in_ring(x, y, a) for x, y in b[:-1]):
        return "contains"
    if all(_point_in_ring(x, y, b) for x, y in a[:-1]):
        return "contained"
    return None


def _polygons(path: Path, id_field: str) -> list[tuple[str, list]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    out = []
    for i, f in enumerate(data.get("features", [])):
        g = f.get("geometry") or {}
        if g.get("type") == "Polygon":
            out.append((str((f.get("properties") or {}).get(id_field, f"feature_{i}")),
                        [tuple(map(float, xy)) for xy in g["coordinates"][0]]))
    return out


# --- checks -----------------------------------------------------------------------


def check_manifest(package: Path, manifest: dict) -> list[dict]:
    out = []
    listed = set()
    for d in manifest.get("datasets", []):
        rel = d["path"]
        listed.add(rel)
        path = package / rel
        exists = path.is_file() or (rel.endswith("/") and path.is_dir())
        if not exists and d.get("required"):
            out.append(finding("fatal", "ManifestRequiredMissing", f"required dataset file missing: {rel}", (d["id"], "dataset")))
        elif not exists:
            out.append(finding("info", "ManifestOptionalAbsent", f"optional dataset not provided: {rel}", (d["id"], "dataset")))
    for path in sorted(package.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(package).as_posix()
        if rel in listed or rel.startswith(IGNORED_PREFIXES) or any(rel.startswith(p.rstrip("/") + "/") for p in listed if p.endswith("/")):
            continue
        out.append(finding("review", "FileNotInManifest", f"file present but not a manifest dataset: {rel}", (rel, "file")))
    if not manifest.get("synthetic_data", False):
        for path in sorted(package.rglob("*")):
            if path.suffix in (".json", ".yaml", ".csv", ".md") and path.is_file() and "TODO_PROVIDER" in path.read_text(encoding="utf-8", errors="ignore"):
                out.append(finding("fatal", "PlaceholderInRealPackage",
                                   f"TODO_PROVIDER left in {path.relative_to(package).as_posix()} while synthetic_data is false",
                                   (path.relative_to(package).as_posix(), "file")))
    return out


def check_policies(package: Path) -> list[dict]:
    out = []
    for rel, key in POLICIES.items():
        path = package / rel
        if not path.is_file():
            severity = "fatal" if rel.endswith("geometry_clean_policy.json") else "review"
            out.append(finding(severity, "PolicyAbsent", f"{rel} absent" + ("" if severity == "fatal" else " (stage runs in v1 unchecked/placeholder mode)"), (rel, "policy")))
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as error:
            out.append(finding("fatal", "PolicyUnreadable", f"{rel}: {error}", (rel, "policy")))
            continue
        if key not in data:
            out.append(finding("fatal", "PolicyUnversioned", f"{rel} lacks {key}", (rel, "policy")))
    return out


def check_swmm(package: Path, manifest: dict) -> list[dict]:
    out = []
    inp = package / "swmm/model.inp"
    if not inp.is_file():
        return out
    section = None
    nodes: set[str] = set()
    coords: set[str] = set()
    for raw in inp.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("["):
            section = line.strip("[]").upper()
            if section not in SWMM_SECTIONS:
                out.append(finding("fatal", "SwmmUnknownSection",
                                   f"[{section}] is not a SWMM 5.2 section (real SWMM rejects it: ERROR 205)", (section, "swmm:section")))
            continue
        if section in NODE_SECTIONS:
            nodes.add(line.split()[0])
        elif section == "COORDINATES":
            coords.add(line.split()[0])
        elif section in ("SUBCATCHMENTS", "SUBAREAS", "INFILTRATION", "RAINGAGES"):
            out.append(finding("fatal", "SwmmRunoffSectionPresent",
                               f"[{section}] present: runoff generation belongs to Surface2D (M247); SWMM must not generate runoff",
                               (section, "swmm:section")))
            section = "__reported__"
    for node in sorted(nodes - coords):
        out.append(finding("review", "SwmmNodeWithoutCoordinates", f"node {node} has no [COORDINATES] entry (spatial candidates impossible)", (node, "swmm:node")))
    for rel, column in (("swmm/node_surface_mapping.csv", "swmm_node_id"), ("swmm/roof_drain_mapping.csv", "swmm_node_id")):
        path = package / rel
        if not path.is_file():
            continue
        with path.open(encoding="utf-8", newline="") as handle:
            for row in csv.DictReader(handle):
                node = (row.get(column) or "").strip()
                if node and "TODO" not in node and node not in nodes:
                    out.append(finding("fatal", "MappingNodeUnknown", f"{rel}: {node} is not a node of swmm/model.inp", (node, "swmm:node")))
    return out


def check_landcover(package: Path) -> list[dict]:
    out = []
    path = package / "landcover/landcover.geojson"
    if not path.is_file():
        return out
    polys = _polygons(path, "class_code")
    rule = None
    table = package / "metadata/dpm_rule_table.json"
    if table.is_file():
        try:
            rule = json.loads(table.read_text(encoding="utf-8")).get("landcover_overlap")
        except ValueError:
            rule = None
    for i, (ida, a) in enumerate(polys):
        for idb, b in polys[i + 1:]:
            relation = polygons_overlap(a, b)
            if relation:
                severity = "review" if rule == "smallest_area_wins" else "fatal"
                out.append(finding(severity, "LandcoverOverlap",
                                   f"landcover polygons {ida!r} and {idb!r} overlap ({relation}); "
                                   + ("rule smallest_area_wins declared" if rule == "smallest_area_wins"
                                      else "v1 first-match lookup would silently pick one; declare landcover_overlap in dpm_rule_table.json"),
                                   (ida, "landcover"), (idb, "landcover")))
    return out


def check_terrain(package: Path) -> list[dict]:
    out = []
    dem = package / "terrain/dem.asc"
    boundary = package / "boundary/computational_boundary.geojson"
    if not dem.is_file() or not boundary.is_file():
        return out
    from scau_preproc.terrain_condition import read_ascii_grid, cell_of
    try:
        grid = read_ascii_grid(dem)
    except Exception as error:  # noqa: BLE001 - report as finding
        return [finding("fatal", "DemUnreadable", str(error), ("dem", "dataset"))]
    rings = _polygons(boundary, "boundary_id")
    if len(rings) != 1:
        out.append(finding("fatal", "BoundaryCount", f"exactly one boundary polygon required, found {len(rings)}", ("computational_boundary", "dataset")))
        return out
    ring = rings[0][1]
    xs, ys = [p[0] for p in ring], [p[1] for p in ring]
    x0, y0 = grid["xllcorner"], grid["yllcorner"]
    x1, y1 = x0 + grid["ncols"] * grid["cellsize"], y0 + grid["nrows"] * grid["cellsize"]
    if min(xs) < x0 or max(xs) > x1 or min(ys) < y0 or max(ys) > y1:
        out.append(finding("fatal", "DemDoesNotCoverBoundary",
                           f"DEM extent [{x0},{x1}]x[{y0},{y1}] does not contain the boundary bbox "
                           f"[{min(xs)},{max(xs)}]x[{min(ys)},{max(ys)}] (nearest-sample clamping would invent terrain)",
                           ("dem", "dataset")))
    nodata_inside = 0
    for r in range(grid["nrows"]):
        for c in range(grid["ncols"]):
            if grid["values"][r][c] == grid["nodata_value"]:
                x = x0 + (c + 0.5) * grid["cellsize"]
                y = y0 + (grid["nrows"] - 1 - r + 0.5) * grid["cellsize"]
                if _point_in_ring(x, y, ring):
                    nodata_inside += 1
    if nodata_inside:
        out.append(finding("review", "DemNoDataInsideBoundary", f"{nodata_inside} NoData DEM cell(s) inside the boundary; sampling there is fatal", ("dem", "dataset")))
    if _ring_area(ring[:-1]) == 0.0:
        out.append(finding("fatal", "BoundaryDegenerate", "boundary polygon has zero area", ("computational_boundary", "dataset")))
    _ = cell_of  # imported for parity with the terrain module; extent check above is sufficient here
    return out


def run(package: Path) -> dict:
    package = Path(package)
    manifest_path = package / "manifest.json"
    if not manifest_path.is_file():
        return {"package": str(package), "status": "fatal",
                "findings": [finding("fatal", "ManifestMissing", "manifest.json missing", ("manifest.json", "file"))]}
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    findings = []
    findings += check_manifest(package, manifest)
    findings += check_policies(package)
    findings += check_swmm(package, manifest)
    findings += check_landcover(package)
    findings += check_terrain(package)
    status = "fatal" if any(f["severity"] == "fatal" for f in findings) else (
        "review" if any(f["severity"] == "review" for f in findings) else "ok")
    return {"package_selfcheck_schema_version": 1, "package": str(package), "synthetic_data": bool(manifest.get("synthetic_data")),
            "status": status, "findings": findings,
            "note": "pre-flight only; certification = scau_preproc validate + real engine cold start (G35 pattern)"}


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: py -3 -m scau_preproc.package_selfcheck <package_dir> [--json out.json]", file=sys.stderr)
        return 2
    report = run(Path(argv[1]))
    if "--json" in argv:
        Path(argv[argv.index("--json") + 1]).write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for f in report["findings"]:
        print(f"{f['severity']:<6} {f['code']:<28} {f['detail']}")
    print(f"status: {report['status']}")
    return 0 if report["status"] != "fatal" else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
