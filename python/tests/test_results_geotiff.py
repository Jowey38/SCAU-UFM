import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

import numpy as np

from scau_results import geotiff as gt


def read_tiff(path: Path) -> dict:
    """Independent minimal baseline-TIFF reader (no GDAL) for test verification."""
    raw = path.read_bytes()
    assert raw[:4] == b"II\x2a\x00"
    (ifd,) = struct.unpack_from("<I", raw, 4)
    (count,) = struct.unpack_from("<H", raw, ifd)
    tags = {}
    for i in range(count):
        tag, kind, n, value = struct.unpack_from("<HHII", raw, ifd + 2 + 12 * i)
        tags[tag] = (kind, n, value)
    w, h = tags[256][2], tags[257][2]
    off, nbytes = tags[273][2], tags[279][2]
    data = np.frombuffer(raw[off:off + nbytes], dtype="<f4").reshape(h, w)
    scale = struct.unpack_from("<3d", raw, tags[33550][2])
    tie = struct.unpack_from("<6d", raw, tags[33922][2])
    kind, n, goff = tags[34735]
    keys = struct.unpack_from(f"<{n}H", raw, goff)
    geokeys = {keys[4 + 4 * i]: keys[4 + 4 * i + 3] for i in range(keys[3])}
    kind, n, value = tags[42113]
    # TIFF 6.0: ASCII values of <= 4 bytes are stored inline in the value field.
    nodata = (struct.pack("<I", value)[:n] if n <= 4 else raw[value:value + n]).rstrip(b"\x00").decode()
    return {"data": data, "scale": scale, "tie": tie, "geokeys": geokeys, "nodata": nodata,
            "sample_format": tags[339][2], "bits": tags[258][2], "compression": tags[259][2]}


SQUARE = np.array([[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]])
TRI = np.array([[10.0, 0.0], [20.0, 0.0], [10.0, 10.0]])


