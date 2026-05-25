# M29 Third-Party Spike Host Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to walk Task 1 → Task 3 in order.

**Goal:** Create the structural skeleton of the SWMM 5.2.4 and D-Flow FM BMI 1.0 spike host programs and isolated CMake scaffolding under `spikes/swmm/` and `spikes/dflowfm/`, with all source aligned to the real third-party API shapes. No vendor source copied. No spike binary built or run.

**Tech Stack:** C++20 (host source only), standalone CMake (not consumed by main build), git.

---

## Task 1: SWMM Spike Skeleton

**Files:**
- Create `spikes/swmm/CMakeLists.txt`
- Create `spikes/swmm/host/swmm_spike_host.cpp`
- Create `spikes/swmm/cases/README.md`
- Create `spikes/swmm/cases/README_manhole_overflow.md`
- Create `spikes/swmm/evidence/abi_gotchas.md`
- Create `spikes/swmm/evidence/interface_gap_matrix.md`
- Create `spikes/swmm/evidence/spike_report.md`

- [ ] **Step 1: Create `spikes/swmm/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(scau_swmm_spike LANGUAGES CXX)

# Spike CMake is fully standalone per third-party-spike-design §2:
#   "spikes/ 不得与 libs/ extern/ third_party/ apps/ 共享 CMake target"
# Do NOT add_subdirectory(this) from the main repo CMakeLists.txt.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SWMM_SOURCE_DIR "" CACHE PATH "Path to a local SWMM 5.2.4 checkout")
if(NOT SWMM_SOURCE_DIR)
    message(FATAL_ERROR
        "SWMM_SOURCE_DIR must point at a local SWMM 5.2.4 checkout, e.g. "
        "/path/to/Stormwater-Management-Model-develop. Build SWMM separately and "
        "point this variable at it; the spike will only consume the public swmm5.h "
        "and link against the built libswmm5.")
endif()

find_path(SWMM_INCLUDE_DIR
    NAMES swmm5.h
    HINTS ${SWMM_SOURCE_DIR}/src/solver/include
    REQUIRED
)

find_library(SWMM_SOLVER_LIB
    NAMES swmm5 libswmm5
    HINTS
        ${SWMM_SOURCE_DIR}/build/src/solver
        ${SWMM_SOURCE_DIR}/build/src/solver/Release
        ${SWMM_SOURCE_DIR}/build/src/solver/Debug
        ${SWMM_SOURCE_DIR}/install/lib
    REQUIRED
)

add_executable(swmm_spike_host host/swmm_spike_host.cpp)
target_include_directories(swmm_spike_host PRIVATE ${SWMM_INCLUDE_DIR})
target_link_libraries(swmm_spike_host PRIVATE ${SWMM_SOLVER_LIB})
```

- [ ] **Step 2: Create `spikes/swmm/host/swmm_spike_host.cpp`**

