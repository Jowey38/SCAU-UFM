"""CRS governance for the input package (M287-B2, stage A).

Everything the pipeline meshes and samples must live in ONE projected,
metre-based coordinate reference system — the project target CRS. B2 makes
that a declared, audited contract instead of an unchecked assumption:

  metadata/crs_policy.json     (crs_policy_schema_version = 1)
    target_crs: "EPSG:32650" | WKT                # projected, metre units (else fatal)
    allow_reprojection: true | false              # false -> any source != target is fatal
    allowed_source_crs: ["EPSG:3857", ...]        # whitelist; sources outside it are fatal
    source_crs_overrides: {"<dataset id>": "EPSG:..."}   # when a file carries no CRS
    dem_policy: "must_match_target" | "declare_only"     # rasters are never resampled here
    audit_sample_count: 5                         # coordinates recorded before/after

Rules (fail-closed, each violation names the dataset):
  * geographic CRS anywhere (source or target)      -> fatal (lat/lon never enters meshing)
  * target not projected / units not metre           -> fatal
  * source CRS undeclared and no override            -> fatal
  * source not in allowed_source_crs                 -> fatal
  * source != target and allow_reprojection false    -> fatal
  * DEM CRS != target with dem_policy must_match     -> fatal (no resampling in B2; B3 owns rasters)
  * reprojection numerically not invertible to <1e-6 m on the audit sample -> fatal

Reprojection: vector inputs (boundary, buildings, landcover, soil zones, mesh
controls) are transformed with pyproj (PROJ, MIT) into a GOVERNED STAGING
package (<output>/governed_package/) that the rest of the pipeline consumes;
the input package is never modified. The transform is pinned by recording the
PROJ pipeline string, PROJ/pyproj versions and the target CRS WKT in
crs_audit.json; two runs with the same inputs write byte-identical files
(pyproj is deterministic for a fixed pipeline; coordinates are rounded to
1e-6 m so the JSON text is stable). SWMM [COORDINATES] are rewritten in the
staged model.inp ONLY as a coordinate-section rewrite (all other sections are
copied verbatim); the original .inp is preserved.

Sources already in the target CRS are copied byte-for-byte (identity is
never run through PROJ).
"""

from __future__ import annotations

import hashlib
import json
import math
import shutil
from pathlib import Path

CRS_POLICY_SCHEMA_VERSION = 1
CRS_AUDIT_SCHEMA_VERSION = 1
VECTOR_DATASETS = ("boundary/computational_boundary.geojson", "buildings/buildings.geojson",
                   "landcover/landcover.geojson", "soil/soil_zones.geojson",
                   "terrain/streams.geojson")  # B3 stream enforcement lines (optional)
COORD_DECIMALS = 6
ROUNDTRIP_TOLERANCE_M = 1.0e-6


class CrsGovernanceError(ValueError):
    def __init__(self, message: str, dataset: str | None = None) -> None:
        self.dataset = dataset
        super().__init__(message if dataset is None else f"{dataset}: {message}")


def _crs(text: str, what: str):
    from pyproj import CRS
    from pyproj.exceptions import CRSError
    try:
        return CRS.from_user_input(text)
    except CRSError as error:
        raise CrsGovernanceError(f"unparseable CRS {text!r} ({error})", what) from error


def _is_metre(crs) -> bool:
    units = {(a.unit_name or "").lower() for a in crs.axis_info}
    return units <= {"metre", "meter", "metres", "meters", "m"} and bool(units)


