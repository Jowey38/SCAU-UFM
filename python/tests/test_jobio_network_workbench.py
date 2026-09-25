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


class ResultsPageHelperTests(unittest.TestCase):
    """P9 thin-shell helpers; the CLI itself is covered by test_results*."""

    def _derived(self, geotiff=True, sampled=True):
        d = {"frames": 4, "cells": 469, "committed_epochs": 3, "threshold_m": 0.01,
             "source_hash": "fnv1a64:aa", "source_stcf_hash": "fnv1a64:bb", "final_surface_state_hash": "fnv1a64:cc",
             "max_depth_m": [0.0, 1.5, 0.2], "arrival_time_s": [None, 60.0, 120.0],
             "series": {"cells": [1, 0], "time_s": [0.0, 60.0],
                        "h": [[0.5, 1.5], [0.0, 0.0]], "eta": [[10.5, 11.5], [10.0, 10.0]],
                        "hu": [[0.0, 1e-3], [0.0, 0.0]], "hv": [[0.0, 0.0], [0.0, 0.0]]},
             "geotiff": {"files": {"max_depth_m": {"path": "/m/max_depth_m.tif"}, "arrival_time_s": {"path": "/m/a.tif"}},
                         "width": 100, "height": 60, "pixel_size_m": 2.0, "crs": None, "crs_source": "undeclared",
                         "georeferenced": False} if geotiff else None,
             "sampled": {"points_xy": [[1.0, 2.0], [3.0, 4.0]], "cells": [1, 0]} if sampled else None}
        return d

    def test_summary_rows_reflect_lamps_and_provenance(self):
        rows = jobio.results_summary_rows({"status": "ok", "derived": self._derived()})
        codes = [r[1] for r in rows]
        self.assertEqual(codes, ["ResultsValidated", "Provenance", "DerivedMaps", "GeoTIFF", "SampledCells"])
        self.assertEqual(rows[3][0], "review")                        # undeclared CRS -> review lamp
        self.assertIn("fnv1a64:cc", rows[1][2])
        self.assertIn("ever wet=2/3", rows[2][2])
        georef = self._derived(); georef["geotiff"].update(crs="EPSG:3395", georeferenced=True, crs_source="pipeline_manifest:x")
        self.assertEqual(jobio.results_summary_rows({"status": "ok", "derived": georef})[3][0], "pass")
        rejected = jobio.results_summary_rows({"status": "fatal", "stderr": "results error: provenance validation failure: sidecar manifest is missing", "derived": None})
        self.assertEqual(rejected, [("fatal", "ResultsRejected", "results error: provenance validation failure: sidecar manifest is missing")])

    def test_series_rows_follow_sampled_order_and_rasters_are_sorted(self):
        d = self._derived()
        rows = jobio.results_series_rows(d)
        self.assertEqual([r[:2] for r in rows], [("0", "1"), ("60", "1"), ("0", "0"), ("60", "0")])
        self.assertEqual(rows[1][2], "1.5000")
        self.assertEqual(jobio.results_series_rows(self._derived(sampled=False) | {"series": None}), [])
        self.assertEqual(jobio.results_raster_paths(d), [("arrival_time_s", "/m/a.tif"), ("max_depth_m", "/m/max_depth_m.tif")])
        self.assertEqual(jobio.results_raster_paths(None), [])

    def test_run_results_fails_closed_before_spawning(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "d.json"
            self.assertEqual(jobio.run_results("/nope.nc", tmp, out)["status"], "repo_root_invalid")
            root = Path(__file__).resolve().parents[2]
            self.assertIn("not found", jobio.run_results(str(Path(tmp) / "nope.nc"), str(root), out)["stderr"])
            nc = Path(tmp) / "r.nc"; nc.write_bytes(b"x"); out.write_text("{}")
            self.assertIn("refusing to overwrite", jobio.run_results(str(nc), str(root), out)["stderr"])
            out2 = Path(tmp) / "d2.json"
            self.assertIn("pixel size", jobio.run_results(str(nc), str(root), out2, geotiff_dir=Path(tmp) / "m")["stderr"])


class LinkedViewHelperTests(unittest.TestCase):
    def test_rows_and_lamps(self):
        gap = {"ledger_in_m3": 380.24, "rpt_lateral_m3": 382.0, "signed_gap_m3": 1.76, "relative_gap": 1.76 / 380.24,
               "diagnostic_code": "ROUTING_GAP_ATTRIBUTION_INSUFFICIENT", "note": "not been reconciled"}
        linked = {"result_manifest_bound": True, "committed_epochs": 3,
                  "swmm_report": {"routing_continuity_error_pct": -6.627, "swmm_version": "5.2 (Build 5.2.4)",
                                  "volume_precision": "3 significant figures", "scope_note": "NOT evidence about any node"},
                  "dflowfm_native": {"status": "not_observed", "note": "run bound no native water balance (mock river)"},
                  "dflowfm_contract": {"provider_id": "dflowfm", "capability": "dflowfm.external_net.v1", "mdu_bound": False,
                                       "boundaries": [{"boundary_id": "lat1", "provider_object_id": 5, "surface_cell": 1}]},
                  "links": [{"engine": "drainage", "node_name": "J1", "cell": 379, "granted_m3": 380.0, "repay_m3": 0.24,
                             "returned_m3": 0.0, "gap": gap,
                             "engine_native": {"lateral_inflow_volume_m3": 382.0, "max_depth_m": 3.0}},
                            {"engine": "river", "node_name": "lat1", "cell": 1, "granted_m3": 1.0, "repay_m3": 0.0,
                             "returned_m3": 0.0, "engine_native": None, "gap": None}]}
        rows = jobio.linked_rows({"status": "ok", "linked": linked})
        self.assertEqual(rows[0], ("drainage", "J1", "379", "380.240", "0.000", "382", "+1.76", "+0.46%",
                                   "ROUTING_GAP_ATTRIBUTION_INSUFFICIENT"))
        self.assertEqual(rows[1], ("river", "lat1", "1", "1.000", "0.000", "-", "-", "-", "-"))
        summary = jobio.linked_summary_rows({"status": "ok", "linked": linked})
        self.assertEqual([s[:2] for s in summary],
                         [("pass", "LinkedView"), ("pass", "SwmmReportBound"), ("review", "SwmmRoutingContinuity"),
                          ("review", "ROUTING_GAP_ATTRIBUTION_INSUFFICIENT"), ("pass", "C3Contract"),
                          ("info", "DFlowNative")])
        self.assertIn("lat1->obj 5 cell 1", summary[4][2])
        self.assertIn("no reconciliation applied", summary[3][2])
        self.assertIn("NOT evidence", summary[2][2])
        # a gap inside report rounding produces no per-link warning row
        linked["links"][0]["gap"] = dict(gap, diagnostic_code="NO_GAP")
        codes = [s[1] for s in jobio.linked_summary_rows({"status": "ok", "linked": linked})]
        self.assertNotIn("ROUTING_GAP_ATTRIBUTION_INSUFFICIENT", codes)
        unbound = dict(linked, result_manifest_bound=False)
        self.assertEqual(jobio.linked_summary_rows({"status": "ok", "linked": unbound})[0][0], "review")
        self.assertEqual(jobio.linked_summary_rows({"status": "fatal", "stderr": "results error: x", "linked": None}),
                         [("fatal", "LinkedViewRejected", "results error: x")])
        self.assertEqual(jobio.linked_rows({"status": "fatal", "linked": None}), [])

    def test_c3_native_rows(self):
        self.assertEqual(jobio.dflowfm_linked_rows({"dflowfm_native": None}),
                         [("info", "DFlowNative", "river engine not part of this run")])
        acc = {"storage_final_m3": 5500.75, "boundary_in_m3": 30.0, "boundary_out_m3": 36.0,
               "native_api_lateral_net_m3": 0.75, "ledger_river_net_m3": 0.75, "signed_gap_m3": 0.0,
               "diagnostic_code": "NO_GAP", "note": "native and ledger agree to fp precision"}
        linked = {"dflowfm_contract": {"provider_id": "dflowfm", "capability": "dflowfm.external_net.v1",
                                       "mdu_bound": True, "boundaries": [{"boundary_id": "lat1", "provider_object_id": 0,
                                                                          "surface_cell": 1}]},
                  "dflowfm_native": {"status": "provenance_validated", "series": [{}, {}, {}], "accounting": acc}}
        rows = jobio.dflowfm_linked_rows(linked)
        self.assertEqual([r[:2] for r in rows],
                         [("pass", "C3Contract"), ("pass", "DFlowNative"), ("pass", "DFlowLateralAccounting:NO_GAP")])
        self.assertIn("bound by bytes", rows[0][2])
        self.assertIn("3 native epochs", rows[1][2])
        # weaker provenance and a gap are both surfaced as review, never hidden
        linked["dflowfm_contract"]["mdu_bound"] = False
        linked["dflowfm_native"]["status"] = "series_validated_mdu_unbound"
        acc.update(diagnostic_code="LATERAL_INTEGRATION_GAP", signed_gap_m3=0.1, note="nothing corrected")
        rows = jobio.dflowfm_linked_rows(linked)
        self.assertEqual([r[0] for r in rows], ["pass", "review", "review"])
        self.assertIn("file not given", rows[0][2])
        self.assertIn("nothing corrected", rows[2][2])

    def test_model_crs_for_result_uses_hash_verified_manifest_only(self):
        root = Path(__file__).resolve().parents[2]
        golden = root / "tests/golden/preproc_crs_governance_case/cases/synthetic_city_block_epsg3395.stcf.nc"
        synthetic = root / "tests/golden/case_export_synthetic_package/cases/mesh/case.stcf.nc"
        with tempfile.TemporaryDirectory() as tmp:
            nc = Path(tmp) / "r.nc"
            self.assertEqual(jobio.model_crs_for_result(str(nc)), (None, "result manifest missing"))
            Path(str(nc) + ".manifest.json").write_text(json.dumps({"source_stcf": str(golden)}), encoding="utf-8")
            crs, why = jobio.model_crs_for_result(str(nc))
            self.assertEqual(crs, "EPSG:3395")
            self.assertTrue(why.startswith("pipeline_manifest:"))
            Path(str(nc) + ".manifest.json").write_text(json.dumps({"source_stcf": str(synthetic)}), encoding="utf-8")
            crs, why = jobio.model_crs_for_result(str(nc))
            self.assertIsNone(crs)                       # that package has crs_governance: null
            self.assertIn("no CRS governance", why)
            Path(str(nc) + ".manifest.json").write_text(json.dumps({"source_stcf": str(Path(tmp) / "nope.stcf.nc")}), encoding="utf-8")
            self.assertEqual(jobio.model_crs_for_result(str(nc))[0], None)

    def test_run_linked_view_fails_closed_before_spawning(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "l.json"
            root = Path(__file__).resolve().parents[2]
            self.assertEqual(jobio.run_linked_view("/x.json", tmp, out)["status"], "repo_root_invalid")
            self.assertIn("not found", jobio.run_linked_view(str(Path(tmp) / "x.json"), str(root), out)["stderr"])
