"""Headless unit tests for scau_preproc.mesh_controls (no gmsh / netCDF).

Run from the repository root: py -3 -m unittest discover -s python/tests -t python
"""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scau_preproc import mesh_controls as mc

BOUNDARY = [(0.0, 0.0), (200.0, 0.0), (200.0, 120.0), (0.0, 120.0), (0.0, 0.0)]
HOLES = [
    [(40.0, 40.0), (70.0, 40.0), (70.0, 70.0), (40.0, 70.0), (40.0, 40.0)],
    [(120.0, 40.0), (150.0, 40.0), (150.0, 70.0), (120.0, 70.0), (120.0, 40.0)],
]
LC = 8.0


def breakline(cid, coords, **props):
    return {"control_id": cid, "control_kind": "breakline", "points": [tuple(c) for c in coords],
            "size_m": props.get("size_m"), "dist_max_m": props.get("dist_max_m"), "properties": props}


def region(cid, coords, size_m):
    return {"control_id": cid, "control_kind": "refinement_region",
            "points": [tuple(c) for c in coords], "size_m": size_m, "dist_max_m": None,
            "properties": {}}


SQUARE = [(78, 72), (118, 72), (118, 108), (78, 108), (78, 72)]


class LoadTests(unittest.TestCase):
    def _write(self, payload: dict) -> Path:
        handle = tempfile.NamedTemporaryFile("w", suffix=".geojson", delete=False, encoding="utf-8")
        json.dump(payload, handle)
        handle.close()
        self.addCleanup(lambda: Path(handle.name).unlink(missing_ok=True))
        return Path(handle.name)

    def test_schema_version_is_enforced(self):
        path = self._write({"type": "FeatureCollection", "mesh_controls_schema_version": 2,
                            "features": []})
        with self.assertRaises(mc.MeshControlError) as ctx:
            mc.load_mesh_controls(path, None, None)
        self.assertEqual(ctx.exception.violations[0]["violation"],
                         "unsupported_mesh_controls_schema_version")

    def test_defaults_fill_missing_sizes_and_geometry_type_is_checked(self):
        path = self._write({
            "type": "FeatureCollection", "mesh_controls_schema_version": 1,
            "features": [
                {"type": "Feature", "properties": {"control_id": "A", "control_kind": "breakline"},
                 "geometry": {"type": "LineString", "coordinates": [[10, 20], [190, 20]]}},
                {"type": "Feature", "properties": {"control_id": "B", "control_kind": "breakline"},
                 "geometry": {"type": "Polygon", "coordinates": [SQUARE]}},
            ]})
        with self.assertRaises(mc.MeshControlError) as ctx:
            mc.load_mesh_controls(path, 3.0, 12.0)
        self.assertEqual([v["control_id"] for v in ctx.exception.violations], ["B"])
        self.assertEqual(ctx.exception.violations[0]["violation"], "geometry_type_mismatch")

    def test_duplicate_ids_rejected(self):
        feature = {"type": "Feature", "properties": {"control_id": "A", "control_kind": "breakline"},
                   "geometry": {"type": "LineString", "coordinates": [[10, 20], [190, 20]]}}
        path = self._write({"type": "FeatureCollection", "mesh_controls_schema_version": 1,
                            "features": [feature, feature]})
        with self.assertRaises(mc.MeshControlError) as ctx:
            mc.load_mesh_controls(path, None, None)
        self.assertEqual(ctx.exception.violations[0]["violation"], "duplicate_control_id")


