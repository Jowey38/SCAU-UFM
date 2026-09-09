"""Headless tests for the E7 project.ufm.json index pure layer."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "jobio_e7", ROOT / "qgis_plugin" / "scau_preproc_workbench" / "jobio.py")
jobio = importlib.util.module_from_spec(_spec)
sys.modules["jobio_e7"] = jobio
_spec.loader.exec_module(jobio)

SAMPLE = ROOT / "samples/d5_gis_preproc_template"


class ProjectIndexTests(unittest.TestCase):
    def test_index_accumulates_jobs_and_ui_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out1, out2 = root / "run1", root / "run2"
            out1.mkdir(); out2.mkdir()
            (out1 / "validation.json").write_text(json.dumps({"status": "ok", "started": "t1", "findings": []}), encoding="utf-8")
            (out1 / "pipeline_manifest.json").write_text(json.dumps({"case_sha256": "abc123def456"}), encoding="utf-8")
            job1 = jobio.build_job_config(str(SAMPLE), str(out1), crs_policy="metadata/crs_policy.json",
                                          terrain_policy="metadata/terrain_condition_policy.json", coupling_maps=True,
                                          coupling_mode="mixed", roof_node_max_distance_m=30.0)
            path = jobio.default_project_index_path(job1)
            self.assertEqual(path, root / "project.ufm.json")
            jobio.update_project_index(path, job1, jobio.load_validation_for(job1), repo_root=str(ROOT))
            index = jobio.load_project_index(path)
            self.assertFalse(index["authoritative"])
            self.assertEqual(index["repo_root"], str(ROOT))
            self.assertEqual(len(index["jobs"]), 1)
            self.assertEqual(index["jobs"][0]["status"], "ok")
            self.assertEqual(index["jobs"][0]["case_sha256"], "abc123def456")
            self.assertIn("validation.json", index["jobs"][0]["artifacts"])
            self.assertEqual(index["ui_state"]["terrain_condition"], {"policy": str(Path("metadata/terrain_condition_policy.json"))})
            self.assertEqual(index["ui_state"]["coupling_maps"], {"mode": "mixed", "roof_node_max_distance_m": 30.0})
            # Second output dir accumulates; same dir replaces.
            job2 = jobio.build_job_config(str(SAMPLE), str(out2))
            jobio.update_project_index(path, job2, None)
            jobio.update_project_index(path, job2, {"status": "fatal", "findings": [{"severity": "fatal"}]})
            index = jobio.load_project_index(path)
            self.assertEqual([j["output_dir"] for j in index["jobs"]], [str(out1), str(out2)])
            self.assertEqual(index["jobs"][1]["status"], "fatal")
            self.assertEqual(index["jobs"][1]["findings"], 1)
            rows = jobio.project_index_rows(path)
            self.assertEqual(rows[0][1], "ProjectIndex")
            self.assertEqual([r[0] for r in rows[1:]], ["pass", "fatal"])

    def test_invalid_or_missing_index(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "project.ufm.json"
            self.assertIsNone(jobio.load_project_index(path))
            self.assertEqual(jobio.project_index_rows(path)[0][1], "NoProjectIndex")
            path.write_text(json.dumps({"project_index_schema_version": 99}), encoding="utf-8")
            self.assertIsNone(jobio.load_project_index(path))


if __name__ == "__main__":
    unittest.main()