def load_policy(path: Path) -> dict:
    path = Path(path)
    if not path.is_file():
        raise CrsGovernanceError(f"crs policy not found: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise CrsGovernanceError(f"crs policy is not valid JSON: {error}") from error
    if data.get("crs_policy_schema_version") != CRS_POLICY_SCHEMA_VERSION:
        raise CrsGovernanceError(f"unsupported crs_policy_schema_version {data.get('crs_policy_schema_version')!r}")
    target_text = data.get("target_crs")
    if not isinstance(target_text, str) or not target_text or "TODO" in target_text:
        raise CrsGovernanceError("target_crs must be a declared EPSG code or WKT (no TODO_PROVIDER)")
    target = _crs(target_text, "target_crs")
    if target.is_geographic or not target.is_projected:
        raise CrsGovernanceError(f"target_crs {target_text!r} must be a projected CRS (geographic lat/lon is never allowed)")
    if not _is_metre(target):
        raise CrsGovernanceError(f"target_crs {target_text!r} must use metre units")
    allowed = data.get("allowed_source_crs")
    if not isinstance(allowed, list) or not allowed:
        raise CrsGovernanceError("allowed_source_crs must be a non-empty list")
    allowed_crs = {}
    for text in allowed:
        crs = _crs(str(text), "allowed_source_crs")
        if crs.is_geographic:
            raise CrsGovernanceError(f"allowed_source_crs contains geographic CRS {text!r}; lat/lon sources are never accepted")
        allowed_crs[str(text)] = crs
    dem_policy = data.get("dem_policy", "must_match_target")
    if dem_policy not in ("must_match_target", "declare_only"):
        raise CrsGovernanceError("dem_policy must be must_match_target or declare_only")
    samples = data.get("audit_sample_count", 5)
    if not isinstance(samples, int) or isinstance(samples, bool) or samples < 1:
        raise CrsGovernanceError("audit_sample_count must be a positive integer")
    overrides = data.get("source_crs_overrides") or {}
    if not isinstance(overrides, dict):
        raise CrsGovernanceError("source_crs_overrides must be an object")
    return {
        "path": str(path),
        "target_text": target_text,
        "target": target,
        "allow_reprojection": bool(data.get("allow_reprojection", True)),
        "allowed": allowed_crs,
        "overrides": {str(k): str(v) for k, v in overrides.items()},
        "dem_policy": dem_policy,
        "audit_sample_count": samples,
    }


# --- source CRS resolution ----------------------------------------------------


def geojson_crs_text(data: dict) -> str | None:
    """RFC 7946 has no crs member; legacy GeoJSON carries {"crs": {"type": "name",
    "properties": {"name": ...}}}. Absent -> None (policy override required)."""
    crs = data.get("crs")
    if isinstance(crs, dict):
        name = (crs.get("properties") or {}).get("name")
        if isinstance(name, str) and name:
            return name.replace("urn:ogc:def:crs:", "").replace("::", ":")
    return None


def resolve_source_crs(dataset_id: str, declared: str | None, policy: dict):
    text = declared or policy["overrides"].get(dataset_id)
    if not text:
        raise CrsGovernanceError("no CRS declared in the file and no source_crs_overrides entry", dataset_id)
    crs = _crs(text, dataset_id)
    if crs.is_geographic:
        raise CrsGovernanceError(f"geographic CRS {text!r} (lat/lon) is never accepted", dataset_id)
    if not any(crs == a for a in policy["allowed"].values()):
        raise CrsGovernanceError(f"source CRS {text!r} is not in allowed_source_crs", dataset_id)
    return text, crs


# --- transformation -----------------------------------------------------------


def make_transformer(source, target):
    from pyproj import Transformer
    return Transformer.from_crs(source, target, always_xy=True)


def _round(v: float) -> float:
    return round(float(v), COORD_DECIMALS)


def transform_coordinates(coords, transformer, depth_hint=None):
    """Recursively transforms nested coordinate arrays (Point/LineString/Polygon)."""
    if not coords:
        return coords
    if isinstance(coords[0], (int, float)):
        x, y = transformer.transform(coords[0], coords[1])
        if not (math.isfinite(x) and math.isfinite(y)):
            raise CrsGovernanceError(f"transform produced a non-finite coordinate for {coords[:2]}")
        return [_round(x), _round(y)] + list(coords[2:])
    return [transform_coordinates(c, transformer) for c in coords]


def audit_samples(features, transformer, inverse, count: int, dataset: str) -> list[dict]:
    """First `count` vertices (deterministic order) before/after with the
    inverse round-trip error; fatal if the round trip exceeds 1e-6 m."""
    samples = []
    def walk(coords):
        if not coords:
            return
        if isinstance(coords[0], (int, float)):
            if len(samples) < count:
                x, y = transformer.transform(coords[0], coords[1])
                bx, by = inverse.transform(x, y)
                error = math.hypot(bx - coords[0], by - coords[1])
                if error > ROUNDTRIP_TOLERANCE_M:
                    raise CrsGovernanceError(
                        f"round-trip error {error:.3e} m at {coords[:2]} exceeds {ROUNDTRIP_TOLERANCE_M} m", dataset)
                samples.append({"source": [coords[0], coords[1]], "target": [_round(x), _round(y)],
                                "roundtrip_error_m": error})
            return
        for c in coords:
            if len(samples) >= count:
                return
            walk(c)
    for feature in features:
        geometry = feature.get("geometry") or {}
        walk(geometry.get("coordinates"))
        if len(samples) >= count:
            break
    return samples


def reproject_geojson(src: Path, dst: Path, transformer, inverse, target_text: str, policy: dict, dataset: str) -> dict:
    data = json.loads(src.read_text(encoding="utf-8"))
    samples = audit_samples(data.get("features", []), transformer, inverse, policy["audit_sample_count"], dataset)
    for feature in data.get("features", []):
        geometry = feature.get("geometry")
        if geometry and "coordinates" in geometry:
            geometry["coordinates"] = transform_coordinates(geometry["coordinates"], transformer)
    data["crs"] = {"type": "name", "properties": {"name": target_text}}
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return {"features": len(data.get("features", [])), "samples": samples}


def rewrite_swmm_coordinates(src: Path, dst: Path, transformer) -> int:
    """Rewrites ONLY the [COORDINATES] section; every other line is verbatim."""
    lines = src.read_text(encoding="utf-8").splitlines(keepends=True)
    out = []
    section = None
    rewritten = 0
    for raw in lines:
        stripped = raw.split(";")[0].strip()
        if stripped.startswith("["):
            section = stripped.upper()
            out.append(raw)
            continue
        if section == "[COORDINATES]" and stripped:
            parts = stripped.split()
            if len(parts) >= 3:
                x, y = transformer.transform(float(parts[1]), float(parts[2]))
                comment = raw.split(";", 1)[1] if ";" in raw else ""
                out.append(f"{parts[0]:<8}{_round(x):<16.6f}{_round(y):<16.6f}" + (f";{comment}" if comment else "\n"))
                rewritten += 1
                continue
        out.append(raw)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("".join(out), encoding="utf-8")
    return rewritten


def _copy_tree(src: Path, dst: Path, skip: set[str]) -> None:
    for path in sorted(src.rglob("*")):
        if path.is_file():
            rel = path.relative_to(src).as_posix()
            if rel in skip:
                continue
            target = dst / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)


