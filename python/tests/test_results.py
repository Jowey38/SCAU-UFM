import json
import tempfile
import unittest
from pathlib import Path

import netCDF4

from scau_results.__main__ import fnv1a64, summarize

FINAL_HASH = "fnv1a64:0123456789abcdef"


class ResultsTests(unittest.TestCase):
    def fixture(self, path, manifest=True):
        source = path.with_suffix(".stcf.nc")
        source.write_bytes(b"SOURCE-STCF")
        with netCDF4.Dataset(path, "w", format="NETCDF3_CLASSIC") as ds:
            ds.createDimension("time", 3)
            ds.createDimension("cell", 2)
            ds.surface_results_schema_version = "2"
            ds.run_status = "completed"
            ds.source_stcf = str(source)
            ds.source_stcf_hash = fnv1a64(source.read_bytes())
            ds.final_surface_state_hash = FINAL_HASH
            ds.committed_epochs = "2"
            time = ds.createVariable("time", "f8", ("time",))
            time.units = "s"
            time[:] = [0, 2, 5]
            for name, units in {"h": "m", "eta": "m", "hu": "m2 s-1", "hv": "m2 s-1", "wet_mask": "1"}.items():
                var = ds.createVariable(name, "f8", ("time", "cell"))
                var.units = units
                var[:] = [[0, 0], [1, 0], [0, 0]] if name in {"h", "eta", "wet_mask"} else 0
        if manifest:
            self.write_manifest(path)

    def write_manifest(self, path, **overrides):
        body = {"manifest_schema_version": 1, "output": path.name, "output_hash": fnv1a64(path.read_bytes()),
                "source_stcf": str(path.with_suffix(".stcf.nc")),
                "source_stcf_hash": fnv1a64(b"SOURCE-STCF"),
                "final_surface_state_hash": FINAL_HASH, "committed_epochs": 2, "frames": 3, **overrides}
        Path(str(path) + ".manifest.json").write_text(json.dumps(body), encoding="utf-8")

    def test_fnv1a64_matches_reference_vectors(self):
        self.assertEqual(fnv1a64(b""), "fnv1a64:cbf29ce484222325")
        self.assertEqual(fnv1a64(b"a"), "fnv1a64:af63dc4c8601ec8c")

    def test_manifest_and_source_tamper_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "result.nc"
            self.fixture(path, manifest=False)
            with self.assertRaisesRegex(ValueError, "manifest is missing"):
                summarize(path, 0.5)
            self.write_manifest(path, output_hash="fnv1a64:0000000000000000")
            with self.assertRaisesRegex(ValueError, "output bytes"):
                summarize(path, 0.5)
            self.write_manifest(path, committed_epochs=3)
            with self.assertRaisesRegex(ValueError, "committed_epochs"):
                summarize(path, 0.5)
            self.write_manifest(path, final_surface_state_hash="fnv1a64:ffffffffffffffff")
            with self.assertRaisesRegex(ValueError, "final_surface_state_hash"):
                summarize(path, 0.5)
            self.write_manifest(path)
            self.assertEqual(summarize(path, 0.5)["committed_epochs"], 2)
            path.with_suffix(".stcf.nc").write_bytes(b"MODIFIED")
            with self.assertRaisesRegex(ValueError, "bytes changed"):
                summarize(path, 0.5)

    def test_analytic_arrival_duration_and_repeatability(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "result.nc"
            self.fixture(path)
            before = path.read_bytes()
            result = summarize(path, 0.5)
            self.assertEqual(result["max_depth_m"], [1, 0])
            self.assertEqual(result["arrival_time_s"], [2, None])
            self.assertEqual(result["duration_s"], [3, 0])
            self.assertEqual(result, summarize(path, 0.5))
            self.assertEqual(path.read_bytes(), before)

    def test_point_and_profile_series_follow_requested_cell_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "result.nc"
            self.fixture(path)
            self.assertIsNone(summarize(path, 0.5)["series"])
            series = summarize(path, 0.5, [1, 0])["series"]
            self.assertEqual(series["cells"], [1, 0])
            self.assertEqual(series["time_s"], [0, 2, 5])
            self.assertEqual(series["h"], [[0, 0, 0], [0, 1, 0]])   # profile order, not file order
            self.assertEqual(series["hu"], [[0, 0, 0], [0, 0, 0]])
            for bad in ([], [0, 0], [2], [-1], [True]):
                with self.subTest(bad=bad), self.assertRaises(ValueError):
                    summarize(path, 0.5, bad)

    def test_invalid_results_are_rejected(self):
        mutations = [
            lambda ds: ds.setncattr("run_status", "incomplete"),
            lambda ds: ds.setncattr("surface_results_schema_version", "1"),
            lambda ds: ds["h"].setncattr("units", "ft"),
            lambda ds: ds["time"].__setitem__(slice(None), [0, 2, 1]),
            lambda ds: ds["h"].__setitem__((1, 0), -1),
            lambda ds: ds["hu"].__setitem__((1, 0), float("nan")),
            lambda ds: ds["wet_mask"].__setitem__((1, 0), 2),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            for i, mutate in enumerate(mutations):
                with self.subTest(i=i):
                    path = Path(tmp) / f"invalid{i}.nc"
                    self.fixture(path)
                    with netCDF4.Dataset(path, "a") as ds:
                        mutate(ds)
                    self.write_manifest(path)  # re-bind bytes so the content check is what fails
                    with self.assertRaises(ValueError) as ctx:
                        summarize(path, 0.5)
                    self.assertNotIn("output bytes", str(ctx.exception))
            with self.assertRaises(ValueError):
                summarize(path, float("nan"))
            partial = Path(tmp) / "result.nc.partial"
            self.fixture(partial)
            with self.assertRaisesRegex(ValueError, "unpublished"):
                summarize(partial, 0.5)
            missing = Path(tmp) / "missing.nc"
            self.fixture(missing)
            with netCDF4.Dataset(missing, "a") as ds:
                ds.renameVariable("eta", "wrong_eta")
            self.write_manifest(missing)
            with self.assertRaisesRegex(ValueError, "eta"):
                summarize(missing, 0.5)


if __name__ == "__main__":
    unittest.main()
