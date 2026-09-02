"""scau_preproc pipeline v1 (M287-B): config-driven synthetic-package -> STCF case.

Stage A-D orchestration over the file-interchange boundary:

  job_config.json
      -> input-package contract checks (manifest, geometry_clean_policy,
         canonical mapping targets)                                [fail-closed]
      -> mesh generation subprocess (meshgen.py; policy timeout kill,
         atomic output, diagnostic GeoJSON on rejection; optional B4
         "mesh_controls" breaklines / refinement regions, hashed into the
         manifest)
      -> optional deterministic re-generation and SHA-256 comparison
      -> authoritative certification via `scau_preproc validate`
      -> pipeline_manifest.json + validation.json reports

The pipeline never repairs geometry, never derives DPM physics beyond the
generator's recorded placeholder rules, and never touches runtime coupling
semantics. UI layers must drive exactly this entry point (stateless
job_config -> reports contract; M287 plan section 3).

Usage: py -3 -m scau_preproc.pipeline <job_config.json>
Exit codes: 0 ok; 2 fail-closed (contract/geometry/validation); 3 timeout.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

JOB_CONFIG_SCHEMA_VERSION = 1
CANONICAL_TARGETS = {
    "node_x", "node_y", "face_nodes", "edge_nodes",
    "phi_t", "phi_xx", "phi_xy", "phi_yy",
    "manning_n", "z_b", "soil_type",
    "omega_edge", "phi_e_n", "phi_et",
}
REQUIRED_JOB_KEYS = {"job_config_schema_version", "package", "output_dir"}
REQUIRED_PACKAGE_FILES = (
    "manifest.json",
    "metadata/geometry_clean_policy.json",
    "boundary/computational_boundary.geojson",
    "buildings/buildings.geojson",
    "landcover/landcover.geojson",
    "terrain/dem.asc",
    "soil/soil_parameters.csv",
)


class PipelineError(SystemExit):
    def __init__(self, message: str, code: int = 2) -> None:
        print(f"error: {message}", file=sys.stderr)
        super().__init__(code)


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_job_config(path: Path) -> dict:
    config = json.loads(path.read_text(encoding="utf-8"))
    missing = REQUIRED_JOB_KEYS - config.keys()
    if missing:
        raise PipelineError(f"job_config missing required keys: {sorted(missing)}")
    if config["job_config_schema_version"] != JOB_CONFIG_SCHEMA_VERSION:
        raise PipelineError(
            "unsupported job_config_schema_version "
            f"{config['job_config_schema_version']} (expected {JOB_CONFIG_SCHEMA_VERSION})"
        )
    return config


def check_package_contract(package: Path) -> dict:
    for relative in REQUIRED_PACKAGE_FILES:
        if not (package / relative).is_file():
            raise PipelineError(f"input package missing required file: {relative}")

    manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
    for target in manifest.get("canonical_targets", []):
        if target not in CANONICAL_TARGETS:
            raise PipelineError(f"manifest declares unknown canonical target: {target}")

    policy = json.loads(
        (package / "metadata/geometry_clean_policy.json").read_text(encoding="utf-8")
    )
    guards = policy.get("mesh_generation_guards")
    if not guards:
        raise PipelineError("geometry_clean_policy missing mesh_generation_guards")
    timeout_s = guards.get("generator_timeout_s")
    if not isinstance(timeout_s, (int, float)) or timeout_s <= 0:
        raise PipelineError("geometry_clean_policy generator_timeout_s must be a positive number")
    if guards.get("partial_mesh_output") != "forbidden":
        raise PipelineError("geometry_clean_policy must forbid partial mesh output")
    if policy.get("reporting", {}).get("unapproved_repair") != "fatal":
        raise PipelineError("geometry_clean_policy must declare unapproved_repair: fatal")
    return {
        "policy_version": policy.get("policy_version", "unversioned"),
        "generator_timeout_s": float(timeout_s),
        "repair_mode": "reject_only (no repair rules executed by pipeline v1)",
    }


def resolve_mesh_controls(job: dict, job_path: Path) -> dict | None:
    """Normalizes the optional job_config "mesh_controls" block (B4). The
    GeoJSON path is resolved relative to the job_config; a missing file is a
    fail-closed contract error (never silently meshed without controls)."""
    block = job.get("mesh_controls")
    if not block:
        return None
    if not isinstance(block, dict) or not block.get("geojson"):
        raise PipelineError("job_config mesh_controls must be an object with a geojson path")
    geojson = Path(block["geojson"])
    if not geojson.is_absolute():
        geojson = (job_path.parent / geojson).resolve()
    if not geojson.is_file():
        raise PipelineError(f"mesh_controls geojson not found: {geojson}")
    resolved = {"geojson": str(geojson)}
    for key in ("default_size_m", "default_dist_max_m"):
        if block.get(key) is not None:
            value = block[key]
            if not isinstance(value, (int, float)) or value <= 0:
                raise PipelineError(f"mesh_controls {key} must be a positive number")
            resolved[key] = float(value)
    return resolved


def run_generator(config_path: Path, timeout_s: float) -> tuple[int | None, str]:
    try:
        completed = subprocess.run(
            [sys.executable, "-m", "scau_preproc.meshgen", str(config_path)],
            capture_output=True,
            text=True,
            timeout=timeout_s,
            cwd=str(Path(__file__).resolve().parents[1]),
        )
        return completed.returncode, completed.stderr
    except subprocess.TimeoutExpired:
        return None, f"generator exceeded policy timeout ({timeout_s}s) and was killed"


def main() -> int:
    if len(sys.argv) != 2:
        raise PipelineError("usage: py -3 -m scau_preproc.pipeline <job_config.json>")
    job_path = Path(sys.argv[1]).resolve()
    job = load_job_config(job_path)
    package = Path(job["package"]).resolve()
    output_dir = Path(job["output_dir"]).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    started = time.strftime("%Y-%m-%dT%H:%M:%S")

    policy_audit = check_package_contract(package)
    mesh_controls = resolve_mesh_controls(job, job_path)
    case_path = output_dir / job.get("case_name", "case.stcf.nc")
    diagnostics_path = output_dir / "generator.diagnostic.geojson"
    generator_config = {
        "package": str(package),
        "output": str(case_path),
        "diagnostics": str(diagnostics_path),
        "report": str(output_dir / "mesh_quality.json"),
        "quality_cells": str(output_dir / "mesh_quality_cells.geojson"),
        "characteristic_length_m": job.get("characteristic_length_m", 8.0),
        "recombine": job.get("recombine", True),
    }
    if mesh_controls:
        generator_config["mesh_controls"] = mesh_controls
    generator_config_path = output_dir / "generator.config.json"
    generator_config_path.write_text(json.dumps(generator_config, indent=2), encoding="utf-8")

    timeout_s = policy_audit["generator_timeout_s"]
    returncode, stderr = run_generator(generator_config_path, timeout_s)
    validation = {
        "job_config": str(job_path),
        "job_config_sha256": sha256_of(job_path),
        "package": str(package),
        "started": started,
        "policy_audit": policy_audit,
        "findings": [],
    }

    def finish(status: str, exit_code: int) -> int:
        validation["status"] = status
        (output_dir / "validation.json").write_text(
            json.dumps(validation, indent=2), encoding="utf-8"
        )
        print(json.dumps({"status": status, "output_dir": str(output_dir)}))
        return exit_code

    if returncode is None:
        validation["findings"].append({
            "severity": "fatal",
            "code": "MeshGenerationFailed_timeout",
            "detail": stderr,
            "partial_output_absent": not case_path.exists()
            and not Path(str(case_path) + ".tmp").exists(),
        })
        return finish("fatal", 3)
    if returncode != 0:
        detail = stderr.strip().splitlines()[-1] if stderr.strip() else f"exit {returncode}"
        finding = {
            "severity": "fatal",
            "code": "MeshControlsRejected" if detail.startswith("MeshControlsRejected:")
            else "MeshGenerationFailed",
            "detail": detail,
            "diagnostic": str(diagnostics_path) if diagnostics_path.exists() else None,
        }
        if diagnostics_path.exists():
            diagnostic = json.loads(diagnostics_path.read_text(encoding="utf-8"))
            finding["objects"] = [
                {key: feature.get("properties", {}).get(key)
                 for key in ("feature_id", "kind", "violation")}
                for feature in diagnostic.get("features", [])
            ]
        validation["findings"].append(finding)
        return finish("fatal", 2)

    case_sha = sha256_of(case_path)
    validation["case"] = {"path": str(case_path), "sha256": case_sha}

    if job.get("determinism_check", True):
        repeat_dir = output_dir / "determinism_repeat"
        repeat_dir.mkdir(exist_ok=True)
        repeat_config = dict(generator_config)
        repeat_config["output"] = str(repeat_dir / case_path.name)
        repeat_config["diagnostics"] = str(repeat_dir / "generator.diagnostic.geojson")
        repeat_config["report"] = str(repeat_dir / "mesh_quality.json")
        repeat_config["quality_cells"] = str(repeat_dir / "mesh_quality_cells.geojson")
        repeat_config_path = repeat_dir / "generator.config.json"
        repeat_config_path.write_text(json.dumps(repeat_config, indent=2), encoding="utf-8")
        repeat_rc, repeat_err = run_generator(repeat_config_path, timeout_s)
        identical = (
            repeat_rc == 0 and sha256_of(Path(repeat_config["output"])) == case_sha
        )
        validation["determinism"] = {"bitwise_identical_reruns": identical}
        if not identical:
            validation["findings"].append({
                "severity": "fatal",
                "code": "NonDeterministicGeneration",
                "detail": repeat_err.strip() if repeat_rc != 0 else "SHA-256 mismatch",
            })
            return finish("fatal", 2)

    validator = job.get("validator_cli")
    if validator:
        completed = subprocess.run(
            [validator, "validate", "--input", str(case_path)],
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        validation["authoritative_validate"] = {
            "exit_code": completed.returncode,
            "stdout": completed.stdout.strip(),
        }
        if completed.returncode != 0:
            validation["findings"].append({
                "severity": "fatal",
                "code": "AuthoritativeValidationFailed",
                "detail": completed.stderr.strip(),
            })
            return finish("fatal", 2)

    if job.get("coupling_maps", False):
        from scau_preproc import coupling_maps
        coupling_dir = output_dir / "coupling"
        try:
            report = coupling_maps.generate(package, case_path, coupling_dir)
            validation["coupling_maps"] = {"status": "ok", "report": report}
        except SystemExit as error:
            validation["findings"].append({
                "severity": "fatal",
                "code": "CouplingMapGenerationFailed",
                "detail": f"exit {error.code}",
            })
            return finish("fatal", 2)

    manifest = {
        "pipeline": "scau_preproc.pipeline",
        "job_config_schema_version": JOB_CONFIG_SCHEMA_VERSION,
        "started": started,
        "package_manifest_sha256": sha256_of(package / "manifest.json"),
        "geometry_clean_policy_sha256": sha256_of(
            package / "metadata/geometry_clean_policy.json"
        ),
        "generator_config_sha256": sha256_of(generator_config_path),
        "mesh_controls": None if not mesh_controls else {
            **mesh_controls,
            "geojson_sha256": sha256_of(Path(mesh_controls["geojson"])),
        },
        "case_sha256": case_sha,
        "mesh_quality": json.loads((output_dir / "mesh_quality.json").read_text(encoding="utf-8")),
        "reproduce": f"py -3 -m scau_preproc.pipeline {job_path.name}",
    }
    (output_dir / "pipeline_manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8"
    )
    return finish("ok", 0)


if __name__ == "__main__":
    sys.exit(main())
