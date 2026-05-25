# M29 Third-Party Spike Host Skeleton Design

**Date:** 2026-05-25

## Goal

Draft the spike host source files and isolated CMake scaffolding under `spikes/swmm/` and `spikes/dflowfm/` per `superpowers/specs/2026-05-08-third-party-spike-design.md` §2, with the spike pseudocode in §5.2 and §6.2 **aligned to the real third-party API shapes** observed in:

- `Stormwater-Management-Model-develop/src/solver/include/swmm5.h` (SWMM 5.2.4)
- `Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h` (D-Flow FM BMI API 1.0)

M29 is the **structural draft** of the spike host programs and their build files. M29 does **not** compile against vendor binaries, does **not** run any spike case, does **not** mirror vendor source into `spikes/<engine>/vendor/`, and does **not** commit any third-party file. M29 produces the source skeleton and the build-file shell that downstream spike work (≥ M30) will populate, build, and run.

## Why Now

- M28 closed the vendor-pollution risk; `Delft3D-main/` and `Stormwater-Management-Model-develop/` are git-ignored.
- The third-party spike spec (`2026-05-08-third-party-spike-design.md`) is conditional-authority per `superpowers/INDEX.md` and is the Phase 1 entry gate for `libs/coupling/drainage/`.
- Drafting host source against the **real** API shape lets M30+ migrate the spike to a compiling, runnable state without having to redesign pseudocode from §5.2 / §6.2.

## Scope

M29 creates:

```
spikes/
├── swmm/
│   ├── CMakeLists.txt                  # standalone, references its own vendor mirror only
│   ├── host/
│   │   └── swmm_spike_host.cpp         # aligned to swmm5.h 5.2.4 real API
│   ├── cases/
│   │   ├── README.md                   # placeholder describing cases/single_pipe.inp expectations
│   │   └── README_manhole_overflow.md
│   └── evidence/
│       ├── abi_gotchas.md              # initial seed entries from header inspection
│       ├── interface_gap_matrix.md     # main-Spec §7.1 row-by-row gap table seeded
│       └── spike_report.md             # empty skeleton with section headers
└── dflowfm/
    ├── CMakeLists.txt
    ├── host/
    │   └── dflowfm_spike_host.cpp      # aligned to dimr_lib/include/bmi.h 1.0 real API
    ├── cases/
    │   ├── README.md
    │   └── README_reach_with_weir.md
    └── evidence/
        ├── abi_gotchas.md
        ├── interface_gap_matrix.md
        └── spike_report.md
```

M29 does **not**:

- Add `spikes/` to the top-level CMake graph. Each spike CMakeLists.txt is fully standalone (per spike-spec §2).
- Place any `.inp` / `.mdu` real input file (cases are documented in README placeholders; real input files come in M30+ once licensing of EPA examples is confirmed).
- Mirror vendor source code; the `find_package` / `find_path` invocations in the spike CMakeLists.txt look for vendor source via a `SWMM_SOURCE_DIR` / `DFLOWFM_SOURCE_DIR` cache var that the developer points at their local SWMM / D-Flow FM checkout.
- Build, link, or run anything in M29 CI; spike binaries stay off the main CI pipeline (spike-spec §2 "二进制构建产物不进 git" + §10 spike CI policy).
- Decide between `extern/<engine>/` vs `spikes/<engine>/vendor/` vs git submodule (M28 boundary preserved).

## SWMM Host Skeleton

### Real-API alignment

| Spike spec §5.2 pseudocode | Real SWMM 5.2.4 API | Comment |
|---|---|---|
| `swmm_open(input, report, output)` | `swmm_open(const char *f1, const char *f2, const char *f3)` | matches; returns int error code (not handle) |
| `swmm_start(0)` | `swmm_start(int saveFlag)` | matches |
| `swmm_setNodeInflow(node_id, q)` | `swmm_setValue(swmm_NODE_LATFLOW, index, value)` | **GAP S1**: pseudocode function name does not exist; real API is enum + index + value |
| `swmm_step(&dt)` | `swmm_step(double *elapsedTime)` | **GAP S2 (potentially BLOCKING)**: `elapsedTime` is OUTPUT (engine reports internal time), not input. Caller cannot dictate dt; `swmm_ROUTESTEP` system property is the closest knob but is set globally, not per-step. Requires spike to verify whether `swmm_setValue(swmm_ROUTESTEP, ...)` mid-run is allowed and respected. |
| `swmm_getNodeValue(node_id, NODE_HEAD)` | `swmm_getValue(swmm_NODE_HEAD, index)` | matches; spike must convert string node id → integer index via `swmm_getIndex(swmm_NODE, name)` |
| `swmm_saveHotStart(path)` | **NOT IN PUBLIC API** | **GAP S3 (potentially BLOCKING)**: no public hot-start save function in `swmm5.h`. Spike must investigate whether `swmm_close` + restart from `.hsf` file produced by main run is the only path (matches spike-spec §5.3 risk note). |
| `swmm_end()` / `swmm_close()` | matches | |

