"""Headless tests for scau_preproc.package_selfcheck (A6 sample-package self-check)."""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from scau_preproc import package_selfcheck as sc

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples/d5_gis_preproc_template"


def codes(report):
    return {(f["severity"], f["code"]) for f in report["findings"]}


class SelfcheckTests(unittest.TestCase):
    def test_sample_package_is_review_only(self):
        report = sc.run(SAMPLE)
        self.assertEqual(report["status"], "review")
        self.assertEqual(codes(report), {("review", "LandcoverOverlap")})   # declared smallest_area_wins

    def test_historic_defects_are_fatal(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            shutil.copytree(SAMPLE, pkg)
            inp = pkg / "swmm/model.inp"
            inp.write_text(inp.read_text(encoding="utf-8") + "\n[END]\n", encoding="utf-8")   # the real bug (ERROR 205)
            table = pkg / "metadata/dpm_rule_table.json"
            data = json.loads(table.read_text(encoding="utf-8"))
            data["landcover_overlap"] = "fatal"
            table.write_text(json.dumps(data), encoding="utf-8")
            mapping = pkg / "swmm/node_surface_mapping.csv"
            mapping.write_text(mapping.read_text(encoding="utf-8").replace("J1", "J9"), encoding="utf-8")
            report = sc.run(pkg)
            self.assertEqual(report["status"], "fatal")
            found = codes(report)
            self.assertIn(("fatal", "SwmmUnknownSection"), found)
            self.assertIn(("fatal", "LandcoverOverlap"), found)
            self.assertIn(("fatal", "MappingNodeUnknown"), found)
            objects = {o["feature_id"] for f in report["findings"] for o in f["objects"]}
            self.assertTrue({"END", "J9", "grass", "water"} <= objects)

    def test_runoff_sections_dem_coverage_and_placeholders(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            shutil.copytree(SAMPLE, pkg)
            inp = pkg / "swmm/model.inp"
            inp.write_text(inp.read_text(encoding="utf-8") + "\n[SUBCATCHMENTS]\nS1 G1 J1 1 50 100 0.5 0\n", encoding="utf-8")
            dem = pkg / "terrain/dem.asc"
            dem.write_text(dem.read_text(encoding="utf-8").replace("cellsize      40", "cellsize      30"), encoding="utf-8")
            manifest = pkg / "manifest.json"
            m = json.loads(manifest.read_text(encoding="utf-8"))
            m["synthetic_data"] = False
            manifest.write_text(json.dumps(m), encoding="utf-8")
            report = sc.run(pkg)
            found = codes(report)
            self.assertIn(("fatal", "SwmmRunoffSectionPresent"), found)
            self.assertIn(("fatal", "DemDoesNotCoverBoundary"), found)
            self.assertIn(("fatal", "PlaceholderInRealPackage"), found)
            (pkg / "metadata/geometry_clean_policy.json").unlink()
            self.assertIn(("fatal", "PolicyAbsent"), codes(sc.run(pkg)))

    def test_overlap_geometry(self):
        square = [(0, 0), (10, 0), (10, 10), (0, 10), (0, 0)]
        inner = [(2, 2), (4, 2), (4, 4), (2, 4), (2, 2)]
        neighbour = [(10, 0), (20, 0), (20, 10), (10, 10), (10, 0)]
        crossing = [(5, 5), (15, 5), (15, 15), (5, 15), (5, 5)]
        self.assertEqual(sc.polygons_overlap(square, inner), "contains")
        self.assertEqual(sc.polygons_overlap(inner, square), "contained")
        self.assertIsNone(sc.polygons_overlap(square, neighbour))       # shared edge is not overlap
        self.assertEqual(sc.polygons_overlap(square, crossing), "crosses")


if __name__ == "__main__":
    unittest.main()
