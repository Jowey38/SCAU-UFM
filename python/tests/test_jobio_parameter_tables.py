"""Headless tests for the E5a parameter-table (P5) and P3 field-mapping pure layer."""

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "jobio_p5", ROOT / "qgis_plugin" / "scau_preproc_workbench" / "jobio.py")
jobio = importlib.util.module_from_spec(_spec)
sys.modules["jobio_p5"] = jobio
_spec.loader.exec_module(jobio)

sys.path.insert(0, str(ROOT / "python"))
from scau_preproc import field_derivation, pipeline  # noqa: E402

SAMPLE = ROOT / "samples/d5_gis_preproc_template"


class RuleTableTests(unittest.TestCase):
    def test_canonical_targets_pinned_to_pipeline(self):
        self.assertEqual(set(jobio.CANONICAL_TARGETS), pipeline.CANONICAL_TARGETS)

    def test_sample_rule_table_rows_and_lint(self):
        table = jobio.load_rule_table(SAMPLE / "metadata/dpm_rule_table.json")
        rows = jobio.rule_class_rows(table)
        self.assertEqual([r["class_code"] for r in rows], ["grass", "road", "water"])
        self.assertEqual(next(r for r in rows if r["class_code"] == "road")["phi_yy"], 0.80)
        self.assertEqual(jobio.rule_interface_rows(table), [
            {"class_a": "road", "class_b": "grass", "omega_edge": 0.9,
             "note": "synthetic: kerb line partially blocks road->grass exchange"}])
        lint = jobio.lint_rule_table(table)
        self.assertEqual(lint[0][:2], ("pass", "RuleTableLint"))
        self.assertIn(("review", "RuleTableApproval", "status=synthetic_unapproved by=None date=None"), lint)
        # Round trip rows -> table is lossless for the tabular part.
        rebuilt = jobio.rule_table_from_rows(table, rows, jobio.rule_interface_rows(table))
        self.assertEqual(rebuilt["classes"], table["classes"])
        self.assertEqual(rebuilt["edges"], table["edges"])
        self.assertEqual(rebuilt["soil"], table["soil"])

    def test_lint_matches_pipeline_closure_laws(self):
        base = jobio.load_rule_table(SAMPLE / "metadata/dpm_rule_table.json")
        bad_rows = jobio.rule_class_rows(base)
        bad_rows[1]["phi_t"] = 0.5                       # road: phi_t < Phi_c.xx
        table = jobio.rule_table_from_rows(base, bad_rows, jobio.rule_interface_rows(base))
        codes = {c for _, c, _ in jobio.lint_rule_table(table)}
        self.assertIn("RuleStorageBelowConveyance", codes)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "t.json"
            path.write_text(json.dumps(table), encoding="utf-8")
            with self.assertRaises(field_derivation.FieldDerivationError):   # pipeline agrees
                field_derivation.load_rule_table(path, package_is_synthetic=True)
        for mutate, code in ((lambda r: r.__setitem__("phi_xy", 2.0), "RuleTensorNotSPD"),
                             (lambda r: r.__setitem__("phi_xx", 1.5), "RuleTensorTooLarge"),
                             (lambda r: r.__setitem__("phi_t", 0.0), "RulePhiTRange")):
            rows = jobio.rule_class_rows(base)
            mutate(rows[0])
            t = jobio.rule_table_from_rows(base, rows, [])
            self.assertIn(code, {c for _, c, _ in jobio.lint_rule_table(t)}, code)
        t = jobio.rule_table_from_rows(base, jobio.rule_class_rows(base),
                                       [{"class_a": "road", "class_b": "sand", "omega_edge": 1.5}])
        codes = {c for _, c, _ in jobio.lint_rule_table(t)}
        self.assertTrue({"RuleInterfaceUnknownClass", "RuleOmegaRange"} <= codes)
        with self.assertRaises(ValueError):
            jobio.rule_table_from_rows(base, [{"class_code": "", "phi_t": 1, "phi_xx": 1, "phi_xy": 0, "phi_yy": 1}], [])

    def test_edit_writes_new_version_and_resets_approval(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            shutil.copytree(SAMPLE, pkg)
            source = pkg / "metadata/dpm_rule_table.json"
            before = source.read_bytes()
            table = jobio.load_rule_table(source)
            table["approval"] = {"status": "approved", "approved_by": "owner", "date": "2026-01-01"}
            rows = jobio.rule_class_rows(table)
            rows[1]["phi_t"] = 0.97
            edited = jobio.rule_table_from_rows(table, rows, jobio.rule_interface_rows(table))
            with self.assertRaises(ValueError):
                jobio.write_rule_table_version(source, edited, edited_by="")
            v1 = jobio.write_rule_table_version(source, edited, edited_by="op", note="kerb test")
            self.assertEqual(v1.name, "dpm_rule_table.v001.json")
            self.assertEqual(source.read_bytes(), before)                     # never in place
            data = json.loads(v1.read_text(encoding="utf-8"))
            self.assertEqual(data["classes"]["road"]["phi_t"], 0.97)
            self.assertEqual(data["approval"]["status"], "synthetic_unapproved")   # edit invalidates approval
            self.assertEqual(data["revision"]["based_on"], "dpm_rule_table.json")
            self.assertEqual(data["revision"]["note"], "kerb test")
            # The new version is a valid pipeline table for a synthetic package.
            rules = field_derivation.load_rule_table(v1, package_is_synthetic=True)
            self.assertEqual(rules["classes"]["road"]["phi_t"], 0.97)
            v2 = jobio.write_rule_table_version(v1, edited, edited_by="op")
            self.assertEqual(v2.name, "dpm_rule_table.v002.json")             # same family, next number
            self.assertEqual(json.loads(v2.read_text(encoding="utf-8"))["revision"]["based_on"], "dpm_rule_table.v001.json")
            rows[1]["phi_xy"] = 5.0
            with self.assertRaises(ValueError):                                # lint blocks a bad write
                jobio.write_rule_table_version(source, jobio.rule_table_from_rows(table, rows, []), edited_by="op")
            self.assertFalse((pkg / "metadata/dpm_rule_table.v003.json").exists())


class SoilTableTests(unittest.TestCase):
    def test_sample_soil_rows_lint_and_versioned_write(self):
        rows = jobio.load_soil_rows(SAMPLE / "soil/soil_parameters.csv")
        self.assertEqual([r["soil_type"] for r in rows], ["0", "1"])
        self.assertEqual(jobio.lint_soil_rows(rows)[0][:2], ("pass", "SoilTableLint"))
        bad = [dict(r) for r in rows]
        bad[1]["theta_i"] = "0.9"
        self.assertIn("SoilThetaIRange", {c for _, c, _ in jobio.lint_soil_rows(bad)})
        bad = [dict(r) for r in rows]
        bad[1]["soil_type"] = "5"
        self.assertIn("SoilTypeNotContiguous", {c for _, c, _ in jobio.lint_soil_rows(bad)})
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            shutil.copytree(SAMPLE, pkg)
            source = pkg / "soil/soil_parameters.csv"
            before = source.read_bytes()
            edited = [dict(r) for r in rows]
            edited[1]["K_s"] = "2.0e-5"
            out = jobio.write_soil_table_version(source, edited, edited_by="op")
            self.assertEqual(out.name, "soil_parameters.v001.csv")
            self.assertEqual(source.read_bytes(), before)
            again = jobio.load_soil_rows(out)
            self.assertEqual(again[1]["K_s"], "2.0e-5")
            self.assertTrue(again[1]["source_or_authority"].startswith("edited_by:op"))
            with self.assertRaises(ValueError):
                jobio.write_soil_table_version(source, bad, edited_by="op")

    def test_parameter_table_rows_for_sample(self):
        job = jobio.build_job_config(str(SAMPLE), "out", dpm_rule_table=str(SAMPLE / "metadata/dpm_rule_table.json"))
        codes = [c for _, c, _ in jobio.parameter_table_rows(job)]
        self.assertEqual(codes[:2], ["RuleTable", "RuleTableLint"])
        self.assertIn("SoilTableLint", codes)
        codes = [c for _, c, _ in jobio.parameter_table_rows({"package": str(SAMPLE), "output_dir": "out"})]
        self.assertIn("RuleTable", codes)   # package default is picked up


class FieldMappingTests(unittest.TestCase):
    def test_discovery_defaults_and_canonical_only_targets(self):
        job = {"package": str(SAMPLE), "output_dir": "out"}
        self.assertEqual(jobio.canonical_targets(job), json.loads((SAMPLE / "manifest.json").read_text())["canonical_targets"])
        rows = jobio.field_mapping_rows(job)
        by_key = {(r["dataset"], r["source_field"]): r for r in rows}
        self.assertEqual(by_key[("dem", "elevation")]["target"], "z_b")                 # field dictionary
        self.assertEqual(by_key[("landcover", "manning_n")]["target"], "manning_n")
        self.assertEqual(by_key[("landcover", "soil_type")]["target"], "soil_type")        # first claim wins
        self.assertEqual(by_key[("soil_zones", "soil_type")]["target"], jobio.UNMAPPED)    # duplicate seed dropped
        self.assertIn("already fed by landcover.soil_type", by_key[("soil_zones", "soil_type")]["note"])
        self.assertEqual(by_key[("buildings", "building_id")]["target"], jobio.UNMAPPED)
        self.assertEqual(by_key[("surface_swmm_mapping", "swmm_node_id")]["source_type"], "text")
        self.assertTrue(all(r["target"] == jobio.UNMAPPED or r["target"] in jobio.CANONICAL_TARGETS for r in rows))
        summary = jobio.field_mapping_summary(job)
        self.assertEqual(summary[1][:2], ("pass", "FieldMappingCoverage"))
        self.assertEqual(summary[2][:2], ("review", "FieldMappingAmbiguousSeed"))

    def test_versioned_write_rejects_non_canonical_and_duplicate_targets(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            shutil.copytree(SAMPLE, pkg)
            job = {"package": str(pkg), "output_dir": tmp}
            rows = jobio.field_mapping_rows(job)
            bad = [dict(r) for r in rows]
            bad[0]["target"] = "roughness"
            with self.assertRaises(ValueError):
                jobio.write_field_mapping_version(job, bad, edited_by="op")
            dup = [dict(r) for r in rows]
            for r in dup:
                if r["source_field"] == "manning_n":
                    r["target"] = "z_b"                     # z_b already fed by dem.elevation
            with self.assertRaises(ValueError):
                jobio.write_field_mapping_version(job, dup, edited_by="op")
            edited = [dict(r) for r in rows]
            for r in edited:
                if (r["dataset"], r["source_field"]) == ("landcover", "soil_type"):
                    r["target"] = jobio.UNMAPPED            # operator moves soil to soil_zones
                if (r["dataset"], r["source_field"]) == ("soil_zones", "soil_type"):
                    r["target"] = "soil_type"
            path = jobio.write_field_mapping_version(job, edited, edited_by="op", note="drop landcover soil")
            self.assertEqual(path.name, "field_mapping.v001.json")
            data = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(data["field_mapping_schema_version"], 1)
            self.assertNotIn(("landcover", "soil_type"), {(m["dataset"], m["source_field"]) for m in data["mappings"]})
            self.assertIn(("soil_zones", "soil_type", "soil_type"),
                          {(m["dataset"], m["source_field"], m["target"]) for m in data["mappings"]})
            # Reloading prefers the latest version and reflects the edit.
            rows2 = {(r["dataset"], r["source_field"]): r["target"] for r in jobio.field_mapping_rows(job)}
            self.assertEqual(rows2[("landcover", "soil_type")], jobio.UNMAPPED)
            self.assertEqual(jobio.load_field_mapping(job)["_file"], "field_mapping.v001.json")
            self.assertIn("file=field_mapping.v001.json", jobio.field_mapping_summary(job)[0][2])
            (pkg / "manifest.json").write_text(json.dumps({**json.loads((pkg / "manifest.json").read_text()),
                                                           "canonical_targets": ["z_b", "roughness"]}), encoding="utf-8")
            with self.assertRaises(ValueError):
                jobio.canonical_targets(job)


if __name__ == "__main__":
    unittest.main()
