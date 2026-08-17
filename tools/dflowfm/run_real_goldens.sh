#!/usr/bin/env bash
# SCAU-UFM real D-Flow FM Phase Gateway.
#
# Runs the eight real-runtime Goldens (G11, G16, G17, checkpoint reload,
# G27, G20, G18, G19)
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
# G27 external-net contract golden: the open-boundary case exercises both
# boundary directions; the closed case exercises the API-lateral dedup leg.
# MDU names stay relative because the test chdirs into each case dir (long
# absolute MDU paths silently disable native output in this build).
OPEN_CASE_DIR="$REPO_ROOT/spikes/dflowfm/cases/single_reach_open_boundary"
export SCAU_DFLOWFM_G27_OPEN_CASE_DIR="$OPEN_CASE_DIR"
export SCAU_DFLOWFM_G27_OPEN_MDU="single_reach_open.mdu"
export SCAU_DFLOWFM_G27_CLOSED_CASE_DIR="$CASE_DIR"
export SCAU_DFLOWFM_G27_CLOSED_MDU="single_reach.mdu"
# G20 long-run policy golden: 10,000 x 60 s + restart replay on the authored
# open-boundary long-run case (adds ~5-10 min to the gateway).
export SCAU_DFLOWFM_G20_CASE_DIR="$OPEN_CASE_DIR"
export SCAU_DFLOWFM_G20_MDU="single_reach_open_longrun.mdu"
export SCAU_DFLOWFM_G20_RESTART_MDU="single_reach_open_longrun_restart300000.mdu"

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
rm -rf "$OPEN_CASE_DIR"/DFM_OUTPUT_* "$OPEN_CASE_DIR"/*.cache

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

# G27 manages its own case-directory cwd via the SCAU_DFLOWFM_G27_* env vars.
run_test dflowfm_external_net

# G20 manages its own case-directory cwd via the SCAU_DFLOWFM_G20_* env vars.
run_test dflowfm_longrun_10000

# G19 is the first golden whose executable statically imports the vcpkg
# netcdf.dll (STCF I/O) AND loads the D-Flow FM runtime. The Windows loader
# resolves netcdff.dll's netcdf dependency against the already-loaded module
# by base name, and the vcpkg build lacks the nc_*_chunking_ints exports the
# runtime needs (error 127). Deploy the runtime's netcdf.dll beside the G19
# executable so one netcdf serves both; it is a verified superset of the 24
# nc_* imports the executable needs.
G19_EXE=$(find_test surface2d_tri_coupling_real)
cp "$RUNTIME_BIN/netcdf.dll" "$(dirname "$G19_EXE")/netcdf.dll"
run_test surface2d_tri_coupling_real

# Restore the vcpkg-deployed netcdf.dll: the runtime copy resolves its own
# dependencies only with RUNTIME_BIN on PATH, so leaving it in place makes a
# later plain (env-less) ctest invocation of G19 hang on a loader error
# dialog instead of skipping. Best-effort: hosted CI always builds fresh.
for vcpkg_netcdf in \
    "$BUILD_DIR/vcpkg_installed/x64-windows/debug/bin/netcdf.dll" \
    "$BUILD_DIR/vcpkg_installed/x64-windows/bin/netcdf.dll"; do
    if [ -f "$vcpkg_netcdf" ]; then
        cp "$vcpkg_netcdf" "$(dirname "$G19_EXE")/netcdf.dll" || true
        break
    fi
done

echo "OK real D-Flow FM phase gateway: 8/8 goldens passed"
