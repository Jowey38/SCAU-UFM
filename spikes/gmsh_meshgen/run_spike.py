"""M287-A spike runner: nominal + fail-closed scenarios with subprocess timeout.

Runs generate_mesh.py as a subprocess (the license and fault-isolation
boundary) and enforces the geometry_clean_policy mesh_generation_guards:
wall-clock timeout with hard kill, no partial output, diagnostic artifacts on
every failure path.

Scenarios:
  1. nominal        synthetic package, lc=8m -> .stcf.nc + quality report;
                    run twice and compare SHA-256 (same-platform determinism).
  2. degenerate     self-intersecting building fixture -> exit 2 +
                    diagnostic GeoJSON, no output file.
  3. timeout        nominal geometry with lc=0.08m (millions of elements)
                    under a 20 s policy-override timeout -> killed, exit 3
                    recorded, no output file.

Usage: py -3 spikes/gmsh_meshgen/run_spike.py <package_dir> <work_dir>
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

SPIKE_DIR = Path(__file__).resolve().parent
DEFAULT_TIMEOUT_S = 60  # geometry_clean_policy.mesh_generation_guards default
TIMEOUT_FIXTURE_S = 20  # recorded policy override for the demonstration


def run_generator(config: dict, timeout_s: int) -> subprocess.CompletedProcess | None:
    config_path = Path(config["work"]) / f"{config['name']}.config.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    try:
        return subprocess.run(
            [sys.executable, str(SPIKE_DIR / "generate_mesh.py"), str(config_path)],
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired:
        return None


def main() -> int:
    package = Path(sys.argv[1]).resolve()
    work = Path(sys.argv[2]).resolve()
    work.mkdir(parents=True, exist_ok=True)
    summary: dict[str, dict] = {}

    # 1. nominal, twice, hash-compare.
    hashes = []
    for attempt in (1, 2):
        output = work / f"nominal_{attempt}.stcf.nc"
        result = run_generator(
            {
                "name": f"nominal_{attempt}",
                "work": str(work),
                "package": str(package),
                "output": str(output),
                "diagnostics": str(work / f"nominal_{attempt}.diagnostic.geojson"),
                "characteristic_length_m": 8.0,
            },
            DEFAULT_TIMEOUT_S,
        )
        if result is None or result.returncode != 0:
            print((result.stderr or "TIMEOUT") if result else "TIMEOUT", file=sys.stderr)
            raise SystemExit("nominal scenario failed; spike cannot proceed")
        report = json.loads(result.stdout.strip().splitlines()[-1])
        hashes.append(report["output_sha256"])
        summary[f"nominal_{attempt}"] = report
    summary["determinism"] = {
        "bitwise_identical_reruns": hashes[0] == hashes[1],
        "sha256": hashes[0],
    }

    # 2. degenerate geometry -> fail-closed with diagnostic, no output.
    degenerate_output = work / "degenerate.stcf.nc"
    degenerate_diag = work / "degenerate.diagnostic.geojson"
    result = run_generator(
        {
            "name": "degenerate",
            "work": str(work),
            "package": str(package),
            "buildings": str(SPIKE_DIR / "fixtures/degenerate_self_intersecting.geojson"),
            "output": str(degenerate_output),
            "diagnostics": str(degenerate_diag),
            "characteristic_length_m": 8.0,
        },
        DEFAULT_TIMEOUT_S,
    )
    summary["degenerate"] = {
        "exit_code": result.returncode if result else "timeout",
        "diagnostic_written": degenerate_diag.exists(),
        "partial_output_absent": not degenerate_output.exists()
        and not Path(str(degenerate_output) + ".tmp").exists(),
        "stderr_tail": (result.stderr.strip().splitlines()[-1] if result and result.stderr else ""),
    }

    # 3. timeout truncation -> killed subprocess, no output.
    timeout_output = work / "timeout.stcf.nc"
    result = run_generator(
        {
            "name": "timeout",
            "work": str(work),
            "package": str(package),
            "output": str(timeout_output),
            "diagnostics": str(work / "timeout.diagnostic.geojson"),
            "characteristic_length_m": 0.08,
        },
        TIMEOUT_FIXTURE_S,
    )
    timeout_diag = {
        "policy_timeout_s": TIMEOUT_FIXTURE_S,
        "killed": result is None,
        "partial_output_absent": not timeout_output.exists()
        and not Path(str(timeout_output) + ".tmp").exists(),
        "outcome": "MeshGenerationFailed: generator exceeded the policy timeout and was killed",
    }
    (work / "timeout.diagnostic.json").write_text(
        json.dumps(timeout_diag, indent=2), encoding="utf-8"
    )
    summary["timeout"] = timeout_diag

    (work / "spike_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))

    ok = (
        summary["determinism"]["bitwise_identical_reruns"]
        and summary["degenerate"]["exit_code"] == 2
        and summary["degenerate"]["diagnostic_written"]
        and summary["degenerate"]["partial_output_absent"]
        and summary["timeout"]["killed"]
        and summary["timeout"]["partial_output_absent"]
    )
    print("SPIKE:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
