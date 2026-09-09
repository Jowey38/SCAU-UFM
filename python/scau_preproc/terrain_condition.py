"""Terrain (DEM) conditioning under explicit policy authorization (M287-B3, stage B).

Any modification of an input DEM is a *repair* in the sense of the master plan
(v3 section 2.2): it must be authorized by a versioned policy and every change
must be auditable, otherwise the pipeline samples the DEM verbatim. B3 makes
three governed operations available, applied in a FIXED order so the result is
a pure function of (DEM, streams, policy):

  metadata/terrain_condition_policy.json  (terrain_condition_policy_schema_version = 1)
    enabled: true | false          # false -> stage is recorded as disabled and the
                                   #   pipeline output is BYTE-IDENTICAL to a run
                                   #   without any terrain policy
    authorization: {authorized_by, date, note}     # mandatory when enabled
    operations:
      reproject:          {enabled, method: nearest | bilinear, target_cellsize_m: null | float}
      stream_enforcement: {enabled, streams: "terrain/streams.geojson", burn_depth_m,
                           sample_spacing_factor}
      fill_depressions:   {enabled, algorithm: "priority_flood", algorithm_version: 1,
                           epsilon_m, connectivity: 4 | 8, nodata_is_outlet,
                           max_fill_depth_review_m: null | float}
    diagnostics: {write_rasters: true}

  order: reproject -> stream_enforcement -> fill_depressions

* reproject (takes over the raster leg B2 deliberately left out): the source
  grid is resampled onto a regular metre grid in the project target CRS
  (extent = transformed source corners, cellsize = source cellsize unless
  overridden). Each target cell centre is inverse-transformed with the pinned
  pyproj Transformer and sampled nearest / bilinear; cells outside the source
  extent become NoData. The PROJ pipeline string is recorded. Requires CRS
  governance (B2) to have run so that a declared target CRS exists.
* stream_enforcement: LineString features are sampled along their length at
  cellsize * sample_spacing_factor; every grid cell hit is lowered ONCE by
  burn_depth_m (never cumulative, never below NoData).
* fill_depressions: Priority-Flood (Barnes, Lehman & Mulla 2014), seeded from
  the grid rim and (optionally) from cells adjacent to NoData, which act as
  outlets. epsilon_m = 0 gives flat fills at the exact spill elevation;
  epsilon_m > 0 imposes a minimal drainage gradient. Heap ties are broken by
  (row, col) so the result is deterministic.

Outputs (<output>/conditioned_terrain/):
  dem.asc                           conditioned DEM consumed by the mesh generator
  depression_depth.asc              fill amount per cell (>= 0; NoData preserved)
  terrain_change.asc                conditioned - pre-conditioning (same target grid)
  terrain_condition_report.json     policy hash, authorization, per-operation
                                    statistics, before/after SHA-256, findings

Findings are review items (never fatal) except structural errors, which raise
TerrainConditionError (fail-closed; nothing is written). A fill deeper than
max_fill_depth_review_m yields a `TerrainFillDepthReview` finding so the
operator inspects depression_depth.asc in QGIS before trusting z_b.

The mesh generator never sees the policy: the pipeline points it at the
conditioned DEM path, so with `enabled: false` (or no policy) the generator
configuration and therefore the case bytes are unchanged.
"""

from __future__ import annotations

import hashlib
import heapq
import json
import math
from pathlib import Path

TERRAIN_POLICY_SCHEMA_VERSION = 1
TERRAIN_REPORT_SCHEMA_VERSION = 1
OPERATION_ORDER = ("reproject", "stream_enforcement", "fill_depressions")
FILL_ALGORITHM = "priority_flood"
FILL_ALGORITHM_VERSION = 1
REPROJECT_METHODS = ("nearest", "bilinear")
COORD_DECIMALS = 6


class TerrainConditionError(ValueError):
    def __init__(self, message: str, dataset: str | None = None) -> None:
        self.dataset = dataset
        super().__init__(message if dataset is None else f"{dataset}: {message}")


# --- policy -------------------------------------------------------------------


def _bool(block: dict, key: str, default: bool, where: str) -> bool:
    value = block.get(key, default)
    if not isinstance(value, bool):
        raise TerrainConditionError(f"{where}.{key} must be true/false")
    return value


def _positive(block: dict, key: str, default, where: str, allow_zero: bool = False, allow_none: bool = False):
    value = block.get(key, default)
    if value is None and allow_none:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)) or (value < 0 or (value == 0 and not allow_zero)):
        raise TerrainConditionError(f"{where}.{key} must be a {'non-negative' if allow_zero else 'positive'} number")
    return float(value)