Object identity is via integer index returned by `swmm_getIndex(swmm_NODE, name)` with `swmm_NODE = 2`. Object types are integers `swmm_GAGE/SUBCATCH/NODE/LINK = 0/1/2/3`, system properties use base-100 offsets (`swmm_SYSTEM = 100`).

DLL export uses `__declspec(dllexport) __stdcall` on Windows; spike host must match calling convention via the public header.

### Skeleton highlights

`spikes/swmm/host/swmm_spike_host.cpp` (full content in M29 plan Task 2) outlines:

- `#include "swmm5.h"` — picked up via `target_include_directories` pointing at `${SWMM_SOURCE_DIR}/src/solver/include`.
- A minimal `main(int argc, char** argv)` that calls `swmm_getVersion`, `swmm_open`, `swmm_start`, then loops 100 `swmm_step` calls.
- Inside the loop, demonstrates **read + write** by calling `swmm_getValue(swmm_NODE_HEAD, ...)` and `swmm_setValue(swmm_NODE_LATFLOW, index, q_inject)`.
- After the loop, demonstrates **error reporting** via `swmm_getError` + `swmm_getMassBalErr`.
- All `swmm_*` calls are wrapped with `static_cast<void>(...)` or have their return code logged; no exceptions cross the C ABI boundary.
- The host is C++20 (matches main repo) but only consumes C linkage (per `extern "C"` in `swmm5.h`).

## D-Flow FM Host Skeleton

### Real-API alignment

| Spike spec §6.2 pseudocode | Real D-Flow FM BMI 1.0 API | Comment |
|---|---|---|
| `bmi::Bmi* model = create_dflowfm_bmi()` | **No class; free C functions** | **GAP D1**: no instance handle. The BMI 1.0 implementation is process-global; the spike host calls `initialize(...)` / `update(...)` / `finalize()` directly without an opaque handle. Multi-instance per process is therefore **not possible** without DLL renaming or process isolation — directly tests spike-spec §3.5 assumption. |
| `model->initialize(config)` | `int initialize(const char* config_file)` | matches; returns int error code |
| `model->update_until(target_time)` | `int update(double dt)` | **GAP D2**: pseudocode `update_until` does not exist; real API takes external dt. Spike must verify whether the engine actually respects the caller-supplied dt or silently multi-steps internally (spike-spec §6.3 BLOCKING risk). |
| `model->set_value("name", &q)` | `void set_var(const char* name, const void* ptr)` | **GAP D3**: real API is free function; ptr points at caller buffer holding the value. Caller owns the buffer. |
| `model->get_value("name", buf)` | `void get_var(const char* name, void** ptr)` | **GAP D4**: returns a pointer-to-pointer into engine internal memory. **Caller does NOT own the buffer**; lifecycle tied to engine. Spike must record this and verify lifetime semantics. |
| `model->save_state(path)` | **NOT IN BMI 1.0 HEADER** | **GAP D5 (potentially BLOCKING)**: no save/restore in BMI 1.0. Spike must investigate whether D-Flow FM provides a non-BMI hot-start path and if so, document it as a separate ABI surface. |
| `get_structure_state("weir_id", "discharge")` | Not in BMI 1.0 base; D-Flow FM extension functions exist via different headers | **GAP D6**: spike must locate D-Flow FM-specific structure access (RTC, weirs) outside the BMI surface — likely in `dflowfm/packages/dflowfm_kernel/`. |
| `model->finalize()` | `int finalize()` | matches |

BMI calling convention is `__declspec(dllexport) __stdcall` on Windows (`BMI_CALLCONV`); host must match.

Variable identification is **by string name** (`"boundary_discharge"`, `"stage_at_section"` etc.); spike must produce an exhaustive list of valid variable names by calling `get_var_count(int*)` + `get_var_name(int, char*)` and dump them to `evidence/abi_gotchas.md`.

Logging is via `set_logger(BMILogger)` where `BMILogger` is a function pointer `void(*)(Level, const char*)`; spike host should install a logger that writes engine messages to `evidence/<run>/dflowfm_log.txt`.

### Skeleton highlights

`spikes/dflowfm/host/dflowfm_spike_host.cpp`:

