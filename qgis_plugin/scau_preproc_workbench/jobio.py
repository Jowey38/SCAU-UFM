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
import subprocess
from pathlib import Path

JOB_CONFIG_SCHEMA_VERSION = 1
DEFAULT_PYTHON_LAUNCHER = ["py", "-3"]


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
    return job


def write_job_config(job: dict, output_dir: str) -> Path:
    path = Path(output_dir) / "job_config.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(job, indent=2), encoding="utf-8")
    return path


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
    launcher = list(python_launcher or DEFAULT_PYTHON_LAUNCHER)
    command = launcher + ["-m", "scau_preproc.pipeline", str(job_config_path)]
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            cwd=str(Path(repo_root) / "python"),
        )
        exit_code: int | None = completed.returncode
        stderr = completed.stderr
    except subprocess.TimeoutExpired:
        exit_code = None
        stderr = f"pipeline exceeded UI timeout ({timeout_s}s) and was killed"
    except FileNotFoundError as error:
        return {
            "exit_code": -1,
            "status": "launcher_missing",
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
    }
    return {name: str(path) for name, path in candidates.items() if path.is_file()}