```cpp
// SWMM 5.2.4 spike host.
//
// Purpose: exercise the real swmm5.h public C API against the spike-spec
//   §3.1-§3.6 assumptions. NOT a coupling implementation. Aligned to the
//   actual swmm_*() function signatures verified in
//   Stormwater-Management-Model-develop/src/solver/include/swmm5.h.
//
// Gaps already visible from header inspection (see
//   spikes/swmm/evidence/interface_gap_matrix.md):
//   S1: no swmm_setNodeInflow; use swmm_setValue(swmm_NODE_LATFLOW, idx, q).
//   S2: swmm_step(double*) writes elapsedTime — engine drives dt, not caller.
//       This contradicts spike-spec §3.2 "外部控制 dt".
//   S3: no swmm_saveHotStart in public API; investigate .hsf path separately.

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "swmm5.h"
}

namespace {

constexpr int kStepsToRun = 100;
constexpr const char *kInjectNodeName = "J1";   // adjust to chosen case
constexpr double kInjectFlow = 0.05;            // CMS or current flow units

void report_error_if_any(int rc, const char *where) {
    if (rc == 0) {
        return;
    }
    std::array<char, 512> msg{};
    swmm_getError(msg.data(), static_cast<int>(msg.size()));
    std::fprintf(stderr, "[spike] %s returned %d: %s\n", where, rc, msg.data());
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "usage: %s <input.inp> <report.rpt> <output.out>\n",
            argv[0]);
        return 2;
    }

    std::printf("[spike] SWMM version = %d\n", swmm_getVersion());

    int rc = swmm_open(argv[1], argv[2], argv[3]);
    report_error_if_any(rc, "swmm_open");
    if (rc != 0) {
        return 1;
    }

    rc = swmm_start(/*saveFlag=*/1);
    report_error_if_any(rc, "swmm_start");

    const int node_index = swmm_getIndex(swmm_NODE, kInjectNodeName);
    std::printf("[spike] swmm_getIndex(NODE, \"%s\") = %d\n",
                kInjectNodeName, node_index);

    for (int step = 0; step < kStepsToRun; ++step) {
        // GAP S1 alignment: lateral inflow via setValue + NODE_LATFLOW enum.
        if (node_index >= 0) {
            swmm_setValue(swmm_NODE_LATFLOW, node_index, kInjectFlow);
        }

        double elapsedDays = 0.0;
        rc = swmm_step(&elapsedDays);
        report_error_if_any(rc, "swmm_step");
        if (rc != 0) {
            break;
        }

        if (node_index >= 0) {
            const double head     = swmm_getValue(swmm_NODE_HEAD,     node_index);
            const double overflow = swmm_getValue(swmm_NODE_OVERFLOW, node_index);
            std::printf("[spike] step=%d elapsedDays=%.6f head=%.6f overflow=%.6f\n",
                        step, elapsedDays, head, overflow);
        }
    }

    float runoffErr = 0.0F;
    float flowErr   = 0.0F;
    float qualErr   = 0.0F;
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    std::printf("[spike] mass balance error: runoff=%.4f%% flow=%.4f%% qual=%.4f%%\n",
                static_cast<double>(runoffErr),
                static_cast<double>(flowErr),
                static_cast<double>(qualErr));

    rc = swmm_end();
    report_error_if_any(rc, "swmm_end");

    rc = swmm_close();
    report_error_if_any(rc, "swmm_close");

    return 0;
}
```

- [ ] **Step 3: Create `spikes/swmm/cases/README.md`**

```markdown
# SWMM spike test cases

This directory will hold `.inp` files driving the SWMM 5.2.4 spike host.
M29 does NOT place real `.inp` files because EPA SWMM5 example licensing must
be verified first (deferred to M30+ placement decision).

Required cases per third-party-spike-design §5.1:

- `single_pipe.inp`: upstream constant inflow → single pipe → free outfall.
  Minimal sanity case for §3.1 lifecycle + §3.2 time advance + §3.3 state IO.
- `manhole_overflow.inp`: case with one surcharging manhole. Backs the
  §3.3 read assumption and the G8 GoldenTest physical basis.

Until those files are placed, the spike host can still be compiled to verify
ABI shape; running requires real `.inp` inputs.
```

- [ ] **Step 4: Create `spikes/swmm/cases/README_manhole_overflow.md`**

```markdown
# manhole_overflow.inp specification

Required content (per third-party-spike-design §5.1):

- One junction "J1" with shallow rim elevation.
- One conduit C1 carrying upstream flow into J1.
- One free outfall downstream.
- An inflow time series large enough to drive J1 into surcharge.

Required for ABI verification of swmm_NODE_OVERFLOW > 0 behaviour and
NODE_HEAD evolution across surcharge transitions.

Source: must be authored or sourced from an EPA-public-domain example. Do NOT
copy from third-party copyrighted example sets. License audit pending in M30.
```

- [ ] **Step 5: Create `spikes/swmm/evidence/abi_gotchas.md`** with the 6-category template seeded with what header inspection already reveals.

(See M29 design §"Evidence Seeds" for full content; brief excerpt below — final file should include every category.)

