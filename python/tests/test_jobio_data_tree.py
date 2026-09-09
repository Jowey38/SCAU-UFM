"""Headless tests for the E2 data tree + E5 report browser pure layer."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "jobio_e2", ROOT / "qgis_plugin" / "scau_preproc_workbench" / "jobio.py")
jobio = importlib.util.module_from_spec(_spec)
sys.modules["jobio_e2"] = jobio
_spec.loader.exec_module(jobio)

SAMPLE = ROOT / "samples/d5_gis_preproc_template"


class DataTreeTests(unittest.TestCase):
    def test_sample_package_tree_groups_and_lamps(self):
        with tempfile.TemporaryDirectory() as tmp:
            tree = jobio.data_tree({"package": str(SAMPLE), "output_dir": tmp})
        groups = {g["group"]: {i["id"]: i for i in g["items"]} for g in tree}
        self.assertEqual(list(groups)[:5], ["Terrain", "Land Cover / Soil", "Geometries", "1D Networks", "Policies"])
        self.assertEqual(groups["Outputs"]["validation"]["lamp"], "missing")   # empty output dir
        self.assertEqual(groups["Terrain"]["dem"]["lamp"], "review")          # TODO_PROVIDER CRS
        self.assertEqual(groups["1D Networks"]["swmm"]["lamp"], "pass")
        self.assertEqual(groups["1D Networks"]["dflowfm"]["lamp"], "review")  # provider_required
        self.assertTrue(groups["Geometries"]["buildings"]["layer"].endswith("buildings.geojson"))
        self.assertEqual(jobio.data_tree_summary(tree)["fatal"], 0)

    def test_missing_required_is_fatal_and_findings_colour_layers(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"; (pkg / "buildings").mkdir(parents=True)
            (pkg / "manifest.json").write_text(json.dumps({"target_crs": "EPSG:32650", "horizontal_units": "metre", "datasets": [
                {"id": "dem", "path": "terrain/dem.asc", "kind": "raster_dem", "required": True},
                {"id": "buildings", "path": "buildings/buildings.geojson", "kind": "vector_gis", "required": True},
                {"id": "soil_zones", "path": "soil/soil_zones.geojson", "kind": "vector_gis", "required": False}]}), encoding="utf-8")
            (pkg / "buildings/buildings.geojson").write_text("{}", encoding="utf-8")
            out = Path(tmp) / "out"; out.mkdir()
            (out / "validation.json").write_text(json.dumps({"status": "fatal", "findings": [
                {"severity": "fatal", "code": "MeshGenerationFailed", "objects": [{"feature_id": "B1", "kind": "building", "violation": "self_intersection"}]}]}), encoding="utf-8")
            tree = jobio.data_tree({"package": str(pkg), "output_dir": str(out)})
        items = {i["id"]: i for g in tree for i in g["items"]}
        self.assertEqual(items["dem"]["lamp"], "fatal")
        self.assertEqual(items["soil_zones"]["lamp"], "missing")
        self.assertEqual(items["buildings"]["lamp"], "fatal")            # finding references a building
        self.assertEqual(items["validation"]["lamp"], "fatal")

    def test_geographic_crs_is_fatal(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"; (pkg / "terrain").mkdir(parents=True)
            (pkg / "manifest.json").write_text(json.dumps({"target_crs": "EPSG:4326", "horizontal_units": "degree", "datasets": [
                {"id": "dem", "path": "terrain/dem.asc", "kind": "raster_dem", "required": True}]}), encoding="utf-8")
            (pkg / "terrain/dem.asc").write_text("x", encoding="utf-8")
            tree = jobio.data_tree({"package": str(pkg), "output_dir": ""})
        self.assertEqual(tree[0]["items"][0]["lamp"], "fatal")


class ReportBrowserTests(unittest.TestCase):
    def test_pages_and_findings_and_locators(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "out"; (out / "coupling/editor").mkdir(parents=True)
            validation = {"status": "review", "package": str(SAMPLE), "policy_audit": {"policy_version": "0.1.0"},
                          "findings": [{"severity": "review", "code": "X", "detail": "d",
                                        "objects": [{"feature_id": "MAP_1", "kind": "coupling:surface_to_swmm", "violation": "v"},
                                                    {"feature_id": "J9", "kind": "swmm:junction", "violation": "v"}]},
                                       {"severity": "fatal", "code": "Y", "detail": "no objects"}]}
            (out / "validation.json").write_text(json.dumps(validation), encoding="utf-8")
            (out / "coupling/editor/links.geojson").write_text("{}", encoding="utf-8")
            job = {"package": str(SAMPLE), "output_dir": str(out),
                   "mesh_controls": {"geojson": str(SAMPLE / "mesh_controls/mesh_controls.geojson")}}
            pages = jobio.report_pages(job)
            self.assertEqual(list(pages), list(jobio.REPORT_PAGES))
            self.assertEqual(dict(pages["import"])["status"], "review")
            self.assertEqual(dict(pages["import"])["policy_version"], "0.1.0")
            rows = jobio.findings_table(validation)
            self.assertEqual([(r["code"], r["feature_id"]) for r in rows], [("X", "MAP_1"), ("X", "J9"), ("Y", None)])
            self.assertEqual(jobio.locate_object(job, "coupling:surface_to_swmm", "MAP_1")[0], "scau_coupling_links")
            self.assertIsNone(jobio.locate_object(job, "swmm:junction", "J9"))      # no nodes layer yet
            self.assertEqual(jobio.locate_object(job, "mesh_control:breakline", "BL_1")[2], "\"control_id\" = 'BL_1'")
            self.assertEqual(jobio.locate_object(job, "building", "B'1")[2], "\"building_id\" = 'B''1'")
            self.assertIsNone(jobio.locate_object(job, None, "x"))


class TerrainConditionRowsTests(unittest.TestCase):
    """B3: the data tree / report browser surface the terrain stage from validation.json."""

    def test_terrain_lamp_report_rows_and_job_config_block(self):
        job = jobio.build_job_config(str(SAMPLE), "out", terrain_policy="metadata/terrain_condition_policy.json")
        self.assertEqual(job["terrain_condition"], {"policy": str(Path("metadata/terrain_condition_policy.json"))})
        self.assertNotIn("terrain_condition", jobio.build_job_config(str(SAMPLE), "out"))
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "conditioned_terrain").mkdir()
            (out / "conditioned_terrain/depression_depth.asc").write_text("ncols 1\n", encoding="utf-8")
            validation = {"status": "ok", "findings": [], "terrain_condition": {
                "policy": "p.json", "enabled": True, "active_operations": ["fill_depressions"],
                "authorization": {"authorized_by": "x", "date": "2026-09-09"},
                "operations": {"fill_depressions": {"algorithm": "priority_flood", "algorithm_version": 1,
                                                    "epsilon_m": 0.0, "cells_filled": 16, "max_fill_depth_m": 0.52}},
                "change_summary": {"cells_changed": 35, "max_abs_change_m": 0.52, "net_change_m3": 60.0},
                "conditioned_dem_sha256": "abc"}}
            (out / "validation.json").write_text(json.dumps(validation), encoding="utf-8")
            job = {"package": str(SAMPLE), "output_dir": str(out)}
            tree = jobio.data_tree(job, validation)
            groups = {g["group"]: {i["id"]: i for i in g["items"]} for g in tree}
            dem = groups["Terrain"]["dem"]
            self.assertEqual(dem["lamp"], "review")
            self.assertIn("conditioned (fill_depressions): 35 cells changed", dem["detail"])
            raster = groups["Outputs"]["conditioned_terrain/depression_depth.asc"]
            self.assertEqual(raster["lamp"], "review")
            self.assertTrue(raster["layer"].endswith("depression_depth.asc"))
            rows = dict(jobio.report_pages(job)["terrain_mesh"])
            self.assertEqual(rows["terrain_enabled"], "True")
            self.assertIn("priority_flood v1", rows["terrain_fill"])
            self.assertIn("cells_changed=35", rows["terrain_change"])
            # Without the stage the page says so and the DEM lamp is the CRS lamp.
            tree = jobio.data_tree(job, {"status": "ok", "findings": []})
            groups = {g["group"]: {i["id"]: i for i in g["items"]} for g in tree}
            self.assertNotIn("conditioned", groups["Terrain"]["dem"]["detail"])
            (out / "validation.json").write_text(json.dumps({"status": "ok", "findings": []}), encoding="utf-8")
            self.assertEqual(dict(jobio.report_pages(job)["terrain_mesh"])["terrain_policy"], "(none: DEM sampled verbatim)")


if __name__ == "__main__":
    unittest.main()
