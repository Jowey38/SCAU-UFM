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

# Coupling mapping modes (mirrors python/scau_preproc/coupling_maps.MODES, C5).
COUPLING_MODES = (
    ("explicit_ids", "显式 ID 表（CSV 权威，缺表 fatal）"),
    ("spatial_candidates", "纯空间候选（无表；全部 review 待确认）"),
    ("mixed", "混合（表内 high，表外空间候选 review）"),
)
DEFAULT_ROOF_NODE_MAX_DISTANCE_M = 50.0

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
    confirmations_dir: str | None = None,
    coupling_mode: str = "explicit_ids",
    roof_node_max_distance_m: float | None = None,
    export_target_dir: str | None = None,
    export_options: dict | None = None,
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
    if coupling_maps and (coupling_mode != "explicit_ids" or roof_node_max_distance_m is not None):
        if coupling_mode not in {mode for mode, _ in COUPLING_MODES}:
            raise ValueError(f"unknown coupling mode {coupling_mode!r}")
        block = {"mode": coupling_mode}
        if roof_node_max_distance_m is not None:
            block["roof_node_max_distance_m"] = float(roof_node_max_distance_m)
        job["coupling_maps"] = block
    if validator_cli:
        job["validator_cli"] = str(Path(validator_cli))
    if mesh_controls_geojson:
        controls = {"geojson": str(Path(mesh_controls_geojson))}
        if mesh_controls_default_size_m is not None:
            controls["default_size_m"] = float(mesh_controls_default_size_m)
        if mesh_controls_default_dist_max_m is not None:
            controls["default_dist_max_m"] = float(mesh_controls_default_dist_max_m)
        job["mesh_controls"] = controls
    if confirmations_dir:
        job["confirmations_dir"] = str(Path(confirmations_dir))
    if export_target_dir:
        job["export_case"] = {"target_dir": str(Path(export_target_dir)), **(export_options or {})}
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
    rows.extend(coupling_mode_rows(validation))
    rows.extend(confirmation_rows(validation))
    rows.extend(export_rows(validation))
    return rows


def exportable(validation: dict | None) -> bool:
    """Export gate (plan page 8): only a clean 'ok' validation may be exported.
    A 'review' status (e.g. unconfirmed coupling candidates or confirmation
    drift, M287-C4) closes the gate just like a fatal finding."""
    return bool(validation) and validation.get("status") == "ok" and not [
        f for f in validation.get("findings", []) if f.get("severity") in ("fatal", "review")
    ]


def coupling_mode_rows(validation: dict | None) -> list[tuple[str, str, str]]:
    """Rows summarising the coupling-map generator report (mode + per-chain
    confidence split, C5)."""
    report = ((validation or {}).get("coupling_maps") or {}).get("report")
    if not report:
        return []
    rows = [("info", "CouplingMode",
             f"mode={report.get('mode', 'explicit_ids')} "
             f"params={report.get('mode_parameters') or {}}")]
    for chain, counts in (report.get("chains") or {}).items():
        if chain == "surface_to_dflowfm":
            continue
        split = counts.get("by_confidence") or {}
        rows.append(("info", "CouplingChainCandidates",
                     f"{chain}: relations={counts.get('relations')} high={split.get('high', 0)} "
                     f"review={split.get('review', 0)} methods={','.join(counts.get('methods', []))}"))
    return rows


def confirmation_rows(validation: dict | None) -> list[tuple[str, str, str]]:
    """Rows summarising the C4 confirmation merge (stage E')."""
    if not validation or "coupling_confirmations" not in validation:
        return []
    report = validation["coupling_confirmations"]
    rows = [("pass" if report.get("status") == "complete" else "review", "CouplingConfirmations",
             f"status={report.get('status')} unconfirmed={report.get('unconfirmed_total')} "
             f"dir={report.get('confirmations_dir') or '(none)'}")]
    for chain, counts in (report.get("chains") or {}).items():
        rows.append(("info", "ConfirmationChain",
                     f"{chain}: candidates={counts.get('candidates')} accepted={counts.get('accepted')} "
                     f"retargeted={counts.get('retargeted')} rejected={counts.get('rejected')} "
                     f"created={counts.get('created')} unconfirmed={counts.get('unconfirmed')} "
                     f"drifted={counts.get('drifted')}"))
    return rows


# --- E4 coupling editor (pure layer) ------------------------------------------

CONFIRMATION_SCHEMA_VERSION = 1
COUPLING_CHAINS = ("surface_to_swmm", "roof_to_swmm")
COUPLING_DECISIONS = ("accept", "reject", "retarget", "create")
_CHAIN_FILES = {"surface_to_swmm": "surface_swmm_mapping.json", "roof_to_swmm": "roof_drain_mapping.json"}


