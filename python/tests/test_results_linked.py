import json
import tempfile
import unittest
from pathlib import Path

from scau_results import linked
from scau_results.linked import fnv1a64

RPT = """
  EPA STORM WATER MANAGEMENT MODEL - VERSION 5.2 (Build 5.2.4)

  *************************
  Runoff Quantity Continuity     hectare-m            mm
  *************************
  Continuity Error (%) .....        0.001

  **************************
  Flow Routing Continuity        hectare-m      10^6 ltr
  **************************
  Continuity Error (%) .....       -6.627

  **********************
  Node Depth Summary
  **********************

  ---------------------------------------------------------------------------------
                                 Average  Maximum  Maximum  Time of Max    Reported
                                   Depth    Depth      HGL   Occurrence   Max Depth
  Node                 Type       Meters   Meters   Meters  days hr:min      Meters
  ---------------------------------------------------------------------------------
  J1                   JUNCTION     1.94     3.00    13.00     0  00:01        0.00
  O1                   OUTFALL      0.68     1.00    10.00     0  00:01        0.00

  *******************
  Node Inflow Summary
  *******************

  -------------------------------------------------------------------------------------------------
                                  Maximum  Maximum                  Lateral       Total        Flow
                                  Lateral    Total  Time of Max      Inflow      Inflow     Balance
                                   Inflow   Inflow   Occurrence      Volume      Volume       Error
  Node                 Type           CMS      CMS  days hr:min    10^6 ltr    10^6 ltr     Percent
  -------------------------------------------------------------------------------------------------
  J1                   JUNCTION     3.521    3.521     0  00:02       0.382       0.382      10.253
  O1                   OUTFALL      0.000    3.775     0  00:02           0       0.399       0.000

"""
US_RPT = RPT.replace("Meters", "Feet").replace("CMS", "CFS").replace("10^6 ltr", "10^6 gal") \
            .replace("hectare-m", "acre-feet")


def summary(outcome="completed", epochs=3, rpt_bytes=RPT.encode(), stcf_hash="fnv1a64:0000000000000001"):
    def link():
        return {"engine": "drainage", "node": 0, "node_name": "J1", "cell": 379,
                "v_granted": 100.0, "v_repay": 0.5, "v_returned": 2.0}
    return {"outcome": outcome, "committed_epochs": epochs, "final_surface_state_hash": "fnv1a64:abc",
            "source_stcf_hash": stcf_hash, "swmm_inp_hash": "fnv1a64:inp",
            "swmm_report_path": "/tmp/x.rpt", "swmm_report_hash": fnv1a64(rpt_bytes),
            "start_time": 0.0, "dt_couple": 60.0,
            "total_drained_volume": 100.5 * epochs, "total_returned_volume": 2.0 * epochs,
            "epochs": [{"logical_time": 60.0 * (i + 1), "drained_volume": 100.5, "returned_volume": 2.0,
                        "link_exchanges": [link()]} for i in range(epochs)]}


class ReportParserTests(unittest.TestCase):
    def test_reads_both_tables_units_and_section_scoped_continuity(self):
        with tempfile.TemporaryDirectory() as tmp:
            rpt = Path(tmp) / "r.rpt"
            rpt.write_bytes((RPT).encode())
            report = linked.parse_swmm_report(rpt)
            self.assertEqual(report["routing_continuity_error_pct"], -6.627)   # NOT the runoff 0.001
            self.assertEqual(report["runoff_continuity_error_pct"], 0.001)
            self.assertEqual(report["swmm_version"], "5.2 (Build 5.2.4)")
            self.assertEqual(report["nodes"]["J1"]["max_depth_m"], 3.0)
            self.assertEqual(report["nodes"]["J1"]["lateral_inflow_volume_m3"], 382.0)
            self.assertEqual(report["nodes"]["O1"]["lateral_inflow_volume_m3"], 0.0)
            self.assertIn("3 significant", report["volume_precision"])
            self.assertEqual(report["report_hash"], fnv1a64(rpt.read_bytes()))

    def test_us_units_and_missing_units_are_rejected_never_assumed(self):
        with tempfile.TemporaryDirectory() as tmp:
            us = Path(tmp) / "us.rpt"
            us.write_bytes((US_RPT).encode())
            with self.assertRaisesRegex(ValueError, "unit mismatch.*Feet"):
                linked.parse_swmm_report(us)
            nounits = Path(tmp) / "nu.rpt"
            nounits.write_text(RPT.replace("Meters", "     ").replace("CMS", "   "), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "does not declare"):
                linked.parse_swmm_report(nounits)
            (Path(tmp) / "bad.rpt").write_text("not swmm", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "not a SWMM report"):
                linked.parse_swmm_report(Path(tmp) / "bad.rpt")