class ValidateTests(unittest.TestCase):
    def violations(self, controls):
        with self.assertRaises(mc.MeshControlError) as ctx:
            mc.validate_mesh_controls(controls, BOUNDARY, HOLES, LC)
        return {v["violation"] for v in ctx.exception.violations}

    def test_nominal_ownership(self):
        controls = [
            breakline("BL", [(10, 20), (190, 20)], size_m=3.0, dist_max_m=12.0),
            breakline("BL_IN", [(85, 78), (112, 104)]),
            region("RR_POND", SQUARE, 2.5),
            region("RR_B1", [(30, 30), (76, 30), (76, 76), (30, 76), (30, 30)], 4.0),
        ]
        ownership = mc.validate_mesh_controls(controls, BOUNDARY, HOLES, LC)
        self.assertEqual(ownership["hole_region"], [1, None])
        self.assertEqual(ownership["breakline_region"], [None, 0])

    def test_boundary_touch_and_hole_cross_rejected(self):
        self.assertIn("vertex_not_strictly_inside_boundary",
                      self.violations([breakline("BL", [(0, 60), (100, 60)])]))
        self.assertIn("crosses_building_hole",
                      self.violations([breakline("BL", [(20, 55), (100, 55)])]))

    def test_region_rules(self):
        self.assertIn("building_straddles_region_boundary", self.violations(
            [region("RR", [(30, 30), (55, 30), (55, 80), (30, 80), (30, 30)], 4.0)]))
        self.assertIn("size_m_out_of_range", self.violations([region("RR", SQUARE, 9.0)]))
        self.assertIn("refinement_regions_overlap", self.violations([
            region("A", SQUARE, 2.5),
            region("B", [(100, 90), (130, 90), (130, 115), (100, 115), (100, 90)], 2.5)]))
        self.assertIn("unclosed_ring", self.violations([region("RR", SQUARE[:-1], 2.5)]))

    def test_breakline_rules(self):
        self.assertIn("dist_max_m_required_positive",
                      self.violations([breakline("BL", [(10, 20), (190, 20)], size_m=3.0)]))
        self.assertIn("degenerate_polyline", self.violations([breakline("BL", [(10, 20), (10, 20)])]))
        self.assertIn("self_intersection", self.violations(
            [breakline("BL", [(10, 90), (60, 110), (60, 90), (10, 110)])]))
        self.assertIn("breakline_crosses_region_boundary", self.violations(
            [region("RR", SQUARE, 2.5), breakline("BL", [(60, 90), (100, 90)])]))


class PostMeshTests(unittest.TestCase):
    def test_breakline_preserved_by_edge_chain(self):
        # nodes 0-1-2 lie on the segment (0,0)-(2,0); 3 is off the line.
        node_x, node_y = [0.0, 1.0, 2.0, 1.0], [0.0, 0.0, 0.0, 1.0]
        controls = [breakline("BL", [(0.0, 0.0), (2.0, 0.0)])]
        good = [(0, 1), (1, 2), (0, 3), (1, 3), (2, 3)]
        reports = mc.assert_breaklines_preserved(controls, node_x, node_y, good)
        self.assertEqual(reports[0]["mesh_edges_on_breakline"], 2)
        with self.assertRaises(mc.MeshControlError):
            mc.assert_breaklines_preserved(controls, node_x, node_y, [(0, 3), (1, 3), (2, 3), (0, 2)])

    def test_cell_diagnostics_square_is_ideal(self):
        node_x, node_y = [0.0, 1.0, 1.0, 0.0, 2.0, 2.0], [0.0, 0.0, 1.0, 1.0, 0.0, 1.0]
        faces = [[0, 1, 2, 3], [1, 4, 5, 2]]
        edge_nodes = [[0, 1], [1, 2], [2, 3], [3, 0], [1, 4], [4, 5], [5, 2]]
        edge_faces = [[0, -1], [0, 1], [0, -1], [0, -1], [1, -1], [1, -1], [1, -1]]
        rows = mc.cell_diagnostics(node_x, node_y, faces, edge_nodes, edge_faces, ["R", None])
        self.assertEqual(rows[0]["equiangle_skewness"], 0.0)
        self.assertEqual(rows[0]["nonorthogonality_deg"], 0.0)
        self.assertEqual(rows[0]["min_angle_deg"], 90.0)
        self.assertEqual(rows[0]["refinement_region"], "R")
        self.assertIsNone(rows[1]["refinement_region"])


if __name__ == "__main__":
    unittest.main()
