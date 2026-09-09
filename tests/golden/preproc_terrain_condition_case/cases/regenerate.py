"""Regenerates the G38 fixture inputs and (optionally) the committed outputs.

  py -3 tests/golden/preproc_terrain_condition_case/cases/regenerate.py [--run]

Inputs written here (deterministic, authored):
  dem_depression.asc     20 x 12 cells @ 10 m over the 200 x 120 m sample block;
                         west->east slope 10.02..10.78 m with a 4 x 4 bowl
                         (ring -0.30 m, core -0.60 m) whose lowest rim exit is
                         the 10.30 m column at x = 75 m
  streams.geojson        one LineString along y = 100 m from the west rim
                         (x = 0) to x = 180 m (EPSG:3857, the sample frame)
  terrain_condition_policy_fill.json     enabled: stream burn 0.20 m -> fill
  terrain_condition_policy_off.json      disabled variant (bitwise evidence)

With --run the sample package is overlaid into a temp dir (DEM + streams
replaced, everything else verbatim), the pipeline is executed twice (enabled
and disabled policy) and the outputs are copied next to this script.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SAMPLE = ROOT / "samples/d5_gis_preproc_template"

NCOLS, NROWS, CELL = 20, 12, 10.0
BOWL_COLS, BOWL_ROWS_FROM_BOTTOM = range(8, 12), range(4, 8)
CORE_COLS, CORE_ROWS_FROM_BOTTOM = range(9, 11), range(5, 7)


def write_dem() -> None:
    lines = [f"ncols         {NCOLS}", f"nrows         {NROWS}", "xllcorner     0", "yllcorner     0",
             f"cellsize      {CELL:g}", "NODATA_value  -9999"]
    for row in range(NROWS):
        rb = NROWS - 1 - row
        values = []
        for col in range(NCOLS):
            x = (col + 0.5) * CELL
            z = 10.0 + 0.004 * x
            if col in BOWL_COLS and rb in BOWL_ROWS_FROM_BOTTOM:
                z -= 0.60 if (col in CORE_COLS and rb in CORE_ROWS_FROM_BOTTOM) else 0.30
            values.append(f"{z:.2f}")
        lines.append(" ".join(values))
    (HERE / "dem_depression.asc").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_streams() -> None:
    data = {"type": "FeatureCollection",
            "crs": {"type": "name", "properties": {"name": "EPSG:3857"}},
            "features": [{"type": "Feature",
                          "properties": {"stream_id": "S1", "note": "synthetic drainage line reaching the west rim"},
                          "geometry": {"type": "LineString", "coordinates": [[0.0, 100.0], [180.0, 100.0]]}}]}
    (HERE / "streams.geojson").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_policies() -> None:
    enabled = {
        "terrain_condition_policy_schema_version": 1,
        "policy_id": "terrain_condition_policy_fill",
        "note": "G38 fixture: authorized stream enforcement + Priority-Flood fill on the synthetic depression DEM.",
        "enabled": True,
        "authorization": {"authorized_by": "synthetic-fixture (SCAU-UFM golden G38)", "date": "2026-09-09",
                          "note": "synthetic data; not a data-owner authorization"},
        "operations": {
            "reproject": {"enabled": False, "method": "nearest", "target_cellsize_m": None},
            "stream_enforcement": {"enabled": True, "streams": "terrain/streams.geojson", "burn_depth_m": 0.2,
                                   "sample_spacing_factor": 0.5},
            "fill_depressions": {"enabled": True, "algorithm": "priority_flood", "algorithm_version": 1,
                                 "epsilon_m": 0.0, "connectivity": 8, "nodata_is_outlet": True,
                                 "max_fill_depth_review_m": 1.0},
        },
        "diagnostics": {"write_rasters": True},
    }
    (HERE / "terrain_condition_policy_fill.json").write_text(json.dumps(enabled, indent=2) + "\n", encoding="utf-8")
    disabled = {**enabled, "policy_id": "terrain_condition_policy_off", "enabled": False,
                "note": "G38 disabled variant: same operations declared, enabled=false -> DEM sampled verbatim."}
    (HERE / "terrain_condition_policy_off.json").write_text(json.dumps(disabled, indent=2) + "\n", encoding="utf-8")


def overlay_package(target: Path, depression_dem: bool) -> None:
    shutil.copytree(SAMPLE, target)
    if depression_dem:
        shutil.copyfile(HERE / "dem_depression.asc", target / "terrain/dem.asc")
        shutil.copyfile(HERE / "streams.geojson", target / "terrain/streams.geojson")
    (target / "metadata/terrain_condition_policy.json").unlink()


def run(package: Path, output: Path, policy: Path, case_name: str) -> dict:
    job = {"job_config_schema_version": 1, "package": str(package), "output_dir": str(output),
           "case_name": case_name, "characteristic_length_m": 8.0, "recombine": True,
           "determinism_check": True, "terrain_condition": {"policy": str(policy)}}
    job_path = output.parent / f"{case_name}.job.json"
    output.mkdir(parents=True, exist_ok=True)
    job_path.write_text(json.dumps(job, indent=2), encoding="utf-8")
    completed = subprocess.run([sys.executable, "-m", "scau_preproc.pipeline", str(job_path)],
                               cwd=str(ROOT / "python"), capture_output=True, text=True)
    print(completed.stdout.strip(), completed.stderr.strip()[-500:])
    return json.loads((output / "validation.json").read_text(encoding="utf-8"))


def main() -> None:
    write_dem()
    write_streams()
    write_policies()
    if "--run" not in sys.argv:
        return
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        pkg = tmp / "pkg"
        overlay_package(pkg, depression_dem=True)
        # Enabled run on the depression DEM.
        v = run(pkg, tmp / "out_fill", HERE / "terrain_condition_policy_fill.json", "synthetic_city_block_conditioned.stcf.nc")
        assert v["status"] == "ok", v["findings"]
        out = tmp / "out_fill"
        shutil.copyfile(out / "synthetic_city_block_conditioned.stcf.nc", HERE / "synthetic_city_block_conditioned.stcf.nc")
        shutil.copyfile(out / "pipeline_manifest.json", HERE / "synthetic_city_block_conditioned.pipeline_manifest.json")
        for name in ("terrain_condition_report.json", "dem.asc", "depression_depth.asc", "terrain_change.asc"):
            shutil.copyfile(out / "conditioned_terrain" / name, HERE / ("conditioned_" + name if name.endswith(".asc") else name))
        # Raw run on the depression DEM (no policy) - the un-conditioned twin.
        v = run(pkg, tmp / "out_raw", HERE / "terrain_condition_policy_off.json", "synthetic_city_block_raw.stcf.nc")
        assert v["status"] == "ok", v["findings"]
        shutil.copyfile(tmp / "out_raw/synthetic_city_block_raw.stcf.nc", HERE / "synthetic_city_block_raw.stcf.nc")
        # Disabled policy on the UNMODIFIED sample: case bytes must equal G30.
        pkg2 = tmp / "pkg2"
        overlay_package(pkg2, depression_dem=False)
        v = run(pkg2, tmp / "out_off", HERE / "terrain_condition_policy_off.json", "synthetic_city_block.stcf.nc")
        assert v["status"] == "ok", v["findings"]
        shutil.copyfile(tmp / "out_off/pipeline_manifest.json", HERE / "synthetic_city_block_terrain_off.pipeline_manifest.json")
        g30 = ROOT / "tests/golden/preproc_gis_synthetic_case/cases/synthetic_city_block.stcf.nc"
        same = (tmp / "out_off/synthetic_city_block.stcf.nc").read_bytes() == g30.read_bytes()
        print("disabled policy case == G30 bytes:", same)
        assert same


if __name__ == "__main__":
    main()