def candidate_sha256(relation: dict) -> str:
    """Canonical candidate hash (MUST match scau_preproc.confirmations)."""
    import hashlib
    payload = json.dumps(relation, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def _cell_key(chain: str) -> str:
    return "cell_index" if chain == "surface_to_swmm" else "overflow_target_cell_index"


def load_candidates(job: dict) -> dict[str, list[dict]]:
    """Generator candidates per chain from <output>/coupling (empty if absent)."""
    coupling = Path(job["output_dir"]) / "coupling"
    result: dict[str, list[dict]] = {}
    for chain, name in _CHAIN_FILES.items():
        path = coupling / name
        try:
            result[chain] = json.loads(path.read_text(encoding="utf-8")).get("relations", []) if path.is_file() else []
        except (OSError, ValueError):
            result[chain] = []
    return result


def confirmations_dir_for(job: dict) -> Path:
    """Where the editor writes decisions: job_config.confirmations_dir, else the
    package's coupling/confirmed/ (the pipeline default)."""
    configured = job.get("confirmations_dir")
    if configured:
        return Path(configured)
    return Path(job["package"]) / "coupling" / "confirmed"


def load_decisions(confirmations_dir: Path) -> dict[tuple[str, str], dict]:
    """(chain, mapping_id) -> decision file content (+ _file)."""
    decisions: dict[tuple[str, str], dict] = {}
    if not Path(confirmations_dir).is_dir():
        return decisions
    for path in sorted(Path(confirmations_dir).glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if isinstance(data, dict) and data.get("chain") in COUPLING_CHAINS and data.get("mapping_id"):
            decisions[(data["chain"], data["mapping_id"])] = {**data, "_file": path.name}
    return decisions


def candidate_status(relation: dict, decision: dict | None) -> str:
    """unconfirmed | accepted | retargeted | rejected | drifted."""
    if decision is None:
        return "unconfirmed"
    if decision.get("candidate_sha256") != candidate_sha256(relation):
        return "drifted"
    return {"accept": "accepted", "reject": "rejected", "retarget": "retargeted"}.get(
        decision.get("decision"), "unconfirmed")


def load_cell_geometries(job: dict) -> dict[int, list]:
    """cell_id -> ring from mesh_quality_cells.geojson (display geometry only)."""
    path = Path(job["output_dir"]) / "mesh_quality_cells.geojson"
    if not path.is_file():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    return {int(f["properties"]["cell_id"]): f["geometry"]["coordinates"][0]
            for f in data.get("features", []) if f.get("geometry")}


def _centroid(ring: list) -> tuple[float, float]:
    points = ring[:-1] if ring and ring[0] == ring[-1] else ring
    return sum(p[0] for p in points) / len(points), sum(p[1] for p in points) / len(points)


def build_coupling_link_layers(job: dict) -> dict:
    """Editor layers as GeoJSON dicts: links (LineString node->cell), nodes
    (Point) and cells (Polygon), each tagged with chain / mapping_id /
    confidence / status so the shell can colour review in red and confirmed
    in green. Roof source geometry is the building centroid (when known)."""
    candidates = load_candidates(job)
    decisions = load_decisions(confirmations_dir_for(job))
    cells = load_cell_geometries(job)
    buildings: dict[str, tuple[float, float]] = {}
    buildings_path = Path(job["package"]) / "buildings/buildings.geojson"
    if buildings_path.is_file():
        for f in json.loads(buildings_path.read_text(encoding="utf-8")).get("features", []):
            if f.get("geometry", {}).get("type") == "Polygon":
                buildings[str(f["properties"].get("building_id"))] = _centroid(f["geometry"]["coordinates"][0])

    links, nodes, cell_features = [], [], []
    summary = {"unconfirmed": 0, "accepted": 0, "retargeted": 0, "rejected": 0, "drifted": 0}
    for chain, relations in candidates.items():
        cell_key = _cell_key(chain)
        for relation in relations:
            decision = decisions.get((chain, relation["mapping_id"]))
            status = candidate_status(relation, decision)
            summary[status] += 1
            cell_id = relation.get(cell_key)
            if status == "retargeted" and decision and isinstance(decision.get("target"), dict):
                cell_id = decision["target"].get("cell_index", cell_id)
            if chain == "surface_to_swmm":
                source = (relation.get("node_x"), relation.get("node_y"))
            else:
                source = buildings.get(str(relation.get("building_id")), (None, None))
            props = {
                "chain": chain,
                "mapping_id": relation["mapping_id"],
                "swmm_node_id": relation.get("swmm_node_id"),
                "building_id": relation.get("building_id"),
                "cell_index": cell_id,
                "candidate_cell_index": relation.get(cell_key),
                "exchange_elevation_m": relation.get("exchange_elevation_m"),
                "method": relation.get("method"),
                "confidence": relation.get("confidence"),
                "status": status,
                "decision_file": decision.get("_file") if decision else None,
            }
            ring = cells.get(cell_id) if cell_id is not None else None
            if ring:
                cell_features.append({"type": "Feature", "properties": props,
                                      "geometry": {"type": "Polygon", "coordinates": [ring]}})
            if source[0] is not None:
                nodes.append({"type": "Feature", "properties": props,
                              "geometry": {"type": "Point", "coordinates": [source[0], source[1]]}})
                if ring:
                    cx, cy = _centroid(ring)
                    links.append({"type": "Feature", "properties": props,
                                  "geometry": {"type": "LineString",
                                               "coordinates": [[source[0], source[1]], [cx, cy]]}})
    def collection(features):
        return {"type": "FeatureCollection", "features": features}
    return {"links": collection(links), "nodes": collection(nodes), "cells": collection(cell_features),
            "summary": summary}


def write_decision(job: dict, chain: str, mapping_id: str, decision: str, *,
                   confirmed_by: str, target: dict | None = None, note: str = "") -> Path:
    """Writes one C4 confirmation file for a generator candidate. The
    candidate hash is computed from the CURRENT candidate file so the decision
    binds to what the operator saw; the pipeline detects later drift."""
    if chain not in COUPLING_CHAINS:
        raise ValueError(f"unknown chain {chain!r}")
    if decision not in ("accept", "reject", "retarget"):
        raise ValueError(f"write_decision handles accept/reject/retarget, got {decision!r}")
    if not confirmed_by.strip():
        raise ValueError("confirmed_by must not be empty")
    relation = next((r for r in load_candidates(job).get(chain, []) if r["mapping_id"] == mapping_id), None)
    if relation is None:
        raise ValueError(f"no candidate {chain}/{mapping_id} in the current output")
    payload = {
        "confirmation_schema_version": CONFIRMATION_SCHEMA_VERSION,
        "chain": chain,
        "mapping_id": mapping_id,
        "decision": decision,
        "candidate_sha256": candidate_sha256(relation),
        "confirmed_by": confirmed_by.strip(),
        "timestamp": _now_iso(),
    }
    if decision == "retarget":
        if not target or not isinstance(target.get("cell_index"), int):
            raise ValueError("retarget needs target.cell_index")
        payload["target"] = {"cell_index": int(target["cell_index"])}
        if target.get("exchange_elevation_m") is not None:
            payload["target"]["exchange_elevation_m"] = float(target["exchange_elevation_m"])
    if note:
        payload["note"] = note
    return _write_confirmation(confirmations_dir_for(job), f"{mapping_id}.json", payload)


def write_create_decision(job: dict, chain: str, mapping_id: str, *, confirmed_by: str,
                          swmm_node_id: str, cell_index: int, exchange_elevation_m: float,
                          building_id: str | None = None, note: str = "") -> Path:
    """Writes a C4 `create` decision (operator-authored link, no candidate)."""
    if chain not in COUPLING_CHAINS:
        raise ValueError(f"unknown chain {chain!r}")
    if not mapping_id.strip() or not confirmed_by.strip() or not swmm_node_id.strip():
        raise ValueError("mapping_id, confirmed_by and swmm_node_id must not be empty")
    if any(r["mapping_id"] == mapping_id for r in load_candidates(job).get(chain, [])):
        raise ValueError(f"{mapping_id} is an existing candidate; use accept/retarget instead")
    if chain == "roof_to_swmm" and not building_id:
        raise ValueError("roof create needs building_id")
    target = {"cell_index": int(cell_index), "swmm_node_id": swmm_node_id.strip(),
              "exchange_elevation_m": float(exchange_elevation_m)}
    if building_id:
        target["building_id"] = building_id
    payload = {
        "confirmation_schema_version": CONFIRMATION_SCHEMA_VERSION,
        "chain": chain,
        "mapping_id": mapping_id.strip(),
        "decision": "create",
        "candidate_sha256": None,
        "confirmed_by": confirmed_by.strip(),
        "timestamp": _now_iso(),
        "target": target,
    }
    if note:
        payload["note"] = note
    return _write_confirmation(confirmations_dir_for(job), f"{mapping_id.strip()}.json", payload)


def remove_decision(job: dict, mapping_id: str) -> bool:
    path = confirmations_dir_for(job) / f"{mapping_id}.json"
    if path.is_file():
        path.unlink()
        return True
    return False


def _now_iso() -> str:
    import time
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def _write_confirmation(directory: Path, name: str, payload: dict) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path)
    return path


def export_rows(validation: dict | None) -> list[tuple[str, str, str]]:
    """Rows for the B6 export stage result (pipeline stage F)."""
    export = (validation or {}).get("case_export")
    if not export:
        return []
    return [("pass", "CaseExported",
             f"{export.get('files')} files -> {export.get('target_dir')} package_hash={str(export.get('package_hash'))[:16]}...")]


def coupling_editor_rows(job: dict) -> list[tuple[str, str, str]]:
    layers = build_coupling_link_layers(job)
    s = layers["summary"]
    total = sum(s.values())
    if total == 0:
        return [("info", "NoCouplingCandidates", "run the pipeline with coupling maps first")]
    severity = "pass" if s["unconfirmed"] == 0 and s["drifted"] == 0 else "review"
    return [(severity, "CouplingEditor",
             f"candidates={total} unconfirmed={s['unconfirmed']} accepted={s['accepted']} "
             f"retargeted={s['retargeted']} rejected={s['rejected']} drifted={s['drifted']} "
             f"dir={confirmations_dir_for(job)}")]


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
        "effective_links": output / "coupling/effective_links.json",
    }
    controls = (job.get("mesh_controls") or {}).get("geojson")
    if controls:
        candidates["mesh_controls"] = Path(controls)
    return {name: str(path) for name, path in candidates.items() if path.is_file()}
