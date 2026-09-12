"""Inspect and summarize completed surface timeseries without modifying inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import netCDF4
import numpy as np


def summarize(path: Path, threshold: float) -> dict:
    if not np.isfinite(threshold) or threshold <= 0:
        raise ValueError("threshold must be finite and positive")
    if path.name.endswith(".partial"):
        raise ValueError("result validation failure: unpublished partial output")
    raw = path.read_bytes()
    with netCDF4.Dataset("results", memory=raw) as ds:
        if getattr(ds, "surface_results_schema_version", None) != "1":
            raise ValueError("migration required: unsupported surface results schema")
        if getattr(ds, "run_status", None) != "completed":
            raise ValueError("result validation failure: incomplete run")
        expected = {"h": "m", "eta": "m", "hu": "m2 s-1", "hv": "m2 s-1", "wet_mask": "1"}
        for name, units in expected.items():
            if name not in ds.variables or ds[name].dimensions != ("time", "cell"):
                raise ValueError(f"result validation failure: {name} shape/variable missing")
            if getattr(ds[name], "units", None) != units:
                raise ValueError(f"unit mismatch: {name}")
        if "time" not in ds.variables or getattr(ds["time"], "units", None) != "s":
            raise ValueError("time units must be model logical seconds")
        time_data = ds["time"][:]
        if np.any(np.ma.getmaskarray(time_data)):
            raise ValueError("compatibility error: missing time value")
        time = np.asarray(time_data)
        if time.ndim != 1 or len(time) < 2 or not np.all(np.isfinite(time)) or np.any(np.diff(time) <= 0):
            raise ValueError("compatibility error: invalid or non-increasing times")
        source_stcf = getattr(ds, "source_stcf", "")
        if not source_stcf:
            raise ValueError("provenance validation failure: source_stcf is missing")
        source_path = Path(source_stcf)
        if not source_path.is_file():
            raise ValueError(f"provenance validation failure: source_stcf is absent: {source_stcf}")
        source_stcf_sha256 = hashlib.sha256(source_path.read_bytes()).hexdigest()
        fields = {}
        for name in expected:
            array = ds[name][:]
            if np.any(np.ma.getmaskarray(array)) or not np.all(np.isfinite(array)):
                raise ValueError(f"result validation failure: missing/non-finite {name}")
            fields[name] = np.asarray(array)
        h = fields["h"]
        if h.shape[0] != len(time) or h.shape[1] == 0 or any(v.shape != h.shape for v in fields.values()):
            raise ValueError("result validation failure: inconsistent or empty field shape")
        if np.any(h < 0) or not np.all(np.isin(fields["wet_mask"], [0, 1])):
            raise ValueError("result validation failure: depth or wet mask")
        wet = h >= threshold
        arrival = [float(time[np.flatnonzero(wet[:, c])[0]]) if wet[:, c].any() else None
                   for c in range(h.shape[1])]
        duration = np.sum(wet[:-1] * np.diff(time)[:, None], axis=0)
        return {"results_schema_version": 1, "source_sha256": hashlib.sha256(raw).hexdigest(),
                "source_stcf": str(source_path), "source_stcf_sha256": source_stcf_sha256,
                "threshold_m": threshold, "duration_method": "left_sample_piecewise_constant",
                "time_units": "model logical seconds", "frames": len(time), "cells": h.shape[1],
                "max_depth_m": np.max(h, axis=0).tolist(), "arrival_time_s": arrival,
                "duration_s": duration.tolist()}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--threshold", type=float, default=0.01)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = summarize(args.input, args.threshold)
        with args.output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(result, sort_keys=True, indent=2, allow_nan=False) + "\n")
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(2, f"results error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
