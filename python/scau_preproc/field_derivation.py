"""Physical field derivation from an APPROVED rule table (M287-B5, stage D).

Pipeline v1 wrote unit placeholders for the anisotropic DPM fields and read the
soil type from the landcover class. B5 makes the derivation rule-driven and
auditable while keeping the *values* a governance input:

  metadata/dpm_rule_table.json   (dpm_rule_table_schema_version = 1)
    approval: {status: synthetic_unapproved | approved, approved_by, date, note}
    closure_limits: {epsilon_phi, cond_max}         # echoed from the C++ validator defaults
    default_class: {phi_t, Phi_c: {xx, xy, yy}, manning_n}  # used only when unmapped_class = default
    unmapped_class: "fatal" | "default"
    classes: {<landcover class_code>: {phi_t, Phi_c: {xx, xy, yy}, note}}
    edges:
      default_omega: 1.0
      interfaces: [{classes: [a, b], omega_edge: w}]  # unordered class pair on an internal edge
      boundary_omega: 1.0
  soil:
      source: "soil_zones" | "landcover"              # soil_type per cell
      missing_zone: "fatal" | {"soil_type": k}
  landcover_overlap: "fatal" | "smallest_area_wins"   # centroid in >1 class polygon

In derived mode the class selection also owns manning_n (same polygon), so
class, roughness and DPM fields are mutually consistent; v1 placeholder mode
keeps its first-match manning lookup untouched (G30 bytes).

Cell fields: phi_t and the symmetric conveyance tensor Phi_c (phi_xx, phi_xy,
phi_yy) come from the class entry of the landcover polygon containing the cell
centroid. Edge fields follow main-spec 5.3 rule 2 / M244-M245 exactly as the
solver's assemble_edge_conveyance_from_tensors does:
    Phi_c,e   = 0.5 * (Phi_c,L + Phi_c,R)          (boundary edge: inside cell)
    phi_e_n   = omega_edge * n^T Phi_c,e n
    phi_et    = omega_edge * t^T Phi_c,e t
with n the unit edge normal and t the unit tangent. omega_edge is the rule
table's interface weight for the (class_L, class_R) pair, else default_omega
(boundary_omega on boundary edges).

Rule-table LINT (fail-closed, points at the offending entry): every class
entry must satisfy the closure laws the C++ validator enforces (phi_t in
(0,1], Phi_c SPD with lambda_min >= epsilon_phi, lambda_max <= 1,
cond <= cond_max, phi_t >= max diagonal). The authoritative check is still
`scau_preproc validate` on the produced case; the lint only makes the error
locatable in the table. Values are never inferred from geometry: an
`unmapped_class: fatal` table aborts on any cell whose class has no entry.

Unapproved tables are accepted ONLY for packages whose manifest declares
`synthetic_data: true`; a real package with `approval.status != approved` is
fatal (M281 / v1.1 section 4 red line: DPM parameter values need the data
owner's approval).
"""

from __future__ import annotations

import json
import math
from pathlib import Path

DPM_RULE_TABLE_SCHEMA_VERSION = 1
DEFAULT_CLOSURE_LIMITS = {"epsilon_phi": 1.0e-6, "cond_max": 1.0e4}


class FieldDerivationError(ValueError):
    def __init__(self, message: str, entry: str | None = None) -> None:
        self.entry = entry
        super().__init__(message if entry is None else f"{entry}: {message}")


# --- rule table ---------------------------------------------------------------


def _tensor(entry: dict, where: str) -> dict:
    tensor = entry.get("Phi_c")
    if not isinstance(tensor, dict) or any(k not in tensor for k in ("xx", "xy", "yy")):
        raise FieldDerivationError("Phi_c must be an object with xx, xy, yy", where)
    try:
        return {k: float(tensor[k]) for k in ("xx", "xy", "yy")}
    except (TypeError, ValueError) as error:
        raise FieldDerivationError("Phi_c components must be numbers", where) from error


