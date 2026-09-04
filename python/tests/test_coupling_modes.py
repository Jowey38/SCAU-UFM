"""Headless tests for coupling_maps mapping modes (M287-C5).

Uses a hand-built 2x2 quad mesh written with netCDF4 (no gmsh) and a tiny
synthetic package so the mode logic is exercised without the full pipeline.
"""

from __future__ import annotations

import csv
import json
import tempfile
import unittest
from pathlib import Path

from scau_preproc import coupling_maps as cm

try:
    import numpy as np
    from netCDF4 import Dataset
    HAVE_NETCDF = True
except ImportError:  # pragma: no cover
    HAVE_NETCDF = False


def write_mesh(path: Path) -> None:
    """2x2 quads over [0,20]x[0,20]; cell k covers column k%2, row k//2. Cell
    z_b = 10 + k so the placeholder crest is recognisable."""
    xs = [0.0, 10.0, 20.0] * 3
    ys = [0.0] * 3 + [10.0] * 3 + [20.0] * 3
    faces = [[0, 1, 4, 3], [1, 2, 5, 4], [3, 4, 7, 6], [4, 5, 8, 7]]
    ds = Dataset(path, "w", format="NETCDF3_CLASSIC")
    ds.createDimension("cell", 4)
    ds.createDimension("nMesh2_node", 9)
    ds.createDimension("nMesh2_max_face_nodes", 4)
    ds.createVariable("mesh2_node_x", "f8", ("nMesh2_node",))[:] = np.array(xs)
    ds.createVariable("mesh2_node_y", "f8", ("nMesh2_node",))[:] = np.array(ys)
    ds.createVariable("mesh2_face_nodes", "i4", ("cell", "nMesh2_max_face_nodes"),
                      fill_value=np.int32(-1))[:] = np.array(faces, dtype="i4")
    ds.createVariable("z_b", "f8", ("cell",))[:] = np.array([10.0, 11.0, 12.0, 13.0])
    ds.close()


def write_package(pkg: Path, *, surface_rows=None, roof_rows=None, nodes=None, buildings=None) -> None:
    (pkg / "swmm").mkdir(parents=True)
    (pkg / "buildings").mkdir()
    (pkg / "boundary").mkdir()
    (pkg / "dflowfm").mkdir()
    nodes = nodes or {"J1": ("junction", 5, 5), "J2": ("junction", 15, 15), "O1": ("outfall", 15, 5)}
    inp = ["[JUNCTIONS]"] + [f"{n} 10 3 0 0 0" for n, (k, _, _) in nodes.items() if k == "junction"]
    inp += ["[OUTFALLS]"] + [f"{n} 9 FREE  NO" for n, (k, _, _) in nodes.items() if k == "outfall"]
    inp += ["[COORDINATES]"] + [f"{n} {x} {y}" for n, (_, x, y) in nodes.items()]
    (pkg / "swmm/model.inp").write_text("\n".join(inp) + "\n", encoding="utf-8")
    buildings = buildings if buildings is not None else {"B1": [[2, 12], [8, 12], [8, 18], [2, 18], [2, 12]]}
    (pkg / "buildings/buildings.geojson").write_text(json.dumps({"type": "FeatureCollection", "features": [
        {"type": "Feature", "properties": {"building_id": b}, "geometry": {"type": "Polygon", "coordinates": [ring]}}
        for b, ring in buildings.items()]}), encoding="utf-8")
    (pkg / "boundary/computational_boundary.geojson").write_text(json.dumps({"type": "FeatureCollection", "features": [
        {"type": "Feature", "properties": {"boundary_id": "B"},
         "geometry": {"type": "Polygon", "coordinates": [[[0, 0], [20, 0], [20, 20], [0, 20], [0, 0]]]}}]}), encoding="utf-8")
    (pkg / "dflowfm/boundary_mapping.csv").write_text("mapping_id,x\nM,TODO_PROVIDER\n", encoding="utf-8")
    if surface_rows is not None:
        with (pkg / "swmm/node_surface_mapping.csv").open("w", newline="", encoding="utf-8") as h:
            w = csv.DictWriter(h, fieldnames=["mapping_id", "surface_zone_id", "inlet_id", "swmm_node_id",
                                              "exchange_elevation_m", "review_status"])
            w.writeheader(); w.writerows(surface_rows)
    if roof_rows is not None:
        with (pkg / "swmm/roof_drain_mapping.csv").open("w", newline="", encoding="utf-8") as h:
            w = csv.DictWriter(h, fieldnames=["mapping_id", "building_id", "roof_drain_id", "swmm_node_id",
                                              "catchment_area_m2", "exchange_elevation_m"])
            w.writeheader(); w.writerows(roof_rows)