```markdown
# SWMM 5.2.4 ABI gotchas

Initial findings from inspection of `swmm5.h` and `swmm5.c` from the
2026-05-24 working tree. Each section is `TBD by spike run` unless noted.

## Thread safety
- TBD (spike must run two threads each calling swmm_step in parallel and observe).
- Initial guess: NOT thread-safe; swmm5.c uses file-scope globals (`IsOpenFlag`,
  `Node[]`, `Link[]`, `Gage[]`, `Subcatch[]`).

## Memory ownership
- `swmm_getError(char *errMsg, int msgLen)`: caller-owned buffer; engine fills.
- `swmm_getValue`: returns double by value; no buffer.
- `swmm_setValue`: caller passes value by copy.
- No engine-owned pointers cross the ABI boundary.

## Units
- TBD; depends on `swmm_FLOWUNITS` system property (CFS/GPM/MGD/CMS/LPS/MLD).
- Head and depth are in feet OR meters depending on FlowUnits (US vs SI).

## Domain / sentinel values
- TBD by spike run.

## OS resources
- swmm_open opens 3 files (input/report/output); engine holds file handles
  until swmm_close.
- TBD: tmp file usage.

## External deps
- OpenMP (CMakeLists installs OpenMP runtime).
- No netCDF, no MKL in 5.2.4 base.
```

- [ ] **Step 6: Create `spikes/swmm/evidence/interface_gap_matrix.md`** with one row per main-Spec §7.1 assumption and a status column.

```markdown
# SWMM 5.2.4 interface gap matrix

Maps main-Spec §7.1 `ISwmmEngine` assumptions to real swmm5.h API.

Severity: MATCH | MATCH_WITH_NOTE | GAP_MITIGABLE | GAP_BLOCKING | TBD

| §7.1 assumption | Real API | Severity | Notes |
|---|---|---|---|
| Open input case | `swmm_open(f1,f2,f3)` | MATCH | rc=0 on success |
| Start simulation | `swmm_start(saveFlag)` | MATCH | |
| Step with caller-supplied dt | `swmm_step(double *elapsedTime)` | GAP_BLOCKING ? | elapsedTime is OUTPUT not INPUT; engine picks internal dt from .inp ROUTING_STEP. Mitigation: setValue(swmm_ROUTESTEP) before each step? — verify in spike run. |
| Inject lateral inflow per node | `swmm_setValue(swmm_NODE_LATFLOW, idx, q)` | MATCH_WITH_NOTE | enum-based, no dedicated function; per spike-spec §5.3 expected |
| Read node head | `swmm_getValue(swmm_NODE_HEAD, idx)` | MATCH | |
| Read node overflow | `swmm_getValue(swmm_NODE_OVERFLOW, idx)` | MATCH | |
| Set outfall stage | `swmm_setValue(swmm_NODE_HEAD, outfall_idx, stage)` | MATCH_WITH_NOTE | only valid for outfall nodes |
| Save hot-start any step | NOT IN PUBLIC API | GAP_BLOCKING ? | swmm5.c has internal hotstart_open/_close but not exported; .hsf file produced by saveFlag=1 may only be readable on restart; verify in spike run |
| Restart from hot-start | swmm_open + .inp with HOTSTART line | MATCH_WITH_NOTE | restart only from saved checkpoint, not arbitrary mid-run |
| Same-process second instance | NOT POSSIBLE | GAP_BLOCKING | file-scope globals in swmm5.c preclude concurrent instances; document for §6.5 snapshot scope |
| End / close | `swmm_end()`, `swmm_close()` | MATCH | |
| Error reporting | `swmm_getError(buf, len)` + return codes | MATCH | |
| Mass balance | `swmm_getMassBalErr(&runoff, &flow, &qual)` | MATCH | useful for §6.3 conservation cross-check |
```

- [ ] **Step 7: Create `spikes/swmm/evidence/spike_report.md`** as an empty skeleton.

```markdown
# SWMM 5.2.4 spike report

**Status:** TBD — spike run not yet executed (M29 = host skeleton only).

## §3.1 Lifecycle
TBD

## §3.2 Time advance
TBD

## §3.3 State read / write
TBD

## §3.4 Hot-start / state save
TBD

## §3.5 Error / exception
TBD

## §3.6 Version stability
TBD

## §4 Must-document
- Thread safety: TBD
- Memory ownership: see evidence/abi_gotchas.md (partial)
- Units: TBD
- Domain / sentinels: TBD
- OS resources: TBD
- External deps: TBD

## Exit-criteria checklist (§7)
- [ ] §3 all assumptions have replayable evidence
- [ ] §4 six must-document items answered
- [ ] interface_gap_matrix.md statuses finalised (no `TBD` left)
- [ ] each GAP_BLOCKING has concrete §7 redesign proposal
- [ ] host binary runs 100 steps clean from cases/
- [ ] abi_gotchas.md covers all 6 categories
```

---

## Task 2: D-Flow FM Spike Skeleton