class GeoTiffTests(unittest.TestCase):
    def test_rasterize_matches_analytic_cells_and_is_deterministic(self):
        grid, origin = gt.rasterize([SQUARE, TRI], 2.0)
        self.assertEqual(grid.shape, (5, 10))
        self.assertEqual(origin, (0.0, 10.0))
        self.assertTrue((grid[:, :5] == 0).all())                   # whole square
        self.assertEqual(grid[4, 5], 1)                              # bottom row of triangle, x=11
        self.assertEqual(grid[0, 9], -1)                             # NE corner outside hypotenuse
        self.assertEqual(grid[4, 9], 1)                              # SE corner inside
        again, _ = gt.rasterize([SQUARE, TRI], 2.0)
        np.testing.assert_array_equal(grid, again)
        with self.assertRaises(ValueError):
            gt.rasterize([SQUARE], 0.0)

    def test_geotiff_bytes_georeference_and_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "maps"
            report = gt.export_maps(out, [SQUARE, TRI], {"max_depth_m": np.array([0.5, 2.0]),
                                                        "arrival_time_s": np.array([np.nan, 60.0])},
                                    2.0, "EPSG:3395")
            self.assertTrue(report["georeferenced"])
            self.assertEqual(report["epsg"], 3395)
            self.assertEqual(report["covered_pixels"], 25 + 15)
            tif = read_tiff(out / "max_depth_m.tif")
            self.assertEqual((tif["bits"], tif["sample_format"], tif["compression"]), (32, 3, 1))
            self.assertEqual(tif["scale"], (2.0, 2.0, 0.0))
            self.assertEqual(tif["tie"], (0.0, 0.0, 0.0, 0.0, 10.0, 0.0))
            self.assertEqual(tif["geokeys"], {1024: 1, 1025: 1, 3072: 3395})
            self.assertEqual(tif["nodata"], "nan")
            self.assertEqual(tif["data"][2, 2], np.float32(0.5))
            self.assertEqual(tif["data"][4, 5], np.float32(2.0))
            self.assertTrue(np.isnan(tif["data"][0, 9]))
            arrival = read_tiff(out / "arrival_time_s.tif")["data"]
            self.assertTrue(np.isnan(arrival[2, 2]))               # never wet -> nan
            self.assertEqual(arrival[4, 5], np.float32(60.0))
            self.assertEqual(report["files"]["max_depth_m"]["sha256"],
                             hashlib.sha256((out / "max_depth_m.tif").read_bytes()).hexdigest())
            with self.assertRaises(FileExistsError):
                gt.export_maps(out, [SQUARE], {"max_depth_m": np.array([1.0])}, 2.0, "EPSG:3395")
            second = Path(tmp) / "again"
            gt.export_maps(second, [SQUARE, TRI], {"max_depth_m": np.array([0.5, 2.0])}, 2.0, "EPSG:3395")
            self.assertEqual((out / "max_depth_m.tif").read_bytes(), (second / "max_depth_m.tif").read_bytes())

    def test_unreferenced_raster_carries_only_raster_type_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "unref"
            report = gt.export_maps(out, [SQUARE], {"max_depth_m": np.array([1.0])}, 5.0, None)
            self.assertFalse(report["georeferenced"])
            self.assertEqual(read_tiff(out / "max_depth_m.tif")["geokeys"], {1025: 1})

    def test_epsg_resolution_rejects_geographic_and_unidentifiable(self):
        self.assertEqual(gt.epsg_code("EPSG:3395"), 3395)
        self.assertIsNone(gt.epsg_code(None))
        with self.assertRaisesRegex(ValueError, "geographic"):
            gt.epsg_code("EPSG:4326")
        with self.assertRaisesRegex(ValueError, "no EPSG"):
            gt.epsg_code("+proj=merc +lat_ts=12.34 +lon_0=5.67 +datum=WGS84")
        # Review P1-05: projected-but-not-metre, geocentric and vertical CRSs
        # were previously accepted and written as ModelTypeProjected.
        with self.assertRaisesRegex(ValueError, "US survey foot|not metres"):
            gt.epsg_code("EPSG:2263")
        with self.assertRaisesRegex(ValueError, "non-projected|geocentric"):
            gt.epsg_code("EPSG:4978")
        with self.assertRaisesRegex(ValueError, "vertical|non-projected"):
            gt.epsg_code("EPSG:5703")

    def test_export_is_all_or_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "maps"
            # float32 overflow must be rejected before any file exists
            with self.assertRaisesRegex(ValueError, "float32"):
                gt.export_maps(out, [SQUARE], {"a": np.array([1.0]), "b": np.array([1e100])}, 2.0, None)
            self.assertFalse(out.exists())
            self.assertEqual([p.name for p in Path(tmp).iterdir()], [])   # no staging leftovers
            # existing destination is a conflict, not a merge target
            out.mkdir()
            (out / "stale.tif").write_bytes(b"x")
            with self.assertRaises(FileExistsError):
                gt.export_maps(out, [SQUARE], {"a": np.array([1.0])}, 2.0, None)
            self.assertEqual([p.name for p in out.iterdir()], ["stale.tif"])
            # wrong field length rejected before write
            with self.assertRaisesRegex(ValueError, "values for 2 cells"):
                gt.export_maps(Path(tmp) / "m2", [SQUARE, TRI], {"a": np.array([1.0])}, 2.0, None)
            self.assertFalse((Path(tmp) / "m2").exists())

    def test_crs_resolution_uses_hash_verified_manifest_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "mesh").mkdir()
            (root / "validation").mkdir()
            stcf = root / "mesh" / "case.stcf.nc"
            stcf.write_bytes(b"STCF")
            self.assertEqual(gt.resolve_crs(stcf, None), (None, "undeclared"))
            self.assertEqual(gt.resolve_crs(stcf, "EPSG:3395"), ("EPSG:3395", "explicit_argument"))
            manifest = root / "validation" / "pipeline_manifest.json"
            manifest.write_text(json.dumps({"case_sha256": "00", "crs_governance": {"target_crs": "EPSG:3395"}}))
            with self.assertRaisesRegex(ValueError, "case_sha256"):
                gt.resolve_crs(stcf, None)
            manifest.write_text(json.dumps({"case_sha256": hashlib.sha256(b"STCF").hexdigest(),
                                            "crs_governance": {"target_crs": "EPSG:3395"}}))
            self.assertEqual(gt.resolve_crs(stcf, None), ("EPSG:3395", "pipeline_manifest:pipeline_manifest.json"))
            self.assertEqual(gt.resolve_crs(stcf, "EPSG:3395")[1], "explicit_argument+confirmed_by_manifest")
            with self.assertRaisesRegex(ValueError, "disagrees"):
                gt.resolve_crs(stcf, "EPSG:32650")
            manifest.write_text(json.dumps({"case_sha256": hashlib.sha256(b"STCF").hexdigest(),
                                            "crs_governance": None}))
            self.assertEqual(gt.resolve_crs(stcf, None), (None, "undeclared"))


if __name__ == "__main__":
    unittest.main()
