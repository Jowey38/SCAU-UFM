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
    dpm_rule_table: str | None = None,
    crs_policy: str | None = None,
    terrain_policy: str | None = None,
    drainage_network: dict | None = None,
    river_sketch: dict | None = None,
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
    if dpm_rule_table:
        job["field_derivation"] = {"dpm_rule_table": str(Path(dpm_rule_table))}
    if crs_policy:
        job["crs"] = {"policy": str(Path(crs_policy))}
    if terrain_policy:
        job["terrain_condition"] = {"policy": str(Path(terrain_policy))}
    if drainage_network is not None or river_sketch is not None:
        job = build_network_job_config(job, drainage=drainage_network, river=river_sketch)
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


# --- E2 data tree --------------------------------------------------------------

DATA_TREE_GROUPS = (
    ("Terrain", ("raster_dem",)),
    ("Land Cover / Soil", ("vector_gis:landcover", "vector_gis:soil_zones", "table")),
    ("Geometries", ("vector_gis:computational_boundary", "vector_gis:buildings")),
    ("1D Networks", ("swmm_5_2_x", "mapping_table", "dflowfm_bmi_or_native")),
    ("Policies", ("policy",)),
)
LAMPS = ("pass", "review", "fatal", "missing")


def _dataset_group(dataset: dict) -> str:
    kind = dataset.get("kind", "")
    key_specific = f"{kind}:{dataset.get('id')}"
    for group, kinds in DATA_TREE_GROUPS:
        if kind in kinds or key_specific in kinds:
            return group
    return "Other"


def _crs_lamp(package: Path, dataset: dict) -> tuple[str, str]:
    """Pass when a metre-based projected CRS is declared, review while the
    declaration is a provider TODO (synthetic template), fatal for lat/lon."""
    manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
    crs = str(manifest.get("target_crs", ""))
    units = str(manifest.get("horizontal_units", ""))
    if dataset.get("kind") == "raster_dem" and dataset.get("metadata"):
        meta_path = package / dataset["metadata"]
        if meta_path.is_file():
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            crs = str(meta.get("target_crs") or meta.get("source_crs") or crs)
            units = str(meta.get("horizontal_units") or units)
    if "4326" in crs or crs.upper().startswith("WGS84") or units.lower() in ("degree", "degrees"):
        return "fatal", f"geographic CRS not allowed ({crs})"
    if not crs or "TODO" in crs:
        return "review", f"CRS undeclared ({crs or 'empty'}); units={units or '?'}"
    if units.lower() not in ("metre", "meter", "m", "metres", "meters"):
        return "review", f"CRS {crs} but horizontal units {units!r}"
    return "pass", f"{crs} / {units}"


def _findings_by_object(validation: dict | None) -> dict[str, list[dict]]:
    index: dict[str, list[dict]] = {}
    for finding in (validation or {}).get("findings", []):
        for obj in finding.get("objects") or []:
            index.setdefault(str(obj.get("kind", "")), []).append({**finding, "object": obj})
    return index


def data_tree(job: dict, validation: dict | None = None) -> list[dict]:
    """[{group, items: [{id, path, kind, required, lamp, detail, layer}]}] for
    the RAS-Mapper-style tree. `layer` is a loadable path when the dataset is a
    GeoJSON (the shell offers "load" on double-click)."""
    package = Path(job["package"])
    manifest_path = package / "manifest.json"
    if not manifest_path.is_file():
        return [{"group": "Package", "items": [{"id": "manifest", "path": "manifest.json", "kind": "manifest",
                                                  "required": True, "lamp": "fatal",
                                                  "detail": "manifest.json missing", "layer": None}]}]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    validation = validation if validation is not None else load_validation_for(job)
    by_kind = _findings_by_object(validation)
    governed = ((validation or {}).get("crs_governance") or {}).get("datasets") or {}
    terrain = (validation or {}).get("terrain_condition")
    groups: dict[str, list[dict]] = {group: [] for group, _ in DATA_TREE_GROUPS}
    groups["Other"] = []
    for dataset in manifest.get("datasets", []):
        path = package / dataset["path"]
        exists = path.is_file() or (dataset["path"].endswith("/") and path.is_dir() and any(path.iterdir()))
        if not exists:
            lamp, detail = ("fatal", "required file missing") if dataset.get("required") else ("missing", "optional; not provided")
        else:
            lamp, detail = "pass", "present"
            if dataset["id"] in governed:
                # B2 ran: the pipeline's audit is the authority for CRS status.
                entry = governed[dataset["id"]]
                lamp = "pass" if entry.get("action") in ("copied_identity", "reprojected", "coordinates_rewritten") else "review"
                detail = f"CRS {entry.get('source_crs')} -> {entry.get('target_crs')} ({entry.get('action')})"
            elif dataset.get("kind") in ("raster_dem", "vector_gis"):
                lamp, detail = _crs_lamp(package, dataset)
            if dataset.get("kind") == "raster_dem" and terrain and terrain.get("enabled"):
                # B3 ran: the DEM the mesh saw is the CONDITIONED one; always a
                # review lamp so the operator looks at the change rasters.
                summary = terrain.get("change_summary") or {}
                lamp = "review"
                detail = (f"conditioned ({', '.join(terrain.get('active_operations') or [])}): "
                          f"{summary.get('cells_changed')} cells changed, max |dz| {summary.get('max_abs_change_m') or 0:.3f} m")
            if dataset.get("kind") == "mapping_table" and any(
                    "TODO_PROVIDER" in line for line in path.read_text(encoding="utf-8").splitlines()[:50]):
                lamp, detail = "review", "provider placeholders present"
            if dataset.get("kind") == "dflowfm_bmi_or_native":
                lamp, detail = "review", "provider_required (no authorized river model)"
        object_kind = {"buildings": "building", "computational_boundary": "boundary"}.get(dataset["id"])
        related = by_kind.get(object_kind, []) if object_kind else []
        if any(f.get("severity") == "fatal" for f in related):
            lamp, detail = "fatal", f"{len(related)} fatal finding(s) reference objects of this layer"
        elif related and lamp == "pass":
            lamp, detail = "review", f"{len(related)} finding(s) reference objects of this layer"
        groups[_dataset_group(dataset)].append({
            "id": dataset["id"], "path": dataset["path"], "kind": dataset.get("kind"),
            "required": bool(dataset.get("required")), "lamp": lamp, "detail": detail,
            "layer": str(path) if path.suffix == ".geojson" and path.is_file() else None,
        })
    outputs = []
    output = Path(job["output_dir"]) if job.get("output_dir") else None
    if output and output.is_dir():
        status = (validation or {}).get("status")
        lamp = {"ok": "pass", "review": "review", "fatal": "fatal"}.get(status, "missing")
        outputs.append({"id": "validation", "path": "validation.json", "kind": "report", "required": True,
                        "lamp": lamp, "detail": f"status={status}", "layer": None})
        for name, kind in (("mesh_quality_cells.geojson", "diagnostic"), ("generator.diagnostic.geojson", "diagnostic"),
                           ("coupling/effective_links.json", "coupling"), ("pipeline_manifest.json", "report"),
                           ("conditioned_terrain/dem.asc", "raster"),
                           ("conditioned_terrain/depression_depth.asc", "raster"),
                           ("conditioned_terrain/terrain_change.asc", "raster")):
            path = output / name
            if path.is_file():
                item_lamp = "pass"
                if name == "generator.diagnostic.geojson":
                    item_lamp = "fatal"
                if name == "coupling/effective_links.json":
                    item_lamp = "pass" if ((validation or {}).get("coupling_confirmations") or {}).get("status") == "complete" else "review"
                if kind == "raster":
                    item_lamp = "review"
                outputs.append({"id": name, "path": name, "kind": kind, "required": False, "lamp": item_lamp,
                                "detail": "present", "layer": str(path) if path.suffix in (".geojson", ".asc") else None})
        export = (validation or {}).get("case_export")
        if export:
            outputs.append({"id": "case_export", "path": export.get("target_dir"), "kind": "package", "required": False,
                            "lamp": "pass", "detail": f"{export.get('files')} files, hash {str(export.get('package_hash'))[:12]}",
                            "layer": None})
    tree = [{"group": group, "items": items} for group, items in groups.items() if items]
    if outputs:
        tree.append({"group": "Outputs", "items": outputs})
    return tree


