"""Headless unit tests for scau_preproc.field_derivation (M287-B5)."""

from __future__ import annotations

import json
import math
import tempfile
import unittest
from pathlib import Path

from scau_preproc import field_derivation as fd

GOOD_TABLE = {
    "dpm_rule_table_schema_version": 1,
    "approval": {"status": "synthetic_unapproved"},
    "unmapped_class": "fatal",
    "classes": {"road": {"phi_t": 0.95, "Phi_c": {"xx": 0.95, "xy": 0.0, "yy": 0.8}},
                "grass": {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "xy": 0.0, "yy": 1.0}}},
    "edges": {"default_omega": 1.0, "interfaces": [{"classes": ["road", "grass"], "omega_edge": 0.9}]},
    "soil": {"source": "soil_zones", "missing_zone": {"soil_type": 0}},
}


def write_table(table: dict) -> Path:
    handle = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8")
    json.dump(table, handle)
    handle.close()
    return Path(handle.name)


def poly(props, ring):
    return {"id": "p", "outer": [tuple(map(float, xy)) for xy in ring], "properties": props}


class RuleTableTests(unittest.TestCase):
    def test_good_table_loads_and_normalizes(self):
        rules = fd.load_rule_table(write_table(GOOD_TABLE), package_is_synthetic=True)
        self.assertEqual(rules["classes"]["road"]["Phi_c"]["yy"], 0.8)
        self.assertEqual(rules["edges"]["interfaces"][frozenset({"road", "grass"})], 0.9)
        self.assertEqual(rules["soil"]["missing_zone"], {"soil_type": 0})

    def test_unapproved_table_rejected_for_real_package(self):
        with self.assertRaises(fd.FieldDerivationError):
            fd.load_rule_table(write_table(GOOD_TABLE), package_is_synthetic=False)
        approved = dict(GOOD_TABLE, approval={"status": "approved", "approved_by": "owner", "date": "2026-09-07"})
        self.assertEqual(fd.load_rule_table(write_table(approved), package_is_synthetic=False)["approval"]["status"], "approved")
        with self.assertRaises(fd.FieldDerivationError):  # approved without who/when
            fd.load_rule_table(write_table(dict(GOOD_TABLE, approval={"status": "approved"})), package_is_synthetic=False)

    def test_lint_catches_every_closure_violation(self):
        limits = fd.DEFAULT_CLOSURE_LIMITS
        bad = [
            {"phi_t": 1.2, "Phi_c": {"xx": 1, "xy": 0, "yy": 1}},          # phi_t > 1
            {"phi_t": 0.5, "Phi_c": {"xx": 0.9, "xy": 0, "yy": 0.9}},      # phi_t < max diag
            {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "xy": 0.0, "yy": 1.0e-9}},  # lambda_min < eps
            {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "xy": 0.5, "yy": 1.0}},    # lambda_max 1.5 > 1
            {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "xy": 0.0, "yy": 1.0e-5}}, # cond 1e5 > cond_max
            {"phi_t": 1.0, "Phi_c": {"xx": -1.0, "xy": 0.0, "yy": 1.0}},   # diagonal <= 0
            {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "yy": 1.0}},               # missing xy
        ]
        for entry in bad:
            with self.assertRaises(fd.FieldDerivationError, msg=json.dumps(entry)):
                fd.lint_class_entry("x", entry, limits)
        ok = fd.lint_class_entry("ok", {"phi_t": 0.9, "Phi_c": {"xx": 0.9, "xy": 0.1, "yy": 0.5}}, limits)
        self.assertEqual(ok["phi_t"], 0.9)

    def test_structural_rules(self):
        for mutate in (
            lambda t: t.update(dpm_rule_table_schema_version=2),
            lambda t: t.update(unmapped_class="default"),                              # needs default_class
            lambda t: t.update(unmapped_class="default", default_class={"phi_t": 1.0, "Phi_c": {"xx": 1, "xy": 0, "yy": 1}}),  # needs manning_n
            lambda t: t["edges"].update(interfaces=[{"classes": ["road", "water"], "omega_edge": 0.5}]),
            lambda t: t["edges"].update(interfaces=[{"classes": ["road", "grass"], "omega_edge": 1.5}]),
            lambda t: t["edges"].update(default_omega=-0.1),
            lambda t: t.update(soil={"source": "guess"}),
            lambda t: t.update(soil={"source": "soil_zones", "missing_zone": {"soil_type": "0"}}),
            lambda t: t.update(classes={}),
        ):
            table = json.loads(json.dumps(GOOD_TABLE))
            mutate(table)
            with self.assertRaises(fd.FieldDerivationError):
                fd.load_rule_table(write_table(table), package_is_synthetic=True)


