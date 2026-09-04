"""Headless tests for the E4 coupling-editor pure layer in jobio.py.

Builds a tiny fake output directory (candidates + cell geometries) and a
package with a buildings file; verifies link-layer construction, status
derivation, decision writing (hash-bound to the current candidate) and that
the written files are accepted by scau_preproc.confirmations (single contract).
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
    "jobio_e4", ROOT / "qgis_plugin" / "scau_preproc_workbench" / "jobio.py")
jobio = importlib.util.module_from_spec(_spec)
sys.modules["jobio_e4"] = jobio
_spec.loader.exec_module(jobio)

from scau_preproc import confirmations as cf  # noqa: E402

SURF = [
    {"mapping_id": "MAP_1", "swmm_node_id": "J1", "cell_index": 0, "node_x": 5.0, "node_y": 5.0,
     "exchange_elevation_m": 10.0, "method": "explicit_id_plus_point_in_cell",
     "confidence": "high", "review_status": "needs_confirmation"},
    {"mapping_id": "SPATIAL_SURF_J2", "swmm_node_id": "J2", "cell_index": 3, "node_x": 15.0, "node_y": 15.0,
     "exchange_elevation_m": 13.0, "method": "spatial_point_in_cell_candidate",
     "confidence": "review", "review_status": "needs_confirmation"},
]
ROOF = [
    {"mapping_id": "ROOF_1", "building_id": "B1", "swmm_node_id": "J1", "overflow_target_cell_index": 2,
     "exchange_elevation_m": 12.0, "method": "spatial_nearest_junction_candidate",
     "confidence": "review", "review_status": "needs_confirmation"},
]


def square(x0, y0):
    return [[x0, y0], [x0 + 10, y0], [x0 + 10, y0 + 10], [x0, y0 + 10], [x0, y0]]


class CouplingEditorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        out = root / "out"; (out / "coupling").mkdir(parents=True)
        pkg = root / "pkg"; (pkg / "buildings").mkdir(parents=True)
        (out / "coupling/surface_swmm_mapping.json").write_text(json.dumps({"relations": SURF}), encoding="utf-8")
        (out / "coupling/roof_drain_mapping.json").write_text(json.dumps({"relations": ROOF}), encoding="utf-8")
        (out / "coupling/mapping_report.json").write_text(json.dumps({"placeholders": {"exchange_width_m": 1.0, "priority_weight": 1.0}}), encoding="utf-8")
        cells = [{"type": "Feature", "properties": {"cell_id": k},
                  "geometry": {"type": "Polygon", "coordinates": [square(10 * (k % 2), 10 * (k // 2))]}} for k in range(4)]
        (out / "mesh_quality_cells.geojson").write_text(json.dumps({"type": "FeatureCollection", "features": cells}), encoding="utf-8")
        (pkg / "buildings/buildings.geojson").write_text(json.dumps({"type": "FeatureCollection", "features": [
            {"type": "Feature", "properties": {"building_id": "B1"},
             "geometry": {"type": "Polygon", "coordinates": [[[2, 12], [8, 12], [8, 18], [2, 18], [2, 12]]]}}]}), encoding="utf-8")
        self.job = {"package": str(pkg), "output_dir": str(out), "confirmations_dir": str(root / "confirmed")}

    def tearDown(self):
        self.tmp.cleanup()

    def test_layers_and_statuses_before_any_decision(self):
        layers = jobio.build_coupling_link_layers(self.job)
        self.assertEqual(layers["summary"], {"unconfirmed": 3, "accepted": 0, "retargeted": 0, "rejected": 0, "drifted": 0})
        self.assertEqual(len(layers["links"]["features"]), 3)
        self.assertEqual(len(layers["cells"]["features"]), 3)
        link = next(f for f in layers["links"]["features"] if f["properties"]["mapping_id"] == "ROOF_1")
        self.assertEqual(link["geometry"]["coordinates"][0], [5.0, 15.0])   # building centroid
        self.assertEqual(link["geometry"]["coordinates"][1], [5.0, 15.0])   # cell 2 centroid
        self.assertEqual(link["properties"]["confidence"], "review")
        self.assertEqual(jobio.coupling_editor_rows(self.job)[0][0], "review")

    def test_decisions_round_trip_through_the_pipeline_contract(self):
        jobio.write_decision(self.job, "surface_to_swmm", "MAP_1", "accept", confirmed_by="op")
        jobio.write_decision(self.job, "surface_to_swmm", "SPATIAL_SURF_J2", "retarget", confirmed_by="op",
                             target={"cell_index": 1, "exchange_elevation_m": 12.5}, note="moved")
        jobio.write_decision(self.job, "roof_to_swmm", "ROOF_1", "reject", confirmed_by="op")
        jobio.write_create_decision(self.job, "surface_to_swmm", "MAP_NEW", confirmed_by="op",
                                    swmm_node_id="J3", cell_index=2, exchange_elevation_m=11.0)
        layers = jobio.build_coupling_link_layers(self.job)
        self.assertEqual(layers["summary"], {"unconfirmed": 0, "accepted": 1, "retargeted": 1, "rejected": 1, "drifted": 0})
        retargeted = next(f for f in layers["cells"]["features"] if f["properties"]["mapping_id"] == "SPATIAL_SURF_J2")
        self.assertEqual(retargeted["properties"]["cell_index"], 1)
        self.assertEqual(retargeted["properties"]["candidate_cell_index"], 3)
        self.assertEqual(jobio.coupling_editor_rows(self.job)[0][0], "pass")

        # The SAME files must be accepted by the pipeline's merge (one contract).
        loaded = cf.load_confirmations(Path(self.job["confirmations_dir"]))
        merged = cf.merge({"surface_to_swmm": SURF, "roof_to_swmm": ROOF}, loaded, cell_count=4,
                          placeholders={"exchange_width_m": 1.0, "priority_weight": 1.0})
        self.assertEqual(merged["report"]["status"], "complete")
        ids = sorted(l["mapping_id"] for l in merged["effective"]["surface_to_swmm"])
        self.assertEqual(ids, ["MAP_1", "MAP_NEW", "SPATIAL_SURF_J2"])

    def test_drift_and_errors(self):
        jobio.write_decision(self.job, "surface_to_swmm", "MAP_1", "accept", confirmed_by="op")
        surf = json.loads((Path(self.job["output_dir"]) / "coupling/surface_swmm_mapping.json").read_text())
        surf["relations"][0]["cell_index"] = 1  # regenerated candidate moved
        (Path(self.job["output_dir"]) / "coupling/surface_swmm_mapping.json").write_text(json.dumps(surf), encoding="utf-8")
        self.assertEqual(jobio.build_coupling_link_layers(self.job)["summary"]["drifted"], 1)
        self.assertTrue(jobio.remove_decision(self.job, "MAP_1"))
        self.assertFalse(jobio.remove_decision(self.job, "MAP_1"))
        with self.assertRaises(ValueError):
            jobio.write_decision(self.job, "surface_to_swmm", "GHOST", "accept", confirmed_by="op")
        with self.assertRaises(ValueError):
            jobio.write_decision(self.job, "surface_to_swmm", "MAP_1", "retarget", confirmed_by="op")
        with self.assertRaises(ValueError):
            jobio.write_decision(self.job, "surface_to_swmm", "MAP_1", "accept", confirmed_by="  ")
        with self.assertRaises(ValueError):
            jobio.write_create_decision(self.job, "surface_to_swmm", "MAP_1", confirmed_by="op",
                                        swmm_node_id="J1", cell_index=0, exchange_elevation_m=1.0)
        with self.assertRaises(ValueError):
            jobio.write_create_decision(self.job, "roof_to_swmm", "R_NEW", confirmed_by="op",
                                        swmm_node_id="J1", cell_index=0, exchange_elevation_m=1.0)

    def test_hash_matches_pipeline_module(self):
        self.assertEqual(jobio.candidate_sha256(SURF[0]), cf.candidate_sha256(SURF[0]))


if __name__ == "__main__":
    unittest.main()
