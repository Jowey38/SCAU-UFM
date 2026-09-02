"""Pure-python job/report I/O for the SCAU PreProc Workbench plugin.

NO qgis imports here: this module is the stateless UI<->CLI contract layer
(M287 plan section 3) and is testable with any CPython. The QGIS shell
(plugin.py / workbench_dialog.py) only renders user choices into a
job_config, calls run_pipeline(), and displays the reports.

The pipeline itself runs in the SYSTEM python (gmsh + netCDF4 live there,
not in the QGIS runtime): the subprocess boundary is simultaneously the
state boundary, the fault boundary, and the GPL boundary.
"""

from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path

JOB_CONFIG_SCHEMA_VERSION = 1
MESH_CONTROLS_SCHEMA_VERSION = 1
DEFAULT_PYTHON_LAUNCHER = ["py", "-3"]

# Mesh-controls scratch layer contract (mirrors python/scau_preproc/mesh_controls.py).
MESH_CONTROL_KINDS = ("breakline", "refinement_region")
MESH_CONTROL_FIELDS = (("control_id", "string"), ("control_kind", "string"),
                       ("size_m", "double"), ("dist_max_m", "double"))

# Heat-map metrics available in mesh_quality_cells.geojson (E3 workbench page).
# (property, label, ascending-is-better, class breaks).
HEATMAP_METRICS = {
    "equiangle_skewness": ("等角偏斜度 (0 好 → 1 差)", True, (0.25, 0.5, 0.75, 0.9)),
    "nonorthogonality_deg": ("非正交度 (deg)", True, (15.0, 30.0, 45.0, 60.0)),
    "min_angle_deg": ("最小内角 (deg)", False, (10.0, 20.0, 30.0, 45.0)),
    "edge_length_ratio": ("边长比", True, (2.0, 3.0, 5.0, 10.0)),
    "area_m2": ("单元面积 (m²)", False, (4.0, 16.0, 36.0, 64.0)),
}


def build_job_config(
    package_dir: str,
    output_dir: str,
    *,
    case_name: str = "case.stcf.nc",
    characteristic_length_m: float = 8.0,
    recombine: bool = True,
    determinism_check: bool = True,
    coupling_maps: bool = False,
    validator_cli: str | None = None,
    mesh_controls_geojson: str | None = None,
    mesh_controls_default_size_m: float | None = None,
    mesh_controls_default_dist_max_m: float | None = None,
) -> dict:
    job = {
        "job_config_schema_version": JOB_CONFIG_SCHEMA_VERSION,
        "package": str(Path(package_dir)),
        "output_dir": str(Path(output_dir)),
        "case_name": case_name,
        "characteristic_length_m": float(characteristic_length_m),
        "recombine": bool(recombine),
        "determinism_check": bool(determinism_check),
        "coupling_maps": bool(coupling_maps),
    }
    if validator_cli:
        job["validator_cli"] = str(Path(validator_cli))
    if mesh_controls_geojson:
        controls = {"geojson": str(Path(mesh_controls_geojson))}
        if mesh_controls_default_size_m is not None:
            controls["default_size_m"] = float(mesh_controls_default_size_m)
        if mesh_controls_default_dist_max_m is not None:
            controls["default_dist_max_m"] = float(mesh_controls_default_dist_max_m)
        job["mesh_controls"] = controls
    return job


