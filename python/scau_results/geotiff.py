"""Dependency-free baseline GeoTIFF export of cell-wise derived maps.

Rasterization is a deterministic point-in-cell sample at pixel centres over
the mesh bounding box (lowest cell index wins on shared edges, matching
`locate_cells`). Pixels covered by no face are NaN; GDAL_NODATA is "nan".

CRS is never invented. `resolve_crs` accepts an explicit user CRS and/or a
PreProc pipeline manifest whose `case_sha256` matches the source STCF bytes;
disagreement is a compatibility error and absence yields an unreferenced
raster whose GeoKey directory carries only the raster-type key.
"""
from __future__ import annotations

import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np

NODATA = float("nan")


def resolve_crs(source_stcf: Path, explicit: str | None) -> tuple[str | None, str]:
    """Return (crs_string_or_None, provenance_label)."""
    discovered = None
    candidates = [source_stcf.parent.parent / "validation" / "pipeline_manifest.json",
                  source_stcf.with_name(source_stcf.name.replace(".stcf.nc", ".pipeline_manifest.json"))]
    for candidate in candidates:
        if not candidate.is_file():
            continue
        manifest = json.loads(candidate.read_text(encoding="utf-8"))
        if manifest.get("case_sha256") != hashlib.sha256(source_stcf.read_bytes()).hexdigest():
            raise ValueError(f"provenance validation failure: {candidate.name} case_sha256 does not match source STCF")
        governance = manifest.get("crs_governance")
        discovered = (governance or {}).get("target_crs")
        source = f"pipeline_manifest:{candidate.name}"
        break
    else:
        source = "none"
    if explicit is not None and discovered is not None and explicit != discovered:
        raise ValueError(f"compatibility error: --crs {explicit} disagrees with governed target_crs {discovered}")
    if explicit is not None:
        return explicit, "explicit_argument" + ("" if discovered is None else "+confirmed_by_manifest")
    if discovered is not None:
        return discovered, source
    return None, "undeclared"


def epsg_code(crs: str | None) -> int | None:
    """EPSG code of a 2-D projected CRS whose horizontal axes are metres, or
    None for unreferenced output. The mesh and pixel size are metres, so any
    other CRS (geographic, geocentric, vertical, compound, foot-based) would
    silently mis-scale or mis-place the raster and is rejected."""
    if crs is None:
        return None
    from pyproj import CRS
    parsed = CRS.from_user_input(crs)
    if not parsed.is_projected:
        kind = "geographic" if parsed.is_geographic else ("vertical" if parsed.is_vertical else "non-projected")
        raise ValueError(f"linkage failure: {kind} CRS {crs} cannot georeference a projected metre raster")
    axes = parsed.axis_info
    if len(axes) != 2:
        raise ValueError(f"linkage failure: CRS {crs} has {len(axes)} axes; a 2-D projected CRS is required")
    for axis in axes:
        if axis.unit_name not in ("metre", "meter", "m") or abs(axis.unit_conversion_factor - 1.0) > 1e-12:
            raise ValueError(f"linkage failure: CRS {crs} axis '{axis.name}' is in {axis.unit_name}, not metres; "
                             "the mesh coordinates and pixel size are metres")
    code = parsed.to_epsg()
    if code is None:
        raise ValueError(f"linkage failure: CRS {crs} has no EPSG identity; GeoTIFF GeoKeys require one")
    return int(code)


def _pip_vectorized(px: np.ndarray, py: np.ndarray, poly: np.ndarray) -> np.ndarray:
    """Even-odd with boundary counted as inside (same convention as locate_cells)."""
    inside = np.zeros(px.shape, dtype=bool)
    on_edge = np.zeros(px.shape, dtype=bool)
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        crosses = (y1 > py) != (y2 > py)
        with np.errstate(divide="ignore", invalid="ignore"):
            x_int = x1 + (py - y1) * (x2 - x1) / (y2 - y1)
        inside ^= crosses & (px < x_int)
        cross = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)
        tol = 1e-9 * max(1.0, abs(x2 - x1) + abs(y2 - y1))
        on_edge |= (np.abs(cross) <= tol) & (px >= min(x1, x2) - 1e-9) & (px <= max(x1, x2) + 1e-9) \
            & (py >= min(y1, y2) - 1e-9) & (py <= max(y1, y2) + 1e-9)
    return inside | on_edge