@unittest.skipUnless(HAVE_NETCDF, "netCDF4/numpy not available")
class ModeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.mesh = self.root / "case.stcf.nc"
        write_mesh(self.mesh)

    def tearDown(self):
        self.tmp.cleanup()

    def run_mode(self, mode, pkg_name="pkg", **package_kwargs):
        pkg = self.root / pkg_name
        write_package(pkg, **package_kwargs)
        out = self.root / f"out_{pkg_name}_{mode}"
        report = cm.generate(pkg, self.mesh, out, mode=mode)
        surf = json.loads((out / "surface_swmm_mapping.json").read_text())["relations"]
        roof = json.loads((out / "roof_drain_mapping.json").read_text())["relations"]
        conf = (out / "simdriver_links.conf").read_text()
        return report, surf, roof, conf

    def test_spatial_candidates_junctions_only_review_with_placeholder_crest(self):
        report, surf, roof, conf = self.run_mode("spatial_candidates")
        self.assertEqual([r["swmm_node_id"] for r in surf], ["J1", "J2"])  # O1 outfall skipped
        self.assertEqual([r["cell_index"] for r in surf], [0, 3])
        self.assertEqual([r["exchange_elevation_m"] for r in surf], [10.0, 13.0])  # cell z_b
        self.assertTrue(all(r["confidence"] == "review" and r["review_status"] == "needs_confirmation"
                            and r["exchange_elevation_source"] == "placeholder:cell_z_b" for r in surf))
        self.assertEqual(report["mode"], "spatial_candidates")
        self.assertEqual(report["chains"]["surface_to_swmm"]["by_confidence"], {"high": 0, "review": 2})
        self.assertIn("cell=3,node=J2,crest=13.0", conf)
        self.assertEqual(len(roof), 1)
        self.assertEqual(roof[0]["swmm_node_id"], "J1")  # nearest junction to centroid (5,15): J1 (10) vs J2 (10) -> tie -> J1 by name
        self.assertEqual(roof[0]["catchment_area_m2"], 36.0)
        self.assertEqual(roof[0]["mapping_id"], "SPATIAL_ROOF_B1")

    def test_explicit_mode_requires_tables(self):
        with self.assertRaises(SystemExit):
            self.run_mode("explicit_ids")

    def test_mixed_mode_partial_table(self):
        rows = [{"mapping_id": "MAP_1", "surface_zone_id": "Z", "inlet_id": "I", "swmm_node_id": "J1",
                 "exchange_elevation_m": 9.0, "review_status": "needs_confirmation"}]
        report, surf, roof, _ = self.run_mode("mixed", surface_rows=rows)
        by_node = {r["swmm_node_id"]: r for r in surf}
        self.assertEqual(by_node["J1"]["confidence"], "high")
        self.assertEqual(by_node["J1"]["exchange_elevation_m"], 9.0)
        self.assertEqual(by_node["J2"]["confidence"], "review")
        self.assertEqual(by_node["J2"]["method"], "spatial_point_in_cell_candidate")
        self.assertEqual(report["chains"]["surface_to_swmm"]["by_confidence"], {"high": 1, "review": 1})
        self.assertEqual(report["spatial_candidates"], {"surface_nodes": ["J2"], "roof_buildings": ["B1"]})

    def test_node_in_hole_is_fatal_and_outside_is_review(self):
        with self.assertRaises(SystemExit):
            self.run_mode("spatial_candidates", pkg_name="hole",
                          nodes={"J1": ("junction", 5, 15)})  # inside B1 footprint
        report, surf, _, _ = self.run_mode("spatial_candidates", pkg_name="outside",
                                           nodes={"J1": ("junction", 5, 5), "J9": ("junction", 40, 40)})
        self.assertEqual([r["swmm_node_id"] for r in surf], ["J1"])
        self.assertEqual([f["code"] for f in report["findings"]], ["SwmmNodeOutsideSurfaceDomain"])

    def test_roof_distance_threshold(self):
        pkg = self.root / "far"
        write_package(pkg)
        out = self.root / "out_far"
        report = cm.generate(pkg, self.mesh, out, mode="spatial_candidates", roof_node_max_distance_m=5.0)
        self.assertEqual(json.loads((out / "roof_drain_mapping.json").read_text())["relations"], [])
        self.assertEqual([f["code"] for f in report["findings"]], ["RoofNoJunctionWithinDistance"])
        with self.assertRaises(SystemExit):
            cm.generate(pkg, self.mesh, out, mode="spatial_candidates", roof_node_max_distance_m=0.0)
        with self.assertRaises(SystemExit):
            cm.generate(pkg, self.mesh, out, mode="nearest_wins")


if __name__ == "__main__":
    unittest.main()