def govern_package(package: Path, policy_path: Path, staging: Path, mesh_controls: Path | None = None,
                   dem_reprojection_authorized: bool = False) -> dict:
    """Validates every dataset's CRS against the policy and materialises the
    governed staging package. Returns the audit (also written to
    staging/crs_audit.json). Raises CrsGovernanceError (fail-closed).

    `dem_reprojection_authorized` is set by the pipeline when a B3 terrain
    policy enables `reproject`: the raster leg is then deferred to terrain
    conditioning instead of being fatal under dem_policy must_match_target."""
    import pyproj

    package = Path(package)
    staging = Path(staging)
    policy = load_policy(policy_path)
    target_text, target = policy["target_text"], policy["target"]
    manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
    datasets = {d["id"]: d for d in manifest.get("datasets", [])}
    audit = {
        "crs_audit_schema_version": CRS_AUDIT_SCHEMA_VERSION,
        "policy": policy["path"],
        "policy_sha256": hashlib.sha256(Path(policy_path).read_bytes()).hexdigest(),
        "target_crs": target_text,
        "target_crs_wkt": target.to_wkt(),
        "proj_version": pyproj.proj_version_str,
        "pyproj_version": pyproj.__version__,
        "coordinate_decimals": COORD_DECIMALS,
        "datasets": {},
    }
    transformers: dict[str, dict] = {}

    def transformer_for(source_text: str, source):
        if source_text not in transformers:
            forward = make_transformer(source, target)
            transformers[source_text] = {
                "forward": forward,
                "inverse": make_transformer(target, source),
                "pipeline": forward.definition,
            }
        return transformers[source_text]

    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    reprojected: set[str] = set()

    # Vector datasets (+ optional mesh controls, which carry no CRS member: they
    # are authored in the package frame and follow the boundary's CRS).
    vector_paths = list(VECTOR_DATASETS)
    id_by_path = {d["path"]: did for did, d in datasets.items()}
    boundary_crs_text = None
    for rel in vector_paths:
        src = package / rel
        if not src.is_file():
            continue
        dataset_id = id_by_path.get(rel, rel)
        data = json.loads(src.read_text(encoding="utf-8"))
        source_text, source = resolve_source_crs(dataset_id, geojson_crs_text(data), policy)
        if dataset_id == "computational_boundary":
            boundary_crs_text = source_text
        entry = {"path": rel, "source_crs": source_text, "target_crs": target_text}
        if source == target:
            entry["action"] = "copied_identity"
        else:
            if not policy["allow_reprojection"]:
                raise CrsGovernanceError(f"source CRS {source_text!r} differs from target and allow_reprojection is false", dataset_id)
            t = transformer_for(source_text, source)
            result = reproject_geojson(src, staging / rel, t["forward"], t["inverse"], target_text, policy, dataset_id)
            entry.update({"action": "reprojected", "proj_pipeline": t["pipeline"], **result})
            reprojected.add(rel)
        audit["datasets"][dataset_id] = entry

    if mesh_controls is not None and Path(mesh_controls).is_file():
        if boundary_crs_text is None:
            raise CrsGovernanceError("mesh controls need the boundary CRS (boundary dataset missing)", "mesh_controls")
        source = _crs(boundary_crs_text, "mesh_controls")
        entry = {"path": str(mesh_controls), "source_crs": boundary_crs_text, "target_crs": target_text,
                 "note": "mesh controls carry no crs member; they inherit the boundary CRS"}
        if source == target:
            entry["action"] = "copied_identity"
            shutil.copyfile(mesh_controls, staging / "mesh_controls.geojson")
        else:
            t = transformer_for(boundary_crs_text, source)
            result = reproject_geojson(Path(mesh_controls), staging / "mesh_controls.geojson",
                                       t["forward"], t["inverse"], target_text, policy, "mesh_controls")
            entry.update({"action": "reprojected", "proj_pipeline": t["pipeline"], **result})
        audit["datasets"]["mesh_controls"] = entry

    # DEM: rasters are never resampled in B2.
    dem = datasets.get("dem")
    if dem:
        meta_path = package / dem["metadata"] if dem.get("metadata") else None
        meta = json.loads(meta_path.read_text(encoding="utf-8")) if meta_path and meta_path.is_file() else {}
        dem_text = meta.get("source_crs") or policy["overrides"].get("dem")
        if not dem_text:
            raise CrsGovernanceError("DEM metadata declares no source_crs and no override exists", "dem")
        dem_crs = _crs(dem_text, "dem")
        if dem_crs.is_geographic:
            raise CrsGovernanceError(f"DEM CRS {dem_text!r} is geographic", "dem")
        matches = dem_crs == target
        if not matches and policy["dem_policy"] == "must_match_target" and not dem_reprojection_authorized:
            raise CrsGovernanceError(
                f"DEM CRS {dem_text!r} differs from target {target_text!r}; rasters are not resampled in B2 "
                "(dem_policy must_match_target) - supply a DEM in the target CRS or authorize reproject in "
                "metadata/terrain_condition_policy.json (B3)", "dem")
        action = ("copied_identity" if matches
                  else "reprojection_deferred_to_terrain_condition" if dem_reprojection_authorized
                  else "declared_mismatch_allowed")
        audit["datasets"]["dem"] = {"path": dem["path"], "source_crs": dem_text, "target_crs": target_text,
                                    "action": action}

    # SWMM coordinates follow the boundary CRS unless overridden.
    swmm_rel = "swmm/model.inp"
    if (package / swmm_rel).is_file():
        swmm_text = policy["overrides"].get("swmm", boundary_crs_text)
        if swmm_text is None:
            raise CrsGovernanceError("SWMM coordinate CRS unknown (no boundary CRS, no override)", "swmm")
        swmm_crs = _crs(swmm_text, "swmm")
        if swmm_crs.is_geographic:
            raise CrsGovernanceError("SWMM [COORDINATES] in a geographic CRS are never accepted", "swmm")
        entry = {"path": swmm_rel, "source_crs": swmm_text, "target_crs": target_text}
        if swmm_crs == target:
            entry["action"] = "copied_identity"
        else:
            if not policy["allow_reprojection"]:
                raise CrsGovernanceError("SWMM coordinates differ from target and allow_reprojection is false", "swmm")
            t = transformer_for(swmm_text, swmm_crs)
            count = rewrite_swmm_coordinates(package / swmm_rel, staging / swmm_rel, t["forward"])
            entry.update({"action": "coordinates_rewritten", "proj_pipeline": t["pipeline"], "nodes": count,
                          "note": "only the [COORDINATES] section is rewritten; all other sections verbatim"})
            reprojected.add(swmm_rel)
        audit["datasets"]["swmm"] = entry

    # Everything else (including identity vectors, DEM, tables, policies) is
    # copied byte-for-byte so the staging package is complete and self-contained.
    _copy_tree(package, staging, skip=reprojected)
    audit["staging_package"] = str(staging)
    audit["reprojected_files"] = sorted(reprojected)
    (staging / "crs_audit.json").write_text(json.dumps(audit, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return audit
