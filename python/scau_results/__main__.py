"""Inspect and summarize completed surface timeseries without modifying inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import netCDF4
import numpy as np


SCHEMA_VERSION = "2"


def fnv1a64(data: bytes) -> str:
    """Repo-convention content hash, byte-compatible with the C++ writer."""
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{h:016x}"


def load_manifest(path: Path, raw: bytes) -> dict:
    manifest_path = Path(str(path) + ".manifest.json")
    if not manifest_path.is_file():
        raise ValueError("provenance validation failure: sidecar manifest is missing")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("manifest_schema_version") != 1:
        raise ValueError("migration required: unsupported manifest schema")
    if manifest.get("output") != path.name:
        raise ValueError("provenance validation failure: manifest names a different output")
    if manifest.get("output_hash") != fnv1a64(raw):
        raise ValueError("provenance validation failure: output bytes do not match manifest")
    return manifest


def _point_in_polygon(x: float, y: float, poly: np.ndarray) -> bool:
    """Even-odd rule with boundary points counted as inside."""
    n = len(poly)
    inside = False
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        cross = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)
        if abs(cross) <= 1e-9 * max(1.0, abs(x2 - x1) + abs(y2 - y1)) and \
                min(x1, x2) - 1e-9 <= x <= max(x1, x2) + 1e-9 and min(y1, y2) - 1e-9 <= y <= max(y1, y2) + 1e-9:
            return True
        if (y1 > y) != (y2 > y):
            x_int = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x < x_int:
                inside = not inside
    return inside


def mesh_polygons(ds) -> list[np.ndarray]:
    """Face polygons in the mesh's own projected metres; fail-closed on other frames."""
    required = ("mesh2_node_x", "mesh2_node_y", "mesh2_face_nodes")
    if any(name not in ds.variables for name in required):
        raise ValueError("linkage failure: result lacks UGRID node coordinates / face connectivity")
    for axis, std in (("mesh2_node_x", "projection_x_coordinate"), ("mesh2_node_y", "projection_y_coordinate")):
        if getattr(ds[axis], "standard_name", None) != std or getattr(ds[axis], "units", None) != "m":
            raise ValueError(f"linkage failure: {axis} is not a projected metre coordinate")
    nx, ny = np.asarray(ds["mesh2_node_x"][:]), np.asarray(ds["mesh2_node_y"][:])
    faces = ds["mesh2_face_nodes"][:]
    fill = getattr(ds["mesh2_face_nodes"], "_FillValue", None)
    start = int(getattr(ds["mesh2_face_nodes"], "start_index", 0))
    faces = np.ma.filled(faces, -1 if fill is None else fill)
    polys = []
    for row in faces:
        idx = [int(v) - start for v in row if (fill is None or v != fill) and v >= 0]
        if len(idx) < 3:
            raise ValueError("linkage failure: degenerate face in connectivity")
        polys.append(np.column_stack([nx[idx], ny[idx]]))
    return polys


def locate_cells(ds, points: list[tuple[float, float]]) -> list[int]:
    """Map (x, y) to cell indices; outside-mesh or non-finite points are linkage failures.
    A point on a shared edge resolves to the lowest index (deterministic)."""
    polys = mesh_polygons(ds)
    result = []
    for x, y in points:
        if not (np.isfinite(x) and np.isfinite(y)):
            raise ValueError("linkage failure: non-finite sample coordinate")
        hit = next((i for i, poly in enumerate(polys)
                    if poly[:, 0].min() <= x <= poly[:, 0].max() and poly[:, 1].min() <= y <= poly[:, 1].max()
                    and _point_in_polygon(x, y, poly)), None)
        if hit is None:
            raise ValueError(f"linkage failure: point ({x}, {y}) is outside the mesh")
        result.append(hit)
    return result


