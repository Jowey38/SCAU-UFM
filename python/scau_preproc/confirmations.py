"""Human-confirmation contract for coupling candidates (M287-C4, pipeline stage E').

The coupling-map generator (coupling_maps.py) emits CANDIDATE relations and is
never edited by hand. Operator decisions live in an independent, versioned
input directory (``coupling/confirmed/*.json``, one file per decision):

    {
      "confirmation_schema_version": 1,
      "chain": "surface_to_swmm" | "roof_to_swmm",
      "mapping_id": "MAP_SURF_SYNTH_001",
      "decision": "accept" | "reject" | "retarget" | "create",
      "candidate_sha256": "<sha256 of the canonical candidate relation>",   // null only for create
      "confirmed_by": "operator id", "timestamp": "2026-09-03T10:00:00",
      "target": {"cell_index": 123, "exchange_elevation_m": 10.2},          // retarget / create
      "note": "free text"
    }

This module merges candidates with confirmations into the EFFECTIVE links:

  accept   -> candidate becomes effective as-is;
  retarget -> candidate becomes effective with the operator-chosen cell (and
              optionally exchange elevation); recorded as operator-decided;
  reject   -> candidate is dropped from the effective set;
  create   -> a new operator-authored link with no candidate (E4 "create link");
  (none)   -> candidate stays unconfirmed: it is NOT effective and blocks export.

Drift detection: a confirmation whose ``candidate_sha256`` no longer matches
the regenerated candidate is stale (the mesh, the SWMM file or the mapping
table changed underneath the operator). Stale confirmations are ignored, the
candidate reverts to unconfirmed, and a ``ConfirmationDrift`` review finding
is reported. Structural errors (unknown mapping id, bad schema, bad decision,
out-of-range cell) are fatal.

Outputs: ``effective_links.json`` and ``effective/simdriver_links.conf``
(SimDriver RuntimeConfig version 2 fragment holding ONLY effective ground
links). No runtime semantics are decided here (Q_limit / deficit /
arbitration remain CouplingLib-owned).
"""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

CONFIRMATION_SCHEMA_VERSION = 1
EFFECTIVE_LINKS_SCHEMA_VERSION = 1
CHAINS = ("surface_to_swmm", "roof_to_swmm")
DECISIONS = ("accept", "reject", "retarget", "create")
CONFIRMED_STATES = ("confirmed",)
_TIMESTAMP = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}")


class ConfirmationError(ValueError):
    """Fail-closed structural error in a confirmation file."""

    def __init__(self, path: Path | None, message: str) -> None:
        self.path = path
        super().__init__(f"{path.name if path else '<merge>'}: {message}")


