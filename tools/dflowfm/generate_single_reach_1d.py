#!/usr/bin/env python3
"""Generate the SCAU-UFM authored minimal D-Flow FM 1D UGRID network.

SPDX-License-Identifier: MIT

This file and its geometry are authored for SCAU-UFM. No Deltares example or
binary test asset is copied. The generator uses the public NetCDF C ABI and
UGRID interoperability field names.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import pathlib
import sys
from typing import Iterable

NC_NOERR = 0
NC_CLOBBER = 0
NC_NETCDF4 = 0x1000
NC_CHAR = 2
NC_INT = 4
NC_DOUBLE = 6
NC_GLOBAL = -1


def _load_netcdf(explicit: str | None) -> ctypes.CDLL:
    candidates: list[pathlib.Path] = []
    if explicit:
        candidates.append(pathlib.Path(explicit))
    env = os.environ.get("DFLOWFM_RUNTIME_BIN_RELEASE")
    if env:
        candidates.append(pathlib.Path(env) / "netcdf.dll")
    source = os.environ.get("DFLOWFM_SOURCE_DIR")
    if source:
        candidates.append(pathlib.Path(source) / "install_fm-suite_release" / "bin" / "netcdf.dll")
    candidates.append(pathlib.Path("Delft3D-main/install_fm-suite_release/bin/netcdf.dll"))

    for candidate in candidates:
        candidate = candidate.resolve()
        if not candidate.is_file():
            continue
        if sys.platform == "win32":
            os.add_dll_directory(str(candidate.parent))
        return ctypes.CDLL(str(candidate))
    raise RuntimeError("netcdf.dll not found; pass --netcdf-dll or set DFLOWFM_RUNTIME_BIN_RELEASE")


def _bind(lib: ctypes.CDLL) -> None:
    lib.nc_strerror.argtypes = [ctypes.c_int]
    lib.nc_strerror.restype = ctypes.c_char_p
    lib.nc_create.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    lib.nc_def_dim.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_int)]
    lib.nc_def_var.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.nc_put_att_text.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]
    lib.nc_put_att_int.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_size_t, ctypes.POINTER(ctypes.c_int)]
    lib.nc_put_att_double.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_size_t, ctypes.POINTER(ctypes.c_double)]
    lib.nc_enddef.argtypes = [ctypes.c_int]
    lib.nc_put_var_int.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    lib.nc_put_var_double.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    lib.nc_put_var_text.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
    lib.nc_close.argtypes = [ctypes.c_int]


def _check(lib: ctypes.CDLL, rc: int, operation: str) -> None:
    if rc != NC_NOERR:
        text = lib.nc_strerror(rc).decode("utf-8", errors="replace")
        raise RuntimeError(f"{operation}: {text} ({rc})")


def _int_array(values: Iterable[int]) -> ctypes.Array[ctypes.c_int]:
    data = list(values)
    return (ctypes.c_int * len(data))(*data)


def _double_array(values: Iterable[float]) -> ctypes.Array[ctypes.c_double]:
    data = list(values)
    return (ctypes.c_double * len(data))(*data)


def generate(output: pathlib.Path, lib: ctypes.CDLL) -> None:
    _bind(lib)
    output.parent.mkdir(parents=True, exist_ok=True)
    ncid = ctypes.c_int()
    _check(lib, lib.nc_create(str(output).encode(), NC_CLOBBER | NC_NETCDF4, ctypes.byref(ncid)), "nc_create")

    dims: dict[str, int] = {}
    vars_: dict[str, int] = {}

    def dim(name: str, size: int) -> None:
        value = ctypes.c_int()
        _check(lib, lib.nc_def_dim(ncid.value, name.encode(), size, ctypes.byref(value)), f"def_dim {name}")
        dims[name] = value.value

    def var(name: str, kind: int, dim_names: list[str]) -> int:
        value = ctypes.c_int()
        dim_ids = _int_array(dims[n] for n in dim_names) if dim_names else None
        _check(lib, lib.nc_def_var(ncid.value, name.encode(), kind, len(dim_names), dim_ids, ctypes.byref(value)), f"def_var {name}")
        vars_[name] = value.value
        return value.value

    def att_text(varid: int, name: str, value: str) -> None:
        raw = value.encode()
        _check(lib, lib.nc_put_att_text(ncid.value, varid, name.encode(), len(raw), raw), f"att {name}")

    def att_int(varid: int, name: str, value: int) -> None:
        data = ctypes.c_int(value)
        _check(lib, lib.nc_put_att_int(ncid.value, varid, name.encode(), NC_INT, 1, ctypes.byref(data)), f"att {name}")

    def att_double(varid: int, name: str, value: float) -> None:
        data = ctypes.c_double(value)
        _check(lib, lib.nc_put_att_double(ncid.value, varid, name.encode(), NC_DOUBLE, 1, ctypes.byref(data)), f"att {name}")

    dim("Two", 2)
    dim("network_nEdges", 1)
    dim("network_nNodes", 2)
    dim("network_nGeometryNodes", 2)
    dim("strLengthIds", 40)
    dim("strLengthLongNames", 80)
    dim("mesh1d_nNodes", 11)
    dim("mesh1d_nEdges", 10)

    crs = var("projected_coordinate_system", NC_INT, [])
    att_text(crs, "name", "SCAU-UFM local Cartesian")
    att_int(crs, "epsg", 0)
    att_text(crs, "grid_mapping_name", "unknown")

    network = var("network", NC_INT, [])
    for key, value in {
        "cf_role": "mesh_topology",
        "long_name": "Topology data of the SCAU-UFM authored 1D network",
        "edge_dimension": "network_nEdges",
        "edge_geometry": "network_geometry",
        "edge_node_connectivity": "network_edge_nodes",
        "node_coordinates": "network_node_x network_node_y",
        "node_dimension": "network_nNodes",
        "node_id": "network_node_id",
        "node_long_name": "network_node_long_name",
        "branch_id": "network_branch_id",
        "branch_long_name": "network_branch_long_name",
        "edge_length": "network_edge_length",
    }.items():
        att_text(network, key, value)
    att_int(network, "topology_dimension", 1)

    network_edge_nodes = var("network_edge_nodes", NC_INT, ["network_nEdges", "Two"])
    att_text(network_edge_nodes, "cf_role", "edge_node_connectivity")
    att_int(network_edge_nodes, "start_index", 1)
    network_branch_id = var("network_branch_id", NC_CHAR, ["network_nEdges", "strLengthIds"])
    network_branch_long = var("network_branch_long_name", NC_CHAR, ["network_nEdges", "strLengthLongNames"])
    network_edge_length = var("network_edge_length", NC_DOUBLE, ["network_nEdges"])
    att_text(network_edge_length, "units", "m")
    network_node_id = var("network_node_id", NC_CHAR, ["network_nNodes", "strLengthIds"])
    network_node_long = var("network_node_long_name", NC_CHAR, ["network_nNodes", "strLengthLongNames"])
    network_node_x = var("network_node_x", NC_DOUBLE, ["network_nNodes"])
    network_node_y = var("network_node_y", NC_DOUBLE, ["network_nNodes"])
    for value in (network_node_x, network_node_y):
        att_text(value, "units", "m")
    geometry = var("network_geometry", NC_INT, [])
    att_text(geometry, "geometry_type", "line")
    att_text(geometry, "node_count", "network_geom_node_count")
    att_text(geometry, "node_coordinates", "network_geom_x network_geom_y")
    geom_count = var("network_geom_node_count", NC_INT, ["network_nEdges"])
    geom_x = var("network_geom_x", NC_DOUBLE, ["network_nGeometryNodes"])
    geom_y = var("network_geom_y", NC_DOUBLE, ["network_nGeometryNodes"])
    branch_order = var("network_branch_order", NC_INT, ["network_nEdges"])
    branch_type = var("network_branch_type", NC_INT, ["network_nEdges"])

    mesh = var("mesh1d", NC_INT, [])
    for key, value in {
        "cf_role": "mesh_topology",
        "long_name": "Topology data of the SCAU-UFM authored 1D mesh",
        "coordinate_space": "network",
        "edge_node_connectivity": "mesh1d_edge_nodes",
        "node_dimension": "mesh1d_nNodes",
        "edge_dimension": "mesh1d_nEdges",
        "node_coordinates": "mesh1d_node_branch mesh1d_node_offset mesh1d_node_x mesh1d_node_y",
        "edge_coordinates": "mesh1d_edge_branch mesh1d_edge_offset mesh1d_edge_x mesh1d_edge_y",
        "node_id": "mesh1d_node_id",
        "node_long_name": "mesh1d_node_long_name",
    }.items():
        att_text(mesh, key, value)
    att_int(mesh, "topology_dimension", 1)

    node_branch = var("mesh1d_node_branch", NC_INT, ["mesh1d_nNodes"])
    edge_branch = var("mesh1d_edge_branch", NC_INT, ["mesh1d_nEdges"])
    att_int(node_branch, "start_index", 0)
    att_int(edge_branch, "start_index", 0)
    node_offset = var("mesh1d_node_offset", NC_DOUBLE, ["mesh1d_nNodes"])
    edge_offset = var("mesh1d_edge_offset", NC_DOUBLE, ["mesh1d_nEdges"])
    node_x = var("mesh1d_node_x", NC_DOUBLE, ["mesh1d_nNodes"])
    node_y = var("mesh1d_node_y", NC_DOUBLE, ["mesh1d_nNodes"])
    edge_x = var("mesh1d_edge_x", NC_DOUBLE, ["mesh1d_nEdges"])
    edge_y = var("mesh1d_edge_y", NC_DOUBLE, ["mesh1d_nEdges"])
    for value in (node_offset, edge_offset, node_x, node_y, edge_x, edge_y):
        att_text(value, "units", "m")
    mesh_node_id = var("mesh1d_node_id", NC_CHAR, ["mesh1d_nNodes", "strLengthIds"])
    mesh_node_long = var("mesh1d_node_long_name", NC_CHAR, ["mesh1d_nNodes", "strLengthLongNames"])
    mesh_edge_nodes = var("mesh1d_edge_nodes", NC_INT, ["mesh1d_nEdges", "Two"])
    att_text(mesh_edge_nodes, "cf_role", "edge_node_connectivity")
    att_int(mesh_edge_nodes, "start_index", 1)
    mesh_edge_type = var("mesh1d_edge_type", NC_INT, ["mesh1d_nEdges"])

    for name, value in {
        "institution": "South China Agricultural University / SCAU-UFM",
        "source": "tools/dflowfm/generate_single_reach_1d.py",
        "history": "Generated deterministically by the SCAU-UFM project",
        "license": "MIT; project-authored geometry, no Deltares test assets copied",
        "Conventions": "CF-1.8 UGRID-1.0 Deltares-0.10",
    }.items():
        att_text(NC_GLOBAL, name, value)

    _check(lib, lib.nc_enddef(ncid.value), "enddef")

    def put_int(name: str, values: Iterable[int]) -> None:
        _check(lib, lib.nc_put_var_int(ncid.value, vars_[name], _int_array(values)), f"put {name}")

    def put_double(name: str, values: Iterable[float]) -> None:
        _check(lib, lib.nc_put_var_double(ncid.value, vars_[name], _double_array(values)), f"put {name}")

    def fixed_strings(values: list[str], width: int) -> bytes:
        return b"".join(v.encode("ascii").ljust(width, b" ")[:width] for v in values)

    def put_text(name: str, values: list[str], width: int) -> None:
        raw = fixed_strings(values, width)
        _check(lib, lib.nc_put_var_text(ncid.value, vars_[name], raw), f"put {name}")

    put_int("network_edge_nodes", [1, 2])
    put_text("network_branch_id", ["branch_main"], 40)
    put_text("network_branch_long_name", ["SCAU-UFM straight single reach"], 80)
    put_double("network_edge_length", [1000.0])
    put_text("network_node_id", ["upstream", "downstream"], 40)
    put_text("network_node_long_name", ["Upstream endpoint", "Downstream endpoint"], 80)
    put_double("network_node_x", [0.0, 1000.0])
    put_double("network_node_y", [0.0, 0.0])
    put_int("network_geom_node_count", [2])
    put_double("network_geom_x", [0.0, 1000.0])
    put_double("network_geom_y", [0.0, 0.0])
    put_int("network_branch_order", [0])
    put_int("network_branch_type", [0])

    nodes = [100.0 * i for i in range(11)]
    edges = [50.0 + 100.0 * i for i in range(10)]
    put_int("mesh1d_node_branch", [0] * 11)
    put_int("mesh1d_edge_branch", [0] * 10)
    put_double("mesh1d_node_offset", nodes)
    put_double("mesh1d_edge_offset", edges)
    put_double("mesh1d_node_x", nodes)
    put_double("mesh1d_node_y", [0.0] * 11)
    put_double("mesh1d_edge_x", edges)
    put_double("mesh1d_edge_y", [0.0] * 10)
    put_text("mesh1d_node_id", [f"node_{i:02d}" for i in range(11)], 40)
    put_text("mesh1d_node_long_name", [f"Reach node {i}" for i in range(11)], 80)
    put_int("mesh1d_edge_nodes", [value for i in range(10) for value in (i + 1, i + 2)])
    put_int("mesh1d_edge_type", [1] * 10)

    _check(lib, lib.nc_close(ncid.value), "close")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--netcdf-dll")
    args = parser.parse_args()
    try:
        generate(args.output.resolve(), _load_netcdf(args.netcdf_dll))
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