**Files:**
- Create `spikes/dflowfm/CMakeLists.txt`
- Create `spikes/dflowfm/host/dflowfm_spike_host.cpp`
- Create `spikes/dflowfm/cases/README.md`
- Create `spikes/dflowfm/cases/README_reach_with_weir.md`
- Create `spikes/dflowfm/evidence/abi_gotchas.md`
- Create `spikes/dflowfm/evidence/interface_gap_matrix.md`
- Create `spikes/dflowfm/evidence/spike_report.md`

- [ ] **Step 1: Create `spikes/dflowfm/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(scau_dflowfm_spike LANGUAGES CXX)

# Spike CMake is fully standalone per third-party-spike-design §2.
# Do NOT add_subdirectory(this) from the main repo.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DFLOWFM_SOURCE_DIR "" CACHE PATH "Path to a local Delft3D / D-Flow FM checkout")
if(NOT DFLOWFM_SOURCE_DIR)
    message(FATAL_ERROR
        "DFLOWFM_SOURCE_DIR must point at a local Delft3D-main checkout, e.g. "
        "/path/to/Delft3D-main. The spike does not vendor any third-party "
        "source; the caller supplies a built D-Flow FM library and its BMI "
        "header location via this cache variable.")
endif()

find_path(DFLOWFM_BMI_INCLUDE_DIR
    NAMES bmi.h
    HINTS ${DFLOWFM_SOURCE_DIR}/src/engines_gpl/dimr/packages/dimr_lib/include
    REQUIRED
)

find_library(DFLOWFM_LIB
    NAMES dflowfm libdflowfm
    HINTS
        ${DFLOWFM_SOURCE_DIR}/build/lib
        ${DFLOWFM_SOURCE_DIR}/install/lib
        ${DFLOWFM_SOURCE_DIR}/lib
    REQUIRED
)

add_executable(dflowfm_spike_host host/dflowfm_spike_host.cpp)
target_include_directories(dflowfm_spike_host PRIVATE ${DFLOWFM_BMI_INCLUDE_DIR})
target_link_libraries(dflowfm_spike_host PRIVATE ${DFLOWFM_LIB})
```

- [ ] **Step 2: Create `spikes/dflowfm/host/dflowfm_spike_host.cpp`**

```cpp
// D-Flow FM BMI 1.0 spike host.
//
// Purpose: exercise the real BMI C API against the spike-spec §3 / §6
//   assumptions. NOT a coupling implementation. Aligned to the actual BMI
//   signatures verified in
//   Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h.
//
// Gaps already visible from header inspection (see
//   spikes/dflowfm/evidence/interface_gap_matrix.md):
//   D1: no instance handle; API is process-global free C functions.
//   D2: update(double dt) takes external dt — must verify engine respects it.
//   D3: set_var/get_var use string-named variables; caller buffer for set,
//       engine-owned pointer for get (caller does NOT free).
//   D4: no save_state in BMI 1.0; investigate engine-specific extension.

#include <array>
#include <cstdio>
#include <cstring>

extern "C" {
#include "bmi.h"
}

namespace {

constexpr int kStepsToRun = 100;
constexpr double kDtSeconds = 60.0;
constexpr const char *kBoundaryDischargeVar = "boundary_discharge";
constexpr const char *kStageVar = "stage_at_section";

void BMI_CALLCONV spike_logger(Level level, const char *msg) {
    std::printf("[bmi log lvl=%d] %s\n", static_cast<int>(level), msg);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <config.mdu>\n", argv[0]);
        return 2;
    }

    set_logger(&spike_logger);

    int rc = initialize(argv[1]);
    if (rc != 0) {
        std::fprintf(stderr, "[spike] initialize returned %d\n", rc);
        return 1;
    }

    int var_count = 0;
    get_var_count(&var_count);
    std::printf("[spike] get_var_count = %d\n", var_count);
    for (int i = 0; i < var_count && i < 32; ++i) {
        std::array<char, MAXSTRINGLEN> name{};
        get_var_name(i, name.data());
        std::printf("[spike]   var[%d] = %s\n", i, name.data());
    }

    double t0 = 0.0;
    double t1 = 0.0;
    double dt_internal = 0.0;
    get_start_time(&t0);
    get_end_time(&t1);
    get_time_step(&dt_internal);
    std::printf("[spike] t0=%.6f t1=%.6f engine_dt=%.6f\n",
                t0, t1, dt_internal);

    for (int step = 0; step < kStepsToRun; ++step) {
        const double q_inject = 1.5;  // SI m^3/s
        // GAP D3: caller provides pointer to caller-owned buffer.
        set_var(kBoundaryDischargeVar, static_cast<const void *>(&q_inject));

        rc = update(kDtSeconds);
        if (rc != 0) {
            std::fprintf(stderr, "[spike] update returned %d at step %d\n", rc, step);
            break;
        }

        double t_now = 0.0;
        get_current_time(&t_now);

        // GAP D3: get_var fills a pointer-to-pointer into engine memory.
        // Caller does NOT free.
        void *engine_ptr = nullptr;
        get_var(kStageVar, &engine_ptr);
        if (engine_ptr != nullptr) {
            const double *stage = static_cast<const double *>(engine_ptr);
            std::printf("[spike] step=%d t=%.6f stage[0]=%.6f\n",
                        step, t_now, stage[0]);
        } else {
            std::printf("[spike] step=%d t=%.6f stage var unavailable\n",
                        step, t_now);
        }
    }

    rc = finalize();
    if (rc != 0) {
        std::fprintf(stderr, "[spike] finalize returned %d\n", rc);
    }
    return 0;
}
```

