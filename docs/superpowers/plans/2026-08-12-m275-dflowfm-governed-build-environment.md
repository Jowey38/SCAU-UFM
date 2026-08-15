# M275 D-Flow FM Governed Build Environment

Status: **FAILED / BLOCKED** at first reproducible source-build attempt.

## Goal

Establish a governed Windows build environment capable of rebuilding the upstream `dflowfm.dll` from the local `Delft3D-main` source snapshot, so that the M274 native water-balance bridge can be compiled into the real runtime and exercised by the approved case matrix.

## What was verified

1. `origin/master` already contains M272, M273 and M274 blocker evidence.
2. A real Cygwin installation was installed at `C:/cygwin64` with the packages required by the profile (`bash`, `make`, `python3`, `diffutils`, `gcc-core`, `git`).
3. Visual Studio 2022, Intel ifx 2026.1, Intel MPI and MKL are present and can be staged into the process environment.
4. Conan 2.31.2 was installed and the governed external profiles were initialized with `run_conan.py initialize external`.
5. The external D-Flow build now advances far enough to enter the PETSc local-recipe build instead of failing immediately on missing tools.

## Reproduced blocker

The first reproducible source build still fails before any D-Flow target is configured.

The failure is inside the local PETSc recipe (`petsc/3.25.3`) when Conan generates and sources its Cygwin shell environment:

- `conanbuild.sh` correctly sources `/cygdrive/.../conanbuildenv-release-x86_64.sh`;
- that generated file defines `script_folder="C:\Users\Administrator\...\b"` using raw Windows backslashes;
- the same script then writes or references shell files through `"$script_folder\..."`;
- under the required Cygwin bash profile (`tools.microsoft.bash:subsystem=cygwin`, `tools.microsoft.bash:path=C:/cygwin64/bin/bash.exe`), those backslash paths are not portable shell paths and the PETSc build aborts before `./configure` completes.

This is not a missing-tool issue anymore. It is a reproducible Windows+Cygwin path-compatibility defect in the generated Conan build environment and/or the local PETSc recipe flow that consumes it.

## Consequence

M275 does not yet deliver a governed rebuilt `dflowfm.dll`. Therefore:

- M274 remains blocked at the same decision boundary;
- the native bridge cannot be compiled into the real runtime in this session;
- the runtime case matrix cannot start;
- G27 cannot be created;
- G19 remains non-gating `REVIEW_REQUIRED`.

## Minimum unblock

One of the following is required before resuming M274 runtime work:

1. fix the local PETSc Conan recipe / generator combination so the generated Cygwin shell environment uses valid Unix paths throughout; or
2. provide a governed alternative upstream D-Flow build path that avoids the broken PETSc+Cygwin shell stage while still reproducing the real runtime used by CI.

## Next action when unblocked

After the shell-path defect is fixed:

1. rebuild unmodified upstream `dflowfm.dll` and verify parity with the current real gateway;
2. compile the M274 bridge into that DLL;
3. validate the exported `dflowfm_get_water_balance_v1` symbol and ABI;
4. run the full M274 closure / timestep / restart matrix before any provider work.
