"""B7 cross-platform determinism experiment for the mesh-generation pipeline.

Regenerates the G30 fixture (synthetic D-5 package, pipeline v1 path: no CRS
transform, no terrain conditioning, placeholder fields) on the CURRENT host and
compares it with the committed fixture bytes, then records an evidence row:

  py -3 tools/preproc/cross_platform_hash.py [--out evidence.json] [--fixture <case.stcf.nc>]

Verdicts (per fixture):
  bitwise            SHA-256 equal to the committed case
  tolerance_1e-12    same topology (node count, faces, edges), every node
                     coordinate and cell field within 1e-12 -> the "recorded
                     1e-12 tolerance" gate wording would pass
  numerically_close  same topology but max |delta| > 1e-12 (value recorded)
  topology_differs   gmsh produced a different mesh -> no tolerance wording
                     can bridge it; platform must be governed (B7 decision)
  regeneration_failed

The record carries platform / python / gmsh / numpy / netCDF4 versions so the
`third_party/compatibility/matrix.md` row for gmsh can be filled from CI
artifacts. This tool is EVIDENCE COLLECTION: it never rewrites a fixture and it
does not decide the gate wording - the B7 evidence document does, once at
least one non-Windows row exists.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples/d5_gis_preproc_template"
G30 = ROOT / "tests/golden/preproc_gis_synthetic_case/cases/synthetic_city_block.stcf.nc"
TOLERANCE = 1.0e-12


def versions() -> dict:
    out = {"platform": platform.platform(), "machine": platform.machine(), "python": sys.version.split()[0]}
    for module in ("gmsh", "numpy", "netCDF4", "pyproj"):
        try:
            mod = __import__(module)
            out[module] = getattr(mod, "__version__", None)
        except Exception as error:  # noqa: BLE001
            out[module] = f"unavailable ({error.__class__.__name__})"
    if isinstance(out.get("gmsh"), str) and out["gmsh"].startswith("unavailable"):
        pass
    else:
        try:
            import gmsh
            gmsh.initialize()
            out["gmsh_library"] = gmsh.option.getString("General.Version")
            gmsh.finalize()
        except Exception:  # noqa: BLE001
            pass
    return out


def regenerate(package: Path, output: Path) -> Path | None:
    """Pipeline v1 path: strip the package's optional policies so the run is the
    G30 recipe (identity CRS is byte-copied anyway; terrain policy disabled)."""
    job = {"job_config_schema_version": 1, "package": str(package), "output_dir": str(output),
           "case_name": "synthetic_city_block.stcf.nc", "characteristic_length_m": 8.0, "recombine": True,
           "determinism_check": True}
    output.mkdir(parents=True, exist_ok=True)
    job_path = output / "m287b_job.json"
    job_path.write_text(json.dumps(job, indent=2), encoding="utf-8")
    completed = subprocess.run([sys.executable, "-m", "scau_preproc.pipeline", str(job_path)],
                               cwd=str(ROOT / "python"), capture_output=True, text=True)
    if completed.returncode != 0:
        print(completed.stderr[-2000:], file=sys.stderr)
        return None
    case = output / "synthetic_city_block.stcf.nc"
    return case if case.is_file() else None


def compare(candidate: Path, reference: Path) -> dict:
    from netCDF4 import Dataset
    import numpy as np
    result = {"reference_sha256": hashlib.sha256(reference.read_bytes()).hexdigest(),
              "candidate_sha256": hashlib.sha256(candidate.read_bytes()).hexdigest()}
    if result["reference_sha256"] == result["candidate_sha256"]:
        result["verdict"] = "bitwise"
        return result
    a, b = Dataset(candidate), Dataset(reference)
    try:
        topo_vars = ("mesh2_face_nodes", "mesh2_edge_nodes", "mesh2_edge_faces", "mesh2_face_edges")
        dims = {name: (len(a.dimensions[name]) if name in a.dimensions else None,
                       len(b.dimensions[name]) if name in b.dimensions else None)
                for name in ("nMesh2_node", "cell", "edge")}
        result["dimensions"] = {k: {"candidate": v[0], "reference": v[1]} for k, v in dims.items()}
        if any(v[0] != v[1] for v in dims.values()):
            result["verdict"] = "topology_differs"
            return result
        for name in topo_vars:
            if not np.array_equal(np.ma.filled(a[name][:], -1), np.ma.filled(b[name][:], -1)):
                result["verdict"] = "topology_differs"
                result["first_topology_mismatch"] = name
                return result
        deltas = {}
        for name in ("mesh2_node_x", "mesh2_node_y", "z_b", "manning_n", "phi_t", "phi_xx", "phi_xy", "phi_yy",
                     "omega_edge", "phi_e_n", "phi_et"):
            if name in a.variables and name in b.variables:
                deltas[name] = float(np.max(np.abs(np.asarray(a[name][:], dtype=float) - np.asarray(b[name][:], dtype=float))))
        result["max_abs_delta"] = deltas
        worst = max(deltas.values()) if deltas else 0.0
        result["max_abs_delta_overall"] = worst
        result["verdict"] = "tolerance_1e-12" if worst <= TOLERANCE else "numerically_close"
        return result
    finally:
        a.close()
        b.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=None, help="evidence JSON path (default: print only)")
    parser.add_argument("--fixture", default=str(G30))
    parser.add_argument("--package", default=str(SAMPLE))
    args = parser.parse_args()
    record = {"b7_evidence_schema_version": 1, "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
              "fixture": (Path(args.fixture).resolve().relative_to(ROOT).as_posix()
                          if Path(args.fixture).resolve().is_relative_to(ROOT) else args.fixture),
              "environment": versions()}
    with tempfile.TemporaryDirectory() as tmp:
        case = regenerate(Path(args.package), Path(tmp) / "out")
        if case is None:
            record["verdict"] = "regeneration_failed"
        else:
            manifest = json.loads((Path(tmp) / "out" / "validation.json").read_text(encoding="utf-8"))
            record["same_host_bitwise_reruns"] = (manifest.get("determinism") or {}).get("bitwise_identical_reruns")
            record.update(compare(case, Path(args.fixture)))
    print(json.dumps(record, indent=2))
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return 0 if record.get("verdict") in ("bitwise", "tolerance_1e-12") else 1


if __name__ == "__main__":
    sys.exit(main())