- [ ] **Step 3: Create `spikes/dflowfm/cases/README.md`**

```markdown
# D-Flow FM spike test cases

This directory will hold `.mdu` files driving the D-Flow FM BMI spike host.
M29 does NOT place real `.mdu` files because Deltares example licensing must
be verified first (deferred to M30+ placement decision).

Required cases per third-party-spike-design §6.1:

- `single_reach.mdu`: single reach, upstream constant Q, downstream constant
  stage. Sanity case for §3.1-§3.3.
- `reach_with_weir.mdu`: reach with one fixed weir. Backs §3.3 structure
  state read and G11 dflowfm_river_steady physical basis.
```

- [ ] **Step 4: Create `spikes/dflowfm/cases/README_reach_with_weir.md`** mirroring the SWMM case description.

```markdown
# reach_with_weir.mdu specification

Required content (per third-party-spike-design §6.1):

- One reach with N grid points, upstream Q boundary, downstream H boundary.
- One fixed weir structure of known crest height.
- Initial stage profile that produces non-trivial weir discharge.

License: must be authored or sourced from a Deltares-public example. Do NOT
copy from copyrighted Delft3D distribution example sets. License audit pending in M30.
```

- [ ] **Step 5: Create `spikes/dflowfm/evidence/abi_gotchas.md`**

```markdown
# D-Flow FM BMI 1.0 ABI gotchas

Initial findings from inspection of
Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h.

## Thread safety
- TBD; BMI free-function style + likely Fortran-backed engine implies main thread only.

## Memory ownership
- `initialize/update/finalize`: int return code, no buffers.
- `set_var(name, const void* ptr)`: caller owns the buffer; engine copies.
- `get_var(name, void** ptr)`: engine writes its own pointer into *ptr.
  Caller MUST NOT free *ptr. Lifetime tied to engine; pointer may invalidate
  across set_var / update.
- `get_var_name(int, char*)`: caller-owned buffer; size = MAXSTRINGLEN (1024).
- `set_logger(BMILogger)`: caller-owned function pointer; engine retains it.

## Units
- TBD; D-Flow FM BMI variable units are documented per variable name; spike
  must enumerate via get_var_name and look up in D-Flow FM user manual.

## Domain / sentinel values
- TBD.

## OS resources
- D-Flow FM is known to depend on the current working directory and on
  environment variables (per spike-spec §6.3 note). Spike must record actual
  dependencies in run.

## External deps
- D-Flow FM upstream depends on netCDF, MPI (optional), PETSc (optional), MKL,
  Fortran runtime. Spike must verify which are required at link time.
```

- [ ] **Step 6: Create `spikes/dflowfm/evidence/interface_gap_matrix.md`**