def mesh_controls_precheck(geojson_path: str | None, characteristic_length_m: float
                           ) -> list[tuple[str, str, str]]:
    """UI-side pre-flight on a mesh-controls GeoJSON, returned as findings rows.

    Only schema/attribute presence is checked here so that the operator gets
    immediate feedback while drawing; every geometric rule is enforced once,
    fail-closed, by the generator (single validation authority)."""
    if not geojson_path:
        return []
    path = Path(geojson_path)
    if not path.is_file():
        return [("fatal", "MeshControlsMissing", f"mesh controls file not found: {path}")]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return [("fatal", "MeshControlsUnreadable", str(error))]
    rows: list[tuple[str, str, str]] = []
    if data.get("mesh_controls_schema_version") != MESH_CONTROLS_SCHEMA_VERSION:
        rows.append(("fatal", "MeshControlsSchema",
                     f"mesh_controls_schema_version must be {MESH_CONTROLS_SCHEMA_VERSION}"))
    counts = {kind: 0 for kind in MESH_CONTROL_KINDS}
    for feature in data.get("features", []):
        props = feature.get("properties") or {}
        cid = str(props.get("control_id") or "?")
        kind = props.get("control_kind")
        if kind not in counts:
            rows.append(("fatal", "MeshControlKind", f"{cid}: unknown control_kind {kind!r}"))
            continue
        counts[kind] += 1
        size = props.get("size_m")
        if kind == "refinement_region" and (size is None or not 0 < float(size) <= characteristic_length_m):
            rows.append(("fatal", "MeshControlSize",
                         f"{cid}: refinement_region size_m must be in (0, {characteristic_length_m}]"))
        if kind == "breakline" and size is not None and not props.get("dist_max_m"):
            rows.append(("fatal", "MeshControlSize", f"{cid}: sized breakline needs dist_max_m > 0"))
    rows.append(("pass" if not rows else "info", "MeshControlsSummary",
                 f"{counts['breakline']} breakline(s), {counts['refinement_region']} "
                 f"refinement region(s); geometric rules are enforced by the generator"))
    return rows


def mesh_controls_template() -> dict:
    """Empty controls collection the shell writes when creating a scratch layer."""
    return {"type": "FeatureCollection",
            "mesh_controls_schema_version": MESH_CONTROLS_SCHEMA_VERSION,
            "features": []}


def heatmap_classes(metric: str) -> list[tuple[float, float, str]]:
    """(lower, upper, label) class ranges for a graduated renderer, ordered
    from best to worst quality so a fixed good->bad colour ramp applies."""
    if metric not in HEATMAP_METRICS:
        raise KeyError(metric)
    _, ascending_is_better, breaks = HEATMAP_METRICS[metric]
    bounds = [float("-inf"), *breaks, float("inf")]
    classes = []
    for lower, upper in zip(bounds[:-1], bounds[1:]):
        label = (f"< {upper:g}" if lower == float("-inf")
                 else f">= {lower:g}" if upper == float("inf") else f"{lower:g} – {upper:g}")
        classes.append((lower, upper, label))
    return classes if ascending_is_better else classes[::-1]


def mesh_quality_summary(job: dict) -> list[tuple[str, str, str]]:
    """Rows summarising mesh_quality.json (global + per-control report)."""
    path = Path(job["output_dir"]) / "mesh_quality.json"
    if not path.is_file():
        return [("info", "NoMeshQualityReport", "run the pipeline first")]
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return [("fatal", "MeshQualityUnreadable", str(error))]
    quality = report.get("quality", {})
    rows = [("info", "MeshQuality",
             f"cells={quality.get('cells')} tri={quality.get('triangles')} "
             f"quad={quality.get('quadrilaterals')} min_angle={quality.get('min_angle_degrees', 0):.2f} "
             f"max_edge_ratio={quality.get('max_edge_length_ratio', 0):.2f}")]
    per_cell = report.get("per_cell_diagnostics") or {}
    if per_cell:
        rows.append(("info", "PerCellDiagnostics",
                     f"max_skewness={per_cell.get('max_equiangle_skewness')} "
                     f"max_nonorthogonality_deg={per_cell.get('max_nonorthogonality_deg')} "
                     "(display only; certification = validate CLI)"))
    controls = report.get("mesh_controls")
    if controls:
        for line in controls.get("breaklines", []):
            rows.append(("pass" if line.get("preserved") else "fatal", "BreaklinePreserved",
                         f"{line['control_id']}: {line.get('mesh_edges_on_breakline')} mesh edges"))
        for region in controls.get("refinement_regions", []):
            rows.append(("info", "RefinementRegion",
                         f"{region['control_id']}: size_m={region['size_m']} "
                         f"cells={region['cells_inside']} "
                         f"mean_edge={region.get('mean_edge_length_inside_m') or 0:.3f} m"))
    return rows


def write_job_config(job: dict, output_dir: str) -> Path:
    path = Path(output_dir) / "job_config.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(job, indent=2), encoding="utf-8")
    return path


def repo_root_valid(repo_root: str) -> bool:
    """True when repo_root actually contains the pipeline package. The plugin
    may run from a COPIED profile deployment, so the repo root can never be
    inferred from __file__ alone; an invalid root must surface as a rendered
    finding, not a subprocess crash (WinError 267)."""
    try:
        return (Path(repo_root) / "python" / "scau_preproc" / "pipeline.py").is_file()
    except OSError:
        return False


