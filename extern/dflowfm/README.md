# extern/dflowfm -- BMI interface contract snapshot (contract-snapshot-only mode)

This directory vendors ONLY the D-Flow FM BMI interface contract, not the engine.

## What is vendored

- `include/bmi.h` -- verbatim copy of the BMI 1.0 C header from the Delft3D
  source tree at `src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h`.
  See `third_party/manifest/dflowfm.version` for the extraction record.

## What is deliberately NOT vendored

- The D-Flow FM Fortran kernel and any other Delft3D engine source.
  The kernel is Deltares-owned code under AGPL/GPL family licenses
  (see `third_party/licenses/dflowfm-LICENSE`); per the project ABI boundary
  policy the engine is never statically embedded or compiled into this repo.

## How the engine is consumed at runtime

The main-graph adapter `libs/coupling/river` (`DFlowFMEngine`,
`src/dflowfm_adapter/dflowfm_engine.cpp`) loads a prebuilt D-Flow FM shared
library (`dflowfm.dll` / `libdflowfm.so`, overridable via the
`SCAU_DFLOWFM_LIBRARY` environment variable) at runtime with
LoadLibrary/dlopen and resolves the BMI symbols by name. The adapter declares
its own function-pointer typedefs; it does not include `bmi.h` and the header
must never appear in any public header or DTO. The vendored `include/bmi.h`
is the authoritative contract those typedefs must match:

| adapter typedef        | bmi.h declaration                              |
|------------------------|------------------------------------------------|
| `InitializeFn`         | `int initialize(const char* config_file)`      |
| `UpdateFn`             | `int update(double dt)`                        |
| `FinalizeFn`           | `int finalize()`                               |
| `GetStartTimeFn`       | `void get_start_time(double* t)`               |
| `GetCurrentTimeFn`     | `void get_current_time(double* t)`             |
| `GetVarFn`             | `void get_var(const char* name, void** ptr)`   |
| `SetVarFn`             | `void set_var(const char* name, const void*)`  |
| `GetVarCountFn`        | `void get_var_count(int* count)`               |
| `GetVarNameFn`         | `void get_var_name(int index, char* name)`     |

If the vendored header is ever refreshed, re-verify the adapter typedefs and
the spike host (`spikes/dflowfm/host/dflowfm_spike_host.cpp`) against it.

## Consumers

- `spikes/dflowfm/CMakeLists.txt` uses `extern/dflowfm/include` as a fallback
  `find_path` hint for `bmi.h` when no external `DFLOWFM_SOURCE_DIR` checkout
  provides it. The spike still requires the caller to supply a built D-Flow FM
  library; only the header contract is vendored here.
