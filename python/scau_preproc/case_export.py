"""Case package exporter (M287-B6, pipeline stage F).

Assembles the standard runnable case package from a FINISHED pipeline output
directory (stage A-E' reports) — the single artefact SimDriver consumes:

    <target>/
      manifest.json                 # package_manifest_schema_version 1: every file's SHA-256,
                                    #   source hashes, gate evidence, reproduce commands
      mesh/case.stcf.nc             # copied byte-for-byte from the pipeline output
      mesh/mesh_quality.json
      mesh/mesh_quality_cells.geojson
      coupling/*_mapping.json       # generator candidates (read-only provenance)
      coupling/mapping_report.json
      coupling/effective_links.json # C4 merge result (status must be complete)
      coupling/confirmed/*.json     # the operator decisions that produced it
      swmm/model.inp                # copied VERBATIM from the input package (never rewritten)
      dflowfm/**                    # copied verbatim when the package ships one
      metadata/*.json               # policies the run was governed by
      simdriver/run.conf            # RuntimeConfig version 2, case-relative paths, effective
                                    #   ground links only; enable_dflowfm=false until C3 river
                                    #   links exist; engine_mode and timing from job_config
      validation/validation.json, pipeline_manifest.json

Gate (fail-closed, nothing is written unless ALL hold):
  validation.status == "ok" and no fatal/review findings; coupling stage ran
  and effective_links.status == "complete"; every referenced source file
  exists; recorded case SHA-256 matches the file on disk. The staged package
  is re-certified with `scau_preproc validate` (when a validator is given),
  then moved into place by a single atomic rename; an existing target is
  never overwritten unless --force, and then only after the new package is
  complete. Exports are deterministic: the same output directory exports to
  byte-identical files (manifest hashes included; no timestamps inside).

The exporter decides nothing physical: crest/width/weight come from the
effective links; run timing from job_config.export_case (declared, recorded).
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PACKAGE_MANIFEST_SCHEMA_VERSION = 1
RUN_CONF_DEFAULTS = {
    "start_time": 0.0,
    "end_time": 600.0,
    "dt_couple": 60.0,
    "dt_surface": 0.05,
    "dt_swmm": 60.0,
    "dt_dflowfm": 60.0,
    "initial_eta": None,          # None -> DEM max + 0.5 m (every cell wet), recorded
    "engine_mode": "mock",
    "enable_whole_system_mass_audit": False,
}
POLICY_FILES = ("metadata/geometry_clean_policy.json",)


class ExportError(SystemExit):
    def __init__(self, message: str, code: int = 2) -> None:
        self.message = message
        print(f"error: {message}", file=sys.stderr)
        super().__init__(code)


def sha256_of(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _load_json(path: Path, what: str) -> dict:
    if not path.is_file():
        raise ExportError(f"{what} missing: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise ExportError(f"{what} is not valid JSON: {path} ({error})") from error


# --- gate ---------------------------------------------------------------------


def check_export_gate(output_dir: Path) -> dict:
    """Returns the loaded reports when the output directory may be exported;
    raises ExportError (listing every blocker) otherwise."""
    output_dir = Path(output_dir)
    validation = _load_json(output_dir / "validation.json", "validation report")
    pipeline_manifest = _load_json(output_dir / "pipeline_manifest.json", "pipeline manifest")
    blockers: list[str] = []
    if validation.get("status") != "ok":
        blockers.append(f"validation status is {validation.get('status')!r} (need ok)")
    for finding in validation.get("findings", []):
        if finding.get("severity") in ("fatal", "review"):
            blockers.append(f"{finding.get('severity')} finding {finding.get('code')}")
    case_info = validation.get("case") or {}
    case_path = Path(case_info.get("path", "")) if case_info.get("path") else None
    if case_path is None or not case_path.is_file():
        blockers.append("validation.case.path missing or file absent")
    elif sha256_of(case_path) != case_info.get("sha256"):
        blockers.append("case file changed since validation (sha256 mismatch)")
    elif pipeline_manifest.get("case_sha256") != case_info.get("sha256"):
        blockers.append("pipeline_manifest.case_sha256 disagrees with validation")
    if "coupling_maps" not in validation:
        blockers.append("coupling stage did not run (job_config.coupling_maps off)")
    confirmations = validation.get("coupling_confirmations")
    if not confirmations:
        blockers.append("confirmation merge (stage E') did not run")
    elif confirmations.get("status") != "complete":
        blockers.append(f"effective links incomplete: {confirmations.get('unconfirmed_total')} unconfirmed")
    effective_path = output_dir / "coupling" / "effective_links.json"
    if not effective_path.is_file():
        blockers.append("coupling/effective_links.json missing")
    if blockers:
        raise ExportError("export gate closed: " + "; ".join(blockers))
    return {"validation": validation, "pipeline_manifest": pipeline_manifest,
            "case_path": case_path, "effective": _load_json(effective_path, "effective links")}


# --- run.conf -----------------------------------------------------------------


def _format_number(value: float) -> str:
    return repr(float(value))


def build_run_conf(effective: dict, options: dict, initial_eta: float, has_dflowfm: bool) -> str:
    """RuntimeConfig version 2 with case-relative paths. Only EFFECTIVE ground
    links are emitted; river/interface links stay absent until C3 provides a
    governed boundary contract (provider_required)."""
    river_links = effective.get("links", {}).get("surface_to_dflowfm", [])
    river_enabled = bool(has_dflowfm and river_links)
    lines = [
        "version = 2",
        "# generated by scau_preproc.case_export (M287-B6); paths are relative to the package root",
        f"start_time = {_format_number(options['start_time'])}",
        f"end_time = {_format_number(options['end_time'])}",
        f"dt_couple = {_format_number(options['dt_couple'])}",
        f"dt_surface = {_format_number(options['dt_surface'])}",
        f"dt_swmm = {_format_number(options['dt_swmm'])}",
        f"dt_dflowfm = {_format_number(options['dt_dflowfm'])}",
        "enable_swmm = true",
        # The river leg is enabled only when the package ships a D-Flow FM
        # model AND governed river links exist (C3); until then the chain is
        # provider_required and the driver config says so explicitly. NOTE:
        # SimDriver's run loop is currently tri-model only (refuses a
        # surface+SWMM package); configure/cold-start succeed, running needs C3.
        f"enable_dflowfm = {'true' if river_enabled else 'false'}",
        "stcf_case_path = mesh/case.stcf.nc",
        "swmm_inp_path = swmm/model.inp",
    ]
    if river_enabled:
        lines.append("dflowfm_mdu_path = dflowfm/model.mdu")
    lines += [
        f"initial_eta = {_format_number(initial_eta)}",
        f"engine_mode = {options['engine_mode']}",
        f"enable_whole_system_mass_audit = {'true' if options['enable_whole_system_mass_audit'] else 'false'}",
        "output_summary_path = run_summary.json",
    ]
    placeholders = effective.get("placeholders") or {}
    width = placeholders.get("exchange_width_m", 1.0)
    weight = placeholders.get("priority_weight", 1.0)
    for link in effective.get("links", {}).get("surface_to_swmm", []):
        lines.append(
            f"surface_drainage_link = cell={link['cell_index']},node={link['swmm_node_id']},"
            f"crest={link['exchange_elevation_m']},width={width},weight={weight}")
    return "\n".join(lines) + "\n"


def _dem_max(package: Path) -> float:
    best = None
    header_keys = {"ncols", "nrows", "xllcorner", "yllcorner", "cellsize", "nodata_value"}
    nodata = None
    for line in (package / "terrain/dem.asc").read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        if len(parts) == 2 and parts[0].lower() in header_keys:
            if parts[0].lower() == "nodata_value":
                nodata = float(parts[1])
            continue
        for value in parts:
            v = float(value)
            if nodata is not None and v == nodata:
                continue
            best = v if best is None else max(best, v)
    if best is None:
        raise ExportError("DEM has no data values")
    return best


# --- assembly -----------------------------------------------------------------


def _copy(src: Path, dst: Path) -> None:
    if not src.is_file():
        raise ExportError(f"source file missing: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)


def _copy_tree(src: Path, dst: Path) -> list[Path]:
    copied = []
    for path in sorted(src.rglob("*")):
        if path.is_file():
            target = dst / path.relative_to(src)
            _copy(path, target)
            copied.append(target)
    return copied


def export_case(output_dir: Path, target_dir: Path, *, options: dict | None = None,
                validator_cli: str | None = None, force: bool = False) -> dict:
    output_dir = Path(output_dir).resolve()
    target_dir = Path(target_dir).resolve()
    options = {**RUN_CONF_DEFAULTS, **(options or {})}
    for key in ("start_time", "end_time", "dt_couple", "dt_surface", "dt_swmm", "dt_dflowfm"):
        if not isinstance(options[key], (int, float)) or isinstance(options[key], bool):
            raise ExportError(f"export_case.{key} must be a number")
    if options["engine_mode"] not in ("mock", "real"):
        raise ExportError("export_case.engine_mode must be mock or real")
    if options["end_time"] <= options["start_time"] or options["dt_couple"] <= 0:
        raise ExportError("export_case timing must satisfy end_time > start_time and dt_couple > 0")

    gate = check_export_gate(output_dir)
    validation = gate["validation"]
    package = Path(validation["package"]).resolve()
    job_config_path = Path(validation["job_config"])
    if target_dir.exists() and not force:
        raise ExportError(f"target exists (use force to replace): {target_dir}")
    if target_dir == output_dir or output_dir in target_dir.parents:
        raise ExportError("target_dir must not be inside the pipeline output directory")

    has_dflowfm = (package / "dflowfm" / "model.mdu").is_file()
    initial_eta = options["initial_eta"]
    if initial_eta is None:
        initial_eta = _dem_max(package) + 0.5
    run_conf = build_run_conf(gate["effective"], options, float(initial_eta), has_dflowfm)

    target_dir.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".case_export_", dir=str(target_dir.parent)))
    try:
        files: list[Path] = []
        # mesh
        _copy(gate["case_path"], staging / "mesh/case.stcf.nc"); files.append(staging / "mesh/case.stcf.nc")
        for name in ("mesh_quality.json", "mesh_quality_cells.geojson"):
            if (output_dir / name).is_file():
                _copy(output_dir / name, staging / "mesh" / name); files.append(staging / "mesh" / name)
        # coupling
        coupling_src = output_dir / "coupling"
        for name in ("surface_swmm_mapping.json", "roof_drain_mapping.json", "surface_dflowfm_mapping.json",
                     "mapping_report.json", "effective_links.json"):
            _copy(coupling_src / name, staging / "coupling" / name); files.append(staging / "coupling" / name)
        confirmations_dir = validation.get("coupling_confirmations", {}).get("confirmations_dir")
        if confirmations_dir and Path(confirmations_dir).is_dir():
            files += _copy_tree(Path(confirmations_dir), staging / "coupling" / "confirmed")
        # 1D inputs verbatim
        _copy(package / "swmm/model.inp", staging / "swmm/model.inp"); files.append(staging / "swmm/model.inp")
        if (package / "dflowfm").is_dir():
            files += _copy_tree(package / "dflowfm", staging / "dflowfm")
        # governance
        for relative in POLICY_FILES:
            _copy(package / relative, staging / relative); files.append(staging / relative)
        _copy(package / "manifest.json", staging / "metadata/package_manifest.json")
        files.append(staging / "metadata/package_manifest.json")
        mesh_controls = (gate["pipeline_manifest"].get("mesh_controls") or {}).get("geojson")
        if mesh_controls and Path(mesh_controls).is_file():
            _copy(Path(mesh_controls), staging / "metadata/mesh_controls.geojson")
            files.append(staging / "metadata/mesh_controls.geojson")
        # reports + job config
        for name in ("validation.json", "pipeline_manifest.json"):
            _copy(output_dir / name, staging / "validation" / name); files.append(staging / "validation" / name)
        if job_config_path.is_file():
            _copy(job_config_path, staging / "validation/job_config.json")
            files.append(staging / "validation/job_config.json")
        # run config
        run_conf_path = staging / "simdriver/run.conf"
        run_conf_path.parent.mkdir(parents=True)
        run_conf_path.write_text(run_conf, encoding="utf-8")
        files.append(run_conf_path)

        certification = {"validator_cli": validator_cli, "exit_code": None, "stdout": None}
        if validator_cli:
            completed = subprocess.run(
                [validator_cli, "validate", "--input", str(staging / "mesh/case.stcf.nc")],
                capture_output=True, text=True, timeout=300.0)
            # The staging directory name is random: report the package-relative path.
            certification.update({"exit_code": completed.returncode,
                                  "stdout": completed.stdout.strip().replace(str(staging), "<package>")})
            if completed.returncode != 0:
                raise ExportError(f"authoritative validate failed on the staged case: {completed.stderr.strip()}")

        manifest = {
            "package_manifest_schema_version": PACKAGE_MANIFEST_SCHEMA_VERSION,
            "exporter": "scau_preproc.case_export",
            "source": {
                "pipeline_output_dir": str(output_dir),
                "input_package": str(package),
                "job_config_sha256": validation.get("job_config_sha256"),
                "case_sha256": gate["pipeline_manifest"]["case_sha256"],
                "package_manifest_sha256": gate["pipeline_manifest"].get("package_manifest_sha256"),
                "geometry_clean_policy_sha256": gate["pipeline_manifest"].get("geometry_clean_policy_sha256"),
                "mesh_controls_sha256": (gate["pipeline_manifest"].get("mesh_controls") or {}).get("geojson_sha256"),
                "coupling_mode": (gate["pipeline_manifest"].get("coupling_maps") or {}).get("mode", "explicit_ids"),
                "confirmation_files_sha256": (gate["pipeline_manifest"].get("confirmations") or {}).get("files_sha256"),
            },
            "gate": {
                "validation_status": validation["status"],
                "effective_links_status": validation["coupling_confirmations"]["status"],
                "effective_surface_links": len(gate["effective"].get("links", {}).get("surface_to_swmm", [])),
                "effective_roof_links": len(gate["effective"].get("links", {}).get("roof_to_swmm", [])),
                "dflowfm": "present" if has_dflowfm else "provider_required",
                "river_leg_enabled": bool(has_dflowfm and gate["effective"].get("links", {}).get("surface_to_dflowfm")),
                "staged_case_certification": certification,
            },
            "run_conf": {**{k: v for k, v in options.items() if k != "initial_eta"},
                         "initial_eta": float(initial_eta),
                         "initial_eta_source": "job_config" if options["initial_eta"] is not None
                         else "dem_max_plus_0.5m"},
            "files": {str(p.relative_to(staging)).replace(os.sep, "/"): sha256_of(p) for p in sorted(files)},
            "reproduce": [
                f"py -3 -m scau_preproc.pipeline {job_config_path.name}",
                f"py -3 -m scau_preproc.case_export {output_dir} {target_dir}",
                "scau_sim simdriver/run.conf   # from the package root",
            ],
        }
        (staging / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                                               encoding="utf-8")
        if target_dir.exists():
            shutil.rmtree(target_dir)
        os.rename(staging, target_dir)
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return manifest


def package_hash(target_dir: Path) -> str:
    """Single digest over the manifest's file table (order-independent of the
    filesystem): the package-level identity used by goldens and evidence."""
    manifest = _load_json(Path(target_dir) / "manifest.json", "package manifest")
    payload = json.dumps(manifest["files"], sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if len(args) != 2:
        raise ExportError("usage: py -3 -m scau_preproc.case_export <pipeline_output_dir> <target_dir> "
                          "[--force] [--validator=<scau_preproc exe>] [--engine-mode=mock|real]")
    validator = next((f.split("=", 1)[1] for f in flags if f.startswith("--validator=")), None)
    options = {}
    for flag in flags:
        if flag.startswith("--engine-mode="):
            options["engine_mode"] = flag.split("=", 1)[1]
    manifest = export_case(Path(args[0]), Path(args[1]), options=options, validator_cli=validator,
                           force="--force" in flags)
    print(json.dumps({"target": args[1], "package_hash": package_hash(Path(args[1])),
                      "files": len(manifest["files"])}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
