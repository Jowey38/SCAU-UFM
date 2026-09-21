import json
import tempfile
import unittest
from pathlib import Path

from scau_results import linked

RPT = """
  EPA STORM WATER MANAGEMENT MODEL - VERSION 5.2 (Build 5.2.4)

  Continuity Error (%) .....        -6.627

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


def summary(outcome="completed", links=True, cell_j1=379):
    def link(cell):
        return {"engine": "drainage", "node": 0, "node_name": "J1", "cell": cell,
                "v_granted": 100.0, "v_repay": 0.5, "v_returned": 2.0}
    epochs = [{"logical_time": 60.0 * (i + 1), **({"link_exchanges": [link(cell_j1 if i < 2 else cell_j1)]} if links else {})}
              for i in range(3)]
    return {"outcome": outcome, "committed_epochs": 3, "final_surface_state_hash": "fnv1a64:abc", "epochs": epochs}


class LinkedViewTests(unittest.TestCase):
    def test_rpt_parser_reads_both_tables_and_units(self):
        with tempfile.TemporaryDirectory() as tmp:
            rpt = Path(tmp) / "r.rpt"
            rpt.write_text(RPT, encoding="utf-8")
            report = linked.parse_swmm_report(rpt)
            self.assertEqual(report["routing_continuity_error_pct"], -6.627)
            self.assertEqual(report["nodes"]["J1"]["max_depth_m"], 3.0)
            self.assertEqual(report["nodes"]["J1"]["lateral_inflow_volume_m3"], 382.0)   # 0.382 x 10^6 L
            self.assertEqual(report["nodes"]["O1"]["lateral_inflow_volume_m3"], 0.0)
            self.assertEqual(report["nodes"]["O1"]["flow_balance_error_pct"], 0.0)
            (Path(tmp) / "bad.rpt").write_text("not swmm", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "not a SWMM report"):
                linked.parse_swmm_report(Path(tmp) / "bad.rpt")

    def test_ledger_aggregates_per_link_and_joins_engine_native(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "s.json").write_text(json.dumps(summary()), encoding="utf-8")
            (tmp / "r.rpt").write_text(RPT, encoding="utf-8")
            (tmp / "m.json").write_text(json.dumps({"final_surface_state_hash": "fnv1a64:abc"}), encoding="utf-8")
            view = linked.linked_view(tmp / "s.json", tmp / "r.rpt", tmp / "m.json")
            self.assertTrue(view["result_manifest_bound"])
            (j1,) = view["links"]
            self.assertEqual((j1["granted_m3"], j1["repay_m3"], j1["returned_m3"]), (300.0, 1.5, 6.0))
            self.assertEqual([s["t"] for s in j1["series"]], [60.0, 120.0, 180.0])
            self.assertAlmostEqual(j1["lateral_gap_m3"], 382.0 - 301.5)
            self.assertEqual(j1["engine_native"]["max_depth_m"], 3.0)
            self.assertIn("C3", view["dflowfm_native"])
            self.assertIsNone(linked.linked_view(tmp / "s.json", None, None)["links"][0]["engine_native"])

    def test_fail_closed_on_wrong_run_missing_node_or_old_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "r.rpt").write_text(RPT.replace("J1  ", "J9  "), encoding="utf-8")
            (tmp / "s.json").write_text(json.dumps(summary()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "no node 'J1'"):
                linked.linked_view(tmp / "s.json", tmp / "r.rpt", None)
            (tmp / "wrong.json").write_text(json.dumps({"final_surface_state_hash": "fnv1a64:zzz"}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "different run"):
                linked.linked_view(tmp / "s.json", None, tmp / "wrong.json")
            (tmp / "old.json").write_text(json.dumps(summary(links=False)), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "no link_exchanges"):
                linked.load_link_ledger(tmp / "old.json")
            (tmp / "rev.json").write_text(json.dumps(summary(outcome="review_required")), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "review_required"):
                linked.load_link_ledger(tmp / "rev.json")
            moved = summary(); moved["epochs"][2]["link_exchanges"][0]["cell"] = 7
            (tmp / "moved.json").write_text(json.dumps(moved), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "changed cell"):
                linked.load_link_ledger(tmp / "moved.json")


if __name__ == "__main__":
    unittest.main()
