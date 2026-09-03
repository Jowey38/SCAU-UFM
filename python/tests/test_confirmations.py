"""Headless unit tests for scau_preproc.confirmations (M287-C4)."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scau_preproc import confirmations as cf

SURF = [
    {"mapping_id": "MAP_1", "swmm_node_id": "J1", "cell_index": 379, "exchange_elevation_m": 10.0,
     "method": "explicit_id_plus_point_in_cell", "confidence": "high", "review_status": "needs_confirmation"},
    {"mapping_id": "MAP_2", "swmm_node_id": "J2", "cell_index": 234, "exchange_elevation_m": 9.5,
     "method": "explicit_id_plus_point_in_cell", "confidence": "high", "review_status": "needs_confirmation"},
]
ROOF = [
    {"mapping_id": "ROOF_1", "building_id": "B1", "roof_drain_id": "RD1", "swmm_node_id": "J1",
     "overflow_target_cell_index": 179, "candidate_distance_m": 18.3, "catchment_area_m2": 900.0,
     "exchange_elevation_m": 10.0, "method": "explicit_ids_plus_nearest_centroid_candidate",
     "confidence": "review", "review_status": "needs_confirmation"},
]
CANDIDATES = {"surface_to_swmm": SURF, "roof_to_swmm": ROOF}
PLACEHOLDERS = {"exchange_width_m": 1.0, "priority_weight": 1.0}


def conf(mapping_id, decision, chain="surface_to_swmm", digest=None, target=None, **extra):
    data = {"confirmation_schema_version": 1, "chain": chain, "mapping_id": mapping_id,
            "decision": decision, "candidate_sha256": digest,
            "confirmed_by": "tester", "timestamp": "2026-09-03T10:00:00", **extra}
    if target is not None:
        data["target"] = target
    return data


class MergeTests(unittest.TestCase):
    def _merge(self, confs):
        loaded = [cf._validate_confirmation(Path(f"c{i}.json"), c) for i, c in enumerate(confs)]
        return cf.merge(CANDIDATES, loaded, cell_count=469, placeholders=PLACEHOLDERS)

    def test_no_confirmations_leaves_everything_unconfirmed(self):
        result = self._merge([])
        self.assertEqual(result["report"]["status"], "incomplete")
        self.assertEqual(result["report"]["unconfirmed_total"], 3)
        self.assertEqual(result["effective"], {"surface_to_swmm": [], "roof_to_swmm": []})

    def test_accept_retarget_reject_and_create(self):
        result = self._merge([
            conf("MAP_1", "accept", digest=cf.candidate_sha256(SURF[0])),
            conf("MAP_2", "retarget", digest=cf.candidate_sha256(SURF[1]),
                 target={"cell_index": 233, "exchange_elevation_m": 9.6}),
            conf("ROOF_1", "reject", chain="roof_to_swmm", digest=cf.candidate_sha256(ROOF[0])),
            conf("MAP_NEW", "create", target={"cell_index": 10, "swmm_node_id": "J3",
                                              "exchange_elevation_m": 10.1}),
        ])
        report = result["report"]
        self.assertEqual(report["status"], "complete")
        surf = report["chains"]["surface_to_swmm"]
        self.assertEqual((surf["accepted"], surf["retargeted"], surf["created"]), (1, 1, 1))
        self.assertEqual(report["chains"]["roof_to_swmm"]["rejected"], 1)
        links = {l["mapping_id"]: l for l in result["effective"]["surface_to_swmm"]}
        self.assertEqual(links["MAP_2"]["cell_index"], 233)
        self.assertEqual(links["MAP_2"]["exchange_elevation_m"], 9.6)
        self.assertTrue(links["MAP_2"]["method"].endswith("+operator_retarget"))
        self.assertEqual(links["MAP_NEW"]["method"], "operator_created")
        self.assertTrue(all(l["review_status"] == "confirmed" for l in links.values()))
        self.assertEqual(result["effective"]["roof_to_swmm"], [])
        fragment = cf.simdriver_fragment(result["effective"]["surface_to_swmm"], PLACEHOLDERS)
        self.assertIn("cell=233,node=J2,crest=9.6", fragment)
        self.assertIn("cell=10,node=J3,crest=10.1", fragment)
        self.assertNotIn("cell=234", fragment)

    def test_drift_ignores_decision_and_reports_review(self):
        stale = dict(SURF[0], cell_index=1)  # candidate moved since confirmation
        result = self._merge([conf("MAP_1", "accept", digest=cf.candidate_sha256(stale))])
        self.assertEqual(result["report"]["chains"]["surface_to_swmm"]["drifted"], 1)
        self.assertEqual(result["report"]["unconfirmed_total"], 3)
        self.assertEqual(result["findings"][0]["code"], "ConfirmationDrift")
        self.assertEqual(result["findings"][0]["severity"], "review")
        self.assertEqual(result["effective"]["surface_to_swmm"], [])

    def test_structural_errors_are_fatal(self):
        digest = cf.candidate_sha256(SURF[0])
        with self.assertRaises(cf.ConfirmationError):
            self._merge([conf("MAP_GHOST", "accept", digest=digest)])
        with self.assertRaises(cf.ConfirmationError):
            self._merge([conf("MAP_1", "accept", digest=digest), conf("MAP_1", "reject", digest=digest)])
        with self.assertRaises(cf.ConfirmationError):
            self._merge([conf("MAP_1", "retarget", digest=digest, target={"cell_index": 469})])
        with self.assertRaises(cf.ConfirmationError):
            self._merge([conf("MAP_1", "create", target={"cell_index": 1, "swmm_node_id": "J1",
                                                          "exchange_elevation_m": 1.0})])


class ValidationTests(unittest.TestCase):
    def check(self, data):
        return cf._validate_confirmation(Path("x.json"), data)

    def test_schema_and_field_rules(self):
        digest = "0" * 64
        good = conf("MAP_1", "accept", digest=digest)
        self.assertEqual(self.check(good)["mapping_id"], "MAP_1")
        bad_cases = [
            dict(good, confirmation_schema_version=2),
            dict(good, chain="river"),
            dict(good, decision="maybe"),
            dict(good, candidate_sha256="abc"),
            dict(good, confirmed_by=""),
            dict(good, timestamp="yesterday"),
            conf("MAP_1", "retarget", digest=digest),                       # missing target
            conf("MAP_1", "retarget", digest=digest, target={"cell_index": 1.5}),
            conf("MAP_1", "accept", digest=digest, target={"cell_index": 1}),  # target not allowed
            conf("N", "create", digest=digest, target={"cell_index": 1}),       # create with hash
            conf("N", "create", target={"cell_index": 1}),                      # create missing node
        ]
        for case in bad_cases:
            with self.assertRaises(cf.ConfirmationError, msg=json.dumps(case)):
                self.check(case)

    def test_load_directory_sorted_and_missing_dir_ok(self):
        self.assertEqual(cf.load_confirmations(Path("/nonexistent/dir")), [])
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("b.json", "a.json"):
                (Path(tmp) / name).write_text(json.dumps(conf(name, "accept", digest="1" * 64)), encoding="utf-8")
            loaded = cf.load_confirmations(Path(tmp))
        self.assertEqual([c["mapping_id"] for c in loaded], ["a.json", "b.json"])

    def test_candidate_hash_is_canonical(self):
        a = {"x": 1, "y": [1, 2]}
        b = {"y": [1, 2], "x": 1}
        self.assertEqual(cf.candidate_sha256(a), cf.candidate_sha256(b))
        self.assertNotEqual(cf.candidate_sha256(a), cf.candidate_sha256({"x": 2, "y": [1, 2]}))


if __name__ == "__main__":
    unittest.main()