- `#include "bmi.h"` — picked up via `${DFLOWFM_SOURCE_DIR}/src/engines_gpl/dimr/packages/dimr_lib/include`.
- A minimal `main(int argc, char** argv)` that calls `initialize(config)`, then 100 iterations of `update(dt)`.
- Inside the loop, calls `get_var("...", &ptr)` for stage and discharge fields, prints the values.
- Calls `set_var("...", &q_inject)` to inject a boundary discharge value.
- Installs a `BMILogger` that captures engine messages.
- Demonstrates the var-enumeration code path: `get_var_count` + `get_var_name` for the first N vars.

## Isolated CMake Scaffolding

Each spike has its own top-level `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(scau_swmm_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SWMM_SOURCE_DIR "" CACHE PATH "Path to a local SWMM 5.2.4 checkout")
if(NOT SWMM_SOURCE_DIR)
    message(FATAL_ERROR
        "SWMM_SOURCE_DIR must point at a local SWMM 5.2.4 checkout, e.g. "
        "/path/to/Stormwater-Management-Model-develop")
endif()

# spike host depends on the SWMM solver lib; we do NOT add_subdirectory(vendor)
# to keep the spike build independent of the main CMake graph.
find_library(SWMM_SOLVER_LIB
    NAMES swmm5
    HINTS ${SWMM_SOURCE_DIR}/build/src/solver ${SWMM_SOURCE_DIR}/install/lib
    REQUIRED
)
find_path(SWMM_INCLUDE_DIR
    NAMES swmm5.h
    HINTS ${SWMM_SOURCE_DIR}/src/solver/include
    REQUIRED
)

add_executable(swmm_spike_host host/swmm_spike_host.cpp)
target_include_directories(swmm_spike_host PRIVATE ${SWMM_INCLUDE_DIR})
target_link_libraries(swmm_spike_host PRIVATE ${SWMM_SOLVER_LIB})
```

`spikes/dflowfm/CMakeLists.txt` mirrors the structure with `DFLOWFM_SOURCE_DIR` and `find_library(DFLOWFM_LIB NAMES dflowfm)`.

**Critical isolation properties** (per spike-spec §2):

- Each spike has its own `project()`; not added by main `CMakeLists.txt`.
- No `target_link_libraries(... scau::coupling_core)`. Spike must not consume `libs/coupling/`.
- No CMake target name collision with main repo (prefix `swmm_spike_*` / `dflowfm_spike_*`).
- Build outputs go to spike-local `build/` which is already covered by main `.gitignore`.

## Evidence Seeds

M29 seeds three evidence files per spike (initial content based on header inspection only; spike runs populate the rest):

- `evidence/abi_gotchas.md`: pre-populated with the 6 categories from spike-spec §4 (thread safety, ownership, units, domain, OS resources, external deps), each header marked "TBD by spike run" with the few items the header already reveals (e.g. SWMM uses `__stdcall`; D-Flow FM BMI uses string-name variable identification with caller-owns-buffer for `set_var` and engine-owns-pointer for `get_var`).
- `evidence/interface_gap_matrix.md`: seeded with rows for each main-Spec §7.1 / §7.3 assumption mapped to the real API and a status placeholder (`TBD` until spike runs verify). Gaps already visible from header inspection (S1, S2, S3, D1–D6) are pre-populated with severity guess (`GAP_MITIGABLE` or `GAP_BLOCKING ?`).
- `evidence/spike_report.md`: skeleton with §3 sub-headers (lifecycle, time advance, state read/write, hot-start, error/exception, version stability) and empty bodies.

## Tests / Verification

M29 has no runnable test (spikes are off the main CI). Verification is:

1. `git status --short` shows the new files under `spikes/swmm/` and `spikes/dflowfm/`.
2. `ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: still 31/31 (main CI untouched).
3. `grep -r "scau::coupling" spikes/`: returns no matches (proves spike has no main-repo dependency).
4. `grep -r "add_subdirectory" spikes/`: returns no matches into main repo (proves CMake isolation).

## Boundaries

- No vendor source mirrored into `spikes/<engine>/vendor/` in M29 (deferred to M30 placement-decision milestone).
- No `.inp` / `.mdu` real input files (placeholders only; M30+ verifies EPA SWMM5 / Deltares example licensing).
- No spike binary built or run in M29.
- No main-repo CMake graph change.
- No connection between `spikes/` and `libs/coupling/`.
- No license review or AGPL/GPL traceability work (deferred to M29 placement decision, separate from this spec).
- No commit of any third-party file. M28 `.gitignore` guard remains active.