def load_policy(path: Path) -> dict:
    path = Path(path)
    if not path.is_file():
        raise TerrainConditionError(f"terrain condition policy not found: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise TerrainConditionError(f"terrain condition policy is not valid JSON: {error}") from error
    if data.get("terrain_condition_policy_schema_version") != TERRAIN_POLICY_SCHEMA_VERSION:
        raise TerrainConditionError(
            f"unsupported terrain_condition_policy_schema_version {data.get('terrain_condition_policy_schema_version')!r}")
    enabled = _bool(data, "enabled", False, "policy")
    operations = data.get("operations") or {}
    if not isinstance(operations, dict):
        raise TerrainConditionError("operations must be an object")
    unknown = set(operations) - set(OPERATION_ORDER)
    if unknown:
        raise TerrainConditionError(f"unknown operations {sorted(unknown)}; allowed: {list(OPERATION_ORDER)}")

    reproject = operations.get("reproject") or {}
    stream = operations.get("stream_enforcement") or {}
    fill = operations.get("fill_depressions") or {}
    policy = {
        "path": str(path),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "enabled": enabled,
        "authorization": data.get("authorization"),
        "reproject": {
            "enabled": _bool(reproject, "enabled", False, "reproject"),
            "method": reproject.get("method", "nearest"),
            "target_cellsize_m": _positive(reproject, "target_cellsize_m", None, "reproject", allow_none=True),
        },
        "stream_enforcement": {
            "enabled": _bool(stream, "enabled", False, "stream_enforcement"),
            "streams": stream.get("streams", "terrain/streams.geojson"),
            "burn_depth_m": _positive(stream, "burn_depth_m", 1.0, "stream_enforcement"),
            "sample_spacing_factor": _positive(stream, "sample_spacing_factor", 0.5, "stream_enforcement"),
        },
        "fill_depressions": {
            "enabled": _bool(fill, "enabled", False, "fill_depressions"),
            "algorithm": fill.get("algorithm", FILL_ALGORITHM),
            "algorithm_version": fill.get("algorithm_version", FILL_ALGORITHM_VERSION),
            "epsilon_m": _positive(fill, "epsilon_m", 0.0, "fill_depressions", allow_zero=True),
            "connectivity": fill.get("connectivity", 8),
            "nodata_is_outlet": _bool(fill, "nodata_is_outlet", True, "fill_depressions"),
            "max_fill_depth_review_m": _positive(fill, "max_fill_depth_review_m", None, "fill_depressions", allow_none=True),
        },
        "write_rasters": _bool(data.get("diagnostics") or {}, "write_rasters", True, "diagnostics"),
    }
    if policy["reproject"]["method"] not in REPROJECT_METHODS:
        raise TerrainConditionError(f"reproject.method must be one of {REPROJECT_METHODS}")
    if policy["fill_depressions"]["algorithm"] != FILL_ALGORITHM:
        raise TerrainConditionError(f"fill_depressions.algorithm must be {FILL_ALGORITHM!r} (the only governed algorithm)")
    if policy["fill_depressions"]["algorithm_version"] != FILL_ALGORITHM_VERSION:
        raise TerrainConditionError(f"fill_depressions.algorithm_version must be {FILL_ALGORITHM_VERSION}")
    if policy["fill_depressions"]["connectivity"] not in (4, 8):
        raise TerrainConditionError("fill_depressions.connectivity must be 4 or 8")
    if not isinstance(policy["stream_enforcement"]["streams"], str) or not policy["stream_enforcement"]["streams"]:
        raise TerrainConditionError("stream_enforcement.streams must be a relative path")
    active = [op for op in OPERATION_ORDER if policy[op]["enabled"]]
    policy["active_operations"] = active if enabled else []
    if enabled:
        if not active:
            raise TerrainConditionError("enabled policy authorizes no operation (enable one or set enabled=false)")
        auth = policy["authorization"]
        if not isinstance(auth, dict) or not str(auth.get("authorized_by", "")).strip() or not auth.get("date"):
            raise TerrainConditionError("enabled policy needs authorization {authorized_by, date} (modifying the DEM is a repair)")
    return policy


# --- ASCII grid I/O -------------------------------------------------------------


def read_ascii_grid(path: Path) -> dict:
    lines = Path(path).read_text(encoding="utf-8").split("\n")
    header: dict[str, float] = {}
    row_start = 0
    for index, line in enumerate(lines):
        parts = line.split()
        if len(parts) == 2 and parts[0].lower() in {"ncols", "nrows", "xllcorner", "yllcorner", "cellsize", "nodata_value"}:
            header[parts[0].lower()] = float(parts[1])
            row_start = index + 1
        elif parts:
            break
    for key in ("ncols", "nrows", "xllcorner", "yllcorner", "cellsize"):
        if key not in header:
            raise TerrainConditionError(f"ASCII grid {path} lacks header key {key}", "dem")
    ncols, nrows = int(header["ncols"]), int(header["nrows"])
    values: list[list[float]] = []
    for line in lines[row_start:]:
        parts = line.split()
        if parts:
            values.append([float(v) for v in parts])
    if len(values) != nrows or any(len(row) != ncols for row in values):
        raise TerrainConditionError(f"ASCII grid shape mismatch in {path}", "dem")
    return {
        "ncols": ncols, "nrows": nrows,
        "xllcorner": header["xllcorner"], "yllcorner": header["yllcorner"],
        "cellsize": header["cellsize"],
        "nodata_value": header.get("nodata_value", -9999.0),
        "values": values,   # row 0 = top (north)
    }


def _fmt(value: float) -> str:
    if math.isfinite(value) and value == int(value) and abs(value) < 1e15:
        return str(int(value))
    return repr(float(value))


def write_ascii_grid(path: Path, grid: dict) -> None:
    lines = [
        f"ncols         {grid['ncols']}",
        f"nrows         {grid['nrows']}",
        f"xllcorner     {_fmt(grid['xllcorner'])}",
        f"yllcorner     {_fmt(grid['yllcorner'])}",
        f"cellsize      {_fmt(grid['cellsize'])}",
        f"NODATA_value  {_fmt(grid['nodata_value'])}",
    ]
    lines.extend(" ".join(_fmt(v) for v in row) for row in grid["values"])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _copy_grid(grid: dict) -> dict:
    return {**grid, "values": [row[:] for row in grid["values"]]}


def _is_nodata(grid: dict, value: float) -> bool:
    return value == grid["nodata_value"]


def cell_of(grid: dict, x: float, y: float) -> tuple[int, int] | None:
    """(row, col) of the cell containing (x, y); None outside the grid."""
    col = math.floor((x - grid["xllcorner"]) / grid["cellsize"])
    row_from_bottom = math.floor((y - grid["yllcorner"]) / grid["cellsize"])
    if not (0 <= col < grid["ncols"] and 0 <= row_from_bottom < grid["nrows"]):
        return None
    return grid["nrows"] - 1 - row_from_bottom, col


def cell_center(grid: dict, row: int, col: int) -> tuple[float, float]:
    x = grid["xllcorner"] + (col + 0.5) * grid["cellsize"]
    y = grid["yllcorner"] + (grid["nrows"] - 1 - row + 0.5) * grid["cellsize"]
    return x, y


# --- operation 1: reprojection --------------------------------------------------


def _sample_source(grid: dict, x: float, y: float, method: str) -> float:
    nodata = grid["nodata_value"]
    if method == "nearest":
        rc = cell_of(grid, x, y)
        return nodata if rc is None else grid["values"][rc[0]][rc[1]]
    # bilinear on cell centres; any NoData corner -> NoData (never invent terrain)
    cs = grid["cellsize"]
    fx = (x - grid["xllcorner"]) / cs - 0.5
    fy = (y - grid["yllcorner"]) / cs - 0.5
    c0, r0b = math.floor(fx), math.floor(fy)
    tx, ty = fx - c0, fy - r0b
    result = 0.0
    for dc, wx in ((0, 1.0 - tx), (1, tx)):
        for dr, wy in ((0, 1.0 - ty), (1, ty)):
            c, rb = c0 + dc, r0b + dr
            if not (0 <= c < grid["ncols"] and 0 <= rb < grid["nrows"]):
                return nodata
            v = grid["values"][grid["nrows"] - 1 - rb][c]
            if v == nodata:
                return nodata
            result += wx * wy * v
    return result


def reproject_grid(grid: dict, source_crs: str, target_crs: str, method: str, target_cellsize: float | None) -> tuple[dict, dict]:
    """Resamples `grid` (in source_crs) onto a regular grid in target_crs.
    Returns (target_grid, audit)."""
    from pyproj import CRS, Transformer
    source = CRS.from_user_input(source_crs)
    target = CRS.from_user_input(target_crs)
    if source.is_geographic or target.is_geographic:
        raise TerrainConditionError("geographic CRS is never accepted for the DEM", "dem")
    forward = Transformer.from_crs(source, target, always_xy=True)
    inverse = Transformer.from_crs(target, source, always_xy=True)
    cs_src = grid["cellsize"]
    x0, y0 = grid["xllcorner"], grid["yllcorner"]
    x1, y1 = x0 + grid["ncols"] * cs_src, y0 + grid["nrows"] * cs_src
    rim = [(x0, y0), (x1, y0), (x1, y1), (x0, y1),
           ((x0 + x1) / 2, y0), ((x0 + x1) / 2, y1), (x0, (y0 + y1) / 2), (x1, (y0 + y1) / 2)]
    transformed = [forward.transform(x, y) for x, y in rim]
    if not all(math.isfinite(x) and math.isfinite(y) for x, y in transformed):
        raise TerrainConditionError("DEM extent transforms to non-finite coordinates", "dem")
    min_x = round(min(p[0] for p in transformed), COORD_DECIMALS)
    min_y = round(min(p[1] for p in transformed), COORD_DECIMALS)
    max_x = max(p[0] for p in transformed)
    max_y = max(p[1] for p in transformed)
    cs = float(target_cellsize) if target_cellsize else cs_src
    ncols = max(1, math.ceil((max_x - min_x) / cs - 1e-9))
    nrows = max(1, math.ceil((max_y - min_y) / cs - 1e-9))
    target_grid = {"ncols": ncols, "nrows": nrows, "xllcorner": min_x, "yllcorner": min_y,
                   "cellsize": cs, "nodata_value": grid["nodata_value"], "values": []}
    nodata_cells = 0
    for row in range(nrows):
        line = []
        for col in range(ncols):
            tx, ty = cell_center(target_grid, row, col)
            sx, sy = inverse.transform(tx, ty)
            value = _sample_source(grid, sx, sy, method) if math.isfinite(sx) and math.isfinite(sy) else grid["nodata_value"]
            if value == grid["nodata_value"]:
                nodata_cells += 1
            line.append(value)
        target_grid["values"].append(line)
    audit = {
        "source_crs": source_crs, "target_crs": target_crs, "method": method,
        "proj_pipeline": forward.definition,
        "source_grid": {k: grid[k] for k in ("ncols", "nrows", "xllcorner", "yllcorner", "cellsize")},
        "target_grid": {k: target_grid[k] for k in ("ncols", "nrows", "xllcorner", "yllcorner", "cellsize")},
        "nodata_cells": nodata_cells,
    }
    return target_grid, audit


# --- operation 2: stream enforcement -------------------------------------------


def _line_strings(geometry: dict) -> list[list[list[float]]]:
    if not geometry:
        return []
    if geometry.get("type") == "LineString":
        return [geometry["coordinates"]]
    if geometry.get("type") == "MultiLineString":
        return list(geometry["coordinates"])
    return []


def burn_streams(grid: dict, streams_path: Path, burn_depth: float, spacing_factor: float,
                 streams_label: str | None = None) -> tuple[dict, dict]:
    data = json.loads(Path(streams_path).read_text(encoding="utf-8"))
    features = data.get("features", [])
    spacing = grid["cellsize"] * spacing_factor
    burned: set[tuple[int, int]] = set()
    lines = 0
    for feature in features:
        for line in _line_strings(feature.get("geometry") or {}):
            lines += 1
            for (ax, ay), (bx, by) in zip(line[:-1], line[1:]):
                length = math.hypot(bx - ax, by - ay)
                steps = max(1, math.ceil(length / spacing))
                for k in range(steps + 1):
                    t = k / steps
                    rc = cell_of(grid, ax + t * (bx - ax), ay + t * (by - ay))
                    if rc is not None and not _is_nodata(grid, grid["values"][rc[0]][rc[1]]):
                        burned.add(rc)
    out = _copy_grid(grid)
    for row, col in burned:
        out["values"][row][col] = grid["values"][row][col] - burn_depth
    audit = {"streams": streams_label or str(streams_path),
             "streams_sha256": hashlib.sha256(Path(streams_path).read_bytes()).hexdigest(),
             "features": len(features), "line_strings": lines, "cells_burned": len(burned),
             "burn_depth_m": burn_depth, "sample_spacing_m": spacing}
    return out, audit


# --- operation 3: Priority-Flood depression filling ----------------------------


def priority_flood(grid: dict, epsilon: float = 0.0, connectivity: int = 8, nodata_is_outlet: bool = True) -> tuple[dict, dict]:
    """Barnes et al. (2014) Priority-Flood. Returns (filled_grid, audit)."""
    nrows, ncols = grid["nrows"], grid["ncols"]
    nodata = grid["nodata_value"]
    values = grid["values"]
    filled = [row[:] for row in values]
    closed = [[False] * ncols for _ in range(nrows)]
    heap: list[tuple[float, int, int]] = []
    offsets = ((-1, 0), (1, 0), (0, -1), (0, 1)) if connectivity == 4 else (
        (-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1))

    def is_seed(r: int, c: int) -> bool:
        if r in (0, nrows - 1) or c in (0, ncols - 1):
            return True
        if nodata_is_outlet:
            return any(values[r + dr][c + dc] == nodata for dr, dc in offsets)
        return False

    seeds = 0
    for r in range(nrows):
        for c in range(ncols):
            if values[r][c] == nodata:
                closed[r][c] = True
            elif is_seed(r, c):
                closed[r][c] = True
                heapq.heappush(heap, (values[r][c], r, c))
                seeds += 1
    while heap:
        elev, r, c = heapq.heappop(heap)
        for dr, dc in offsets:
            nr, nc = r + dr, c + dc
            if not (0 <= nr < nrows and 0 <= nc < ncols) or closed[nr][nc]:
                continue
            closed[nr][nc] = True
            if epsilon > 0.0:
                if filled[nr][nc] <= elev:
                    filled[nr][nc] = elev + epsilon
            elif filled[nr][nc] < elev:
                filled[nr][nc] = elev
            heapq.heappush(heap, (filled[nr][nc], nr, nc))

    cells_filled = 0
    max_depth = 0.0
    volume = 0.0
    area = grid["cellsize"] ** 2
    for r in range(nrows):
        for c in range(ncols):
            if values[r][c] == nodata:
                continue
            depth = filled[r][c] - values[r][c]
            if depth > 0.0:
                cells_filled += 1
                max_depth = max(max_depth, depth)
                volume += depth * area
    out = {**grid, "values": filled}
    audit = {"algorithm": FILL_ALGORITHM, "algorithm_version": FILL_ALGORITHM_VERSION,
             "reference": "Barnes, Lehman & Mulla (2014) Priority-Flood",
             "epsilon_m": epsilon, "connectivity": connectivity, "nodata_is_outlet": nodata_is_outlet,
             "seed_cells": seeds, "cells_filled": cells_filled, "max_fill_depth_m": max_depth,
             "fill_volume_m3": volume}
    return out, audit


# --- stage driver -------------------------------------------------------------


def _difference_grid(after: dict, before: dict) -> dict:
    nodata = after["nodata_value"]
    values = []
    for row_a, row_b in zip(after["values"], before["values"]):
        values.append([nodata if (a == nodata or b == nodata) else a - b for a, b in zip(row_a, row_b)])
    return {**after, "values": values}


def _grid_sha(grid: dict) -> str:
    payload = json.dumps({k: grid[k] for k in ("ncols", "nrows", "xllcorner", "yllcorner", "cellsize", "nodata_value", "values")},
                         separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def condition_package(package: Path, policy_path: Path, output: Path, *, target_crs: str | None = None,
                      dem_source_crs: str | None = None) -> dict:
    """Runs the authorized operations on <package>/terrain/dem.asc and writes
    the conditioned DEM + diagnostics + report into `output`. Returns the
    report dict (also written as terrain_condition_report.json). With
    `enabled: false` nothing is written and the report says so."""
    package, output = Path(package), Path(output)
    policy = load_policy(policy_path)
    dem_path = package / "terrain/dem.asc"
    if not dem_path.is_file():
        raise TerrainConditionError(f"DEM not found: {dem_path}", "dem")
    report = {
        "terrain_report_schema_version": TERRAIN_REPORT_SCHEMA_VERSION,
        "policy": policy["path"],
        "policy_sha256": policy["sha256"],
        "enabled": policy["enabled"],
        "authorization": policy["authorization"] if policy["enabled"] else None,
        "operation_order": list(OPERATION_ORDER),
        "active_operations": policy["active_operations"],
        "source_dem": str(dem_path),
        "source_dem_sha256": hashlib.sha256(dem_path.read_bytes()).hexdigest(),
        "conditioned_dem": None,
        "conditioned_dem_sha256": None,
        "operations": {},
        "change_summary": None,
        "diagnostic_rasters": {},
        "findings": [],
    }
    if not policy["enabled"]:
        report["note"] = "policy disabled: DEM consumed verbatim; pipeline output identical to a run without a terrain policy"
        return report

    grid = read_ascii_grid(dem_path)
    if policy["reproject"]["enabled"]:
        if not target_crs:
            raise TerrainConditionError("reproject requires CRS governance (a declared target CRS); add metadata/crs_policy.json", "dem")
        if not dem_source_crs:
            raise TerrainConditionError("reproject requires the DEM source CRS (terrain/dem_metadata.json source_crs or crs_policy override)", "dem")
        from pyproj import CRS
        if CRS.from_user_input(dem_source_crs) == CRS.from_user_input(target_crs):
            report["operations"]["reproject"] = {"action": "identity_skipped", "source_crs": dem_source_crs, "target_crs": target_crs}
        else:
            grid, audit = reproject_grid(grid, dem_source_crs, target_crs, policy["reproject"]["method"],
                                         policy["reproject"]["target_cellsize_m"])
            report["operations"]["reproject"] = {"action": "reprojected", **audit}
    baseline = _copy_grid(grid)   # pre-conditioning grid in the TARGET frame

    if policy["stream_enforcement"]["enabled"]:
        streams_path = package / policy["stream_enforcement"]["streams"]
        if not streams_path.is_file():
            raise TerrainConditionError(f"streams not found: {streams_path}", "streams")
        grid, audit = burn_streams(grid, streams_path, policy["stream_enforcement"]["burn_depth_m"],
                                   policy["stream_enforcement"]["sample_spacing_factor"],
                                   streams_label=policy["stream_enforcement"]["streams"])
        report["operations"]["stream_enforcement"] = audit
        if audit["cells_burned"] == 0:
            report["findings"].append({"severity": "review", "code": "TerrainStreamsMissedGrid",
                                       "detail": "stream enforcement enabled but no stream sample fell on a valid DEM cell",
                                       "objects": [{"feature_id": "streams", "kind": "dataset", "violation": "terrain_condition"}]})

    pre_fill = _copy_grid(grid)
    if policy["fill_depressions"]["enabled"]:
        fp = policy["fill_depressions"]
        grid, audit = priority_flood(grid, fp["epsilon_m"], fp["connectivity"], fp["nodata_is_outlet"])
        report["operations"]["fill_depressions"] = audit
        limit = fp["max_fill_depth_review_m"]
        if limit is not None and audit["max_fill_depth_m"] > limit:
            report["findings"].append({"severity": "review", "code": "TerrainFillDepthReview",
                                       "detail": f"max fill depth {audit['max_fill_depth_m']:.3f} m exceeds review threshold {limit} m; "
                                                 "inspect depression_depth.asc",
                                       "objects": [{"feature_id": "dem", "kind": "dataset", "violation": "terrain_condition"}]})

    # Change summary against the pre-conditioning grid in the same frame.
    nodata = grid["nodata_value"]
    changed, max_abs, total, count = 0, 0.0, 0.0, 0
    for row_a, row_b in zip(grid["values"], baseline["values"]):
        for a, b in zip(row_a, row_b):
            if a == nodata or b == nodata:
                continue
            count += 1
            d = a - b
            if d != 0.0:
                changed += 1
                max_abs = max(max_abs, abs(d))
                total += d
    report["change_summary"] = {"valid_cells": count, "cells_changed": changed, "max_abs_change_m": max_abs,
                                "net_change_m3": total * grid["cellsize"] ** 2,
                                "grid": {k: grid[k] for k in ("ncols", "nrows", "xllcorner", "yllcorner", "cellsize")}}

    output.mkdir(parents=True, exist_ok=True)
    conditioned_path = output / "dem.asc"
    write_ascii_grid(conditioned_path, grid)
    report["conditioned_dem"] = str(conditioned_path)
    report["conditioned_dem_sha256"] = hashlib.sha256(conditioned_path.read_bytes()).hexdigest()
    report["conditioned_grid_sha256"] = _grid_sha(grid)
    if policy["write_rasters"]:
        depth_path = output / "depression_depth.asc"
        change_path = output / "terrain_change.asc"
        write_ascii_grid(depth_path, _difference_grid(grid, pre_fill))
        write_ascii_grid(change_path, _difference_grid(grid, baseline))
        report["diagnostic_rasters"] = {"depression_depth": str(depth_path), "terrain_change": str(change_path)}
    (output / "terrain_condition_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report
