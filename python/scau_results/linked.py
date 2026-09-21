"""Read-only 1D/2D linked view: CouplingLib ledger per link vs engine-native summaries.

The run summary's `link_exchanges` is the truth of what crossed each 2D<->1D
interface per committed epoch. SWMM's `.rpt` node tables are the engine's own
post-hoc view. This module joins them by node name; it never re-derives
physics and never treats the engine's numbers as a correction to the ledger.
D-Flow FM native results are not consumed (C3 provider contract incomplete).
"""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

M3_PER_MEGALITRE = 1000.0


def _table(text: str, title: str) -> list[list[str]]:
    """Rows of a `.rpt` summary table: after the title, skip header lines until
    the second dashed rule, then read until a blank line."""
    start = text.find(f"\n  {title}\n")
    if start < 0:
        raise ValueError(f"swmm report lacks '{title}'")
    lines = text[start:].splitlines()[1:]
    rules = 0
    rows = []
    for line in lines:
        if re.fullmatch(r"\s*-{10,}\s*", line):
            rules += 1
            continue
        if rules < 2:
            continue
        if not line.strip():
            break
        rows.append(line.split())
    return rows


def parse_swmm_report(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    if "EPA STORM WATER MANAGEMENT MODEL" not in text:
        raise ValueError("not a SWMM report file")
    nodes: dict[str, dict] = {}
    for row in _table(text, "Node Depth Summary"):
        name, kind, avg, mx, hgl = row[0], row[1], row[2], row[3], row[4]
        nodes[name] = {"type": kind, "avg_depth_m": float(avg), "max_depth_m": float(mx), "max_hgl_m": float(hgl)}
    for row in _table(text, "Node Inflow Summary"):
        name = row[0]
        # ... max_lateral max_total day hh:mm lateral_vol total_vol balance_err
        entry = nodes.setdefault(name, {"type": row[1]})
        entry["lateral_inflow_volume_m3"] = float(row[6]) * M3_PER_MEGALITRE
        entry["total_inflow_volume_m3"] = float(row[7]) * M3_PER_MEGALITRE
        entry["flow_balance_error_pct"] = float(row[8])
    cont = re.search(r"Continuity Error \(%\) \.+\s+(-?[\d.]+)", text)
    return {"report_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "routing_continuity_error_pct": float(cont.group(1)) if cont else None,
            "nodes": nodes}


def load_link_ledger(summary_path: Path) -> dict:
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    if summary.get("outcome") != "completed":
        raise ValueError(f"run outcome is {summary.get('outcome')!r}; only completed runs are linked")
    epochs = summary.get("epochs") or []
    if not epochs or "link_exchanges" not in epochs[0]:
        raise ValueError("run summary has no link_exchanges (older driver); re-run with the current driver")
    ledger: dict[tuple[str, str], dict] = {}
    for epoch in epochs:
        for link in epoch["link_exchanges"]:
            key = (link["engine"], link["node_name"])
            entry = ledger.setdefault(key, {"engine": link["engine"], "node_name": link["node_name"],
                                            "node": link["node"], "cell": link["cell"],
                                            "granted_m3": 0.0, "repay_m3": 0.0, "returned_m3": 0.0,
                                            "series": []})
            if entry["cell"] != link["cell"]:
                raise ValueError(f"link {key} changed cell across epochs")
            entry["granted_m3"] += link["v_granted"]
            entry["repay_m3"] += link["v_repay"]
            entry["returned_m3"] += link["v_returned"]
            entry["series"].append({"t": epoch["logical_time"], "granted": link["v_granted"],
                                    "repay": link["v_repay"], "returned": link["v_returned"]})
    return {"summary_sha256": hashlib.sha256(summary_path.read_bytes()).hexdigest(),
            "final_surface_state_hash": summary.get("final_surface_state_hash"),
            "committed_epochs": summary.get("committed_epochs"),
            "links": list(ledger.values())}


def linked_view(summary_path: Path, swmm_report: Path | None, result_manifest: Path | None) -> dict:
    ledger = load_link_ledger(summary_path)
    if result_manifest is not None:
        manifest = json.loads(result_manifest.read_text(encoding="utf-8"))
        if manifest.get("final_surface_state_hash") != ledger["final_surface_state_hash"]:
            raise ValueError("linkage failure: surface result manifest belongs to a different run "
                             "(final_surface_state_hash mismatch)")
        ledger["result_manifest_bound"] = True
    report = parse_swmm_report(swmm_report) if swmm_report is not None else None
    for link in ledger["links"]:
        link["engine_native"] = None
        if report is None or link["engine"] != "drainage":
            continue
        node = report["nodes"].get(link["node_name"])
        if node is None:
            raise ValueError(f"linkage failure: SWMM report has no node {link['node_name']!r}")
        link["engine_native"] = node
        # Ledger lateral into the node vs SWMM's own accumulated lateral inflow.
        # The rpt volume is rounded to 0.001 x 10^6 L = 1 m3, so the comparison
        # is a diagnostic gap, never a tolerance that corrects the ledger.
        ledger_in = link["granted_m3"] + link["repay_m3"]
        link["lateral_gap_m3"] = node.get("lateral_inflow_volume_m3", float("nan")) - ledger_in
        link["lateral_gap_note"] = "rpt lateral volume is rounded to 1 m3"
    ledger["swmm_report"] = None if report is None else {
        "sha256": report["report_sha256"],
        "routing_continuity_error_pct": report["routing_continuity_error_pct"]}
    ledger["dflowfm_native"] = "not consumed: C3 provider contract incomplete"
    return ledger
