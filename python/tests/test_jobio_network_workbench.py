"""Headless tests for P10/P11 pure jobio helpers."""
from __future__ import annotations
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("jobio_network", ROOT / "qgis_plugin/scau_preproc_workbench/jobio.py")
jobio = importlib.util.module_from_spec(spec); sys.modules["jobio_network"] = jobio; spec.loader.exec_module(jobio)
sys.path.insert(0, str(ROOT / "python"))


def feature(element, props, geometry):
    return {"type": "Feature", "properties": {"element": element, **props}, "geometry": geometry}


class NetworkWorkbenchTests(unittest.TestCase):
    def test_drainage_draft_preflight_and_mode_exclusivity(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "drainage.geojson"
            path.write_text(json.dumps({"drainage_network_schema_version": 1, "features": [
                feature("junction", {"node_id": "J1", "invert_elevation_m": 0}, {"type": "Point", "coordinates": [0, 0]}),
                feature("outfall", {"node_id": "O1", "invert_elevation_m": 0}, {"type": "Point", "coordinates": [1, 0]}),
                feature("conduit", {"link_id": "C1", "from_node": "J1", "to_node": "O1", "xsection": {"shape": "CIRCULAR"}}, {"type": "LineString", "coordinates": [[0, 0], [1, 0]]}),
            ]}), encoding="utf-8")
            self.assertEqual(jobio.validate_network_geojson(path, "drainage_network")[0][0], "pass")
            with self.assertRaises(ValueError):
                jobio.build_network_job_config({}, drainage={"mode": "authored", "geojson": str(path), "external_inp": "original.inp"})

    def test_river_draft_preflight_and_provider_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "river.geojson"
            path.write_text(json.dumps({"type": "FeatureCollection", "river_sketch_schema_version": 1, "features": [
                feature("river_node", {"node_id": "A"}, {"type": "Point", "coordinates": [0, 0]}),
                feature("river_node", {"node_id": "B"}, {"type": "Point", "coordinates": [1, 0]}),
                feature("river_branch", {"branch_id": "R", "from_node": "A", "to_node": "B"}, {"type": "LineString", "coordinates": [[0, 0], [1, 0]]}),
            ]}), encoding="utf-8")
            self.assertEqual(jobio.validate_network_geojson(path, "river_sketch")[0][0], "pass")
            rows = jobio.network_rows({"river_sketch": {"geojson": str(path)}}, "river_sketch")
            self.assertEqual(rows[0][0], "pass")


if __name__ == "__main__":
    unittest.main()