class DerivationTests(unittest.TestCase):
    def setUp(self):
        self.rules = fd.load_rule_table(write_table(GOOD_TABLE), package_is_synthetic=True)
        self.landcover = [poly({"class_code": "road", "soil_type": 0, "manning_n": 0.015}, [[0, 0], [20, 0], [20, 10], [0, 10], [0, 0]]),
                          poly({"class_code": "grass", "soil_type": 1, "manning_n": 0.03}, [[0, 10], [20, 10], [20, 20], [0, 20], [0, 10]])]
        self.zones = [poly({"soil_type": 1}, [[0, 10], [20, 10], [20, 20], [0, 20], [0, 10]])]
        # 2x2 quads; cells 0,1 on the road row (y 0..10), cells 2,3 on grass.
        self.node_x = [0.0, 10.0, 20.0] * 3
        self.node_y = [0.0] * 3 + [10.0] * 3 + [20.0] * 3
        self.centroids = [(5, 5), (15, 5), (5, 15), (15, 15)]
        # edges: horizontal interface between cell 0 and 2 (nodes 3-4), vertical between 0 and 1 (nodes 1-4), boundary 0-1
        self.edge_nodes = [[3, 4], [1, 4], [0, 1]]
        self.edge_faces = [[0, 2], [0, 1], [0, -1]]

    def test_cell_fields_follow_classes_and_zones(self):
        cells = fd.derive_cell_fields(self.centroids, self.landcover, self.rules, self.zones)
        self.assertEqual(cells["class_code"], ["road", "road", "grass", "grass"])
        self.assertEqual(cells["phi_t"], [0.95, 0.95, 1.0, 1.0])
        self.assertEqual(cells["phi_yy"], [0.8, 0.8, 1.0, 1.0])
        self.assertEqual(cells["soil_type"], [0, 0, 1, 1])            # road cells default to 0 (missing zone)
        self.assertEqual(cells["missing_soil_cells"], [0, 1])
        self.assertEqual(cells["manning_n"], [0.015, 0.015, 0.03, 0.03])

    def test_landcover_overlap_rule(self):
        pond = poly({"class_code": "water", "soil_type": 0, "manning_n": 0.02}, [[12, 12], [18, 12], [18, 18], [12, 18], [12, 12]])
        table = json.loads(json.dumps(GOOD_TABLE)); table["classes"]["water"] = GOOD_TABLE["classes"]["grass"]
        strict = fd.load_rule_table(write_table(table), True)
        with self.assertRaises(fd.FieldDerivationError):
            fd.derive_cell_fields(self.centroids, self.landcover + [pond], strict, self.zones)
        table["landcover_overlap"] = "smallest_area_wins"
        lenient = fd.load_rule_table(write_table(table), True)
        cells = fd.derive_cell_fields(self.centroids, self.landcover + [pond], lenient, self.zones)
        self.assertEqual(cells["class_code"][3], "water")           # cell (15,15) -> pond wins over grass
        self.assertEqual(cells["manning_n"][3], 0.02)
        self.assertEqual(cells["overlapping_landcover_cells"], 1)

    def test_edge_projection_matches_spec_rule_2(self):
        cells = fd.derive_cell_fields(self.centroids, self.landcover, self.rules, self.zones)
        edges = fd.derive_edge_fields(self.node_x, self.node_y, self.edge_nodes, self.edge_faces, cells, self.rules)
        # interface edge (road|grass), horizontal -> normal along y:
        #   mean yy = (0.8 + 1.0)/2 = 0.9; omega 0.9 -> phi_e_n = 0.81; tangent uses mean xx = 0.975 -> phi_et = 0.8775
        self.assertAlmostEqual(edges["omega_edge"][0], 0.9)
        self.assertAlmostEqual(edges["phi_e_n"][0], 0.81, places=12)
        self.assertAlmostEqual(edges["phi_et"][0], 0.8775, places=12)
        # road|road vertical edge -> normal along x: xx = 0.95, omega 1
        self.assertAlmostEqual(edges["phi_e_n"][1], 0.95, places=12)
        self.assertAlmostEqual(edges["phi_et"][1], 0.8, places=12)
        # boundary edge: inside cell only, boundary omega 1
        self.assertAlmostEqual(edges["phi_e_n"][2], 0.8, places=12)
        self.assertEqual(edges["interface_edges"], 1)
        self.assertTrue(all(v <= 1.0 for v in edges["phi_e_n"] + edges["phi_et"]))

    def test_isotropic_projection_is_exactly_one_on_oblique_edges(self):
        iso = fd.load_rule_table(write_table(dict(GOOD_TABLE, classes={"g": GOOD_TABLE["classes"]["grass"]},
                                                  edges={"default_omega": 1.0})), True)
        cells = {"class_code": ["g", "g"], "phi_xx": [1.0, 1.0], "phi_xy": [0.0, 0.0], "phi_yy": [1.0, 1.0]}
        node_x, node_y = [0.0, 0.3], [0.0, 0.7]  # oblique edge whose naive nx^2+ny^2 rounds above 1
        edges = fd.derive_edge_fields(node_x, node_y, [[0, 1]], [[0, 1]], cells, iso)
        self.assertEqual(edges["phi_e_n"], [1.0])
        self.assertEqual(edges["phi_et"], [1.0])

    def test_unmapped_class_and_missing_zone_are_fatal_when_configured(self):
        strict = json.loads(json.dumps(GOOD_TABLE)); strict["soil"]["missing_zone"] = "fatal"
        rules = fd.load_rule_table(write_table(strict), True)
        with self.assertRaises(fd.FieldDerivationError):
            fd.derive_cell_fields(self.centroids, self.landcover, rules, self.zones)      # road cells have no zone
        with self.assertRaises(fd.FieldDerivationError):
            fd.derive_cell_fields(self.centroids, self.landcover[1:], self.rules, self.zones)  # cells 0,1 unmapped
        lenient = json.loads(json.dumps(GOOD_TABLE)); lenient["unmapped_class"] = "default"
        lenient["default_class"] = {"phi_t": 1.0, "Phi_c": {"xx": 1.0, "xy": 0.0, "yy": 1.0}, "manning_n": 0.03}
        rules = fd.load_rule_table(write_table(lenient), True)
        cells = fd.derive_cell_fields(self.centroids, self.landcover[1:], rules, self.zones)
        self.assertEqual(cells["unmapped_cells"], [0, 1])
        self.assertEqual(cells["phi_t"][:2], [1.0, 1.0])
        self.assertEqual(cells["manning_n"][:2], [0.03, 0.03])

    def test_report_shape(self):
        cells = fd.derive_cell_fields(self.centroids, self.landcover, self.rules, self.zones)
        edges = fd.derive_edge_fields(self.node_x, self.node_y, self.edge_nodes, self.edge_faces, cells, self.rules)
        report = fd.derivation_report(self.rules, cells, edges)
        self.assertEqual(report["cells_by_class"], {"road": 2, "grass": 2})
        self.assertEqual(report["omega_edge_values"], [0.9, 1.0])
        self.assertEqual(report["phi_t_range"], [0.95, 1.0])


if __name__ == "__main__":
    unittest.main()
