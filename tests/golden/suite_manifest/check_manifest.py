#!/usr/bin/env python3
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "tests" / "golden" / "suite_manifest" / "goldensuite.json"
TESTS_CMAKELISTS = ROOT / "tests" / "golden" / "CMakeLists.txt"

REQUIRED = {
    "G1": ("hydrostatic_step", "implemented", True),
    "G2": ("phi_t_jump_hydrostatic", "implemented", True),
    "G3": ("phi_c_edge_zero_velocity", "implemented", True),
    "G4": ("narrow_gap_blockage", "implemented", True),
    "G5": ("dpm_drag_decay", "implemented", True),
    "G6": ("phi_c_spd_reject", "implemented", True),
    "G7": ("stcf_v4_to_v5_migration", "pending", False),
    "G8": ("swmm_single_pipe_surcharge", "implemented", True),
    "G9": ("cpu_gpu_deterministic_match", "pending", False),
    "G10": ("snapshot_replay_mass_deficit", "implemented", False),
    "G11": ("dflowfm_river_steady", "pending", False),
    "G12": ("dual_engine_shared_cell", "pending", False),
    "G13": ("runoff_urban_block", "implemented", True),
    "G14": ("roof_swmm_transfer", "implemented", True),
}


def fail(msg: str) -> None:
    print(f"::error::{msg}")
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

    for test_id, (name, status, ci_gate) in REQUIRED.items():
        if test_id not in entries:
            fail(f"missing manifest entry for {test_id}")
        item = entries[test_id]
        if item.get("name") != name:
            fail(f"{test_id}: expected name {name}, got {item.get('name')}")
        expected_path = f"tests/golden/{name}/"
        if item.get("test_path") != expected_path:
            fail(f"{test_id}: expected test_path {expected_path}, got {item.get('test_path')}")
        if item.get("status") != status:
            fail(f"{test_id}: expected status {status}, got {item.get('status')}")
        if item.get("ci_gate") is not ci_gate:
            fail(f"{test_id}: expected ci_gate {ci_gate}, got {item.get('ci_gate')}")
        if item.get("reference_path") != "tests/golden/reference/tolerances.md":
            fail(f"{test_id}: expected canonical reference_path")
        if item.get("applicable_phase") is None:
            fail(f"{test_id}: missing applicable_phase")

        # Same-name/path invariant.
        expected_dir = ROOT / "tests" / "golden" / name
        if status == "implemented" and not expected_dir.is_dir():
            fail(f"{test_id}: implemented test directory missing: {expected_dir}")

    # No extra unknown IDs.
    unknown = sorted(set(entries) - set(REQUIRED))
    if unknown:
        fail(f"unknown manifest test_id(s): {', '.join(unknown)}")

        # For every active implemented entry, some tests/golden/**/CMakeLists.txt
    # must register the add_test token, and at least one golden CMakeLists must
    # set the LABELS golden property.
    cmake_texts = []
    if TESTS_CMAKELISTS.exists():
        cmake_texts.append(TESTS_CMAKELISTS.read_text(encoding="utf-8"))
    golden_cmake_by_name = {}
    for path in sorted((ROOT / "tests" / "golden").glob("*/CMakeLists.txt")):
        golden_cmake_by_name[path.parent.name] = path.read_text(encoding="utf-8")
        cmake_texts.append(golden_cmake_by_name[path.parent.name])
    combined_cmake = "\n".join(cmake_texts)
    for test_id, (name, status, ci_gate) in REQUIRED.items():
        if status == "implemented":
            token = f"add_test(NAME test_{name}"
            if token not in combined_cmake:
                fail(f"{test_id}: implemented test missing add_test token {token}")
        if status == "implemented" and ci_gate:
            per_test_cmake = golden_cmake_by_name.get(name, "")
            if "LABELS golden" not in per_test_cmake:
                fail(f"{test_id}: active gate test CMakeLists must set LABELS golden")

    print("OK goldensuite manifest completeness passes")


if __name__ == "__main__":
    main()