def data_tree_summary(tree: list[dict]) -> dict[str, int]:
    counts = {lamp: 0 for lamp in LAMPS}
    for group in tree:
        for item in group["items"]:
            counts[item["lamp"]] += 1
    return counts


def load_validation_for(job: dict) -> dict | None:
    path = Path(job.get("output_dir", "")) / "validation.json" if job.get("output_dir") else None
    if path and path.is_file():
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None
    return None


# --- E5 report browser ---------------------------------------------------------

REPORT_PAGES = ("import", "terrain_mesh", "field", "coupling", "export", "reproducibility")


def _read_json(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else None
    except (OSError, ValueError):
        return None


def _field_page(output: Path, manifest: dict) -> list[tuple[str, str]]:
    """Stage D rows: rule-table derivation report when present (B5), else the
    recorded v1 placeholder rules."""
    report = _read_json(output / "field_derivation.json")
    def s(v):
        return "" if v is None else (json.dumps(v, ensure_ascii=False) if isinstance(v, (dict, list)) else str(v))
    if not report:
        return [("mode", "v1 placeholders (no job_config.field_derivation)"),
                ("manning_n", "landcover first-match LUT"), ("z_b", "DEM nearest sample"),
                ("soil_type", "landcover soil_type"), ("phi_t / Phi_c / omega_edge / phi_e_n / phi_et", "unit placeholders")]
    approval = report.get("approval") or {}
    rows = [("mode", "rule-table derivation (B5)"), ("rule_table", report.get("rule_table")),
            ("rule_table_sha256", report.get("rule_table_sha256") or (manifest.get("field_derivation") or {}).get("dpm_rule_table_sha256")),
            ("approval", f"{approval.get('status')} by={approval.get('approved_by')} date={approval.get('date')}"),
            ("cells_by_class", s(report.get("cells_by_class"))), ("phi_t_range", s(report.get("phi_t_range"))),
            ("landcover_overlap", f"{report.get('landcover_overlap_rule')} ({report.get('overlapping_landcover_cells')} cells)"),
            ("unmapped_cells_defaulted", s(report.get("unmapped_cells_defaulted"))),
            ("soil", f"{report.get('soil_source')} (missing-zone defaulted: {report.get('soil_missing_zone_defaulted')})"),
            ("edge_rule", report.get("edge_rule")), ("interface_edges", s(report.get("interface_edges"))),
            ("phi_e_n_range", s(report.get("phi_e_n_range"))), ("omega_edge_values", s(report.get("omega_edge_values")))]
    return [(k, s(v)) for k, v in rows]


def report_pages(job: dict) -> dict[str, list[tuple[str, str]]]:
    """Key/value rows per report page, all read from disk on every call."""
    output = Path(job["output_dir"])
    validation = _read_json(output / "validation.json") or {}
    manifest = _read_json(output / "pipeline_manifest.json") or {}
    quality = _read_json(output / "mesh_quality.json") or {}
    mapping = _read_json(output / "coupling/mapping_report.json") or {}
    effective = _read_json(output / "coupling/effective_links.json") or {}
    export = validation.get("case_export") or {}
    package_manifest = _read_json(Path(export["target_dir"]) / "manifest.json") if export.get("target_dir") else None

    def kv(*pairs):
        return [(str(k), "" if v is None else (json.dumps(v, ensure_ascii=False) if isinstance(v, (dict, list)) else str(v)))
                for k, v in pairs]

    audit = validation.get("policy_audit") or {}
    crs = validation.get("crs_governance") or {}
    terrain = validation.get("terrain_condition") or {}
    terrain_ops = terrain.get("operations") or {}
    fill = terrain_ops.get("fill_depressions") or {}
    change = terrain.get("change_summary") or {}
    pages = {
        "import": kv(("status", validation.get("status")), ("package", validation.get("package")),
                     ("governed_package", validation.get("governed_package")),
                     ("crs_policy", crs.get("policy") or "(none: v1 unchecked mode)"),
                     ("target_crs", crs.get("target_crs")), ("proj_version", crs.get("proj_version")),
                     ("reprojected_files", crs.get("reprojected_files")),
                     ("job_config", validation.get("job_config")), ("job_config_sha256", validation.get("job_config_sha256")),
                     ("policy_version", audit.get("policy_version")), ("generator_timeout_s", audit.get("generator_timeout_s")),
                     ("repair_mode", audit.get("repair_mode")), ("started", validation.get("started"))),
        "terrain_mesh": kv(("terrain_policy", terrain.get("policy") or "(none: DEM sampled verbatim)"),
                           ("terrain_enabled", terrain.get("enabled")),
                           ("terrain_operations", terrain.get("active_operations")),
                           ("terrain_authorization", terrain.get("authorization")),
                           ("terrain_fill", None if not fill else
                            f"{fill.get('algorithm')} v{fill.get('algorithm_version')} eps={fill.get('epsilon_m')} "
                            f"filled={fill.get('cells_filled')} max_depth={fill.get('max_fill_depth_m')}"),
                           ("terrain_change", None if not change else
                            f"cells_changed={change.get('cells_changed')} max_abs={change.get('max_abs_change_m')} "
                            f"net_m3={change.get('net_change_m3')}"),
                           ("conditioned_dem_sha256", terrain.get("conditioned_dem_sha256")),
                           ("gmsh_version", quality.get("gmsh_version")),
                           ("characteristic_length_m", quality.get("characteristic_length_m")),
                           ("recombine", quality.get("recombine")), ("nodes", quality.get("nodes")),
                           ("edges", quality.get("edges")), ("cells", (quality.get("quality") or {}).get("cells")),
                           ("triangles", (quality.get("quality") or {}).get("triangles")),
                           ("quadrilaterals", (quality.get("quality") or {}).get("quadrilaterals")),
                           ("min_angle_degrees", (quality.get("quality") or {}).get("min_angle_degrees")),
                           ("max_edge_length_ratio", (quality.get("quality") or {}).get("max_edge_length_ratio")),
                           ("max_equiangle_skewness", (quality.get("per_cell_diagnostics") or {}).get("max_equiangle_skewness")),
                           ("max_nonorthogonality_deg", (quality.get("per_cell_diagnostics") or {}).get("max_nonorthogonality_deg")),
                           ("mesh_controls", (quality.get("mesh_controls") or {}).get("geojson")),
                           ("breaklines_preserved", [b.get("control_id") for b in (quality.get("mesh_controls") or {}).get("breaklines", [])]),
                           ("determinism", validation.get("determinism")),
                           ("authoritative_validate", (validation.get("authoritative_validate") or {}).get("stdout"))),
        "field": _field_page(output, manifest),
        "coupling": kv(("mode", mapping.get("mode")), ("mode_parameters", mapping.get("mode_parameters")),
                       ("chains", mapping.get("chains")), ("generator_findings", len(mapping.get("findings") or [])),
                       ("effective_status", effective.get("status")), ("unconfirmed_total", effective.get("unconfirmed_total")),
                       ("confirmations_loaded", effective.get("confirmations_loaded")),
                       ("effective_surface_links", len((effective.get("links") or {}).get("surface_to_swmm", []))),
                       ("effective_roof_links", len((effective.get("links") or {}).get("roof_to_swmm", []))),
                       ("placeholders", mapping.get("placeholders"))),
        "export": kv(("target_dir", export.get("target_dir")), ("package_hash", export.get("package_hash")),
                     ("files", export.get("files")),
                     ("gate", (package_manifest or {}).get("gate")), ("run_conf", (package_manifest or {}).get("run_conf"))),
        "reproducibility": kv(("reproduce", manifest.get("reproduce")), ("case_sha256", manifest.get("case_sha256")),
                              ("package_manifest_sha256", manifest.get("package_manifest_sha256")),
                              ("geometry_clean_policy_sha256", manifest.get("geometry_clean_policy_sha256")),
                              ("generator_config_sha256", manifest.get("generator_config_sha256")),
                              ("mesh_controls_sha256", (manifest.get("mesh_controls") or {}).get("geojson_sha256")),
                              ("terrain_condition", manifest.get("terrain_condition")),
                              ("confirmations", manifest.get("confirmations")),
                              ("package_reproduce", (package_manifest or {}).get("reproduce"))),
    }
    return pages


def findings_table(validation: dict | None) -> list[dict]:
    """One row per (finding, object): the E5 findings list with locators."""
    rows = []
    for finding in (validation or {}).get("findings", []):
        objects = finding.get("objects") or [None]
        for obj in objects:
            rows.append({"severity": finding.get("severity"), "code": finding.get("code"),
                         "detail": finding.get("detail", ""),
                         "feature_id": (obj or {}).get("feature_id"), "kind": (obj or {}).get("kind"),
                         "violation": (obj or {}).get("violation")})
    return rows


def locate_object(job: dict, kind: str | None, feature_id: str | None) -> tuple[str, str, str] | None:
    """(layer_name, layer_path, expression) so the shell can select + zoom to
    the offending object. Returns None when the kind has no map layer."""
    if not kind or feature_id is None:
        return None
    package = Path(job["package"])
    output = Path(job["output_dir"])
    fid = str(feature_id).replace("'", "''")
    if kind.startswith("mesh_control:"):
        controls = (job.get("mesh_controls") or {}).get("geojson")
        if controls and Path(controls).is_file():
            return ("scau_mesh_controls", str(controls), f"\"control_id\" = '{fid}'")
        return None
    if kind.startswith("coupling:"):
        editor = output / "coupling/editor/links.geojson"
        if editor.is_file():
            return ("scau_coupling_links", str(editor), f"\"mapping_id\" = '{fid}'")
        return None
    if kind == "building":
        return ("scau_buildings", str(package / "buildings/buildings.geojson"), f"\"building_id\" = '{fid}'")
    if kind in ("boundary", "building_constraint"):
        diagnostic = output / "generator.diagnostic.geojson"
        if diagnostic.is_file():
            return ("scau_generator_diagnostic", str(diagnostic), f"\"feature_id\" = '{fid}'")
        return None
    if kind.startswith("swmm:"):
        editor = output / "coupling/editor/nodes.geojson"
        if editor.is_file():
            return ("scau_coupling_nodes", str(editor), f"\"swmm_node_id\" = '{fid}'")
        return None
    return None


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


# --- E5a parameter tables (P5) --------------------------------------------------
#
# Browsing is read-only over metadata/dpm_rule_table.json and
# soil/soil_parameters.csv. Editing NEVER rewrites the source file: a new
# versioned sibling (<stem>.v<NNN>.json / .csv) is written with a `revision`
# block, and an edited rule table drops back to approval.status
# "synthetic_unapproved" (an edit invalidates the data owner's approval, M281).
# The pipeline stays the single validation authority; the lint here only makes
# a bad value locatable while typing.

RULE_CLASS_COLUMNS = ("class_code", "phi_t", "phi_xx", "phi_xy", "phi_yy", "note")
RULE_INTERFACE_COLUMNS = ("class_a", "class_b", "omega_edge", "note")
SOIL_COLUMNS = ("soil_type", "soil_name", "K_s", "psi_f", "theta_s", "theta_i",
                "K_s_units", "psi_f_units", "theta_units", "source_or_authority")
DEFAULT_CLOSURE_LIMITS = {"epsilon_phi": 1.0e-6, "cond_max": 1.0e4}


def rule_table_path(job: dict) -> Path | None:
    configured = (job.get("field_derivation") or {}).get("dpm_rule_table")
    if configured:
        return Path(configured)
    default = Path(job["package"]) / "metadata" / "dpm_rule_table.json"
    return default if default.is_file() else None


def soil_table_path(job: dict) -> Path:
    return Path(job["package"]) / "soil" / "soil_parameters.csv"


def load_rule_table(path: Path) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def rule_class_rows(table: dict) -> list[dict]:
    rows = []
    for code, entry in sorted((table.get("classes") or {}).items()):
        tensor = entry.get("Phi_c") or {}
        rows.append({"class_code": code, "phi_t": entry.get("phi_t"), "phi_xx": tensor.get("xx"),
                     "phi_xy": tensor.get("xy"), "phi_yy": tensor.get("yy"), "note": entry.get("note", "")})
    return rows


def rule_interface_rows(table: dict) -> list[dict]:
    rows = []
    for item in (table.get("edges") or {}).get("interfaces") or []:
        classes = list(item.get("classes") or ["", ""])
        rows.append({"class_a": classes[0], "class_b": classes[1] if len(classes) > 1 else "",
                     "omega_edge": item.get("omega_edge"), "note": item.get("note", "")})
    return rows


def rule_table_from_rows(base: dict, class_rows: list[dict], interface_rows: list[dict]) -> dict:
    """Rebuilds a rule table from edited rows, keeping every non-tabular block
    (approval, closure_limits, soil, unmapped_class, ...) of `base`."""
    table = json.loads(json.dumps(base))
    classes = {}
    for row in class_rows:
        code = str(row.get("class_code", "")).strip()
        if not code:
            raise ValueError("class_code must not be empty")
        if code in classes:
            raise ValueError(f"duplicate class_code {code!r}")
        try:
            classes[code] = {"phi_t": float(row["phi_t"]),
                             "Phi_c": {"xx": float(row["phi_xx"]), "xy": float(row["phi_xy"]), "yy": float(row["phi_yy"])}}
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"class {code!r}: phi_t / Phi_c must be numbers ({error})") from error
        if row.get("note"):
            classes[code]["note"] = str(row["note"])
    table["classes"] = classes
    interfaces = []
    for row in interface_rows:
        a, b = str(row.get("class_a", "")).strip(), str(row.get("class_b", "")).strip()
        if not a or not b:
            raise ValueError("interface rows need class_a and class_b")
        try:
            omega = float(row["omega_edge"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"interface {a}/{b}: omega_edge must be a number ({error})") from error
        entry = {"classes": [a, b], "omega_edge": omega}
        if row.get("note"):
            entry["note"] = str(row["note"])
        interfaces.append(entry)
    table.setdefault("edges", {})["interfaces"] = interfaces
    return table


def lint_rule_table(table: dict) -> list[tuple[str, str, str]]:
    """UI pre-flight mirroring the closure laws the pipeline enforces
    (phi_t in (0,1], Phi_c SPD with lambda in [epsilon_phi, 1], cond <= cond_max,
    phi_t >= max diagonal, interface classes known, omega in [0,1])."""
    import math
    limits = {**DEFAULT_CLOSURE_LIMITS, **(table.get("closure_limits") or {})}
    rows: list[tuple[str, str, str]] = []
    classes = table.get("classes") or {}
    if not classes:
        rows.append(("fatal", "RuleTableEmpty", "classes must not be empty"))
    for code, entry in sorted(classes.items()):
        try:
            phi_t = float(entry["phi_t"])
            xx, xy, yy = (float(entry["Phi_c"][k]) for k in ("xx", "xy", "yy"))
        except (KeyError, TypeError, ValueError):
            rows.append(("fatal", "RuleClassIncomplete", f"{code}: phi_t and Phi_c.xx/xy/yy are required numbers"))
            continue
        if not 0.0 < phi_t <= 1.0:
            rows.append(("fatal", "RulePhiTRange", f"{code}: phi_t={phi_t} must be in (0, 1]"))
        half_trace = 0.5 * (xx + yy)
        radius = math.hypot(0.5 * (xx - yy), xy)
        lam_min, lam_max = half_trace - radius, half_trace + radius
        if lam_min < limits["epsilon_phi"]:
            rows.append(("fatal", "RuleTensorNotSPD", f"{code}: lambda_min={lam_min:.3e} < epsilon_phi={limits['epsilon_phi']}"))
        if lam_max > 1.0 + 1e-12:
            rows.append(("fatal", "RuleTensorTooLarge", f"{code}: lambda_max={lam_max:.6f} > 1"))
        if lam_min > 0 and lam_max / lam_min > limits["cond_max"]:
            rows.append(("fatal", "RuleTensorCondition", f"{code}: condition number {lam_max / lam_min:.3e} > {limits['cond_max']}"))
        if phi_t < max(xx, yy) - 1e-12:
            rows.append(("fatal", "RuleStorageBelowConveyance", f"{code}: phi_t={phi_t} < max(Phi_c.xx, Phi_c.yy)={max(xx, yy)}"))
    for item in (table.get("edges") or {}).get("interfaces") or []:
        pair = item.get("classes") or []
        for code in pair:
            if code not in classes:
                rows.append(("fatal", "RuleInterfaceUnknownClass", f"interface {pair}: class {code!r} has no entry"))
        omega = item.get("omega_edge")
        if not isinstance(omega, (int, float)) or isinstance(omega, bool) or not 0.0 <= float(omega) <= 1.0:
            rows.append(("fatal", "RuleOmegaRange", f"interface {pair}: omega_edge={omega!r} must be in [0, 1]"))
    approval = table.get("approval") or {}
    rows.append(("pass" if approval.get("status") == "approved" else "review", "RuleTableApproval",
                 f"status={approval.get('status')} by={approval.get('approved_by')} date={approval.get('date')}"))
    if not any(r[0] == "fatal" for r in rows):
        rows.insert(0, ("pass", "RuleTableLint", f"{len(classes)} class(es) satisfy the closure laws (authority: pipeline)"))
    return rows


def load_soil_rows(path: Path) -> list[dict]:
    import csv
    with Path(path).open(encoding="utf-8", newline="") as handle:
        return [dict(row) for row in csv.DictReader(handle)]


def lint_soil_rows(rows: list[dict]) -> list[tuple[str, str, str]]:
    out: list[tuple[str, str, str]] = []
    seen = set()
    for row in rows:
        try:
            soil_type = int(row["soil_type"])
            k_s, psi_f, theta_s, theta_i = (float(row[k]) for k in ("K_s", "psi_f", "theta_s", "theta_i"))
        except (KeyError, TypeError, ValueError):
            out.append(("fatal", "SoilRowIncomplete", f"{row.get('soil_type')!r}: soil_type/K_s/psi_f/theta_s/theta_i must be numbers"))
            continue
        if soil_type in seen:
            out.append(("fatal", "SoilTypeDuplicate", f"soil_type {soil_type} appears twice"))
        seen.add(soil_type)
        if k_s < 0:
            out.append(("fatal", "SoilKsNegative", f"soil_type {soil_type}: K_s={k_s} < 0"))
        if psi_f <= 0:
            out.append(("fatal", "SoilPsiNotPositive", f"soil_type {soil_type}: psi_f={psi_f} must be > 0"))
        if not 0 < theta_s <= 1:
            out.append(("fatal", "SoilThetaSRange", f"soil_type {soil_type}: theta_s={theta_s} must be in (0, 1]"))
        if not 0 <= theta_i < theta_s:
            out.append(("fatal", "SoilThetaIRange", f"soil_type {soil_type}: theta_i={theta_i} must be in [0, theta_s)"))
    expected = list(range(len(rows)))
    if sorted(seen) != expected:
        out.append(("fatal", "SoilTypeNotContiguous", f"soil_type values {sorted(seen)} must be 0..{len(rows) - 1} (STCF soil_type_entry index)"))
    if not any(r[0] == "fatal" for r in out):
        out.insert(0, ("pass", "SoilTableLint", f"{len(rows)} soil type(s) valid (authority: pipeline)"))
    return out


def next_version_path(source: Path) -> Path:
    """<dir>/<stem>.v<NNN><suffix> with NNN = 1 + highest existing version.
    A source that is itself a version (stem ends in .vNNN) shares the family."""
    import re
    source = Path(source)
    match = re.match(r"^(.*)\.v(\d{3,})$", source.stem)
    family = match.group(1) if match else source.stem
    highest = 0
    for sibling in source.parent.glob(f"{family}.v*{source.suffix}"):
        m = re.match(rf"^{re.escape(family)}\.v(\d{{3,}})$", sibling.stem)
        if m:
            highest = max(highest, int(m.group(1)))
    return source.parent / f"{family}.v{highest + 1:03d}{source.suffix}"


def write_rule_table_version(source: Path, table: dict, *, edited_by: str, note: str = "") -> Path:
    """Writes the edited table as a NEW version next to `source` (never in
    place). Approval is reset: edited values are unapproved until the data
    owner re-approves them (the pipeline refuses unapproved tables for real
    packages)."""
    import hashlib
    if not edited_by.strip():
        raise ValueError("edited_by must not be empty")
    fatal = [r for r in lint_rule_table(table) if r[0] == "fatal"]
    if fatal:
        raise ValueError("; ".join(f"{code}: {detail}" for _, code, detail in fatal))
    source = Path(source)
    target = next_version_path(source)
    payload = json.loads(json.dumps(table))
    payload["approval"] = {"status": "synthetic_unapproved", "approved_by": None, "date": None,
                           "note": f"edited by {edited_by.strip()} on {_now_iso()}; re-approval required before use on real data"}
    payload["revision"] = {"based_on": source.name,
                           "based_on_sha256": hashlib.sha256(source.read_bytes()).hexdigest() if source.is_file() else None,
                           "edited_by": edited_by.strip(), "timestamp": _now_iso(), "note": note}
    tmp = target.with_suffix(target.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(target)
    return target


def write_soil_table_version(source: Path, rows: list[dict], *, edited_by: str) -> Path:
    import csv
    if not edited_by.strip():
        raise ValueError("edited_by must not be empty")
    fatal = [r for r in lint_soil_rows(rows) if r[0] == "fatal"]
    if fatal:
        raise ValueError("; ".join(f"{code}: {detail}" for _, code, detail in fatal))
    source = Path(source)
    target = next_version_path(source)
    columns = list(SOIL_COLUMNS)
    for row in rows:
        for key in row:
            if key not in columns:
                columns.append(key)
    tmp = target.with_suffix(target.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        for row in sorted(rows, key=lambda r: int(r["soil_type"])):
            writer.writerow({**{c: "" for c in columns}, **row,
                             "source_or_authority": f"edited_by:{edited_by.strip()} (re-approval required)"})
    tmp.replace(target)
    return target


def parameter_table_rows(job: dict) -> list[tuple[str, str, str]]:
    """Status rows for the P5 page (which tables are loaded + lint)."""
    rows: list[tuple[str, str, str]] = []
    rule_path = rule_table_path(job)
    if rule_path is None or not rule_path.is_file():
        rows.append(("info", "NoRuleTable", "no dpm_rule_table.json (v1 placeholder fields); set one on the job page"))
    else:
        try:
            rows.append(("info", "RuleTable", str(rule_path)))
            rows.extend(lint_rule_table(load_rule_table(rule_path)))
        except (OSError, ValueError) as error:
            rows.append(("fatal", "RuleTableUnreadable", str(error)))
    soil_path = soil_table_path(job)
    if soil_path.is_file():
        try:
            rows.append(("info", "SoilTable", str(soil_path)))
            rows.extend(lint_soil_rows(load_soil_rows(soil_path)))
        except (OSError, ValueError) as error:
            rows.append(("fatal", "SoilTableUnreadable", str(error)))
    else:
        rows.append(("fatal", "SoilTableMissing", str(soil_path)))
    return rows


# --- N1/N2 network workbench pure helpers -----------------------------------------

NETWORK_MODES = ("external", "authored")


def _network_block(job: dict, key: str) -> dict:
    value = job.get(key) or {}
    if not isinstance(value, dict):
        raise ValueError(f"{key} must be an object")
    mode = value.get("mode", "authored") if key == "drainage_network" else "authored"
    if key == "drainage_network" and mode not in NETWORK_MODES:
        raise ValueError(f"unknown drainage mode {mode!r}")
    return value


def validate_network_geojson(path: str | Path, kind: str) -> list[tuple[str, str, str]]:
    """Pure UI preflight; authoritative validation remains the author module."""
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        if kind == "drainage_network":
            from scau_preproc.swmm_author import geojson_to_model
            geojson_to_model(data)
        elif kind == "river_sketch":
            from scau_preproc.dflowfm_author import validate_sketch
            validate_sketch(data)
        else:
            raise ValueError(f"unknown network kind {kind!r}")
    except (OSError, ValueError, TypeError, KeyError) as error:
        return [("fatal", "NetworkDraftInvalid", str(error))]
    return [("pass", "NetworkDraftValid", f"{kind}: {path}")]


def network_rows(job: dict, kind: str) -> list[tuple[str, str, str]]:
    block = _network_block(job, kind)
    path = block.get("geojson")
    if not path:
        return [("info", "NetworkDraftMissing", f"{kind} draft not configured")]
    rows = validate_network_geojson(path, kind)
    if kind == "river_sketch":
        manifest = Path(block.get("output_dir", job.get("output_dir", ""))) / "authoring_manifest.json"
        if manifest.is_file():
            try:
                data = json.loads(manifest.read_text(encoding="utf-8"))
                required = data.get("provider_required") or []
                rows.append(("review" if required else "pass", "ProviderRequired", f"{len(required)} hydraulic field group(s)"))
            except (OSError, ValueError):
                rows.append(("fatal", "AuthoringManifestUnreadable", str(manifest)))
    return rows


def author_network(job: dict, kind: str) -> dict:
    block = _network_block(job, kind)
    path = block.get("geojson")
    if not path:
        raise ValueError(f"{kind}.geojson is required")
    output = Path(block.get("output_dir") or job.get("output_dir", ""))
    output.mkdir(parents=True, exist_ok=True)
    if kind == "drainage_network":
        from scau_preproc.swmm_author import author_network as author
        external = block.get("external_inp") if block.get("mode") == "external" else None
        return author(path, output / "model.inp", external_inp=external, parser_cli=block.get("parser_cli"))
    from scau_preproc.dflowfm_author import author_river
    return author_river(path, output, block.get("case_name", "river"))


def build_network_job_config(job: dict, *, drainage: dict | None = None, river: dict | None = None) -> dict:
    result = dict(job)
    if drainage is not None:
        if drainage.get("mode") not in NETWORK_MODES:
            raise ValueError("drainage mode must be external or authored")
        if drainage.get("mode") == "external" and not drainage.get("external_inp"):
            raise ValueError("external mode requires external_inp")
        if drainage.get("mode") == "authored" and drainage.get("external_inp"):
            raise ValueError("authored mode cannot include external_inp")
        result["drainage_network"] = dict(drainage)
    if river is not None:
        result["river_sketch"] = dict(river)
    return result


# --- P3 field mapping ---------------------------------------------------------------
#
# Source fields are discovered from the package datasets (GeoJSON property
# names, CSV headers, DEM = "elevation"); targets are ONLY the canonical set the
# pipeline accepts (manifest.canonical_targets, itself validated against
# scau_preproc.pipeline.CANONICAL_TARGETS) plus "(unmapped)". The mapping is
# written as a versioned metadata/field_mapping.v<NNN>.json; it is a
# declaration for the importer (M287-D, BLOCKED on M281) and never changes what
# the current synthetic pipeline computes.

CANONICAL_TARGETS = (
    "node_x", "node_y", "face_nodes", "edge_nodes",
    "phi_t", "phi_xx", "phi_xy", "phi_yy",
    "manning_n", "z_b", "soil_type",
    "omega_edge", "phi_e_n", "phi_et",
)
UNMAPPED = "(unmapped)"
FIELD_MAPPING_SCHEMA_VERSION = 1
# Targets that the generator produces itself (mesh topology / derived DPM
# fields); they never need a source column in the package.
GENERATOR_OWNED_TARGETS = frozenset({"node_x", "node_y", "face_nodes", "edge_nodes", "omega_edge", "phi_e_n",
                                     "phi_et", "phi_t", "phi_xx", "phi_xy", "phi_yy"})
# Default suggestions: identical names map to themselves; nothing else is inferred.
_DEFAULT_TARGET_BY_SOURCE = {"elevation": "z_b", "manning_n": "manning_n", "soil_type": "soil_type"}


def canonical_targets(job: dict) -> list[str]:
    """Targets offered by the dropdown: the manifest's canonical_targets when
    declared (every entry must be canonical), else the full canonical set."""
    manifest_path = Path(job["package"]) / "manifest.json"
    declared = None
    if manifest_path.is_file():
        try:
            declared = json.loads(manifest_path.read_text(encoding="utf-8")).get("canonical_targets")
        except (OSError, ValueError):
            declared = None
    if declared:
        unknown = [t for t in declared if t not in CANONICAL_TARGETS]
        if unknown:
            raise ValueError(f"manifest declares non-canonical targets {unknown}")
        return list(declared)
    return list(CANONICAL_TARGETS)


def discover_source_fields(job: dict) -> list[dict]:
    """[{dataset, layer, source_field, source_type, sample}] from the package."""
    import csv
    package = Path(job["package"])
    manifest_path = package / "manifest.json"
    if not manifest_path.is_file():
        return []
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    fields: list[dict] = []
    for dataset in manifest.get("datasets", []):
        path = package / dataset["path"]
        kind = dataset.get("kind")
        if not path.is_file():
            continue
        if kind == "raster_dem":
            fields.append({"dataset": dataset["id"], "layer": path.name, "kind": kind, "source_field": "elevation",
                           "source_type": "float", "sample": "cell value"})
        elif path.suffix == ".geojson":
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            seen: dict[str, object] = {}
            for feature in data.get("features", []):
                for key, value in (feature.get("properties") or {}).items():
                    seen.setdefault(key, value)
            for key, value in seen.items():
                fields.append({"dataset": dataset["id"], "layer": path.name, "kind": kind, "source_field": key,
                               "source_type": type(value).__name__ if value is not None else "null",
                               "sample": "" if value is None else str(value)[:40]})
        elif path.suffix == ".csv":
            with path.open(encoding="utf-8", newline="") as handle:
                reader = csv.DictReader(handle)
                first = next(reader, None) or {}
                for key in reader.fieldnames or []:
                    fields.append({"dataset": dataset["id"], "layer": path.name, "kind": kind, "source_field": key,
                                   "source_type": "text", "sample": str(first.get(key, ""))[:40]})
    return fields


def field_dictionary_targets(job: dict) -> dict[tuple[str, str], str]:
    """(dataset|layer, source_field) -> target_field from metadata/field_dictionary.csv.
    Canonical targets are kept; an explicit 'N/A' is recorded as UNMAPPED so it
    overrides the same-name default (e.g. soil_parameters.soil_type is the LUT
    key, not a cell field); non-canonical targets are ignored."""
    import csv
    path = Path(job["package"]) / "metadata" / "field_dictionary.csv"
    out: dict[tuple[str, str], str] = {}
    if not path.is_file():
        return out
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            target = (row.get("target_field") or "").strip()
            if target in CANONICAL_TARGETS or target.upper() == "N/A":
                value = target if target in CANONICAL_TARGETS else UNMAPPED
                field = (row.get("source_field") or "").strip()
                out[((row.get("dataset") or "").strip().lower(), field)] = value
                out[((row.get("layer") or "").strip().lower(), field)] = value
    return out


def field_mapping_path(job: dict) -> Path:
    return Path(job["package"]) / "metadata" / "field_mapping.json"


def load_field_mapping(job: dict) -> dict | None:
    """Latest version (highest .vNNN) or the base file, else None."""
    base = field_mapping_path(job)
    candidates = sorted(base.parent.glob("field_mapping.v*.json")) if base.parent.is_dir() else []
    path = candidates[-1] if candidates else (base if base.is_file() else None)
    if path is None:
        return None
    try:
        return {**json.loads(path.read_text(encoding="utf-8")), "_file": path.name}
    except (OSError, ValueError):
        return None


def field_mapping_rows(job: dict) -> list[dict]:
    """Rows for the P3 table: discovered source fields with the current target.
    A saved field_mapping (latest version) is authoritative: fields it does not
    list are unmapped. Without one, the field dictionary, then a same-name
    default for spatial datasets, seed the table."""
    targets = set(canonical_targets(job))
    saved = load_field_mapping(job)
    existing = {(m["dataset"], m["source_field"]): m["target"] for m in ((saved or {}).get("mappings") or [])}
    dictionary = field_dictionary_targets(job)
    rows = []
    claimed: dict[str, tuple[str, str]] = {}
    for field in discover_source_fields(job):
        key = (field["dataset"], field["source_field"])
        target = existing.get(key)
        if saved is not None:
            rows.append({**field, "target": target if target in targets else UNMAPPED, "note": ""})
            continue
        if target is None:
            for alias in (field["dataset"].lower(), field["layer"].split(".")[0].lower()):
                target = dictionary.get((alias, field["source_field"]))
                if target:
                    break
        if target is None and field.get("kind") in ("vector_gis", "raster_dem"):
            # Same-name defaults only for spatial datasets: lookup-table columns
            # (soil_parameters.soil_type is the LUT key) are never cell fields.
            target = _DEFAULT_TARGET_BY_SOURCE.get(field["source_field"])
        target = target if target in targets else UNMAPPED
        note = ""
        if target != UNMAPPED:
            # A canonical target has exactly one source; the first dataset (manifest
            # order) keeps the seed, later candidates are left for the operator.
            if target in claimed:
                note = f"seed dropped: {target} already fed by {claimed[target][0]}.{claimed[target][1]}"
                target = UNMAPPED
            else:
                claimed[target] = key
        rows.append({**field, "target": target, "note": note})
    return rows


def write_field_mapping_version(job: dict, rows: list[dict], *, edited_by: str, note: str = "") -> Path:
    """Writes metadata/field_mapping.v<NNN>.json (never overwrites). Every
    target must be canonical (or unmapped); a canonical target may be fed by
    at most one source field."""
    if not edited_by.strip():
        raise ValueError("edited_by must not be empty")
    allowed = set(canonical_targets(job))
    mappings = []
    used: dict[str, tuple[str, str]] = {}
    for row in rows:
        target = row.get("target") or UNMAPPED
        if target == UNMAPPED:
            continue
        if target not in allowed:
            raise ValueError(f"{row['dataset']}.{row['source_field']}: target {target!r} is not a canonical field")
        if target in used:
            raise ValueError(f"target {target!r} mapped from both {used[target]} and "
                             f"({row['dataset']}, {row['source_field']})")
        used[target] = (row["dataset"], row["source_field"])
        mappings.append({"dataset": row["dataset"], "layer": row.get("layer"), "source_field": row["source_field"],
                         "target": target})
    payload = {"field_mapping_schema_version": FIELD_MAPPING_SCHEMA_VERSION,
               "package": Path(job["package"]).name,
               "edited_by": edited_by.strip(), "timestamp": _now_iso(), "note": note,
               "status": "declaration_only (importer binding waits for M281 / M287-D)",
               "mappings": sorted(mappings, key=lambda m: (m["dataset"], m["source_field"]))}
    target_path = next_version_path(field_mapping_path(job))
    target_path.parent.mkdir(parents=True, exist_ok=True)
    tmp = target_path.with_suffix(target_path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(target_path)
    return target_path


def field_mapping_summary(job: dict) -> list[tuple[str, str, str]]:
    rows = field_mapping_rows(job)
    mapped = [r for r in rows if r["target"] != UNMAPPED]
    current = load_field_mapping(job)
    missing = sorted(set(canonical_targets(job)) - {r["target"] for r in mapped} - GENERATOR_OWNED_TARGETS)
    out = [("info", "FieldMapping", f"{len(mapped)}/{len(rows)} source fields mapped; "
                                    f"file={current['_file'] if current else '(none: dictionary + defaults)'}"),
           ("review" if missing else "pass", "FieldMappingCoverage",
            f"sampled targets without a source: {missing or 'none'}")]
    for r in rows:
        if r.get("note"):
            out.append(("review", "FieldMappingAmbiguousSeed", f"{r['dataset']}.{r['source_field']}: {r['note']}"))
    return out
# --- E7 project index (project.ufm.json) ------------------------------------------
#
# A REGENERABLE, NON-AUTHORITATIVE index of one operator's work on a package:
# which job configs were run, where their outputs live, the last known status /
# case hash, and a snapshot of the dialog fields so the session can be reopened.
# Nothing downstream reads it (the pipeline, exporter and goldens only trust
# job_config.json / validation.json / pipeline_manifest.json); deleting it loses
# convenience, never evidence.

PROJECT_INDEX_SCHEMA_VERSION = 1
PROJECT_INDEX_NAME = "project.ufm.json"
UI_STATE_KEYS = ("package", "output_dir", "case_name", "characteristic_length_m", "recombine", "determinism_check",
                 "coupling_maps", "validator_cli", "mesh_controls", "confirmations_dir", "field_derivation", "crs",
                 "terrain_condition", "export_case")


def default_project_index_path(job: dict) -> Path:
    """Beside the output directory: one project index per output root."""
    return Path(job["output_dir"]).parent / PROJECT_INDEX_NAME if job.get("output_dir") else Path(PROJECT_INDEX_NAME)


def load_project_index(path: Path) -> dict | None:
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict) or data.get("project_index_schema_version") != PROJECT_INDEX_SCHEMA_VERSION:
        return None
    return data


def update_project_index(path: Path, job: dict, validation: dict | None, *, repo_root: str | None = None) -> Path:
    """Merges the current job into the index (keyed by output_dir) and refreshes
    the UI snapshot. Atomic write; the previous file is re-read so several
    output dirs of one project accumulate."""
    path = Path(path)
    index = load_project_index(path) or {
        "project_index_schema_version": PROJECT_INDEX_SCHEMA_VERSION,
        "authoritative": False,
        "note": "regenerable UI index; evidence lives in job_config.json / validation.json / pipeline_manifest.json",
        "created": _now_iso(),
        "jobs": [],
    }
    index["updated"] = _now_iso()
    index["package"] = job.get("package")
    if repo_root:
        index["repo_root"] = repo_root
    manifest = _read_json(Path(job["output_dir"]) / "pipeline_manifest.json") if job.get("output_dir") else None
    entry = {
        "output_dir": job.get("output_dir"),
        "job_config": str(Path(job["output_dir"]) / "job_config.json") if job.get("output_dir") else None,
        "last_run": (validation or {}).get("started"),
        "status": (validation or {}).get("status"),
        "case_sha256": (manifest or {}).get("case_sha256"),
        "findings": len((validation or {}).get("findings", [])),
        "artifacts": {},
    }
    if job.get("output_dir"):
        output = Path(job["output_dir"])
        for name in ("validation.json", "pipeline_manifest.json", "mesh_quality.json", "mesh_quality_cells.geojson",
                     "field_derivation.json", "coupling/effective_links.json", "conditioned_terrain/terrain_condition_report.json"):
            if (output / name).is_file():
                entry["artifacts"][name] = str(output / name)
        export = (validation or {}).get("case_export")
        if export:
            entry["artifacts"]["case_export"] = export.get("target_dir")
    jobs = [j for j in index.get("jobs", []) if j.get("output_dir") != entry["output_dir"]]
    jobs.append(entry)
    index["jobs"] = sorted(jobs, key=lambda j: str(j.get("output_dir")))
    index["ui_state"] = {key: job[key] for key in UI_STATE_KEYS if key in job}
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path)
    return path


def project_index_rows(path: Path) -> list[tuple[str, str, str]]:
    index = load_project_index(path)
    if index is None:
        return [("info", "NoProjectIndex", f"no {PROJECT_INDEX_NAME} at {path}")]
    rows = [("info", "ProjectIndex", f"package={index.get('package')} jobs={len(index.get('jobs', []))} updated={index.get('updated')}")]
    for job in index.get("jobs", []):
        lamp = {"ok": "pass", "review": "review", "fatal": "fatal"}.get(job.get("status"), "info")
        rows.append((lamp, "ProjectJob", f"{job.get('output_dir')}: status={job.get('status')} "
                                         f"case={str(job.get('case_sha256'))[:12]} findings={job.get('findings')}"))
    return rows
