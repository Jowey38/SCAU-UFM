"""Governed D-Flow FM 1D river sketch authoring (N2-A/N2-B)."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any

import netCDF4

SCHEMA_VERSION = 1
GENERATOR_VERSION = "dflowfm_author-1"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _point(feature: dict[str, Any]) -> tuple[float, float]:
    g = feature.get("geometry") or {}
    if g.get("type") != "Point" or len(g.get("coordinates", [])) != 2:
        raise ValueError("river_node geometry must be a 2D Point")
    x, y = g["coordinates"]
    if not all(isinstance(v, (int, float)) and math.isfinite(float(v)) for v in (x, y)):
        raise ValueError("coordinates must be finite numbers")
    return float(x), float(y)


def _line(feature: dict[str, Any]) -> list[tuple[float, float]]:
    g = feature.get("geometry") or {}
    if g.get("type") != "LineString" or len(g.get("coordinates", [])) < 2:
        raise ValueError("river line geometry must have at least two points")
    result = []
    for c in g["coordinates"]:
        if len(c) != 2 or not all(isinstance(v, (int, float)) and math.isfinite(float(v)) for v in c):
            raise ValueError("line coordinates must be finite 2D numbers")
        result.append((float(c[0]), float(c[1])))
    return result


def _length(coords: list[tuple[float, float]]) -> float:
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(coords, coords[1:]))


def validate_sketch(data: dict[str, Any]) -> dict[str, Any]:
    if data.get("type") != "FeatureCollection" or data.get("river_sketch_schema_version") != 1:
        raise ValueError("unsupported river_sketch schema; expected FeatureCollection version 1")
    features = data.get("features")
    if not isinstance(features, list) or not features:
        raise ValueError("river sketch requires features")
    nodes: dict[str, dict[str, Any]] = {}; branches: dict[str, dict[str, Any]] = {}; interfaces: dict[str, dict[str, Any]] = {}; ids: set[str] = set()
    for feature in features:
        p = feature.get("properties") or {}; element = p.get("element")
        key = {"river_node": "node_id", "river_branch": "branch_id", "surface_interface_candidate": "interface_id"}.get(element)
        identifier = p.get(key) if key else None
        if not key or not isinstance(identifier, str) or not identifier or identifier in ids:
            raise ValueError("each river feature needs a unique supported id")
        ids.add(identifier)
        if element == "river_node": _point(feature); nodes[identifier] = feature
        elif element == "river_branch":
            coords = _line(feature)
            if p.get("from_node") not in nodes or p.get("to_node") not in nodes: raise ValueError(f"branch {identifier} references unknown node")
            a, b = _point(nodes[p["from_node"]]), _point(nodes[p["to_node"]])
            if math.hypot(coords[0][0]-a[0], coords[0][1]-a[1]) > 1e-7 or math.hypot(coords[-1][0]-b[0], coords[-1][1]-b[1]) > 1e-7: raise ValueError(f"branch {identifier} endpoints are not snapped")
            branches[identifier] = feature
        else:
            _line(feature)
            if p.get("branch_id") not in branches or p.get("side") not in {"left", "right"}: raise ValueError(f"interface {identifier} has invalid branch or side")
            interfaces[identifier] = feature
    degree = {i: 0 for i in nodes}
    for f in branches.values(): degree[f["properties"]["from_node"]] += 1; degree[f["properties"]["to_node"]] += 1
    if not nodes or not branches or any(v == 0 for v in degree.values()): raise ValueError("river sketch must have branches and no isolated nodes")
    return {"nodes": nodes, "branches": branches, "interfaces": interfaces}


def read_sketch(path: str | Path) -> dict[str, Any]:
    return validate_sketch(json.loads(Path(path).read_text(encoding="utf-8")))


def write_ugrid(sketch: dict[str, Any], output: str | Path) -> Path:
    parts = validate_sketch(sketch); output = Path(output); output.parent.mkdir(parents=True, exist_ok=True)
    nodes = sorted(parts["nodes"].items()); branches = sorted(parts["branches"].items()); index = {i: n for n, (i, _) in enumerate(nodes)}
    with netCDF4.Dataset(output, "w", format="NETCDF4") as ds:
        ds.createDimension("Two", 2); ds.createDimension("network_nNodes", len(nodes)); ds.createDimension("network_nEdges", len(branches)); ds.createDimension("network_nGeometryNodes", sum(len(_line(f)) for _, f in branches))
        topology = ds.createVariable("network", "i4"); topology.cf_role = "mesh_topology"; topology.topology_dimension = 1; topology.edge_node_connectivity = "network_edge_nodes"; topology.node_coordinates = "network_node_x network_node_y"; topology.node_dimension = "network_nNodes"; topology.edge_dimension = "network_nEdges"; topology.edge_geometry = "network_geometry"
        edge_nodes = ds.createVariable("network_edge_nodes", "i4", ("network_nEdges", "Two")); edge_nodes.start_index = 1
        node_id = ds.createVariable("network_node_id", str, ("network_nNodes",)); branch_id = ds.createVariable("network_branch_id", str, ("network_nEdges",)); node_x = ds.createVariable("network_node_x", "f8", ("network_nNodes",)); node_y = ds.createVariable("network_node_y", "f8", ("network_nNodes",)); edge_length = ds.createVariable("network_edge_length", "f8", ("network_nEdges",)); edge_length.units = "m"
        geometry = ds.createVariable("network_geometry", "i4"); geometry.geometry_type = "line"; geometry.node_count = "network_geom_node_count"; geometry.node_coordinates = "network_geom_x network_geom_y"; counts = ds.createVariable("network_geom_node_count", "i4", ("network_nEdges",)); gx = ds.createVariable("network_geom_x", "f8", ("network_nGeometryNodes",)); gy = ds.createVariable("network_geom_y", "f8", ("network_nGeometryNodes",))
        for n, (identifier, feature) in enumerate(nodes): node_id[n] = identifier; node_x[n] = _point(feature)[0]; node_y[n] = _point(feature)[1]
        for n, (identifier, feature) in enumerate(branches): branch_id[n] = identifier
        edge_nodes[:] = [[index[f["properties"]["from_node"]]+1, index[f["properties"]["to_node"]]+1] for _, f in branches]
        coords = [_line(f) for _, f in branches]; counts[:] = [len(c) for c in coords]; gx[:] = [p[0] for c in coords for p in c]; gy[:] = [p[1] for c in coords for p in c]; edge_length[:] = [float(f["properties"].get("length_m") if f["properties"].get("length_m") is not None else _length(c)) for (_, f), c in zip(branches, coords)]
        ds.Conventions = "CF-1.8 UGRID-1.0"; ds.source = GENERATOR_VERSION
    return output


def read_ugrid(path: str | Path) -> dict[str, Any]:
    with netCDF4.Dataset(path) as ds:
        required = {"network_edge_nodes", "network_node_id", "network_branch_id", "network_node_x", "network_node_y", "network_edge_length"}
        if required - set(ds.variables): raise ValueError(f"UGRID missing variables: {sorted(required - set(ds.variables))}")
        return {"node_ids": [str(v) for v in ds.variables["network_node_id"][:]], "branch_ids": [str(v) for v in ds.variables["network_branch_id"][:]], "edge_nodes": ds.variables["network_edge_nodes"][:].tolist(), "node_x": ds.variables["network_node_x"][:].tolist(), "node_y": ds.variables["network_node_y"][:].tolist(), "edge_length": ds.variables["network_edge_length"][:].tolist()}


def validate_ugrid(path: str | Path) -> dict[str, Any]:
    data = read_ugrid(path); n = len(data["node_ids"])
    if len(set(data["node_ids"])) != n or len(set(data["branch_ids"])) != len(data["edge_nodes"]): raise ValueError("UGRID IDs must be unique")
    if any(len(e) != 2 or not all(1 <= int(v) <= n for v in e) for e in data["edge_nodes"]): raise ValueError("UGRID edge connectivity is out of range")
    return data


def write_mdu_template(output: str | Path, network_name: str) -> Path:
    output = Path(output); output.parent.mkdir(parents=True, exist_ok=True); output.write_text(f"[General]\nProgram = D-Flow FM\n\n[Geometry]\nNetFile = {network_name}_net.nc\n\n[Hydrology]\n# TODO_PROVIDER: boundary type, direction, and IDs\nBoundaryFile = TODO_PROVIDER\n\n[Physics]\n# TODO_PROVIDER: cross sections, roughness, and initial conditions\n", encoding="utf-8", newline="\n"); return output


def export_readiness(manifest: dict[str, Any]) -> dict[str, Any]:
    required = manifest.get("provider_required"); return {"exportable": isinstance(required, list) and not required, "finding": None if isinstance(required, list) and not required else "RiverHydraulicsProviderRequired"}


def author_river(sketch_path: str | Path, output_dir: str | Path, case_name: str = "river") -> dict[str, Any]:
    sketch_path = Path(sketch_path); output_dir = Path(output_dir); output_dir.mkdir(parents=True, exist_ok=True); sketch = json.loads(sketch_path.read_text(encoding="utf-8")); validate_sketch(sketch); net = write_ugrid(sketch, output_dir / f"{case_name}_net.nc"); mdu = write_mdu_template(output_dir / f"{case_name}.mdu", case_name)
    manifest = {"schema_version": 1, "mode": "authored", "generator_version": GENERATOR_VERSION, "sketch_sha256": sha256(sketch_path), "net_sha256": sha256(net), "mdu_sha256": sha256(mdu), "provider_required": [{"file": mdu.name, "key": "BoundaryFile", "reason": "boundary type, direction, and ID are provider-supplied"}, {"file": mdu.name, "key": "Physics", "reason": "cross sections, roughness, and initial conditions are provider-supplied"}], "run_smoke": {"dll_initialize_ok": False, "skipped": True}}
    manifest["export_readiness"] = export_readiness(manifest); (output_dir / "authoring_manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"); return manifest