```markdown
# D-Flow FM BMI 1.0 interface gap matrix

Maps main-Spec §7.3 / §7.4 `IDFlowFMEngine` assumptions to real BMI 1.0 API.

Severity: MATCH | MATCH_WITH_NOTE | GAP_MITIGABLE | GAP_BLOCKING | TBD

| §7.3-§7.4 assumption | Real API | Severity | Notes |
|---|---|---|---|
| Open with config file | `int initialize(const char *config_file)` | MATCH | |
| Step with caller-supplied dt | `int update(double dt)` | MATCH_WITH_NOTE | spike-spec §6.3 risk: verify engine actually respects caller dt |
| Read state by name | `void get_var(const char *name, void** ptr)` | MATCH_WITH_NOTE | engine-owned pointer; lifetime tied to engine. Spike must lock down lifetime rules |
| Write state by name | `void set_var(const char *name, const void *ptr)` | MATCH | caller-owned buffer, engine copies |
| Save state mid-run | NOT IN BMI 1.0 | GAP_BLOCKING ? | investigate D-Flow FM non-BMI hot-start path |
| Multi-instance in process | NOT POSSIBLE | GAP_BLOCKING | free-function API implies single global state |
| RTC / weir / gate state read | NOT IN BMI 1.0 base | GAP_MITIGABLE ? | likely available via extra D-Flow FM-specific headers outside BMI; spike must locate |
| Variable enumeration | `get_var_count`, `get_var_name(i, char*)` | MATCH | useful for §7.5 RiverExchangePoint field discovery |
| Logging | `set_logger(BMILogger)` | MATCH | function-pointer callback; install before initialize |
| Time accessors | `get_start_time/end_time/current_time/time_step` | MATCH | |
| Finalize | `int finalize()` | MATCH | |
```

- [ ] **Step 7: Create `spikes/dflowfm/evidence/spike_report.md`** (same skeleton as SWMM, all TBD).

```markdown
# D-Flow FM BMI 1.0 spike report

**Status:** TBD — spike run not yet executed (M29 = host skeleton only).

## §3.1 Lifecycle
TBD

## §3.2 Time advance
TBD

## §3.3 State read / write
TBD

## §3.4 Hot-start / state save
TBD

## §3.5 Error / exception
TBD

## §3.6 Version stability
TBD

## §4 Must-document
- Thread safety: TBD
- Memory ownership: see evidence/abi_gotchas.md (partial; get_var pointer lifetime is critical)
- Units: TBD
- Domain / sentinels: TBD
- OS resources: TBD (working directory dependence noted)
- External deps: TBD (netCDF, MPI, PETSc, MKL likely)

## Exit-criteria checklist (§7)
- [ ] §3 all assumptions have replayable evidence
- [ ] §4 six must-document items answered
- [ ] interface_gap_matrix.md statuses finalised (no `TBD` left)
- [ ] each GAP_BLOCKING has concrete §7 redesign proposal
- [ ] host binary runs 100 steps clean from cases/
- [ ] abi_gotchas.md covers all 6 categories
```

---

## Task 3: Verify Isolation And Write Evidence

- [ ] **Step 1: Confirm main CMake graph unchanged**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, 31/31. M29 must not touch the main build graph; CMake should not reconfigure.

- [ ] **Step 2: Verify spike has no main-repo dependency**

```bash
grep -r "scau::coupling" spikes/ ; echo "exit=$?"
grep -r "add_subdirectory" spikes/ ; echo "exit=$?"
grep -r "libs/coupling" spikes/ ; echo "exit=$?"
```

Expected: each grep returns exit 1 (no matches).

- [ ] **Step 3: Create evidence document**

Create `superpowers/specs/2026-05-25-plan-m29-third-party-spike-host-skeleton-evidence.md` recording:
- File list created under `spikes/swmm/` and `spikes/dflowfm/`.
- Gap counts (S1-S3, D1-D6) with severity guesses.
- Verification that ctest 31/31 still passes.
- Verification of isolation via grep results.

- [ ] **Step 4: Inspect status and diff**

```bash
git status --short
```

Expected: 14 new files under `spikes/`, plus 3 new M29 markdown files (design / plan / evidence); existing transcript and the two .gitignored vendor dirs remain absent.

---

## Boundaries

- No vendor source mirrored into `spikes/<engine>/vendor/`.
- No `.inp` / `.mdu` real input files.
- No spike binary built or run.
- No main-repo CMake graph change.
- No connection between `spikes/` and `libs/coupling/`.
- No license review.
- No commit of any third-party file; M28 .gitignore guard remains active.