def candidate_sha256(relation: dict) -> str:
    """Canonical hash of a candidate relation (sorted keys, compact JSON)."""
    payload = json.dumps(relation, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


# --- loading ------------------------------------------------------------------


def load_confirmations(directory: Path) -> list[dict]:
    """Reads every ``*.json`` under ``directory`` (sorted by name for
    determinism). Missing directory -> no confirmations (not an error)."""
    directory = Path(directory)
    if not directory.is_dir():
        return []
    confirmations = []
    for path in sorted(directory.glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as error:
            raise ConfirmationError(path, f"invalid JSON ({error})") from error
        confirmations.append(_validate_confirmation(path, data))
    return confirmations


def _validate_confirmation(path: Path, data: dict) -> dict:
    if not isinstance(data, dict):
        raise ConfirmationError(path, "confirmation must be a JSON object")
    if data.get("confirmation_schema_version") != CONFIRMATION_SCHEMA_VERSION:
        raise ConfirmationError(
            path, f"unsupported confirmation_schema_version "
                  f"{data.get('confirmation_schema_version')!r} (expected {CONFIRMATION_SCHEMA_VERSION})")
    chain = data.get("chain")
    if chain not in CHAINS:
        raise ConfirmationError(path, f"unknown chain {chain!r}")
    decision = data.get("decision")
    if decision not in DECISIONS:
        raise ConfirmationError(path, f"unknown decision {decision!r}")
    mapping_id = data.get("mapping_id")
    if not isinstance(mapping_id, str) or not mapping_id:
        raise ConfirmationError(path, "mapping_id must be a non-empty string")
    for key in ("confirmed_by", "timestamp"):
        if not isinstance(data.get(key), str) or not data[key]:
            raise ConfirmationError(path, f"{key} must be a non-empty string")
    if not _TIMESTAMP.match(data["timestamp"]):
        raise ConfirmationError(path, "timestamp must be ISO-8601 (YYYY-MM-DDTHH:MM:SS...)")
    digest = data.get("candidate_sha256")
    if decision == "create":
        if digest is not None:
            raise ConfirmationError(path, "create decisions must not carry candidate_sha256")
    elif not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ConfirmationError(path, "candidate_sha256 must be a 64-hex sha256")
    target = data.get("target")
    if decision in ("retarget", "create"):
        if not isinstance(target, dict) or not isinstance(target.get("cell_index"), int) \
                or isinstance(target.get("cell_index"), bool):
            raise ConfirmationError(path, f"{decision} requires target.cell_index (integer)")
        if "exchange_elevation_m" in target and not isinstance(
                target["exchange_elevation_m"], (int, float)):
            raise ConfirmationError(path, "target.exchange_elevation_m must be a number")
        if decision == "create":
            required = ("swmm_node_id",) if chain == "surface_to_swmm" else ("swmm_node_id", "building_id")
            for key in required + ("exchange_elevation_m",):
                if key not in target:
                    raise ConfirmationError(path, f"create on {chain} requires target.{key}")
    elif target not in (None, {}):
        raise ConfirmationError(path, f"{decision} must not carry a target")
    return {**data, "_source": str(path)}


# --- merge --------------------------------------------------------------------


def _cell_key(chain: str) -> str:
    return "cell_index" if chain == "surface_to_swmm" else "overflow_target_cell_index"


def merge(candidates: dict[str, list[dict]], confirmations: list[dict],
          cell_count: int, placeholders: dict) -> dict:
    """Merges candidate relations (``{chain: [relation, ...]}``) with
    confirmations. Returns ``{"effective": {...}, "report": {...},
    "findings": [...]}``; structural errors raise ConfirmationError."""
    by_key: dict[tuple[str, str], list[dict]] = {}
    for confirmation in confirmations:
        by_key.setdefault((confirmation["chain"], confirmation["mapping_id"]), []).append(confirmation)
    for key, group in by_key.items():
        if len(group) > 1:
            raise ConfirmationError(
                None, f"{key[0]}/{key[1]} has {len(group)} confirmations "
                      f"({', '.join(Path(c['_source']).name for c in group)}); exactly one allowed")

    findings: list[dict] = []
    effective: dict[str, list[dict]] = {chain: [] for chain in CHAINS}
    counts = {chain: {"candidates": 0, "accepted": 0, "retargeted": 0, "rejected": 0,
                      "created": 0, "unconfirmed": 0, "drifted": 0} for chain in CHAINS}
    consumed: set[tuple[str, str]] = set()

    for chain in CHAINS:
        cell_key = _cell_key(chain)
        for relation in candidates.get(chain, []):
            counts[chain]["candidates"] += 1
            mapping_id = relation["mapping_id"]
            digest = candidate_sha256(relation)
            confirmation = (by_key.get((chain, mapping_id)) or [None])[0]
            if confirmation is None:
                counts[chain]["unconfirmed"] += 1
                continue
            consumed.add((chain, mapping_id))
            if confirmation["decision"] == "create":
                raise ConfirmationError(
                    Path(confirmation["_source"]),
                    f"create decision reuses existing candidate mapping_id {mapping_id}")
            if confirmation["candidate_sha256"] != digest:
                counts[chain]["drifted"] += 1
                counts[chain]["unconfirmed"] += 1
                findings.append({
                    "severity": "review",
                    "code": "ConfirmationDrift",
                    "detail": f"{chain}/{mapping_id}: candidate changed since confirmation "
                              f"({Path(confirmation['_source']).name}); decision ignored, "
                              "candidate reverted to unconfirmed",
                    "objects": [{"feature_id": mapping_id, "kind": f"coupling:{chain}",
                                 "violation": "candidate_sha256_mismatch"}],
                })
                continue
            decision = confirmation["decision"]
            if decision == "reject":
                counts[chain]["rejected"] += 1
                continue
            link = dict(relation)
            if decision == "retarget":
                target = confirmation["target"]
                cell = target["cell_index"]
                if not 0 <= cell < cell_count:
                    raise ConfirmationError(
                        Path(confirmation["_source"]),
                        f"target.cell_index {cell} outside mesh (cells={cell_count})")
                link[cell_key] = cell
                if "exchange_elevation_m" in target:
                    link["exchange_elevation_m"] = float(target["exchange_elevation_m"])
                link["method"] = f"{relation['method']}+operator_retarget"
                counts[chain]["retargeted"] += 1
            else:
                counts[chain]["accepted"] += 1
            link.update({
                "confidence": "confirmed",
                "review_status": "confirmed",
                "decision": decision,
                "candidate_sha256": digest,
                "confirmed_by": confirmation["confirmed_by"],
                "confirmed_at": confirmation["timestamp"],
                "confirmation_file": Path(confirmation["_source"]).name,
            })
            effective[chain].append(link)

    for confirmation in confirmations:
        key = (confirmation["chain"], confirmation["mapping_id"])
        if key in consumed:
            continue
        if confirmation["decision"] != "create":
            raise ConfirmationError(
                Path(confirmation["_source"]),
                f"{confirmation['decision']} refers to unknown candidate {key[0]}/{key[1]}")
        chain = confirmation["chain"]
        target = confirmation["target"]
        cell = target["cell_index"]
        if not 0 <= cell < cell_count:
            raise ConfirmationError(
                Path(confirmation["_source"]),
                f"target.cell_index {cell} outside mesh (cells={cell_count})")
        link = {
            "mapping_id": confirmation["mapping_id"],
            "swmm_node_id": target["swmm_node_id"],
            _cell_key(chain): cell,
            "exchange_elevation_m": float(target["exchange_elevation_m"]),
            "method": "operator_created",
            "confidence": "confirmed",
            "review_status": "confirmed",
            "decision": "create",
            "candidate_sha256": None,
            "confirmed_by": confirmation["confirmed_by"],
            "confirmed_at": confirmation["timestamp"],
            "confirmation_file": Path(confirmation["_source"]).name,
        }
        if chain == "roof_to_swmm":
            link["building_id"] = target["building_id"]
        for key in ("roof_drain_id", "inlet_id", "surface_zone_id", "catchment_area_m2"):
            if key in target:
                link[key] = target[key]
        effective[chain].append(link)
        counts[chain]["created"] += 1

    for chain in CHAINS:
        effective[chain].sort(key=lambda r: r["mapping_id"])
    unconfirmed_total = sum(c["unconfirmed"] for c in counts.values())
    status = "complete" if unconfirmed_total == 0 else "incomplete"
    report = {
        "effective_links_schema_version": EFFECTIVE_LINKS_SCHEMA_VERSION,
        "status": status,
        "unconfirmed_total": unconfirmed_total,
        "confirmations_loaded": len(confirmations),
        "chains": counts,
        "placeholders": dict(placeholders),
        "runtime_semantics": "none (Q_limit/deficit/arbitration remain CouplingLib-owned)",
    }
    return {"effective": effective, "report": report, "findings": findings}


# --- writers ------------------------------------------------------------------


def simdriver_fragment(effective_surface: list[dict], placeholders: dict) -> str:
    lines = ["version = 2",
             "# effective ground links only (M287-C4 confirmed); candidates live in ../simdriver_links.conf"]
    width = placeholders.get("exchange_width_m", 1.0)
    weight = placeholders.get("priority_weight", 1.0)
    for link in effective_surface:
        lines.append(
            f"surface_drainage_link = cell={link['cell_index']},node={link['swmm_node_id']},"
            f"crest={link['exchange_elevation_m']},width={width},weight={weight}")
    return "\n".join(lines) + "\n"


def write_effective(out_dir: Path, merged: dict, sources: dict) -> dict:
    """Writes effective_links.json + effective/simdriver_links.conf; returns paths."""
    out_dir = Path(out_dir)
    effective_dir = out_dir / "effective"
    effective_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        **merged["report"],
        "sources": sources,
        "links": merged["effective"],
    }
    links_path = out_dir / "effective_links.json"
    links_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    conf_path = effective_dir / "simdriver_links.conf"
    conf_path.write_text(
        simdriver_fragment(merged["effective"]["surface_to_swmm"], merged["report"]["placeholders"]),
        encoding="utf-8")
    return {"effective_links": str(links_path), "simdriver_fragment": str(conf_path)}


def candidates_from_mapping_dir(coupling_dir: Path) -> tuple[dict[str, list[dict]], dict]:
    """Loads the generator's candidate relations + placeholders from a coupling dir."""
    coupling_dir = Path(coupling_dir)
    files = {"surface_to_swmm": "surface_swmm_mapping.json", "roof_to_swmm": "roof_drain_mapping.json"}
    candidates = {}
    for chain, name in files.items():
        data = json.loads((coupling_dir / name).read_text(encoding="utf-8"))
        candidates[chain] = data.get("relations", [])
    report = json.loads((coupling_dir / "mapping_report.json").read_text(encoding="utf-8"))
    return candidates, report.get("placeholders", {})


def run(coupling_dir: Path, confirmations_dir: Path | None, cell_count: int) -> dict:
    """Stage E' entry: merge + write. Returns {report, findings, paths}."""
    candidates, placeholders = candidates_from_mapping_dir(coupling_dir)
    confirmations = load_confirmations(confirmations_dir) if confirmations_dir else []
    merged = merge(candidates, confirmations, cell_count, placeholders)
    sources = {
        "confirmations_dir": str(confirmations_dir) if confirmations_dir else None,
        "confirmation_files": {
            Path(c["_source"]).name: hashlib.sha256(Path(c["_source"]).read_bytes()).hexdigest()
            for c in confirmations
        },
        "candidate_files": {
            name: hashlib.sha256((Path(coupling_dir) / name).read_bytes()).hexdigest()
            for name in ("surface_swmm_mapping.json", "roof_drain_mapping.json")
        },
    }
    paths = write_effective(coupling_dir, merged, sources)
    return {"report": merged["report"], "findings": merged["findings"], "paths": paths}
