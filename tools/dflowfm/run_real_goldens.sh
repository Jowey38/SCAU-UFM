#!/usr/bin/env bash
# SCAU-UFM real D-Flow FM Phase Gateway.
#
# Runs the five real-runtime Goldens (G11, G16, G17, checkpoint reload, G18)
# against the authored single_reach_1d case in one reproducible command.
# This is the local/release gateway and the exact sequence the future
# self-hosted CI job executes.
#
# Usage:
#   SCAU_DFLOWFM_LIBRARY=/path/to/dflowfm.dll \
#     tools/dflowfm/run_real_goldens.sh <build-dir> [config]
#
# Requirements:
#   - SCAU_DFLOWFM_LIBRARY points at the real BMI DLL; its dependent runtime
#     DLLs live in the same directory (that directory is prepended to PATH).
#   - <build-dir> already contains the built golden test executables.
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <build-dir> [config]" >&2
    exit 2
fi

# Normalize to an absolute path: tests are executed with cwd at the case
# directory, so a caller-relative build dir must be resolved up front.
BUILD_DIR=$(cd "$1" && pwd)
CONFIG=${2:-Debug}
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CASE_DIR="$REPO_ROOT/spikes/dflowfm/cases/single_reach_1d"

if [ -z "${SCAU_DFLOWFM_LIBRARY:-}" ] || [ ! -f "$SCAU_DFLOWFM_LIBRARY" ]; then
    echo "error: SCAU_DFLOWFM_LIBRARY must point at the real dflowfm BMI library" >&2
    exit 2
fi

RUNTIME_BIN=$(cd "$(dirname "$SCAU_DFLOWFM_LIBRARY")" && pwd)
export PATH="$RUNTIME_BIN:$PATH"
# Use the MDU file name relative to the case directory: every golden below is
# executed with cwd=CASE_DIR, and this real D-Flow build silently skips
# map/his/rst output (and truncates cache naming) when initialized through a
# long absolute MDU path. The relative form keeps native restart output alive,
# which the checkpoint-reload golden depends on.
export SCAU_DFLOWFM_G11_MDU="single_reach.mdu"
# RefDate is fixed in the authored MDU, so the 600 s native checkpoint name is
# deterministic. The river-steady run below regenerates it before reload runs.
export SCAU_DFLOWFM_G11_CHECKPOINT_600="$CASE_DIR/DFM_OUTPUT_single_reach/single_reach_20260723_001000_rst.nc"

find_test() {
    local name=$1
    local candidate
    for candidate in \
        "$BUILD_DIR/tests/golden/$name/$CONFIG/test_$name.exe" \
        "$BUILD_DIR/tests/golden/$name/$CONFIG/test_$name" \
        "$BUILD_DIR/tests/golden/$name/test_$name.exe" \
        "$BUILD_DIR/tests/golden/$name/test_$name"; do
        if [ -x "$candidate" ] || [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    echo "error: built test executable not found for $name under $BUILD_DIR" >&2
    return 1
}

run_test() {
    local name=$1
    local exe
    exe=$(find_test "$name")
    echo "=== real golden: $name"
    (cd "$CASE_DIR" && "$exe" --gtest_color=no)
}

rm -rf "$CASE_DIR"/DFM_OUTPUT_* "$CASE_DIR"/*.cache

# Order matters: the first run regenerates the deterministic 600 s checkpoint
# consumed by the checkpoint-reload Golden.
run_test dflowfm_river_steady
run_test dflowfm_lateral_response
run_test dual_engine_shared_cell_real_both
run_test tri_coupling_real_minimal

if [ ! -f "$SCAU_DFLOWFM_G11_CHECKPOINT_600" ]; then
    echo "error: expected 600 s checkpoint was not produced: $SCAU_DFLOWFM_G11_CHECKPOINT_600" >&2
    exit 1
fi
run_test dflowfm_checkpoint_reload

echo "OK real D-Flow FM phase gateway: 5/5 goldens passed"