def lint_class_entry(name: str, entry: dict, limits: dict) -> dict:
    """Returns the normalized entry; raises on any closure-law violation."""
    where = f"classes.{name}"
    try:
        phi_t = float(entry["phi_t"])
    except (KeyError, TypeError, ValueError) as error:
        raise FieldDerivationError("phi_t must be a number", where) from error
    if not math.isfinite(phi_t) or phi_t <= 0.0 or phi_t > 1.0:
        raise FieldDerivationError(f"phi_t = {phi_t} outside (0, 1]", where)
    tensor = _tensor(entry, where)
    xx, xy, yy = tensor["xx"], tensor["xy"], tensor["yy"]
    if not all(math.isfinite(v) for v in (xx, xy, yy)):
        raise FieldDerivationError("Phi_c components must be finite", where)
    if xx <= 0.0 or yy <= 0.0:
        raise FieldDerivationError("Phi_c diagonal entries must be positive", where)
    det = xx * yy - xy * xy
    trace = xx + yy
    disc = math.sqrt(max(0.0, trace * trace - 4.0 * det))
    lambda_min, lambda_max = 0.5 * (trace - disc), 0.5 * (trace + disc)
    if lambda_min < limits["epsilon_phi"]:
        raise FieldDerivationError(f"Phi_c lambda_min = {lambda_min:g} below epsilon_phi", where)
    if lambda_max > 1.0:
        raise FieldDerivationError(f"Phi_c lambda_max = {lambda_max:g} above 1", where)
    if lambda_max / lambda_min > limits["cond_max"]:
        raise FieldDerivationError("Phi_c condition number above cond_max", where)
    if phi_t < max(xx, yy):
        raise FieldDerivationError(f"phi_t = {phi_t} below max diagonal of Phi_c ({max(xx, yy)})", where)
    return {"phi_t": phi_t, "Phi_c": tensor, "note": entry.get("note")}