def summarize(path: Path, threshold: float, cells: list[int] | None = None,
              points: list[tuple[float, float]] | None = None,
              geotiff_dir: Path | None = None, pixel_size: float | None = None,
              crs: str | None = None) -> dict:
    """Validate a completed result file and derive per-cell maps.

    `cells` optionally selects an ordered list of cell indices whose full
    h/eta/hu/hv series are emitted (point series, or a profile when the
    order follows a transect). Indices are validated against the file.
    """
    if not np.isfinite(threshold) or threshold <= 0:
        raise ValueError("threshold must be finite and positive")
    if path.name.endswith(".partial"):
        raise ValueError("result validation failure: unpublished partial output")
    raw = path.read_bytes()
    manifest = load_manifest(path, raw)
    with netCDF4.Dataset("results", memory=raw) as ds:
        if getattr(ds, "surface_results_schema_version", None) != SCHEMA_VERSION:
            raise ValueError("migration required: unsupported surface results schema")
        if getattr(ds, "run_status", None) != "completed":
            raise ValueError("result validation failure: incomplete run")
        for attr in ("source_stcf_hash", "final_surface_state_hash", "committed_epochs"):
            if str(getattr(ds, attr, "")) != str(manifest.get(attr, "")) or not getattr(ds, attr, ""):
                raise ValueError(f"provenance validation failure: {attr} disagrees with manifest")
        expected = {"h": "m", "eta": "m", "hu": "m2 s-1", "hv": "m2 s-1", "wet_mask": "1"}
        for name, units in expected.items():
            if name not in ds.variables or ds[name].dimensions != ("time", "cell"):
                raise ValueError(f"result validation failure: {name} shape/variable missing")
            if getattr(ds[name], "units", None) != units:
                raise ValueError(f"unit mismatch: {name}")
        if "time" not in ds.variables or getattr(ds["time"], "units", None) != "s":
            raise ValueError("time units must be model logical seconds")
        time_data = ds["time"][:]
        if np.any(np.ma.getmaskarray(time_data)):
            raise ValueError("compatibility error: missing time value")
        time = np.asarray(time_data)
        if time.ndim != 1 or len(time) < 2 or not np.all(np.isfinite(time)) or np.any(np.diff(time) <= 0):
            raise ValueError("compatibility error: invalid or non-increasing times")
        source_stcf = getattr(ds, "source_stcf", "")
        if not source_stcf:
            raise ValueError("provenance validation failure: source_stcf is missing")
        source_path = Path(source_stcf)
        if not source_path.is_file():
            raise ValueError(f"provenance validation failure: source_stcf is absent: {source_stcf}")
        source_bytes = source_path.read_bytes()
        if fnv1a64(source_bytes) != ds.source_stcf_hash:
            raise ValueError("provenance validation failure: source_stcf bytes changed since the run")
        source_stcf_sha256 = hashlib.sha256(source_bytes).hexdigest()
        fields = {}
        for name in expected:
            array = ds[name][:]
            if np.any(np.ma.getmaskarray(array)) or not np.all(np.isfinite(array)):
                raise ValueError(f"result validation failure: missing/non-finite {name}")
            fields[name] = np.asarray(array)
        h = fields["h"]
        if h.shape[0] != len(time) or h.shape[1] == 0 or any(v.shape != h.shape for v in fields.values()):
            raise ValueError("result validation failure: inconsistent or empty field shape")
        if np.any(h < 0) or not np.all(np.isin(fields["wet_mask"], [0, 1])):
            raise ValueError("result validation failure: depth or wet mask")
        wet = h >= threshold
        arrival = [float(time[np.flatnonzero(wet[:, c])[0]]) if wet[:, c].any() else None
                   for c in range(h.shape[1])]
        duration = np.sum(wet[:-1] * np.diff(time)[:, None], axis=0)
        max_depth = np.max(h, axis=0)
        geotiff = None
        if geotiff_dir is not None:
            if pixel_size is None:
                raise ValueError("GeoTIFF export requires an explicit --pixel-size")
            from scau_results import geotiff as gt
            resolved_crs, crs_source = gt.resolve_crs(source_path, crs)
            arrival_raster = np.array([np.nan if a is None else a for a in arrival], dtype=float)
            geotiff = gt.export_maps(geotiff_dir, mesh_polygons(ds),
                                     {"max_depth_m": max_depth, "arrival_time_s": arrival_raster,
                                      "duration_s": duration}, pixel_size, resolved_crs)
            geotiff["crs_source"] = crs_source
            geotiff["never_wet_encoding"] = "nan (same as uncovered pixels; distinguish via duration_s == 0)"
        series = None
        sampled = None
        if points is not None:
            if cells is not None:
                raise ValueError("use either --cells or --points, not both")
            located = locate_cells(ds, points)
            sampled = {"points_xy": [list(p) for p in points], "cells": located,
                       "coordinate_system": "mesh projected coordinates (metres); no CRS declared in file"}
            cells = sorted(set(located), key=located.index)
        if cells is not None:
            if not cells or len(set(cells)) != len(cells):
                raise ValueError("cell selection must be a non-empty list of distinct indices")
            for index in cells:
                if not isinstance(index, int) or isinstance(index, bool) or not 0 <= index < h.shape[1]:
                    raise ValueError(f"linkage failure: cell index {index!r} is outside 0..{h.shape[1] - 1}")
            series = {"cells": list(cells), "time_s": time.tolist(),
                      **{name: fields[name][:, cells].T.tolist() for name in ("h", "eta", "hu", "hv")}}
        return {"results_schema_version": 2, "source_sha256": hashlib.sha256(raw).hexdigest(),
                "source_hash": fnv1a64(raw), "final_surface_state_hash": ds.final_surface_state_hash,
                "committed_epochs": int(ds.committed_epochs), "series": series, "sampled": sampled,
                "geotiff": geotiff,
                "source_stcf": str(source_path), "source_stcf_sha256": source_stcf_sha256,
                "source_stcf_hash": str(ds.source_stcf_hash),
                "threshold_m": threshold, "duration_method": "left_sample_piecewise_constant",
                "time_units": "model logical seconds", "frames": len(time), "cells": h.shape[1],
                "max_depth_m": max_depth.tolist(), "arrival_time_s": arrival,
                "duration_s": duration.tolist()}


