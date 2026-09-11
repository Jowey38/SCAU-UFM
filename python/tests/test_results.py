import tempfile
import unittest
from pathlib import Path

import netCDF4

from scau_results.__main__ import summarize


class ResultsTests(unittest.TestCase):
    def fixture(self, path):
        with netCDF4.Dataset(path, "w", format="NETCDF3_CLASSIC") as ds:
            ds.createDimension("time", 3)
            ds.createDimension("cell", 2)
            ds.surface_results_schema_version = "1"
            ds.run_status = "completed"
            time = ds.createVariable("time", "f8", ("time",))
            time.units = "s"
            time[:] = [0, 2, 5]
            for name, units in {"h": "m", "eta": "m", "hu": "m2 s-1", "hv": "m2 s-1", "wet_mask": "1"}.items():
                var = ds.createVariable(name, "f8", ("time", "cell"))
                var.units = units
                var[:] = [[0, 0], [1, 0], [0, 0]] if name in {"h", "eta", "wet_mask"} else 0

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

    def test_invalid_results_are_rejected(self):
        mutations = [
            lambda ds: ds.setncattr("run_status", "incomplete"),
            lambda ds: ds.setncattr("surface_results_schema_version", "0"),
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
                    with self.assertRaises(ValueError):
                        summarize(path, 0.5)
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
            with self.assertRaisesRegex(ValueError, "eta"):
                summarize(missing, 0.5)


if __name__ == "__main__":
    unittest.main()
