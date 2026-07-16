# G8 Real SWMM `.inp` Evidence

Date: 2026-07-01

## Scope

This evidence backs G8 `swmm_single_pipe_surcharge` with real SWMM 5.2.x
standalone spike execution. It does not change the main GoldenSuite gate:
G8 remains `ci_gate:false` until real SWMM is wired into the main CMake/test
runtime.

## Authored cases

- `spikes/swmm/cases/single_pipe.inp`
- `spikes/swmm/cases/manhole_overflow.inp`

Both files are authored for this repository and are not copied from EPA or
third-party example sets.

## Build commands

External SWMM checkout/build used for this local run:

```bash
cmake -S H:/githubcode/SCAU-UFM/Stormwater-Management-Model-develop -B H:/bswmm-src -G "Visual Studio 17 2022" -A x64
cmake --build H:/bswmm-src --config Debug
```

Standalone spike host:

```bash
cmake -S H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/spikes/swmm \
  -B H:/bswmm-spike \
  -G "Visual Studio 17 2022" \
  -A x64 \
  -DSWMM_SOURCE_DIR=H:/githubcode/SCAU-UFM/Stormwater-Management-Model-develop \
  -DSWMM_BUILD_DIR=H:/bswmm-src
cmake --build H:/bswmm-spike --config Debug
```

On Windows/Git Bash, `swmm5.dll` must be discoverable beside the spike host or
on the Windows DLL search path. This run copied it beside the executable:

```bash
cp H:/bswmm-src/src/solver/Debug/swmm5.dll H:/bswmm-spike/Debug/
```

## Run commands

```bash
H:/bswmm-spike/Debug/swmm_spike_host.exe \
  H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/spikes/swmm/cases/single_pipe.inp \
  H:/bswmm-spike/single_pipe.rpt \
  H:/bswmm-spike/single_pipe.out

H:/bswmm-spike/Debug/swmm_spike_host.exe \
  H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/spikes/swmm/cases/manhole_overflow.inp \
  H:/bswmm-spike/manhole_overflow.rpt \
  H:/bswmm-spike/manhole_overflow.out
```

## Observed results

SWMM version reported by the public C API:

```text
[spike] SWMM version = 52004
```

`single_pipe.inp`:

- `swmm_getIndex(NODE, "J1") = 0`
- `NODE_HEAD` rises smoothly to about `0.310136`
- `NODE_OVERFLOW` remains `0.000000`
- mass balance error: `runoff=0.0000% flow=0.0000% qual=0.0000%`

Representative output:

```text
[spike] step=0 elapsedDays=0.000006 head=0.006405 overflow=0.000000
[spike] step=10 elapsedDays=0.003357 head=0.309995 overflow=0.000000
[spike] step=40 elapsedDays=0.013773 head=0.310136 overflow=0.000000
[spike] mass balance error: runoff=0.0000% flow=0.0000% qual=0.0000%
```

`manhole_overflow.inp`:

- `swmm_getIndex(NODE, "J1") = 1`
- `NODE_HEAD` rises from `0.003021` to the overflow/surcharge cap around
  `0.450000`
- `NODE_OVERFLOW` becomes positive at step 7 and stabilizes near `0.384070`
- mass balance error: `runoff=0.0000% flow=0.0000% qual=0.0000%`

Representative output:

```text
[spike] step=0 elapsedDays=0.000006 head=0.003021 overflow=0.000000
[spike] step=6 elapsedDays=0.000303 head=0.401958 overflow=0.000000
[spike] step=7 elapsedDays=0.000348 head=0.450000 overflow=0.392762
[spike] step=20 elapsedDays=0.001853 head=0.450000 overflow=0.384070
[spike] step=99 elapsedDays=0.010997 head=0.450000 overflow=0.384070
[spike] mass balance error: runoff=0.0000% flow=0.0000% qual=0.0000%
```

## Interpretation

The mock-boundary G8 Golden now has a real-SWMM physical backing case: an
authored shallow-manhole network produces observable `NODE_OVERFLOW > 0` and
head saturation through the SWMM public C API.

Remaining promotion work:

- add/restore a real `SwmmEngine` in the main CMake graph;
- make real SWMM runtime deterministic in CI;
- move G8 from standalone spike evidence to CI-gated Golden only after that
  runtime exists.