def rasterize(polys: list[np.ndarray], pixel_size: float) -> tuple[np.ndarray, tuple[float, float]]:
    """Return (cell_index_grid[int, NaN as -1], (min_x, max_y))."""
    if not (math.isfinite(pixel_size) and pixel_size > 0):
        raise ValueError("pixel_size must be finite and positive")
    allx = np.concatenate([p[:, 0] for p in polys])
    ally = np.concatenate([p[:, 1] for p in polys])
    min_x, max_x, min_y, max_y = allx.min(), allx.max(), ally.min(), ally.max()
    width = int(math.ceil((max_x - min_x) / pixel_size - 1e-9))
    height = int(math.ceil((max_y - min_y) / pixel_size - 1e-9))
    if width < 1 or height < 1 or width * height > 50_000_000:
        raise ValueError("raster size is empty or exceeds the 50M pixel guard")
    grid = np.full((height, width), -1, dtype=np.int64)
    xs = min_x + (np.arange(width) + 0.5) * pixel_size
    ys = max_y - (np.arange(height) + 0.5) * pixel_size   # row 0 = north
    for index, poly in enumerate(polys):
        c0 = max(0, int((poly[:, 0].min() - min_x) / pixel_size) - 1)
        c1 = min(width, int((poly[:, 0].max() - min_x) / pixel_size) + 2)
        r0 = max(0, int((max_y - poly[:, 1].max()) / pixel_size) - 1)
        r1 = min(height, int((max_y - poly[:, 1].min()) / pixel_size) + 2)
        if c0 >= c1 or r0 >= r1:
            continue
        px, py = np.meshgrid(xs[c0:c1], ys[r0:r1])
        hit = _pip_vectorized(px, py, poly)
        block = grid[r0:r1, c0:c1]
        block[hit & (block < 0)] = index
    return grid, (float(min_x), float(max_y))


def _ifd_entry(tag: int, kind: int, count: int, value_or_offset: int) -> bytes:
    return struct.pack("<HHII", tag, kind, count, value_or_offset)