def main_link(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="scau_results link")
    parser.add_argument("summary", type=Path, help="run_summary.json written by scau_sim")
    parser.add_argument("--swmm-report", type=Path, default=None, help="SWMM .rpt from the same run")
    parser.add_argument("--result-manifest", type=Path, default=None,
                        help="<run.nc>.manifest.json; binds the view to the surface result by state hash")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    from scau_results.linked import linked_view
    try:
        view = linked_view(args.summary, args.swmm_report, args.result_manifest)
        with args.output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(view, sort_keys=True, indent=2, allow_nan=False) + "\n")
    except (OSError, ValueError, KeyError) as error:
        parser.exit(2, f"results error: {error}\n")
    return 0


def main() -> int:
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "link":
        return main_link(sys.argv[2:])
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--threshold", type=float, default=0.01)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", default=None,
                        help="ordered cell indices for point/profile series extraction")
    parser.add_argument("--points", type=float, nargs="+", default=None, metavar="XY",
                        help="x y pairs in the mesh's own projected metres; located to cells, no CRS transform")
    parser.add_argument("--geotiff-dir", type=Path, default=None,
                        help="write max_depth_m/arrival_time_s/duration_s GeoTIFFs here (requires --pixel-size)")
    parser.add_argument("--pixel-size", type=float, default=None, help="raster cell size in mesh metres")
    parser.add_argument("--crs", default=None,
                        help="EPSG CRS for GeoTIFF GeoKeys; must agree with the governed pipeline manifest if one is found")
    args = parser.parse_args()
    points = None
    if args.points is not None:
        if len(args.points) % 2 or not args.points:
            parser.exit(2, "results error: --points needs x y pairs\n")
        points = list(zip(args.points[0::2], args.points[1::2]))
    try:
        result = summarize(args.input, args.threshold, args.cells, points,
                           args.geotiff_dir, args.pixel_size, args.crs)
        with args.output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(result, sort_keys=True, indent=2, allow_nan=False) + "\n")
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(2, f"results error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