class LedgerIntegrityTests(unittest.TestCase):
    def _load(self, tmp, s):
        p = Path(tmp) / "s.json"
        p.write_text(json.dumps(s), encoding="utf-8")
        return linked.load_link_ledger(p)

    def test_aggregates_and_balance_invariant(self):
        with tempfile.TemporaryDirectory() as tmp:
            view = self._load(tmp, summary())
            (j1,) = view["links"]
            self.assertEqual((j1["granted_m3"], j1["repay_m3"], j1["returned_m3"]), (300.0, 1.5, 6.0))
            self.assertEqual(view["time_window_s"], [0.0, 180.0])

    def test_adversarial_summaries_are_rejected(self):
        cases = {}
        s = summary(); s["committed_epochs"] = 500; cases["declared cycles"] = (s, "committed_epochs=500")
        s = summary(); s["epochs"][1]["logical_time"] = -1; cases["reversed time"] = (s, "logical_time")
        s = summary(); s["epochs"][0]["link_exchanges"][0]["v_granted"] = -100; cases["negative"] = (s, "non-negative")
        s = summary(); s["epochs"][0]["link_exchanges"].append(dict(s["epochs"][0]["link_exchanges"][0]))
        cases["duplicate link"] = (s, "appears twice")
        s = summary(); s["epochs"][2]["link_exchanges"][0]["cell"] = 7; cases["moved cell"] = (s, "identity")
        s = summary(); s["epochs"][0]["drained_volume"] = 999; cases["ledger != net"] = (s, "net write-back")
        s = summary(); s["total_drained_volume"] = 1; cases["run total"] = (s, "run-total")
        s = summary(outcome="review_required"); cases["not completed"] = (s, "review_required")
        s = summary(); del s["source_stcf_hash"]; cases["missing field"] = (s, "required fields")
        s = summary(); s["final_surface_state_hash"] = None; cases["None hash"] = (s, "required fields")
        s = summary(); del s["epochs"][0]["link_exchanges"]; cases["old driver"] = (s, "no link_exchanges")
        with tempfile.TemporaryDirectory() as tmp:
            for name, (bad, pattern) in cases.items():
                with self.subTest(name=name), self.assertRaisesRegex(ValueError, pattern):
                    self._load(tmp, bad)


class LinkedViewTests(unittest.TestCase):
    def _setup(self, tmp):
        tmp = Path(tmp)
        (tmp / "s.json").write_text(json.dumps(summary()), encoding="utf-8")
        (tmp / "r.rpt").write_bytes((RPT).encode())
        return tmp

    def test_report_must_be_the_runs_own_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = self._setup(tmp)
            view = linked.linked_view(tmp / "s.json", tmp / "r.rpt", None)
            (j1,) = view["links"]
            self.assertEqual(j1["engine_native"]["max_depth_m"], 3.0)
            self.assertEqual(j1["gap"]["signed_gap_m3"], 382.0 - 301.5)
            self.assertEqual(j1["gap"]["diagnostic_code"], "ROUTING_GAP_ATTRIBUTION_INSUFFICIENT")
            self.assertIn("not been reconciled", j1["gap"]["note"])
            self.assertEqual(view["swmm_report"]["routing_continuity_error_pct"], -6.627)
            self.assertIn("NOT evidence", view["swmm_report"]["scope_note"])
            # same node name, different run's report -> refused by byte hash
            (tmp / "other.rpt").write_text(RPT.replace("0.382", "9.382"), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "do not match the hash recorded by this run"):
                linked.linked_view(tmp / "s.json", tmp / "other.rpt", None)
            # a run that recorded no report hash (mock) cannot bind ANY report
            s = summary(); s["swmm_report_hash"] = ""
            (tmp / "mock.json").write_text(json.dumps(s), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "records no SWMM report hash"):
                linked.linked_view(tmp / "mock.json", tmp / "r.rpt", None)
            # report missing the node
            (tmp / "s9.json").write_text(json.dumps(summary(rpt_bytes=RPT.replace("J1  ", "J9  ").encode())),
                                         encoding="utf-8")
            (tmp / "r9.rpt").write_bytes(RPT.replace("J1  ", "J9  ").encode())
            with self.assertRaisesRegex(ValueError, "no inflow row for node 'J1'"):
                linked.linked_view(tmp / "s9.json", tmp / "r9.rpt", None)

    def test_gap_within_report_rounding_is_no_gap(self):
        s = summary()
        for e in s["epochs"]:
            e["link_exchanges"][0]["v_granted"] = 382.0 / 3 - 0.5  # ledger_in = 3*(that + 0.5) = 382.0
            e["drained_volume"] = 382.0 / 3
        s["total_drained_volume"] = 382.0
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "s.json").write_text(json.dumps(s), encoding="utf-8")
            (tmp / "r.rpt").write_bytes((RPT).encode())
            gap = linked.linked_view(tmp / "s.json", tmp / "r.rpt", None)["links"][0]["gap"]
            self.assertEqual(gap["diagnostic_code"], "NO_GAP")
            self.assertAlmostEqual(gap["rpt_rounding_half_ulp_m3"], 0.5)   # 382 -> 3 s.f. -> ±0.5 m3

    def test_surface_result_binding_uses_full_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = self._setup(tmp)
            (tmp / "ghost.nc.manifest.json").write_text(json.dumps({"final_surface_state_hash": "fnv1a64:abc"}))
            with self.assertRaisesRegex(ValueError, "does not exist beside its manifest"):
                linked.linked_view(tmp / "s.json", None, tmp / "ghost.nc.manifest.json")
            with self.assertRaisesRegex(ValueError, "must be the"):
                linked.linked_view(tmp / "s.json", None, tmp / "s.json")
            s = summary(); s["final_surface_state_hash"] = None
            (tmp / "nohash.json").write_text(json.dumps(s), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "required fields"):
                linked.linked_view(tmp / "nohash.json", None, None)


if __name__ == "__main__":
    unittest.main()
