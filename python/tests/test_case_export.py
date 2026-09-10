"""Headless unit tests for scau_preproc.case_export (M287-B6)."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scau_preproc import case_export as ce


def make_output(root: Path, *, status="ok", findings=None, confirmations_status="complete",
                unconfirmed=0, with_coupling=True) -> tuple[Path, Path]:
    out = root / "out"; pkg = root / "pkg"
    (out / "coupling").mkdir(parents=True); (pkg / "swmm").mkdir(parents=True)
    (pkg / "metadata").mkdir(); (pkg / "terrain").mkdir(); (pkg / "coupling/confirmed").mkdir(parents=True)
    case = out / "case.stcf.nc"; case.write_bytes(b"NETCDF-BYTES")
    sha = ce.sha256_of(case)
    (pkg / "swmm/model.inp").write_text("[TITLE]\nx\n", encoding="utf-8")
    (pkg / "metadata/geometry_clean_policy.json").write_text("{}", encoding="utf-8")
    (pkg / "manifest.json").write_text("{}", encoding="utf-8")
    (pkg / "terrain/dem.asc").write_text("ncols 2\nnrows 1\nxllcorner 0\nyllcorner 0\ncellsize 1\nNODATA_value -9999\n10.0 10.9\n", encoding="utf-8")
    (pkg / "coupling/confirmed/A.json").write_text("{}", encoding="utf-8")
    job = root / "job.json"; job.write_text("{}", encoding="utf-8")
    validation = {"status": status, "findings": findings or [], "package": str(pkg), "job_config": str(job),
                  "job_config_sha256": "0" * 64, "case": {"path": str(case), "sha256": sha}}
    if with_coupling:
        validation["coupling_maps"] = {"status": "ok", "report": {}}
        validation["coupling_confirmations"] = {"status": confirmations_status, "unconfirmed_total": unconfirmed,
                                                "confirmations_dir": str(pkg / "coupling/confirmed")}
    (out / "validation.json").write_text(json.dumps(validation), encoding="utf-8")
    (out / "pipeline_manifest.json").write_text(json.dumps({"case_sha256": sha, "coupling_maps": {"mode": "explicit_ids"}}), encoding="utf-8")
    for name in ("surface_swmm_mapping.json", "roof_drain_mapping.json", "surface_dflowfm_mapping.json", "mapping_report.json"):
        (out / "coupling" / name).write_text("{}", encoding="utf-8")
    (out / "coupling/effective_links.json").write_text(json.dumps({
        "status": confirmations_status, "placeholders": {"exchange_width_m": 1.0, "priority_weight": 1.0},
        "links": {"surface_to_swmm": [{"mapping_id": "M1", "swmm_node_id": "J1", "cell_index": 7, "exchange_elevation_m": 10.2}],
                  "roof_to_swmm": []}}), encoding="utf-8")
    (out / "mesh_quality.json").write_text("{}", encoding="utf-8")
    return out, pkg


class GateTests(unittest.TestCase):
    def test_gate_blockers_are_listed(self):
        with tempfile.TemporaryDirectory() as tmp:
            out, _ = make_output(Path(tmp), status="review",
                                 findings=[{"severity": "review", "code": "CouplingCandidatesUnconfirmed"}],
                                 confirmations_status="incomplete", unconfirmed=2)
            with self.assertRaises(ce.ExportError) as ctx:
                ce.check_export_gate(out)
            message = ctx.exception.message
            for needle in ("status is 'review'", "review finding CouplingCandidatesUnconfirmed", "2 unconfirmed"):
                self.assertIn(needle, message)

    def test_gate_requires_coupling_and_matching_case_hash(self):
        with tempfile.TemporaryDirectory() as tmp:
            out, _ = make_output(Path(tmp), with_coupling=False)
            with self.assertRaises(ce.ExportError) as ctx:
                ce.check_export_gate(out)
            self.assertIn("coupling stage did not run", ctx.exception.message)
        with tempfile.TemporaryDirectory() as tmp:
            out, _ = make_output(Path(tmp))
            (out / "case.stcf.nc").write_bytes(b"CHANGED")
            with self.assertRaises(ce.ExportError) as ctx:
                ce.check_export_gate(out)
            self.assertIn("sha256 mismatch", ctx.exception.message)


class ExportTests(unittest.TestCase):
    def test_export_is_complete_atomic_and_deterministic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out, pkg = make_output(root)
            manifest = ce.export_case(out, root / "case")
            files = set(manifest["files"])
            for expected in ("mesh/case.stcf.nc", "swmm/model.inp", "simdriver/run.conf", "coupling/effective_links.json",
                             "coupling/confirmed/A.json", "metadata/geometry_clean_policy.json", "validation/validation.json"):
                self.assertIn(expected, files)
            self.assertNotIn("manifest.json", files)
            conf = (root / "case/simdriver/run.conf").read_text(encoding="utf-8")
            self.assertIn("surface_drainage_link = cell=7,node=J1,crest=10.2,width=1.0,weight=1.0", conf)
            self.assertIn("initial_eta = 11.4", conf)          # DEM max 10.9 + 0.5
            self.assertIn("stcf_case_path = mesh/case.stcf.nc", conf)
            self.assertIn("enable_dflowfm = false", conf)
            self.assertNotIn("dflowfm_mdu_path", conf)
            self.assertEqual(manifest["run_conf"]["initial_eta_source"], "dem_max_plus_0.5m")
            self.assertEqual(manifest["gate"]["effective_surface_links"], 1)
            # no staging leftovers, second export identical
            self.assertEqual([p.name for p in root.iterdir() if p.name.startswith(".case_export_")], [])
            ce.export_case(out, root / "case_b")
            self.assertEqual(ce.package_hash(root / "case"), ce.package_hash(root / "case_b"))
            # existing target is protected unless force
            with self.assertRaises(ce.ExportError):
                ce.export_case(out, root / "case")
            ce.export_case(out, root / "case", force=True)
            with self.assertRaises(ce.ExportError):
                ce.export_case(out, out / "inside")

    def test_options_validation_and_overrides(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out, _ = make_output(root)
            with self.assertRaises(ce.ExportError):
                ce.export_case(out, root / "x", options={"engine_mode": "hybrid"})
            with self.assertRaises(ce.ExportError):
                ce.export_case(out, root / "x", options={"end_time": 0.0})
            manifest = ce.export_case(out, root / "y", options={"initial_eta": 12.0, "engine_mode": "real",
                                                                   "end_time": 120.0})
            conf = (root / "y/simdriver/run.conf").read_text(encoding="utf-8")
            self.assertIn("initial_eta = 12.0", conf)
            self.assertIn("engine_mode = real", conf)
            self.assertIn("end_time = 120.0", conf)
            self.assertEqual(manifest["run_conf"]["initial_eta_source"], "job_config")

    def test_failed_certification_leaves_nothing_behind(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out, _ = make_output(root)
            fake = root / "fake_validate.py"
            fake.write_text("import sys; sys.stderr.write('nope'); sys.exit(3)\n", encoding="utf-8")
            with self.assertRaises(ce.ExportError):
                ce.export_case(out, root / "z", validator_cli=sys_executable_wrapper(fake))
            self.assertFalse((root / "z").exists())
            self.assertEqual([p.name for p in root.iterdir() if p.name.startswith(".case_export_")], [])


def sys_executable_wrapper(script: Path) -> str:
    """Wrap the validator in a native executable shim for the host platform."""
    import os
    import shlex
    import sys
    if os.name == "nt":
        shim = script.with_suffix(".cmd")
        shim.write_text(f'@"{sys.executable}" "{script}" %*\n', encoding="utf-8")
    else:
        shim = script.with_suffix(".sh")
        shim.write_text(
            f'#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(script))} "$@"\n',
            encoding="utf-8", newline="\n")
        shim.chmod(0o700)
    return str(shim)


if __name__ == "__main__":
    unittest.main()