def load_rule_table(path: Path, package_is_synthetic: bool) -> dict:
    path = Path(path)
    if not path.is_file():
        raise FieldDerivationError(f"dpm rule table not found: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise FieldDerivationError(f"dpm rule table is not valid JSON: {error}") from error
    if data.get("dpm_rule_table_schema_version") != DPM_RULE_TABLE_SCHEMA_VERSION:
        raise FieldDerivationError(
            f"unsupported dpm_rule_table_schema_version {data.get('dpm_rule_table_schema_version')!r}")
    approval = data.get("approval") or {}
    status = approval.get("status")
    if status not in ("approved", "synthetic_unapproved"):
        raise FieldDerivationError("approval.status must be 'approved' or 'synthetic_unapproved'")
    if status != "approved" and not package_is_synthetic:
        raise FieldDerivationError(
            "rule table is not approved and the package is not synthetic: DPM values need data-owner approval (M281)")
    if status == "approved" and not (approval.get("approved_by") and approval.get("date")):
        raise FieldDerivationError("approved tables must carry approval.approved_by and approval.date")
    limits = {**DEFAULT_CLOSURE_LIMITS, **(data.get("closure_limits") or {})}
    classes = data.get("classes")
    if not isinstance(classes, dict) or not classes:
        raise FieldDerivationError("classes must be a non-empty object keyed by landcover class_code")
    normalized = {str(name): lint_class_entry(str(name), entry, limits) for name, entry in classes.items()}
    unmapped = data.get("unmapped_class", "fatal")
    if unmapped not in ("fatal", "default"):
        raise FieldDerivationError("unmapped_class must be 'fatal' or 'default'")
    default_class = None
    if unmapped == "default":
        if "default_class" not in data:
            raise FieldDerivationError("unmapped_class=default requires default_class")
        default_class = lint_class_entry("default_class", data["default_class"], limits)
        manning_default = data["default_class"].get("manning_n")
        if not isinstance(manning_default, (int, float)) or isinstance(manning_default, bool) or manning_default < 0:
            raise FieldDerivationError("default_class.manning_n (>= 0) is required: cells without a landcover polygon have no roughness source", "default_class")
        default_class["manning_n"] = float(manning_default)
    edges = data.get("edges") or {}
    default_omega = float(edges.get("default_omega", 1.0))
    boundary_omega = float(edges.get("boundary_omega", default_omega))
    for name, value in (("default_omega", default_omega), ("boundary_omega", boundary_omega)):
        if not 0.0 <= value <= 1.0:
            raise FieldDerivationError(f"edges.{name} = {value} outside [0, 1]")
    interfaces: dict[frozenset, float] = {}
    for index, item in enumerate(edges.get("interfaces") or []):
        pair = item.get("classes")
        if not isinstance(pair, list) or len(pair) != 2:
            raise FieldDerivationError("interfaces[].classes must list exactly two class codes", f"edges.interfaces[{index}]")
        for cls in pair:
            if str(cls) not in normalized:
                raise FieldDerivationError(f"interface references unknown class {cls!r}", f"edges.interfaces[{index}]")
        omega = float(item.get("omega_edge", default_omega))
        if not 0.0 <= omega <= 1.0:
            raise FieldDerivationError(f"omega_edge = {omega} outside [0, 1]", f"edges.interfaces[{index}]")
        key = frozenset(str(c) for c in pair)
        if key in interfaces:
            raise FieldDerivationError("duplicate interface pair", f"edges.interfaces[{index}]")
        interfaces[key] = omega
    overlap = data.get("landcover_overlap", "fatal")
    if overlap not in ("fatal", "smallest_area_wins"):
        raise FieldDerivationError("landcover_overlap must be 'fatal' or 'smallest_area_wins'")
    soil = data.get("soil") or {}
    soil_source = soil.get("source", "landcover")
    if soil_source not in ("soil_zones", "landcover"):
        raise FieldDerivationError("soil.source must be 'soil_zones' or 'landcover'")
    missing_zone = soil.get("missing_zone", "fatal")
    if missing_zone != "fatal" and not (isinstance(missing_zone, dict) and isinstance(missing_zone.get("soil_type"), int)):
        raise FieldDerivationError("soil.missing_zone must be 'fatal' or {'soil_type': <int>}")
    return {
        "path": str(path),
        "approval": {"status": status, "approved_by": approval.get("approved_by"), "date": approval.get("date")},
        "closure_limits": limits,
        "classes": normalized,
        "unmapped_class": unmapped,
        "default_class": default_class,
        "edges": {"default_omega": default_omega, "boundary_omega": boundary_omega, "interfaces": interfaces},
        "soil": {"source": soil_source, "missing_zone": missing_zone},
        "landcover_overlap": overlap,
    }


# --- derivation ---------------------------------------------------------------


def _point_in_ring(x: float, y: float, ring: list) -> bool:
    inside = False
    for i in range(len(ring) - 1):
        (x1, y1), (x2, y2) = ring[i], ring[i + 1]
        if (y1 > y) != (y2 > y) and x1 + (y - y1) * (x2 - x1) / (y2 - y1) > x:
            inside = not inside
    return inside


def _ring_area(ring: list) -> float:
    return abs(sum(ring[i][0] * ring[i + 1][1] - ring[i + 1][0] * ring[i][1] for i in range(len(ring) - 1))) / 2.0


def select_landcover(x: float, y: float, landcover: list[dict], rules: dict) -> tuple[dict | None, bool]:
    """Landcover polygon owning a centroid. Overlapping polygons of different
    classes are ambiguous: fatal by default; with landcover_overlap =
    smallest_area_wins the most specific (smallest) polygon is chosen and the
    overlap is counted in the report (v1 placeholder mode kept first-match)."""
    hits = [p for p in landcover if _point_in_ring(x, y, p["outer"])]
    if not hits:
        return None, False
    distinct = {str(p["properties"].get("class_code")) for p in hits}
    if len(distinct) <= 1:
        return hits[0], len(hits) > 1
    if rules["landcover_overlap"] == "fatal":
        raise FieldDerivationError(
            f"centroid ({x:g}, {y:g}) lies in overlapping landcover polygons of classes {sorted(distinct)}; "
            "declare landcover_overlap: smallest_area_wins in the rule table or fix the layer")
    return min(hits, key=lambda p: (_ring_area(p["outer"]), str(p["properties"].get("class_code")))), True


def derive_cell_fields(centroids, landcover: list[dict], rules: dict, soil_zones: list[dict] | None) -> dict:
    """Per-cell class, phi_t, Phi_c, soil_type and manning_n. `landcover` /
    `soil_zones` are polygon dicts as loaded by meshgen.load_geojson_polygons."""
    classes, phi_t, xx, xy, yy, soil_type, manning = [], [], [], [], [], [], []
    unmapped: list[int] = []
    missing_soil: list[int] = []
    overlaps = 0
    for index, (x, y) in enumerate(centroids):
        polygon, overlapped = select_landcover(x, y, landcover, rules)
        overlaps += int(overlapped)
        if polygon and "manning_n" in polygon["properties"]:
            manning.append(float(polygon["properties"]["manning_n"]))
        elif polygon is None and rules["default_class"] is not None:
            manning.append(rules["default_class"]["manning_n"])
        else:
            manning.append(math.nan)
        class_code = str(polygon["properties"].get("class_code")) if polygon else None
        entry = rules["classes"].get(class_code) if class_code is not None else None
        if entry is None:
            unmapped.append(index)
            if rules["unmapped_class"] == "fatal":
                classes.append(class_code)
                phi_t.append(math.nan); xx.append(math.nan); xy.append(math.nan); yy.append(math.nan)
                soil_type.append(-1)
                continue
            entry = rules["default_class"]
            class_code = class_code if class_code is not None else "<none>"
        classes.append(class_code)
        phi_t.append(entry["phi_t"])
        xx.append(entry["Phi_c"]["xx"]); xy.append(entry["Phi_c"]["xy"]); yy.append(entry["Phi_c"]["yy"])
        if rules["soil"]["source"] == "soil_zones":
            zone = next((z for z in (soil_zones or []) if _point_in_ring(x, y, z["outer"])), None)
            if zone is None:
                missing_soil.append(index)
                soil_type.append(rules["soil"]["missing_zone"]["soil_type"]
                                 if rules["soil"]["missing_zone"] != "fatal" else -1)
            else:
                soil_type.append(int(zone["properties"]["soil_type"]))
        else:
            soil_type.append(int(polygon["properties"]["soil_type"]) if polygon and "soil_type" in polygon["properties"] else -1)
    if unmapped and rules["unmapped_class"] == "fatal":
        sample = ", ".join(f"cell {i} (class {classes[i]!r})" for i in unmapped[:5])
        raise FieldDerivationError(f"{len(unmapped)} cell(s) have no rule-table class entry: {sample}")
    if missing_soil and rules["soil"]["missing_zone"] == "fatal":
        raise FieldDerivationError(f"{len(missing_soil)} cell(s) fall outside every soil zone: cells {missing_soil[:5]}")
    if any(s < 0 for s in soil_type):
        bad = [i for i, s in enumerate(soil_type) if s < 0]
        raise FieldDerivationError(f"{len(bad)} cell(s) have no soil_type (landcover polygons lack soil_type): cells {bad[:5]}")
    if any(math.isnan(v) for v in manning):
        bad = [i for i, v in enumerate(manning) if math.isnan(v)]
        raise FieldDerivationError(f"{len(bad)} cell(s) have no manning_n on their landcover polygon: cells {bad[:5]}")
    return {"class_code": classes, "phi_t": phi_t, "phi_xx": xx, "phi_xy": xy, "phi_yy": yy,
            "soil_type": soil_type, "manning_n": manning, "unmapped_cells": unmapped,
            "missing_soil_cells": missing_soil, "overlapping_landcover_cells": overlaps}


def derive_edge_fields(node_x, node_y, edge_nodes, edge_faces, cell_fields: dict, rules: dict) -> dict:
    """Spec 5.3 rule 2: arithmetic-mean edge tensor projected on the unit
    normal (phi_e_n) and tangent (phi_et), scaled by omega_edge."""
    omega_out, phi_e_n_out, phi_et_out = [], [], []
    interfaces = rules["edges"]["interfaces"]
    interface_edges = 0
    for (a, b), (left, right) in zip(edge_nodes, edge_faces):
        dx, dy = node_x[b] - node_x[a], node_y[b] - node_y[a]
        length = math.hypot(dx, dy)
        if length == 0.0:
            raise FieldDerivationError(f"degenerate edge ({a}, {b}) has zero length")
        tx, ty = dx / length, dy / length
        nx, ny = ty, -tx
        cells = [c for c in (left, right) if c >= 0]
        if not cells:
            raise FieldDerivationError(f"edge ({a}, {b}) has no adjacent face")
        if len(cells) == 2:
            pair = frozenset((cell_fields["class_code"][cells[0]], cell_fields["class_code"][cells[1]]))
            omega = interfaces.get(pair, rules["edges"]["default_omega"])
            if pair in interfaces:
                interface_edges += 1
        else:
            omega = rules["edges"]["boundary_omega"]
        exx = sum(cell_fields["phi_xx"][c] for c in cells) / len(cells)
        exy = sum(cell_fields["phi_xy"][c] for c in cells) / len(cells)
        eyy = sum(cell_fields["phi_yy"][c] for c in cells) / len(cells)
        # Unit-vector form of d^T Phi d: half_trace*(dx^2+dy^2) + half_diff*(dx^2-dy^2)
        # + 2 xy dx dy with dx^2+dy^2 == 1 by construction. Algebraically identical
        # to the naive quadratic form; for an isotropic tensor it is EXACTLY
        # half_trace, whereas the naive form yields 1.0000000000000002 on some
        # edges and the C++ file validator (phi_e_n <= 1, strict) rejects the case.
        half_trace = 0.5 * (exx + eyy)
        half_diff = 0.5 * (exx - eyy)
        quad_n = half_trace + half_diff * (nx * nx - ny * ny) + 2.0 * exy * nx * ny
        quad_t = half_trace + half_diff * (tx * tx - ty * ty) + 2.0 * exy * tx * ty
        omega_out.append(omega)
        phi_e_n_out.append(omega * quad_n)
        phi_et_out.append(omega * quad_t)
    return {"omega_edge": omega_out, "phi_e_n": phi_e_n_out, "phi_et": phi_et_out,
            "interface_edges": interface_edges}


def derivation_report(rules: dict, cell_fields: dict, edge_fields: dict) -> dict:
    by_class: dict[str, int] = {}
    for code in cell_fields["class_code"]:
        by_class[str(code)] = by_class.get(str(code), 0) + 1
    return {
        "field_derivation_schema_version": 1,
        "rule_table": rules["path"],
        "approval": rules["approval"],
        "closure_limits": rules["closure_limits"],
        "cells_by_class": by_class,
        "unmapped_cells_defaulted": len(cell_fields["unmapped_cells"]),
        "landcover_overlap_rule": rules["landcover_overlap"],
        "overlapping_landcover_cells": cell_fields["overlapping_landcover_cells"],
        "manning_source": "landcover polygon chosen by the same class selection (derived mode)",
        "soil_source": rules["soil"]["source"],
        "soil_missing_zone_defaulted": len(cell_fields["missing_soil_cells"]),
        "phi_t_range": [min(cell_fields["phi_t"]), max(cell_fields["phi_t"])],
        "edge_rule": "spec 5.3 rule 2: phi_e_n = omega * n^T mean(Phi_c) n; phi_et = omega * t^T mean(Phi_c) t",
        "interface_edges": edge_fields["interface_edges"],
        "phi_e_n_range": [min(edge_fields["phi_e_n"]), max(edge_fields["phi_e_n"])],
        "omega_edge_values": sorted({round(v, 12) for v in edge_fields["omega_edge"]}),
        "runtime_semantics": "none (fields only; closure certified by scau_preproc validate)",
    }
