"""Governed SWMM network authoring (N1-B)."""
from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from . import inp_io


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _features(data: dict) -> tuple[list[dict], list[dict], list[dict]]:
    nodes, outfalls, links = [], [], []
    ids: set[str] = set()
    for feature in data.get("features", []):
        p = feature.get("properties", {})
        element = p.get("element")
        identifier = p.get("node_id") or p.get("link_id")
        if element not in {"junction", "outfall", "conduit"} or not identifier:
            raise ValueError("feature must be a junction, outfall, or conduit with an id")
        if identifier in ids:
            raise ValueError(f"duplicate drainage element id: {identifier}")
        ids.add(identifier)
        if element == "junction": nodes.append(feature)
        elif element == "outfall": outfalls.append(feature)
        else: links.append(feature)
    node_ids = {f["properties"]["node_id"] for f in nodes + outfalls}
    if not outfalls:
        raise ValueError("drainage network requires at least one outfall")
    if not nodes:
        raise ValueError("drainage network requires at least one junction")
    for feature in links:
        p = feature["properties"]
        if p.get("from_node") not in node_ids or p.get("to_node") not in node_ids:
            raise ValueError(f"conduit {p.get('link_id')} references an unknown node")
        x = p.get("xsection") or {}
        if x.get("shape", "").upper() not in {"CIRCULAR", "RECT_CLOSED", "RECT_OPEN", "TRAPEZOIDAL"}:
            raise ValueError(f"conduit {p.get('link_id')} has unsupported xsection shape")
    return nodes, outfalls, links


def geojson_to_model(data: dict) -> dict[str, list[list[object]]]:
    if data.get("drainage_network_schema_version") != 1:
        raise ValueError("unsupported drainage_network_schema_version")
    nodes, outfalls, links = _features(data)
    options = data.get("options", {"FLOW_UNITS": "CMS", "FLOW_ROUTING": "DYNWAVE", "LINK_OFFSETS": "DEPTH"})
    model: dict[str, list[list[object]]] = {"TITLE": [["SCAU-UFM authored drainage network"]], "OPTIONS": []}
    for key, value in options.items():
        model["OPTIONS"].append([str(key).upper(), value])
    model["JUNCTIONS"] = []
    model["OUTFALLS"] = []
    model["CONDUITS"] = []
    model["XSECTIONS"] = []
    model["COORDINATES"] = []
    for feature in nodes:
        p = feature["properties"]
        model["JUNCTIONS"].append([p["node_id"], p["invert_elevation_m"], p.get("max_depth_m", 1.0), p.get("init_depth_m", 0.0), p.get("surcharge_depth_m", 0.0), p.get("ponded_area_m2", 0.0)])
    for feature in outfalls:
        p = feature["properties"]
        model["OUTFALLS"].append([p["node_id"], p["invert_elevation_m"], p.get("outfall_type", "FREE").upper(), p.get("fixed_stage_m") or "NO", "NO"])
    for feature in links:
        p = feature["properties"]
        length = p.get("length_m")
        if length is None:
            coords = feature.get("geometry", {}).get("coordinates", [])
            length = sum(((b[0]-a[0])**2 + (b[1]-a[1])**2) ** 0.5 for a, b in zip(coords, coords[1:]))
        model["CONDUITS"].append([p["link_id"], p["from_node"], p["to_node"], length, p.get("roughness_manning", 0.013), p.get("in_offset_m", 0.0), p.get("out_offset_m", 0.0), 0.0, 0.0])
        x = p["xsection"]
        model["XSECTIONS"].append([p["link_id"], x["shape"].upper(), x.get("geom1_m", 1.0), x.get("geom2_m", 0.0), x.get("geom3_m", 0.0), x.get("geom4_m", 0.0), x.get("barrels", 1)])
    for feature in nodes + outfalls:
        coords = feature.get("geometry", {}).get("coordinates", [])
        model["COORDINATES"].append([feature["properties"]["node_id"], coords[0], coords[1]])
    model["REPORT"] = [["INPUT", "NO"], ["CONTROLS", "NO"], ["NODES", "NONE"], ["LINKS", "NONE"]]
    return model


def author_network(geojson: str | Path, output_inp: str | Path, *, external_inp: str | Path | None = None, parser_cli: str | None = None) -> dict:
    geojson = Path(geojson); output_inp = Path(output_inp)
    if external_inp is not None:
        source = Path(external_inp)
        if not source.is_file(): raise ValueError(f"external INP missing: {source}")
        output_inp.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(source, output_inp)
        mode = "external"
    else:
        data = json.loads(geojson.read_text(encoding="utf-8"))
        if output_inp.is_file():
            source_copy = output_inp.with_name("model.source.inp")
            if source_copy.exists():
                raise ValueError(f"authored source preservation already exists: {source_copy}")
            shutil.copyfile(output_inp, source_copy)
        inp_io.write_inp(geojson_to_model(data), output_inp)
        mode = "authored"
    parsed = inp_io.parse_inp(output_inp)
    result = {"mode": mode, "inp_sha256": sha256(output_inp), "sections_written": list(parsed), "engine_parse": {"engine": "swmm 5.2.4", "ok": None}}
    if parser_cli:
        completed = subprocess.run([parser_cli, "swmm-parse", "--inp", str(output_inp)], capture_output=True, text=True, check=False)
        result["engine_parse"] = json.loads(completed.stdout) if completed.returncode == 0 and completed.stdout.strip().startswith("{") else {"engine": "swmm 5.2.4", "ok": False, "detail": completed.stderr.strip()}
    manifest = output_inp.with_name(output_inp.name + ".authoring.json")
    result["source_geojson_sha256"] = sha256(geojson) if geojson.is_file() else None
    manifest.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result
