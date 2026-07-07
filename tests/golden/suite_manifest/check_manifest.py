#!/usr/bin/env python3
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "tests" / "golden" / "suite_manifest" / "goldensuite.json"
GOLDEN_CMAKELISTS = ROOT / "tests" / "golden" / "CMakeLists.txt"
RUNOFF_CMAKELISTS = ROOT / "tests" / "golden" / "runoff_urban_block" / "CMakeLists.txt"

REQUIRED = {
    "M247-EF": {
        "name": "runoff_urban_block",
        "status": "implemented",
        "ci_gate": True,
        "reference_path": "superpowers/specs/2026-06-21-m247e-golden-runoff-urban-block-evidence.md",
        "applicable_phase": "Phase 1+",
    },
}


def fail(message: str) -> None:
    print(f"::error::{message}")
    sys.exit(1)


def main() -> None:
    if not MANIFEST.exists():
        fail(f"missing manifest: {MANIFEST}")
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        fail("manifest root must be a JSON array")

    entries = {}
    for item in data:
        if not isinstance(item, dict):
            fail("manifest entries must be JSON objects")
        test_id = item.get("test_id")
        if test_id in entries:
            fail(f"duplicate test_id: {test_id}")
        entries[test_id] = item

    unknown = sorted(set(entries) - set(REQUIRED))
    if unknown:
        fail(f"unknown manifest test_id(s): {', '.join(unknown)}")

    for test_id, expected in REQUIRED.items():
        item = entries.get(test_id)
        if item is None:
            fail(f"missing manifest entry for {test_id}")
        name = expected["name"]
        if item.get("name") != name:
            fail(f"{test_id}: expected name {name}, got {item.get('name')}")
        if item.get("test_path") != f"tests/golden/{name}/":
            fail(f"{test_id}: invalid test_path {item.get('test_path')}")
        for key in ("status", "ci_gate", "reference_path", "applicable_phase"):
            if item.get(key) != expected[key]:
                fail(f"{test_id}: expected {key}={expected[key]}, got {item.get(key)}")
        if item["status"] == "implemented" and not (ROOT / item["test_path"]).is_dir():
            fail(f"{test_id}: implemented test directory missing: {item['test_path']}")

    cmake_text = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (GOLDEN_CMAKELISTS, RUNOFF_CMAKELISTS)
        if path.exists()
    )
    if "add_test(NAME test_golden_runoff_urban_block" not in cmake_text:
        fail("M247-EF: missing test_golden_runoff_urban_block add_test registration")
    if "LABELS \"golden;runoff;m247\"" not in cmake_text:
        fail("M247-EF: missing golden/runoff/m247 test labels")
    if "add_test(\n    NAME test_goldensuite_manifest" not in cmake_text:
        fail("manifest check must be registered as a CTest test")

    print("OK GoldenSuite M247 runoff manifest completeness passes")


if __name__ == "__main__":
    main()
