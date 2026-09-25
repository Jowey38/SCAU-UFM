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

    def test_run_without_river_engine_reports_no_native(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = self._setup(tmp)
            view = linked.linked_view(tmp / "s.json", None, None)
            self.assertIsNone(view["dflowfm_native"])
            self.assertNotIn("dflowfm_contract", view)


MDU = b"[model]\nProgram = D-Flow FM\n[geometry]\nNetFile = single_reach_net.nc\n"


def c3_summary(epochs=3, mdu_bytes=MDU, native=True, lateral_per_epoch=0.25, tamper=None):
    """Tri-model summary as the C3 driver writes it: one drainage + one river link,
    a frozen `dflowfm` block, and one cumulative native record per epoch."""
    s = summary(epochs=epochs)
    for i, e in enumerate(s["epochs"]):
        e["link_exchanges"].append({"engine": "river", "node": 5, "node_name": "lat1", "cell": 1,
                                    "v_granted": lateral_per_epoch, "v_repay": 0.0, "v_returned": 0.0})
        e["drained_volume"] += lateral_per_epoch
        if native:
            cum = lateral_per_epoch * (i + 1)
            e["dflowfm_native"] = {"storage_m3": 5500.0 + cum, "boundary_in_m3": 10.0 * (i + 1),
                                   "boundary_out_m3": 12.0 * (i + 1), "api_lateral_in_m3": cum,
                                   "api_lateral_out_m3": 0.0, "volume_error_cumulative_m3": -1e-12}
        else:
            e["dflowfm_native"] = None
    s["total_drained_volume"] += lateral_per_epoch * epochs
    s["total_dflowfm_lateral_volume"] = lateral_per_epoch * epochs
    s["dflowfm"] = {"provider_id": "dflowfm", "capability": "dflowfm.external_net.v1",
                    "mdu_path": "single_reach.mdu", "mdu_hash": fnv1a64(mdu_bytes) if native else "",
                    "native_observed": native,
                    "flux_convention": {"positive_direction": "into_river_domain"},
                    "units": {"volume": "m3", "time": "s"},
                    "boundaries": [{"boundary_id": "lat1", "provider_object_id": 5, "surface_cell": 1,
                                    "exchange_kind": "api_lateral"}]}
    if tamper:
        tamper(s)
    return s


class C3NativeLinkageTests(unittest.TestCase):
    def _write(self, tmp, s, name="s.json"):
        p = Path(tmp) / name
        p.write_text(json.dumps(s), encoding="utf-8")
        return p

    def test_provenance_validated_linkage_and_accounting(self):
        with tempfile.TemporaryDirectory() as tmp:
            mdu = Path(tmp) / "single_reach.mdu"
            mdu.write_bytes(MDU)
            view = linked.linked_view(self._write(tmp, c3_summary()), None, None, mdu)
            self.assertTrue(view["dflowfm_contract_bound"])
            self.assertTrue(view["dflowfm_contract"]["mdu_bound"])
            self.assertEqual(view["dflowfm_contract"]["boundaries"][0]["boundary_id"], "lat1")
            native = view["dflowfm_native"]
            self.assertEqual(native["status"], "provenance_validated")
            self.assertEqual([r["t"] for r in native["series"]], [60.0, 120.0, 180.0])
            acc = native["accounting"]
            self.assertAlmostEqual(acc["native_api_lateral_net_m3"], 0.75)
            self.assertAlmostEqual(acc["ledger_river_net_m3"], 0.75)
            self.assertEqual(acc["diagnostic_code"], "NO_GAP")
            self.assertAlmostEqual(acc["boundary_net_m3"], -6.0)
            self.assertIn("definitions", acc)
            # river link stays ledger-only per node: the native view is aggregate
            river = [l for l in view["links"] if l["engine"] == "river"][0]
            self.assertIsNone(river["engine_native"])
            # without the MDU the series is still validated, but provenance is weaker and SAYS so
            unbound = linked.linked_view(self._write(tmp, c3_summary(), "u.json"), None, None)
            self.assertEqual(unbound["dflowfm_native"]["status"], "series_validated_mdu_unbound")
            self.assertFalse(unbound["dflowfm_contract"]["mdu_bound"])

    def test_gap_is_reported_not_corrected(self):
        def widen(s):
            s["epochs"][-1]["dflowfm_native"]["api_lateral_in_m3"] += 0.1
        with tempfile.TemporaryDirectory() as tmp:
            view = linked.linked_view(self._write(tmp, c3_summary(tamper=widen)), None, None)
            acc = view["dflowfm_native"]["accounting"]
            self.assertEqual(acc["diagnostic_code"], "LATERAL_INTEGRATION_GAP")
            self.assertAlmostEqual(acc["signed_gap_m3"], 0.1)
            self.assertAlmostEqual(acc["ledger_river_net_m3"], 0.75)      # untouched

    def test_mock_river_run_is_ledger_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            view = linked.linked_view(self._write(tmp, c3_summary(native=False)), None, None)
            self.assertEqual(view["dflowfm_native"]["status"], "not_observed")
            mdu = Path(tmp) / "m.mdu"; mdu.write_bytes(MDU)
            with self.assertRaisesRegex(ValueError, "recorded no MDU hash"):
                linked.linked_view(self._write(tmp, c3_summary(native=False), "m.json"), None, None, mdu)

    def test_c3_rejections(self):
        def t(fn):
            return fn
        cases = {
            # C3-05/06: wrong case / wrong run -> the MDU bytes are not this run's
            "tampered mdu": (None, "do not match the hash recorded by this run", b"[model]\nProgram = X\n"),
            # C3-01 identity
            "duplicate boundary id": (t(lambda s: s["dflowfm"]["boundaries"].append(
                {"boundary_id": "lat1", "provider_object_id": 6, "surface_cell": 0, "exchange_kind": "api_lateral"})),
                "boundary_id_duplicate", None),
            "duplicate object": (t(lambda s: s["dflowfm"]["boundaries"].append(
                {"boundary_id": "lat2", "provider_object_id": 5, "surface_cell": 0, "exchange_kind": "api_lateral"})),
                "provider_object_duplicate", None),
            "link not frozen": (t(lambda s: s["dflowfm"]["boundaries"][0].__setitem__("boundary_id", "other")),
                                "is not a frozen boundary", None),
            "link identity drift": (t(lambda s: s["dflowfm"]["boundaries"][0].__setitem__("provider_object_id", 7)),
                                    "differs from frozen boundary", None),
            # C3-02/03: exchange kind / capability / provider
            "exchange kind": (t(lambda s: s["dflowfm"]["boundaries"][0].__setitem__("exchange_kind", "open_boundary")),
                              "exchange_kind_unsupported", None),
            "capability": (t(lambda s: s["dflowfm"].__setitem__("capability", "dflowfm.external_net.v2")),
                           "capability_unsupported", None),
            "provider": (t(lambda s: s["dflowfm"].__setitem__("provider_id", "swmm")), "provider_mismatch", None),
            # C3-04 time / series integrity
            "missing native epoch": (t(lambda s: s["epochs"][1].__setitem__("dflowfm_native", None)),
                                     "native_observation_missing", None),
            "cumulative decreased": (t(lambda s: s["epochs"][2]["dflowfm_native"].__setitem__("boundary_in_m3", 1.0)),
                                     "monotonicity_violated", None),
            "negative gross": (t(lambda s: s["epochs"][0]["dflowfm_native"].__setitem__("api_lateral_in_m3", -1.0)),
                               "native_observation_invalid", None),
            "non-finite": (t(lambda s: s["epochs"][0]["dflowfm_native"].__setitem__("storage_m3", "nan")),
                           "native_observation_invalid", None),
            # C3-07: native scope without input identity
            "native without mdu hash": (t(lambda s: s["dflowfm"].__setitem__("mdu_hash", "")),
                                        "case_identity_missing", None),
            # river links but no contract block (older driver) -> fail closed
            "no contract block": (t(lambda s: s.__setitem__("dflowfm", None)), "no dflowfm contract block", None),
        }
        with tempfile.TemporaryDirectory() as tmp:
            for name, (tamper, pattern, mdu_bytes) in cases.items():
                with self.subTest(name=name):
                    mdu = None
                    if mdu_bytes is not None:
                        mdu = Path(tmp) / f"{name}.mdu"; mdu.write_bytes(mdu_bytes)
                    p = self._write(tmp, c3_summary(tamper=tamper), f"{name}.json")
                    with self.assertRaisesRegex(ValueError, pattern):
                        linked.linked_view(p, None, None, mdu)


if __name__ == "__main__":
    unittest.main()
