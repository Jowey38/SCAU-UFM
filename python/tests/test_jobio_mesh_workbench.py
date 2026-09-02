"""Headless tests for the QGIS plugin's pure jobio layer (E3 additions).

jobio.py has no qgis imports; it is loaded straight from qgis_plugin/.
Run from the repository root: py -3 -m unittest discover -s python/tests -t python
"""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "jobio", ROOT / "qgis_plugin" / "scau_preproc_workbench" / "jobio.py")
jobio = importlib.util.module_from_spec(_spec)
sys.modules["jobio"] = jobio
_spec.loader.exec_module(jobio)

SAMPLE_CONTROLS = ROOT / "samples/d5_gis_preproc_template/mesh_controls/mesh_controls.geojson"


class JobConfigTests(unittest.TestCase):
    def test_mesh_controls_block_only_when_requested(self):
        job = jobio.build_job_config("pkg", "out")
        self.assertNotIn("mesh_controls", job)
        job = jobio.build_job_config("pkg", "out", mesh_controls_geojson="c.geojson",
                                     mesh_controls_default_size_m=3)
        self.assertEqual(job["mesh_controls"], {"geojson": "c.geojson", "default_size_m": 3.0})

    def test_layer_paths_include_controls_and_heatmap(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "mesh_quality_cells.geojson").write_text("{}", encoding="utf-8")
            job = jobio.build_job_config(str(out), str(out), mesh_controls_geojson=str(SAMPLE_CONTROLS))
            paths = jobio.layer_paths(job)
        self.assertEqual(Path(paths["mesh_controls"]), SAMPLE_CONTROLS)
        self.assertIn("mesh_quality_cells", paths)


class PrecheckTests(unittest.TestCase):
    def test_sample_controls_pass(self):
        rows = jobio.mesh_controls_precheck(str(SAMPLE_CONTROLS), 8.0)
        self.assertEqual([r[0] for r in rows], ["pass"])
        self.assertIn("3 breakline(s), 2 refinement region(s)", rows[0][2])

    def test_missing_file_and_bad_sizes(self):
        self.assertEqual(jobio.mesh_controls_precheck("nope.geojson", 8.0)[0][1], "MeshControlsMissing")
        with tempfile.NamedTemporaryFile("w", suffix=".geojson", delete=False, encoding="utf-8") as handle:
            json.dump({"type": "FeatureCollection", "mesh_controls_schema_version": 1, "features": [
                {"type": "Feature", "properties": {"control_id": "R", "control_kind": "refinement_region",
                                                   "size_m": 9.0},
                 "geometry": {"type": "Polygon", "coordinates": [[[0, 0], [1, 0], [1, 1], [0, 0]]]}},
                {"type": "Feature", "properties": {"control_id": "B", "control_kind": "breakline",
                                                   "size_m": 2.0},
                 "geometry": {"type": "LineString", "coordinates": [[0, 0], [1, 1]]}},
                {"type": "Feature", "properties": {"control_id": "X", "control_kind": "nope"},
                 "geometry": {"type": "LineString", "coordinates": [[0, 0], [1, 1]]}},
            ]}, handle)
        rows = jobio.mesh_controls_precheck(handle.name, 8.0)
        Path(handle.name).unlink()
        codes = [r[1] for r in rows if r[0] == "fatal"]
        self.assertEqual(codes, ["MeshControlSize", "MeshControlSize", "MeshControlKind"])

    def test_disabled_returns_nothing(self):
        self.assertEqual(jobio.mesh_controls_precheck(None, 8.0), [])


class HeatmapTests(unittest.TestCase):
    def test_classes_cover_line_and_order_good_to_bad(self):
        classes = jobio.heatmap_classes("equiangle_skewness")
        self.assertEqual(len(classes), 5)
        self.assertEqual(classes[0][0], float("-inf"))
        self.assertEqual(classes[-1][1], float("inf"))
        self.assertEqual(classes[1][2], "0.25 – 0.5")
        # min_angle: larger is better, so the first class is the largest range.
        angle = jobio.heatmap_classes("min_angle_deg")
        self.assertEqual(angle[0][1], float("inf"))
        with self.assertRaises(KeyError):
            jobio.heatmap_classes("not_a_metric")

    def test_quality_summary_reads_controls_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "mesh_quality.json").write_text(json.dumps({
                "quality": {"cells": 10, "triangles": 2, "quadrilaterals": 8,
                            "min_angle_degrees": 20.5, "max_edge_length_ratio": 3.0},
                "per_cell_diagnostics": {"max_equiangle_skewness": 0.5, "max_nonorthogonality_deg": 30},
                "mesh_controls": {
                    "breaklines": [{"control_id": "BL", "preserved": True, "mesh_edges_on_breakline": 4}],
                    "refinement_regions": [{"control_id": "RR", "size_m": 2.5, "cells_inside": 6,
                                            "mean_edge_length_inside_m": 2.25}],
                }}), encoding="utf-8")
            rows = jobio.mesh_quality_summary({"output_dir": str(out)})
        self.assertEqual([r[1] for r in rows],
                         ["MeshQuality", "PerCellDiagnostics", "BreaklinePreserved", "RefinementRegion"])
        self.assertEqual(rows[2][0], "pass")
        self.assertEqual(jobio.mesh_quality_summary({"output_dir": "/nonexistent"})[0][1],
                         "NoMeshQualityReport")


if __name__ == "__main__":
    unittest.main()