def write_geotiff(path: Path, data: np.ndarray, origin: tuple[float, float], pixel_size: float,
                  epsg: int | None) -> None:
    """Little-endian baseline TIFF, float32 single strip, PixelIsArea."""
    if path.exists():
        raise FileExistsError(f"refusing to overwrite {path}")
    height, width = data.shape
    pixels = np.ascontiguousarray(data, dtype="<f4").tobytes()
    geokeys = [(1025, 0, 1, 1)]                       # GTRasterTypeGeoKey = RasterPixelIsArea
    if epsg is not None:
        geokeys = [(1024, 0, 1, 1), (1025, 0, 1, 1), (3072, 0, 1, epsg)]   # ModelTypeProjected + ProjectedCSType
    geokey_dir = struct.pack("<4H", 1, 1, 0, len(geokeys)) + b"".join(struct.pack("<4H", *k) for k in geokeys)
    pixel_scale = struct.pack("<3d", pixel_size, pixel_size, 0.0)
    tiepoint = struct.pack("<6d", 0.0, 0.0, 0.0, origin[0], origin[1], 0.0)
    # TIFF 6.0 §2: values of 4 bytes or fewer live inline in the IFD value
    # field, never behind an offset. "nan\0" is exactly 4 bytes.
    nodata = b"nan\x00"
    (nodata_inline,) = struct.unpack("<I", nodata)

    entries = 15
    ifd_offset = 8
    ifd_size = 2 + entries * 12 + 4
    cursor = ifd_offset + ifd_size
    off_scale, cursor = cursor, cursor + len(pixel_scale)
    off_tie, cursor = cursor, cursor + len(tiepoint)
    off_geo, cursor = cursor, cursor + len(geokey_dir)
    off_pixels = cursor + (cursor % 2)

    ifd = struct.pack("<H", entries)
    ifd += _ifd_entry(256, 4, 1, width)
    ifd += _ifd_entry(257, 4, 1, height)
    ifd += _ifd_entry(258, 3, 1, 32)
    ifd += _ifd_entry(259, 3, 1, 1)
    ifd += _ifd_entry(262, 3, 1, 1)
    ifd += _ifd_entry(273, 4, 1, off_pixels)
    ifd += _ifd_entry(277, 3, 1, 1)
    ifd += _ifd_entry(278, 4, 1, height)
    ifd += _ifd_entry(279, 4, 1, len(pixels))
    ifd += _ifd_entry(284, 3, 1, 1)
    ifd += _ifd_entry(339, 3, 1, 3)
    ifd += _ifd_entry(33550, 12, 3, off_scale)
    ifd += _ifd_entry(33922, 12, 6, off_tie)
    ifd += _ifd_entry(34735, 3, len(geokey_dir) // 2, off_geo)
    ifd += _ifd_entry(42113, 2, len(nodata), nodata_inline)
    ifd += struct.pack("<I", 0)

    body = b"II" + struct.pack("<HI", 42, ifd_offset) + ifd + pixel_scale + tiepoint + geokey_dir
    body += b"\x00" * (off_pixels - len(body)) + pixels
    with path.open("xb") as stream:
        stream.write(body)


FLOAT32_MAX = float(np.finfo(np.float32).max)


def export_maps(out_dir: Path, polys: list[np.ndarray], fields: dict[str, np.ndarray], pixel_size: float,
                crs: str | None) -> dict:
    """All-or-nothing: validate every input, stage every raster in a sibling
    temp directory, then publish the whole set by a single directory rename.
    A rejected request leaves no .tif behind; an existing destination is a
    conflict, never overwritten."""
    epsg = epsg_code(crs)
    if not fields:
        raise ValueError("no fields to export")
    for name, values in fields.items():
        arr = np.asarray(values, dtype=float)
        finite = arr[np.isfinite(arr)]
        if finite.size and np.max(np.abs(finite)) > FLOAT32_MAX:
            raise ValueError(f"result validation failure: field {name!r} exceeds float32 range; "
                             "GeoTIFF export would silently overflow to inf")
        if arr.ndim != 1 or arr.shape[0] != len(polys):
            raise ValueError(f"result validation failure: field {name!r} has {arr.shape} values for {len(polys)} cells")
    if out_dir.exists():
        raise FileExistsError(f"refusing to write into existing GeoTIFF directory {out_dir}")
    grid, origin = rasterize(polys, pixel_size)
    import tempfile
    out_dir.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{out_dir.name}.", dir=out_dir.parent))
    try:
        written = {}
        for name, values in fields.items():
            values = np.asarray(values, dtype=float)
            raster = np.where(grid >= 0, values[np.clip(grid, 0, None)], NODATA).astype("<f4")
            staged = staging / f"{name}.tif"
            write_geotiff(staged, raster, origin, pixel_size, epsg)
            written[name] = {"path": str(out_dir / f"{name}.tif"),
                             "sha256": hashlib.sha256(staged.read_bytes()).hexdigest()}
        staging.rename(out_dir)   # same parent: atomic; fails if out_dir appeared meanwhile
    except BaseException:
        import shutil
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return {"files": written, "crs": crs, "epsg": epsg, "pixel_size_m": pixel_size,
            "width": int(grid.shape[1]), "height": int(grid.shape[0]),
            "origin_upper_left": [origin[0], origin[1]],
            "covered_pixels": int((grid >= 0).sum()), "nodata": "nan",
            "georeferenced": epsg is not None}
