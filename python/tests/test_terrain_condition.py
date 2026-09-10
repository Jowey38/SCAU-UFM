"""Headless unit tests for scau_preproc.terrain_condition (M287-B3)."""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from scau_preproc import crs_governance as cg
from scau_preproc import terrain_condition as tc

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples/d5_gis_preproc_template"
G38 = ROOT / "tests/golden/preproc_terrain_condition_case/cases"

BASE_POLICY = {
    "terrain_condition_policy_schema_version": 1, "enabled": True,
    "authorization": {"authorized_by": "unit-test", "date": "2026-09-09"},
    "operations": {"fill_depressions": {"enabled": True}},
}


def write_policy(tmp: Path, name: str = "policy.json", **overrides) -> Path:
    path = tmp / name
    path.write_text(json.dumps({**BASE_POLICY, **overrides}), encoding="utf-8")
    return path


def grid(rows: list[list[float]], cellsize: float = 10.0, nodata: float = -9999.0) -> dict:
    return {"ncols": len(rows[0]), "nrows": len(rows), "xllcorner": 0.0, "yllcorner": 0.0,
            "cellsize": cellsize, "nodata_value": nodata, "values": [r[:] for r in rows]}


class PolicyTests(unittest.TestCase):
    def test_policy_rules(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            policy = tc.load_policy(write_policy(tmp))
            self.assertEqual(policy["active_operations"], ["fill_depressions"])
            self.assertEqual(policy["fill_depressions"]["connectivity"], 8)
            off = tc.load_policy(write_policy(tmp, enabled=False, authorization=None))
            self.assertEqual(off["active_operations"], [])
            for bad in (dict(terrain_condition_policy_schema_version=2),
                        dict(authorization=None),                                  # enabled needs authorization
                        dict(authorization={"authorized_by": "", "date": "x"}),
                        dict(operations={}),                                       # enabled but nothing authorized
                        dict(operations={"smooth": {"enabled": True}}),
                        dict(operations={"fill_depressions": {"enabled": True, "algorithm": "wang_liu"}}),
                        dict(operations={"fill_depressions": {"enabled": True, "algorithm_version": 2}}),
                        dict(operations={"fill_depressions": {"enabled": True, "connectivity": 6}}),
                        dict(operations={"fill_depressions": {"enabled": True, "epsilon_m": -1}}),
                        dict(operations={"reproject": {"enabled": True, "method": "cubic"}}),
                        dict(operations={"stream_enforcement": {"enabled": True, "burn_depth_m": 0}}),
                        dict(enabled="yes")):
                with self.assertRaises(tc.TerrainConditionError, msg=json.dumps(bad)):
                    tc.load_policy(write_policy(tmp, **bad))


class PriorityFloodTests(unittest.TestCase):
    def test_fills_pit_to_spill_elevation_and_is_deterministic(self):
        g = grid([[5, 5, 5, 5, 5],
                  [5, 1, 1, 4, 5],
                  [5, 1, 1, 4, 5],
                  [5, 4, 4, 4, 5],
                  [5, 5, 5, 5, 5]])
        filled, audit = tc.priority_flood(g, 0.0, 8, True)
        self.assertEqual(filled["values"][1][1], 5.0)            # spill over the 5 rim
        self.assertEqual(audit["cells_filled"], 9)
        self.assertEqual(audit["max_fill_depth_m"], 4.0)
        self.assertAlmostEqual(audit["fill_volume_m3"], (4 * 4 + 5 * 1) * 100.0)
        again, _ = tc.priority_flood(g, 0.0, 8, True)
        self.assertEqual(filled["values"], again["values"])
        self.assertEqual(g["values"][1][1], 1)                    # input untouched
        # 4-connectivity gives the same result for this bowl.
        four, _ = tc.priority_flood(g, 0.0, 4, True)
        self.assertEqual(four["values"], filled["values"])

    def test_epsilon_imposes_drainage_gradient(self):
        g = grid([[5, 5, 5],
                  [5, 1, 5],
                  [5, 5, 5]])
        filled, _ = tc.priority_flood(g, 0.01, 8, True)
        self.assertAlmostEqual(filled["values"][1][1], 5.01)

    def test_nodata_acts_as_outlet_when_authorized(self):
        nd = -9999.0
        g = grid([[5, 5, 5, 5],
                  [5, 1, 1, 5],
                  [5, 1, nd, 5],
                  [5, 5, 5, 5]])
        drained, audit = tc.priority_flood(g, 0.0, 8, True)
        self.assertEqual(drained["values"][1][1], 1.0)            # NoData neighbour drains the pit
        self.assertEqual(drained["values"][2][2], nd)             # NoData preserved
        self.assertEqual(audit["cells_filled"], 0)
        closed, audit = tc.priority_flood(g, 0.0, 8, False)
        self.assertEqual(closed["values"][1][1], 5.0)
        self.assertEqual(audit["cells_filled"], 3)

    def test_sloping_grid_without_pits_is_unchanged(self):
        g = grid([[1, 2, 3, 4], [1, 2, 3, 4], [1, 2, 3, 4]])
        filled, audit = tc.priority_flood(g, 0.0, 8, True)
        self.assertEqual(filled["values"], g["values"])
        self.assertEqual(audit["cells_filled"], 0)


class StreamAndRasterTests(unittest.TestCase):
    def test_burn_lowers_each_hit_cell_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            streams = Path(tmp) / "streams.geojson"
            streams.write_text(json.dumps({"type": "FeatureCollection", "features": [
                {"type": "Feature", "properties": {}, "geometry": {"type": "LineString", "coordinates": [[0, 15], [40, 15]]}},
                {"type": "Feature", "properties": {}, "geometry": {"type": "MultiLineString",
                                                                    "coordinates": [[[5, 15], [35, 15]]]}}]}), encoding="utf-8")
            g = grid([[10, 10, 10, 10], [10, 10, 10, 10], [10, 10, 10, -9999]])
            out, audit = tc.burn_streams(g, streams, 0.5, 0.5)
            self.assertEqual(out["values"][1], [9.5, 9.5, 9.5, 9.5])   # overlapping lines burn once
            self.assertEqual(out["values"][0], [10, 10, 10, 10])
            self.assertEqual(audit["cells_burned"], 4)
            self.assertEqual(audit["line_strings"], 2)

    def test_ascii_roundtrip_and_cell_geometry(self):
        g = tc.read_ascii_grid(SAMPLE / "terrain/dem.asc")
        self.assertEqual((g["ncols"], g["nrows"], g["cellsize"]), (5, 3, 40.0))
        self.assertEqual(tc.cell_of(g, 0.0, 0.0), (2, 0))
        self.assertEqual(tc.cell_of(g, 199.9, 119.9), (0, 4))
        self.assertIsNone(tc.cell_of(g, 200.0, 50.0))
        self.assertEqual(tc.cell_center(g, 2, 0), (20.0, 20.0))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "dem.asc"
            tc.write_ascii_grid(path, g)
            first = path.read_bytes()
            self.assertNotIn(b"\r", first)
            again = tc.read_ascii_grid(path)
            self.assertEqual(again["values"], g["values"])
            tc.write_ascii_grid(path, g)
            self.assertEqual(path.read_bytes(), first)

    def test_reprojection_resamples_onto_target_grid(self):
        g = tc.read_ascii_grid(G38 / "dem_depression.asc")
        out, audit = tc.reproject_grid(g, "EPSG:3857", "EPSG:3395", "nearest", None)
        self.assertEqual(out["ncols"], 20)
        self.assertEqual(out["nrows"], 12)                            # 119.19 m northing / 10 m -> 12 rows
        self.assertEqual(out["cellsize"], 10.0)
        self.assertIn("proj=pipeline", audit["proj_pipeline"])
        valid = [v for row in out["values"] for v in row if v != out["nodata_value"]]
        self.assertTrue(valid)
        self.assertTrue(all(9.7 <= v <= 10.8 for v in valid))
        again, _ = tc.reproject_grid(g, "EPSG:3857", "EPSG:3395", "nearest", None)
        self.assertEqual(out["values"], again["values"])
        bilinear, _ = tc.reproject_grid(g, "EPSG:3857", "EPSG:3395", "bilinear", 20.0)
        self.assertEqual((bilinear["ncols"], bilinear["nrows"]), (10, 6))
        with self.assertRaises(tc.TerrainConditionError):
            tc.reproject_grid(g, "EPSG:4326", "EPSG:3395", "nearest", None)


class ConditionPackageTests(unittest.TestCase):
    def _package(self, tmp: Path) -> Path:
        pkg = tmp / "pkg"
        shutil.copytree(SAMPLE, pkg)
        shutil.copyfile(G38 / "dem_depression.asc", pkg / "terrain/dem.asc")
        shutil.copyfile(G38 / "streams.geojson", pkg / "terrain/streams.geojson")
        return pkg

    def test_disabled_policy_writes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            report = tc.condition_package(self._package(tmp), write_policy(tmp, enabled=False, authorization=None), tmp / "out")
            self.assertFalse(report["enabled"])
            self.assertIsNone(report["conditioned_dem"])
            self.assertFalse((tmp / "out").exists())

    def test_enabled_fixture_policy_matches_committed_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            pkg = self._package(tmp)
            report = tc.condition_package(pkg, G38 / "terrain_condition_policy_fill.json", tmp / "out")
            committed = json.loads((G38 / "terrain_condition_report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["active_operations"], ["stream_enforcement", "fill_depressions"])
            self.assertEqual(report["operations"]["fill_depressions"], committed["operations"]["fill_depressions"])
            self.assertEqual(report["operations"]["stream_enforcement"]["cells_burned"], 19)
            self.assertEqual(report["conditioned_dem_sha256"], committed["conditioned_dem_sha256"])
            # Git may check out text fixtures with CRLF; generated rasters use LF.
            self.assertEqual((tmp / "out/dem.asc").read_bytes(),
                             (G38 / "conditioned_dem.asc").read_text(encoding="utf-8").encode("utf-8"))
            self.assertEqual((tmp / "out/depression_depth.asc").read_bytes(),
                             (G38 / "conditioned_depression_depth.asc").read_text(encoding="utf-8").encode("utf-8"))
            self.assertEqual(report["conditioned_grid_sha256"], committed["conditioned_grid_sha256"])
            self.assertEqual(report["findings"], [])
            conditioned = tc.read_ascii_grid(tmp / "out/dem.asc")
            # Every bowl cell sits exactly at the 10.30 m spill column; the stream row is 0.2 m lower than authored.
            for rb in range(4, 8):
                for col in range(8, 12):
                    self.assertEqual(conditioned["values"][conditioned["nrows"] - 1 - rb][col], 10.3)
            raw = tc.read_ascii_grid(pkg / "terrain/dem.asc")
            self.assertAlmostEqual(conditioned["values"][1][5], raw["values"][1][5] - 0.2)

    def test_review_finding_and_missing_streams(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            pkg = self._package(tmp)
            review = write_policy(tmp, "review.json", operations={"fill_depressions": {"enabled": True, "max_fill_depth_review_m": 0.5}})
            report = tc.condition_package(pkg, review, tmp / "r")
            self.assertEqual([f["code"] for f in report["findings"]], ["TerrainFillDepthReview"])
            self.assertEqual(report["findings"][0]["severity"], "review")
            missing = write_policy(tmp, "missing.json", operations={"stream_enforcement": {"enabled": True, "streams": "terrain/nope.geojson"}})
            with self.assertRaises(tc.TerrainConditionError) as ctx:
                tc.condition_package(pkg, missing, tmp / "m")
            self.assertEqual(ctx.exception.dataset, "streams")
            reproject = write_policy(tmp, "reproject.json", operations={"reproject": {"enabled": True}})
            with self.assertRaises(tc.TerrainConditionError):
                tc.condition_package(pkg, reproject, tmp / "p")          # no CRS governance -> no target CRS

    def test_reproject_takes_over_from_crs_governance(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            pkg = self._package(tmp)
            crs_policy = tmp / "crs.json"
            crs_policy.write_text(json.dumps({"crs_policy_schema_version": 1, "target_crs": "EPSG:3395", "allow_reprojection": True,
                                              "allowed_source_crs": ["EPSG:3857"], "dem_policy": "must_match_target",
                                              "audit_sample_count": 2}), encoding="utf-8")
            with self.assertRaises(cg.CrsGovernanceError):
                cg.govern_package(pkg, crs_policy, tmp / "s0")
            audit = cg.govern_package(pkg, crs_policy, tmp / "s1", dem_reprojection_authorized=True)
            self.assertEqual(audit["datasets"]["dem"]["action"], "reprojection_deferred_to_terrain_condition")
            self.assertIn("terrain/streams.geojson", audit["reprojected_files"])   # streams follow the vectors
            policy = write_policy(tmp, "rp.json", operations={"reproject": {"enabled": True, "method": "nearest"},
                                                              "fill_depressions": {"enabled": True}})
            report = tc.condition_package(tmp / "s1", policy, tmp / "out", target_crs="EPSG:3395",
                                          dem_source_crs=audit["datasets"]["dem"]["source_crs"])
            self.assertEqual(report["operations"]["reproject"]["action"], "reprojected")
            self.assertEqual(report["operations"]["reproject"]["target_grid"]["nrows"], 12)
            self.assertGreater(report["operations"]["fill_depressions"]["cells_filled"], 0)
            identity = tc.condition_package(pkg, policy, tmp / "out2", target_crs="EPSG:3857", dem_source_crs="EPSG:3857")
            self.assertEqual(identity["operations"]["reproject"]["action"], "identity_skipped")


if __name__ == "__main__":
    unittest.main()
