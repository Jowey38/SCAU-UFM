"""Headless unit tests for scau_preproc.crs_governance (M287-B2)."""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from scau_preproc import crs_governance as cg

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples/d5_gis_preproc_template"
BASE_POLICY = {"crs_policy_schema_version": 1, "target_crs": "EPSG:3857", "allow_reprojection": True,
               "allowed_source_crs": ["EPSG:3857", "EPSG:3395"], "source_crs_overrides": {},
               "dem_policy": "must_match_target", "audit_sample_count": 3}


def write_policy(tmp: Path, **overrides) -> Path:
    path = tmp / "crs_policy.json"
    path.write_text(json.dumps({**BASE_POLICY, **overrides}), encoding="utf-8")
    return path


class PolicyTests(unittest.TestCase):
    def test_policy_rules(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            self.assertEqual(cg.load_policy(write_policy(tmp))["target_text"], "EPSG:3857")
            for bad in (dict(target_crs="EPSG:4326"), dict(target_crs="TODO_PROVIDER"), dict(target_crs="EPSG:99999999"),
                        dict(allowed_source_crs=[]), dict(allowed_source_crs=["EPSG:4326"]), dict(dem_policy="resample"),
                        dict(audit_sample_count=0), dict(crs_policy_schema_version=2)):
                with self.assertRaises(cg.CrsGovernanceError, msg=json.dumps(bad)):
                    cg.load_policy(write_policy(tmp, **bad))

    def test_source_resolution(self):
        with tempfile.TemporaryDirectory() as tmp:
            policy = cg.load_policy(write_policy(Path(tmp), source_crs_overrides={"landcover": "EPSG:3395"}))
        self.assertEqual(cg.resolve_source_crs("x", "EPSG:3857", policy)[0], "EPSG:3857")
        self.assertEqual(cg.resolve_source_crs("landcover", None, policy)[0], "EPSG:3395")
        with self.assertRaises(cg.CrsGovernanceError):
            cg.resolve_source_crs("x", None, policy)
        with self.assertRaises(cg.CrsGovernanceError):
            cg.resolve_source_crs("x", "EPSG:4326", policy)
        with self.assertRaises(cg.CrsGovernanceError):
            cg.resolve_source_crs("x", "EPSG:32650", policy)
        self.assertEqual(cg.geojson_crs_text({"crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::3857"}}}), "EPSG:3857")
        self.assertIsNone(cg.geojson_crs_text({}))


class GovernTests(unittest.TestCase):
    def test_identity_policy_copies_everything_verbatim(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            audit = cg.govern_package(SAMPLE, write_policy(tmp), tmp / "staging",
                                      mesh_controls=SAMPLE / "mesh_controls/mesh_controls.geojson")
            self.assertEqual(audit["reprojected_files"], [])
            self.assertTrue(all(d["action"] == "copied_identity" for d in audit["datasets"].values()))
            for rel in ("boundary/computational_boundary.geojson", "swmm/model.inp", "terrain/dem.asc", "manifest.json"):
                self.assertEqual((tmp / "staging" / rel).read_bytes(), (SAMPLE / rel).read_bytes(), rel)
            self.assertTrue((tmp / "staging/crs_audit.json").is_file())

    def test_reprojection_is_deterministic_audited_and_invertible(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            policy = write_policy(tmp, target_crs="EPSG:3395", allowed_source_crs=["EPSG:3857"], dem_policy="declare_only")
            a1 = cg.govern_package(SAMPLE, policy, tmp / "s1", mesh_controls=SAMPLE / "mesh_controls/mesh_controls.geojson")
            a2 = cg.govern_package(SAMPLE, policy, tmp / "s2", mesh_controls=SAMPLE / "mesh_controls/mesh_controls.geojson")
            self.assertEqual(sorted(a1["reprojected_files"]),
                             ["boundary/computational_boundary.geojson", "buildings/buildings.geojson",
                              "landcover/landcover.geojson", "soil/soil_zones.geojson", "swmm/model.inp"])
            for rel in a1["reprojected_files"] + ["mesh_controls.geojson"]:
                self.assertEqual((tmp / "s1" / rel).read_bytes(), (tmp / "s2" / rel).read_bytes(), rel)
            boundary = json.loads((tmp / "s1/boundary/computational_boundary.geojson").read_text())
            ring = boundary["features"][0]["geometry"]["coordinates"][0]
            self.assertEqual(boundary["crs"]["properties"]["name"], "EPSG:3395")
            self.assertAlmostEqual(max(p[1] for p in ring), 119.196674, places=5)   # Mercator compression of y=120
            self.assertAlmostEqual(max(p[0] for p in ring), 200.0, places=5)
            entry = a1["datasets"]["computational_boundary"]
            self.assertIn("proj=pipeline", entry["proj_pipeline"])
            self.assertEqual(len(entry["samples"]), 3)
            self.assertTrue(all(s["roundtrip_error_m"] < 1e-9 for s in entry["samples"]))
            self.assertEqual(a1["datasets"]["dem"]["action"], "declared_mismatch_allowed")
            self.assertEqual(a1["datasets"]["swmm"]["nodes"], 3)
            # Only [COORDINATES] lines changed in the staged .inp.
            src = [l for l in (SAMPLE / "swmm/model.inp").read_text().splitlines() if not l.startswith(("J", "O1"))]
            dst = [l for l in (tmp / "s1/swmm/model.inp").read_text().splitlines() if not l.startswith(("J", "O1"))]
            self.assertEqual(src, dst)

    def test_fail_closed_cases(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            with self.assertRaises(cg.CrsGovernanceError) as ctx:
                cg.govern_package(SAMPLE, write_policy(tmp, allowed_source_crs=["EPSG:32650"]), tmp / "a")
            self.assertEqual(ctx.exception.dataset, "computational_boundary")
            with self.assertRaises(cg.CrsGovernanceError) as ctx:
                cg.govern_package(SAMPLE, write_policy(tmp, target_crs="EPSG:3395", allowed_source_crs=["EPSG:3857"]), tmp / "b")
            self.assertEqual(ctx.exception.dataset, "dem")   # must_match_target
            with self.assertRaises(cg.CrsGovernanceError) as ctx:
                cg.govern_package(SAMPLE, write_policy(tmp, target_crs="EPSG:3395", allowed_source_crs=["EPSG:3857"],
                                                       dem_policy="declare_only", allow_reprojection=False), tmp / "c")
            self.assertIn("allow_reprojection", str(ctx.exception))
            # undeclared layer -> fatal; override rescues it
            pkg = tmp / "pkg"; shutil.copytree(SAMPLE, pkg)
            p = pkg / "landcover/landcover.geojson"; d = json.loads(p.read_text()); del d["crs"]; p.write_text(json.dumps(d))
            with self.assertRaises(cg.CrsGovernanceError) as ctx:
                cg.govern_package(pkg, write_policy(tmp), tmp / "d")
            self.assertEqual(ctx.exception.dataset, "landcover")
            audit = cg.govern_package(pkg, write_policy(tmp, source_crs_overrides={"landcover": "EPSG:3857"}), tmp / "e")
            self.assertEqual(audit["datasets"]["landcover"]["action"], "copied_identity")


if __name__ == "__main__":
    unittest.main()