def subprocess_env() -> dict[str, str]:
    """Environment for the SYSTEM python subprocess. The QGIS host exports
    PYTHONHOME/PYTHONPATH for its bundled interpreter (qgis-bin.env); inherited
    by `py -3` they make the system interpreter load QGIS's stdlib and die
    with "SRE module mismatch" before the pipeline writes validation.json."""
    env = {key: value for key, value in os.environ.items()
           if key not in ("PYTHONHOME", "PYTHONPATH", "PYTHONNOUSERSITE")}
    env.setdefault("PYTHONUTF8", "1")
    return env


def run_pipeline(
    job_config_path: Path,
    repo_root: str,
    *,
    python_launcher: list[str] | None = None,
    timeout_s: float = 900.0,
) -> dict:
    """Runs scau_preproc.pipeline as a subprocess; returns a result dict.

    Never raises on pipeline failure: fail-closed results are data the UI
    must render (fatal findings), not exceptions to swallow.
    """
    if not repo_root_valid(repo_root):
        return {
            "exit_code": -1,
            "status": "repo_root_invalid",
            "stderr": "repo root does not contain python/scau_preproc/pipeline.py: "
                      f"{repo_root!r} - set the SCAU-UFM checkout directory in the dialog",
            "validation": None,
        }
    launcher = list(python_launcher or DEFAULT_PYTHON_LAUNCHER)
    command = launcher + ["-m", "scau_preproc.pipeline", str(job_config_path)]
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            cwd=str(Path(repo_root) / "python"),
            env=subprocess_env(),
        )
        exit_code: int | None = completed.returncode
        stderr = completed.stderr
    except subprocess.TimeoutExpired:
        exit_code = None
        stderr = f"pipeline exceeded UI timeout ({timeout_s}s) and was killed"
    except OSError as error:
        return {
            "exit_code": -1,
            "status": "launcher_error",
            "stderr": str(error),
            "validation": None,
        }
    return {
        "exit_code": exit_code,
        "status": "timeout" if exit_code is None else ("ok" if exit_code == 0 else "fatal"),
        "stderr": stderr.strip() if stderr else "",
        "validation": load_validation(job_config_path),
    }


def load_validation(job_config_path: Path) -> dict | None:
    try:
        job = json.loads(Path(job_config_path).read_text(encoding="utf-8"))
        validation_path = Path(job["output_dir"]) / "validation.json"
        if validation_path.is_file():
            return json.loads(validation_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, KeyError):
        pass
    return None


def findings_rows(validation: dict | None) -> list[tuple[str, str, str]]:
    """Flattens validation.json into (severity, code, detail) table rows."""
    if not validation:
        return [("fatal", "NoValidationReport",
                 "pipeline did not write validation.json (crashed before stage A?)")]
    rows = [
        (str(f.get("severity", "?")), str(f.get("code", "?")), str(f.get("detail", "")))
        for f in validation.get("findings", [])
    ]
    if not rows:
        determinism = validation.get("determinism", {}).get("bitwise_identical_reruns")
        rows.append(("pass", "PipelineOk",
                     f"status={validation.get('status')}, bitwise_reruns={determinism}"))
    return rows


def exportable(validation: dict | None) -> bool:
    """Export gate: only a clean 'ok' validation may be exported (plan page 8)."""
    return bool(validation) and validation.get("status") == "ok" and not [
        f for f in validation.get("findings", []) if f.get("severity") == "fatal"
    ]


def layer_paths(job: dict) -> dict[str, str]:
    """Map-visualizable artifacts for the QGIS shell (existence-checked)."""
    package = Path(job["package"])
    output = Path(job["output_dir"])
    candidates = {
        "boundary": package / "boundary/computational_boundary.geojson",
        "buildings": package / "buildings/buildings.geojson",
        "landcover": package / "landcover/landcover.geojson",
        "generator_diagnostic": output / "generator.diagnostic.geojson",
        "mesh_quality_cells": output / "mesh_quality_cells.geojson",
    }
    controls = (job.get("mesh_controls") or {}).get("geojson")
    if controls:
        candidates["mesh_controls"] = Path(controls)
    return {name: str(path) for name, path in candidates.items() if path.is_file()}
