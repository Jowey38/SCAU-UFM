"""Read-only 1D/2D linked view: CouplingLib ledger per link vs engine-native summaries.

The run summary's `link_exchanges` is the truth of what crossed each 2D<->1D
interface per committed epoch. SWMM's `.rpt` node tables are the engine's own
post-hoc view. This module joins them by node name; it never re-derives
physics and never treats the engine's numbers as a correction to the ledger.
D-Flow FM native results are not consumed (C3 provider contract incomplete).

Every input must prove it belongs to THIS run before it is joined:
  * the surface result manifest is validated by the same code path as
    `scau_results summarize` (bytes, source STCF, state hash, epoch count),
  * the SWMM report must be the file the run produced, by byte hash,
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


def linked_view(summary_path: Path, swmm_report: Path | None, result_manifest: Path | None) -> dict:
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
    ledger["dflowfm_native"] = "not consumed: C3 provider contract incomplete"
    return ledger
