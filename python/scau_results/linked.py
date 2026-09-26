"""Read-only 1D/2D linked view: CouplingLib ledger per link vs engine-native summaries.

The run summary's `link_exchanges` is the truth of what crossed each 2D<->1D
interface per committed epoch. SWMM's `.rpt` node tables are the engine's own
post-hoc view. This module joins them by node name; it never re-derives
physics and never treats the engine's numbers as a correction to the ledger.
D-Flow FM native results are consumed ONLY through the C3 provider contract the
driver froze into the summary (`dflowfm` block + per-epoch `dflowfm_native`):
identity, MDU bytes, time base and cumulative monotonicity are re-validated
here independently of the C++ validator, and the native api-lateral net is
compared against the river ledger as a diagnostic, never as a correction.

Every input must prove it belongs to THIS run before it is joined:
  * the surface result manifest is validated by the same code path as
    `scau_results summarize` (bytes, source STCF, state hash, epoch count),
  * the SWMM report must be the file the run produced, by byte hash,
  * the D-Flow FM MDU must be the file the run hashed, by byte hash,
  * report units must be SI as declared in the table headers, never assumed.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from pathlib import Path

M3_PER_MEGALITRE = 1000.0

REQUIRED_SUMMARY_FIELDS = ("outcome", "committed_epochs", "final_surface_state_hash",
                           "source_stcf_hash", "start_time", "dt_couple", "epochs")

# C3 frozen contract values (mirror libs/coupling/driver/.../dflowfm_provider_contract.hpp).
C3_PROVIDER_ID = "dflowfm"
C3_CAPABILITY = "dflowfm.external_net.v1"
C3_EXCHANGE_KIND = "api_lateral"
C3_NATIVE_FIELDS = ("storage_m3", "boundary_in_m3", "boundary_out_m3",
                    "api_lateral_in_m3", "api_lateral_out_m3", "volume_error_cumulative_m3")
C3_CUMULATIVE_FIELDS = ("boundary_in_m3", "boundary_out_m3", "api_lateral_in_m3", "api_lateral_out_m3")


# --- SWMM report -----------------------------------------------------------------

def _table(text: str, title: str) -> tuple[list[str], list[list[str]]]:
    """(header lines, rows) of a `.rpt` summary table. Header = lines between the
    first and second dashed rule; rows follow until a blank line."""
    start = text.find(f"\n  {title}\n")
    if start < 0:
        raise ValueError(f"swmm report lacks '{title}'")
    lines = text[start:].splitlines()[1:]
    rules = 0
    header, rows = [], []
    for line in lines:
        if re.fullmatch(r"\s*-{10,}\s*", line):
            rules += 1
            if rules == 2:
                continue
            if rules > 2:
                break
            continue
        if rules == 1:
            header.append(line)
        elif rules == 2:
            if not line.strip():
                break
            rows.append(line.split())
    return header, rows


def _require_si(header: list[str], title: str) -> None:
    """Fail closed unless the table declares metres / CMS / 10^6 ltr. SWMM writes
    Feet / CFS / 10^6 gal for US projects (text.h w_MGAL, VolUnitsWords)."""
    joined = " ".join(header)
    bad = [w for w in ("Feet", "CFS", "MGD", "GPM", "10^6 gal", "acre") if w in joined]
    if bad:
        raise ValueError(f"unit mismatch: {title} reports US units {bad}; only SI (Meters/CMS/10^6 ltr) is accepted")
    need = {"Node Depth Summary": ("Meters",), "Node Inflow Summary": ("CMS", "10^6 ltr")}[title]
    missing = [w for w in need if w not in joined]
    if missing:
        raise ValueError(f"unit mismatch: {title} header does not declare {missing}; refusing to assume SI")


def _continuity_error(text: str, section: str) -> float | None:
    """The 'Continuity Error (%)' line INSIDE the named section only."""
    start = text.find(f"\n  {section}")
    if start < 0:
        return None
    block = text[start:]
    hit = re.search(r"Continuity Error \(%\) \.+\s+(-?[\d.]+)", block)
    if not hit:
        return None
    # The match must belong to THIS section. A new section is an asterisk
    # banner followed by a title line; our own closing banner is followed by
    # a dotted data row ("Dry Weather Inflow ....") or a column rule, never a title.
    between = block[len(section) + 3: hit.start()]
    for m in re.finditer(r"\n  \*{5,}[^\n]*\n  ([^\n]*)", between):
        following = m.group(1).strip()
        if following and " ...." not in following and not re.match(r"[-\d]", following):
            return None
    return float(hit.group(1))


def parse_swmm_report(path: Path) -> dict:
    raw = path.read_bytes()
    # Hash the exact bytes; parse a newline-normalised view (SWMM on Windows
    # writes CRLF, and test fixtures written via text mode do too).
    text = raw.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    if "EPA STORM WATER MANAGEMENT MODEL" not in text:
        raise ValueError("not a SWMM report file")
    version = re.search(r"VERSION\s+([\d.]+)\s*\(Build\s+([\d.]+)\)", text)
    nodes: dict[str, dict] = {}
    header, rows = _table(text, "Node Depth Summary")
    _require_si(header, "Node Depth Summary")
    for row in rows:
        if len(row) < 5:
            raise ValueError(f"malformed Node Depth Summary row: {row}")
        nodes[row[0]] = {"type": row[1], "avg_depth_m": float(row[2]), "max_depth_m": float(row[3]),
                         "max_hgl_m": float(row[4])}
    header, rows = _table(text, "Node Inflow Summary")
    _require_si(header, "Node Inflow Summary")
    for row in rows:
        # name type max_lat max_tot day hh:mm lat_vol tot_vol balance_err [ltr]
        if len(row) < 9:
            raise ValueError(f"malformed Node Inflow Summary row: {row}")
        entry = nodes.setdefault(row[0], {"type": row[1]})
        entry["lateral_inflow_volume_m3"] = float(row[6]) * M3_PER_MEGALITRE
        entry["total_inflow_volume_m3"] = float(row[7]) * M3_PER_MEGALITRE
        entry["flow_balance_error_pct"] = float(row[8])
    return {"report_sha256": hashlib.sha256(raw).hexdigest(),
            "report_hash": fnv1a64(raw),
            "swmm_version": None if version is None else f"{version.group(1)} (Build {version.group(2)})",
            "runoff_continuity_error_pct": _continuity_error(text, "Runoff Quantity Continuity"),
            "routing_continuity_error_pct": _continuity_error(text, "Flow Routing Continuity"),
            # statsrpt.c writes volumes with %12.3g: three significant figures.
            "volume_precision": "3 significant figures (SWMM %12.3g), i.e. relative, not a fixed 1 m3",
            "nodes": nodes}


def fnv1a64(data: bytes) -> str:
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{h:016x}"


# --- ledger ----------------------------------------------------------------------

def _finite_nonneg(value, what: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
        raise ValueError(f"result validation failure: {what} must be a finite non-negative number, got {value!r}")
    return float(value)


def load_link_ledger(summary_path: Path) -> dict:
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    missing = [k for k in REQUIRED_SUMMARY_FIELDS if k not in summary or summary[k] in (None, "")]
    if missing:
        raise ValueError(f"run summary lacks required fields {missing} (older driver); re-run with the current driver")
    if summary["outcome"] != "completed":
        raise ValueError(f"run outcome is {summary['outcome']!r}; only completed runs are linked")
    epochs = summary["epochs"]
    declared = summary["committed_epochs"]
    if not isinstance(epochs, list) or not epochs or len(epochs) != declared:
        raise ValueError(f"result validation failure: committed_epochs={declared} but {len(epochs) if isinstance(epochs, list) else 'no'} epoch records")
    start, dt = float(summary["start_time"]), float(summary["dt_couple"])
    if not (math.isfinite(dt) and dt > 0):
        raise ValueError("result validation failure: dt_couple must be finite and positive")
    ledger: dict[tuple[str, str], dict] = {}
    net_ledger_drained = 0.0
    net_ledger_returned = 0.0
    previous_t = None
    for i, epoch in enumerate(epochs):
        if "link_exchanges" not in epoch:
            raise ValueError("run summary has no link_exchanges (older driver); re-run with the current driver")
        t = epoch.get("logical_time")
        if not isinstance(t, (int, float)) or not math.isfinite(t):
            raise ValueError(f"result validation failure: epoch {i} has non-finite logical_time")
        expected_t = start + (i + 1) * dt
        if abs(t - expected_t) > 1e-9 * max(1.0, abs(expected_t)):
            raise ValueError(f"compatibility error: epoch {i} logical_time {t} != start + (i+1)*dt_couple = {expected_t}")
        if previous_t is not None and t <= previous_t:
            raise ValueError(f"compatibility error: epoch {i} time {t} not strictly increasing")
        previous_t = t
        seen_in_epoch: set[tuple[str, str]] = set()
        epoch_in = epoch_out = 0.0
        for link in epoch["link_exchanges"]:
            key = (str(link["engine"]), str(link["node_name"]))
            if key in seen_in_epoch:
                raise ValueError(f"result validation failure: link {key} appears twice in epoch {i}")
            seen_in_epoch.add(key)
            granted = _finite_nonneg(link.get("v_granted"), f"{key} v_granted")
            repay = _finite_nonneg(link.get("v_repay"), f"{key} v_repay")
            returned = _finite_nonneg(link.get("v_returned"), f"{key} v_returned")
            entry = ledger.setdefault(key, {"engine": key[0], "node_name": key[1],
                                            "node": link["node"], "cell": link["cell"],
                                            "granted_m3": 0.0, "repay_m3": 0.0, "returned_m3": 0.0,
                                            "series": []})
            if entry["cell"] != link["cell"] or entry["node"] != link["node"]:
                raise ValueError(f"result validation failure: link {key} changed node/cell identity across epochs")
            entry["granted_m3"] += granted
            entry["repay_m3"] += repay
            entry["returned_m3"] += returned
            entry["series"].append({"t": t, "granted": granted, "repay": repay, "returned": returned})
            epoch_in += granted + repay
            epoch_out += returned
        # Gross per-link movements vs the epoch's NET write-back must agree on
        # the balance (the C++ integration test pins the same invariant).
        net = float(epoch.get("drained_volume", 0.0)) - float(epoch.get("returned_volume", 0.0))
        if abs((epoch_in - epoch_out) - net) > 1e-9 * max(1.0, abs(net)):
            raise ValueError(f"result validation failure: epoch {i} ledger balance {epoch_in - epoch_out} != net write-back {net}")
        net_ledger_drained += epoch_in
        net_ledger_returned += epoch_out
    total_net = float(summary.get("total_drained_volume", 0.0)) - float(summary.get("total_returned_volume", 0.0))
    if abs((net_ledger_drained - net_ledger_returned) - total_net) > 1e-9 * max(1.0, abs(total_net)):
        raise ValueError("result validation failure: run-total ledger balance disagrees with summary totals")
    return {"summary_sha256": hashlib.sha256(summary_path.read_bytes()).hexdigest(),
            "final_surface_state_hash": summary["final_surface_state_hash"],
            "source_stcf_hash": summary["source_stcf_hash"],
            "swmm_inp_hash": summary.get("swmm_inp_hash") or None,
            "swmm_report_path": summary.get("swmm_report_path") or None,
            "swmm_report_hash": summary.get("swmm_report_hash") or None,
            "committed_epochs": declared,
            "time_window_s": [start, previous_t],
            "start_time": start,
            "dt_couple": dt,
            "total_dflowfm_lateral_volume": summary.get("total_dflowfm_lateral_volume"),
            "dflowfm_contract": summary.get("dflowfm"),
            "dflowfm_native_series": [(float(e["logical_time"]), e.get("dflowfm_native")) for e in epochs],
            "links": list(ledger.values())}


# --- joins -----------------------------------------------------------------------

def _bind_surface_result(ledger: dict, result_manifest: Path) -> None:
    """Full result validation (same path as `summarize`), then run-identity match."""
    from scau_results.__main__ import summarize
    if not str(result_manifest).endswith(".manifest.json"):
        raise ValueError("linkage failure: --result-manifest must be the <run.nc>.manifest.json sidecar")
    result_nc = Path(str(result_manifest)[: -len(".manifest.json")])
    if not result_nc.is_file():
        raise ValueError(f"linkage failure: surface result {result_nc.name} does not exist beside its manifest")
    derived = summarize(result_nc, threshold=1.0)   # validation only; threshold irrelevant
    for key in ("final_surface_state_hash", "source_stcf_hash", "committed_epochs"):
        mine, theirs = ledger[key], derived[key]
        if mine in (None, "") or theirs in (None, "") or str(mine) != str(theirs):
            raise ValueError(f"linkage failure: surface result manifest belongs to a different run ({key} mismatch)")
    ledger["result_manifest_bound"] = True
    ledger["surface_result"] = {"path": str(result_nc), "output_hash": derived["source_hash"]}


def _bind_swmm_report(ledger: dict, swmm_report: Path) -> dict:
    report = parse_swmm_report(swmm_report)
    expected = ledger.get("swmm_report_hash")
    if not expected:
        raise ValueError("linkage failure: run summary records no SWMM report hash (mock run or older driver); "
                         "the report cannot be proven to belong to this run")
    if report["report_hash"] != expected:
        raise ValueError("linkage failure: SWMM report bytes do not match the hash recorded by this run "
                         f"(summary {expected}, file {report['report_hash']})")
    return report


# --- C3 D-Flow FM provider contract ------------------------------------------------

def _c3_reject(what: str) -> ValueError:
    return ValueError(f"LINKAGE_REJECTED ({what})")


def _bind_dflowfm_contract(ledger: dict, contract, mdu_path: Path | None) -> dict:
    """Re-validate the frozen C3 contract the driver wrote into the summary and
    bind the river input by bytes. Returns the validated contract view."""
    if not isinstance(contract, dict):
        raise _c3_reject("run summary carries no dflowfm contract block: river engine disabled or older driver")
    if contract.get("provider_id") != C3_PROVIDER_ID:
        raise _c3_reject(f"dflowfm_contract_provider_mismatch: {contract.get('provider_id')!r}")
    if contract.get("capability") != C3_CAPABILITY:
        raise _c3_reject(f"dflowfm_contract_capability_unsupported: {contract.get('capability')!r}")
    boundaries = contract.get("boundaries")
    if not isinstance(boundaries, list) or not boundaries:
        raise _c3_reject("dflowfm_contract_boundary_identity_missing: no boundaries frozen")
    seen_ids: set[str] = set()
    seen_objects: set[int] = set()
    for b in boundaries:
        bid, obj = b.get("boundary_id"), b.get("provider_object_id")
        if not bid or not isinstance(obj, int) or isinstance(obj, bool) or obj < 0:
            raise _c3_reject(f"dflowfm_contract_boundary_identity_invalid: {b!r}")
        if b.get("exchange_kind") != C3_EXCHANGE_KIND:
            raise _c3_reject(f"dflowfm_contract_exchange_kind_unsupported: {b.get('exchange_kind')!r}")
        if bid in seen_ids:
            raise _c3_reject(f"dflowfm_contract_boundary_id_duplicate: {bid!r}")
        if obj in seen_objects:
            raise _c3_reject(f"dflowfm_contract_provider_object_duplicate: {obj}")
        seen_ids.add(bid)
        seen_objects.add(obj)
    # Every river ledger link must be a frozen boundary with the SAME identity
    # triple (name, object, cell); a link the contract does not know is foreign.
    by_id = {b["boundary_id"]: b for b in boundaries}
    for link in ledger["links"]:
        if link["engine"] != "river":
            continue
        b = by_id.get(link["node_name"])
        if b is None:
            raise _c3_reject(f"river ledger link {link['node_name']!r} is not a frozen boundary")
        if b["provider_object_id"] != link["node"] or b.get("surface_cell") != link["cell"]:
            raise _c3_reject(f"river ledger link {link['node_name']!r} identity (object {link['node']}, cell "
                             f"{link['cell']}) differs from frozen boundary ({b['provider_object_id']}, {b.get('surface_cell')})")
    native_observed = bool(contract.get("native_observed"))
    recorded_hash = contract.get("mdu_hash") or None
    if native_observed and not recorded_hash:
        raise _c3_reject("dflowfm_contract_case_identity_missing: native scope bound without MDU hash")
    mdu_bound = False
    if mdu_path is not None:
        if not recorded_hash:
            raise _c3_reject("run recorded no MDU hash (mock run); the MDU cannot be proven to belong to this run")
        actual = fnv1a64(mdu_path.read_bytes())
        if actual != recorded_hash:
            raise _c3_reject(f"D-Flow FM MDU bytes do not match the hash recorded by this run (summary {recorded_hash}, file {actual})")
        mdu_bound = True
    return {"provider_id": contract["provider_id"], "capability": contract["capability"],
            "mdu_path": contract.get("mdu_path"), "mdu_hash": recorded_hash, "mdu_bound": mdu_bound,
            "native_observed": native_observed, "boundaries": boundaries,
            "flux_convention": contract.get("flux_convention"), "units": contract.get("units")}


def _validate_native_series(native_rows: list[tuple[float, dict | None]]) -> list[dict]:
    """Per-epoch native records: present at every committed epoch, finite,
    non-negative gross classes, cumulative classes non-decreasing. The time base
    was already pinned by load_link_ledger (t[i] == start + (i+1)*dt)."""
    series = []
    previous = None
    for i, (t, native) in enumerate(native_rows):
        if not isinstance(native, dict):
            raise _c3_reject(f"dflowfm_contract_native_observation_missing: epoch {i} has no native record")
        row = {"t": t}
        for key in C3_NATIVE_FIELDS:
            value = native.get(key)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise _c3_reject(f"dflowfm_contract_native_observation_invalid: epoch {i} {key}={value!r}")
            if key != "volume_error_cumulative_m3" and value < 0:
                raise _c3_reject(f"dflowfm_contract_native_observation_invalid: epoch {i} {key} negative")
            row[key] = float(value)
        if previous is not None:
            for key in C3_CUMULATIVE_FIELDS:
                if row[key] < previous[key]:
                    raise _c3_reject(f"dflowfm_contract_native_monotonicity_violated: epoch {i} {key} decreased")
        series.append(row)
        previous = row
    return series


def dflowfm_accounting(ledger: dict, series: list[dict]) -> dict:
    """C3-08: state the accounting definitions side by side and report the
    difference. Native api-lateral net = what the engine integrated from the
    lateral discharge the driver wrote; ledger river in-out = CouplingLib
    granted+repay minus returned over all river links. Same intended quantity,
    two integrators; a gap is reported, never reconciled."""
    ledger_in = sum(l["granted_m3"] + l["repay_m3"] for l in ledger["links"] if l["engine"] == "river")
    ledger_out = sum(l["returned_m3"] for l in ledger["links"] if l["engine"] == "river")
    final = series[-1]
    native_net = final["api_lateral_in_m3"] - final["api_lateral_out_m3"]
    ledger_net = ledger_in - ledger_out
    gap = native_net - ledger_net
    scale = max(1.0, abs(ledger_net))
    return {"definitions": {
                "native_api_lateral_net_m3": "cumulative engine-integrated api lateral in - out since initialize",
                "ledger_river_net_m3": "sum over river links of (v_granted + v_repay) - v_returned, per committed epoch",
                "boundary_net_m3": "cumulative open-boundary in - out (external to both ledger and surface)"},
            "native_api_lateral_in_m3": final["api_lateral_in_m3"],
            "native_api_lateral_out_m3": final["api_lateral_out_m3"],
            "native_api_lateral_net_m3": native_net,
            "ledger_river_in_m3": ledger_in, "ledger_river_out_m3": ledger_out, "ledger_river_net_m3": ledger_net,
            "summary_total_dflowfm_lateral_volume_m3": ledger.get("total_dflowfm_lateral_volume"),
            "boundary_in_m3": final["boundary_in_m3"], "boundary_out_m3": final["boundary_out_m3"],
            "boundary_net_m3": final["boundary_in_m3"] - final["boundary_out_m3"],
            "storage_final_m3": final["storage_m3"],
            "volume_error_cumulative_m3": final["volume_error_cumulative_m3"],
            "signed_gap_m3": gap,
            "diagnostic_code": "NO_GAP" if abs(gap) <= 1e-9 * scale else "LATERAL_INTEGRATION_GAP",
            "note": ("native and ledger agree to fp precision" if abs(gap) <= 1e-9 * scale else
                     "native lateral integration differs from the ledger; original values preserved, nothing corrected")}


def gap_diagnostics(link: dict, node: dict) -> dict:
    """Signed/relative gap plus a conservative attribution verdict. Diagnostics
    explain a discrepancy; they never reconcile, correct or hide it."""
    ledger_in = link["granted_m3"] + link["repay_m3"]
    rpt = node["lateral_inflow_volume_m3"]
    signed = rpt - ledger_in
    relative = signed / ledger_in if ledger_in > 0 else (0.0 if signed == 0 else math.inf)
    # %12.3g: the report value is only good to 3 significant figures.
    rounding_m3 = 0.5 * 10 ** (math.floor(math.log10(abs(rpt))) - 2) if rpt > 0 else 0.0
    code = "NO_GAP" if abs(signed) <= rounding_m3 else "ROUTING_GAP_ATTRIBUTION_INSUFFICIENT"
    return {"ledger_in_m3": ledger_in, "rpt_lateral_m3": rpt, "signed_gap_m3": signed,
            "relative_gap": relative, "rpt_rounding_half_ulp_m3": rounding_m3,
            "diagnostic_code": code,
            "note": ("difference within SWMM report rounding" if code == "NO_GAP" else
                     "discrepancy detected; evidence is insufficient to attribute it to a known "
                     "routing-continuity behaviour. SWMM's lateral total is a trapezoidal integral "
                     "(stats.c) while the ledger is per-epoch granted+repay; the accounting "
                     "definitions have not been reconciled. Original values preserved; nothing corrected.")}


def linked_view(summary_path: Path, swmm_report: Path | None, result_manifest: Path | None,
                dflowfm_mdu: Path | None = None) -> dict:
    ledger = load_link_ledger(summary_path)
    ledger["result_manifest_bound"] = False
    if result_manifest is not None:
        _bind_surface_result(ledger, result_manifest)
    report = _bind_swmm_report(ledger, swmm_report) if swmm_report is not None else None
    for link in ledger["links"]:
        link["engine_native"] = None
        link["gap"] = None
        if report is None or link["engine"] != "drainage":
            continue
        node = report["nodes"].get(link["node_name"])
        if node is None or "lateral_inflow_volume_m3" not in node:
            raise ValueError(f"linkage failure: SWMM report has no inflow row for node {link['node_name']!r}")
        link["engine_native"] = node
        link["gap"] = gap_diagnostics(link, node)
    ledger["swmm_report"] = None if report is None else {
        "path": str(swmm_report), "sha256": report["report_sha256"], "hash": report["report_hash"],
        "swmm_version": report["swmm_version"],
        "runoff_continuity_error_pct": report["runoff_continuity_error_pct"],
        "routing_continuity_error_pct": report["routing_continuity_error_pct"],
        "volume_precision": report["volume_precision"],
        "scope_note": "global model continuity error is NOT evidence about any single node's gap"}

    # C3: consume the native river result only through the frozen contract.
    contract_block = ledger.pop("dflowfm_contract")
    native_rows = ledger.pop("dflowfm_native_series")
    has_river_links = any(l["engine"] == "river" for l in ledger["links"])
    if contract_block is None and dflowfm_mdu is None and not has_river_links:
        ledger["dflowfm_native"] = None                     # river engine not part of this run
    else:
        ledger["dflowfm_contract_bound"] = False
        contract = _bind_dflowfm_contract(ledger, contract_block, dflowfm_mdu)
        ledger["dflowfm_contract_bound"] = True
        ledger["dflowfm_contract"] = contract
        if not contract["native_observed"]:
            ledger["dflowfm_native"] = {"status": "not_observed",
                                        "note": "run bound no native water balance (mock river); ledger-only river links"}
        else:
            series = _validate_native_series(native_rows)
            ledger["dflowfm_native"] = {"status": "provenance_validated" if contract["mdu_bound"] else
                                                  "series_validated_mdu_unbound",
                                        "series": series,
                                        "accounting": dflowfm_accounting(ledger, series),
                                        "scope_note": "aggregate native water balance; open boundaries carry no "
                                                      "per-object identity in this contract"}
    ledger.pop("total_dflowfm_lateral_volume", None)
    return ledger
