# M274 D-Flow FM Native Water-Balance Bridge Feasibility

Conclusion level: **FAILED / BLOCKED**

M274 does not justify a production D-Flow external-net provider, G27 registration, or G19 promotion. The native cumulative quantities are technically reachable from a bridge compiled inside the D-Flow FM DLL, but this session could not produce and run a governed rebuilt runtime.

## Baseline

- SCAU-UFM base: `origin/master@4cc05e9` after M272 and M273 merged.
- External source: local `Delft3D-main` snapshot used by the governed real-D-Flow gateway.
- Existing runtime: `Delft3D-main/install_fm-suite_release/bin/dflowfm.dll`.
- Compiler: Intel ifx 2026.1.0.
- Contract artifacts:
  - `spikes/dflowfm/contract/dflowfm_water_balance_v1.h`;
  - `spikes/dflowfm/bridge/dflowfm_water_balance_bridge.F90`.

## Confirmed source contract

The D-Flow kernel maintains the required cumulative physical classes in `m_flow`:

- open boundary: `vinbndcum`, `voutbndcum`;
- lateral 1D/2D: `vinlatcum`, `voutlatcum`;
- point source/sink: `vinsrccum`, `voutsrccum`;
- qext 1D/2D: `vinextcum`, `voutextcum`;
- rain/evaporation: `vinraincum`, `voutevacum`;
- groundwater: `vingrwcum`, `voutgrwcum`;
- storage/error: `vol1tot`, `volerrcum`.

`flow_f0isf1` updates the open-boundary cumulative volumes using the completed native timestep and also updates the other source/sink cumulative classes. This is materially better than M273 end-of-step `q1` integration because the engine owns classification and timestep integration.

## ABI feasibility

A fixed-width v1 C ABI was authored with:

- explicit `abi_version` and `struct_size`;
- explicit component-validity bits;
- SI cumulative volumes with separate nonnegative in/out fields;
- no Fortran, BMI, CouplingLib, or native object exposure.

The Fortran `bind(C)` bridge compiles successfully against the existing Release `m_flow.mod` and `m_flowtimes.mod` with ifx 2026.1 after disabling MSYS argument rewriting.

An independent bridge DLL is not viable. Linking only the bridge plus `dflowfm_kernel.lib` pulls unresolved transitive module dependencies (`timers`, `m_missing`, `m_alloc_generated`, `m_cell_geometry`, and others). The bridge must therefore be compiled into the external `dflowfm.dll`, where those modules already form one linked runtime.

## External rebuild blocker

The current source snapshot uses a Conan 2 external build flow. Conan 2.31.2 and the governed external profiles were installed successfully. Local recipes and network source downloads also worked.

A clean dependency build then failed while building PETSc. The profile requires `C:/cygwin64/bin/bash.exe` and an initialized Visual Studio/oneAPI environment. The machine has Git Bash, Visual Studio 2022, ifx 2026.1, Intel MPI and MKL, but no governed Cygwin installation matching the recipe. A compatibility symlink was insufficient: PETSc's Cygwin command could not resolve the expected Windows paths and exited before configuration.

Existing D-Flow build files cannot be reused safely for the bridge: they contain absolute `E:/Delft3D-main` and historical Conan cache paths that do not exist on this machine. Editing generated project files or linking an incomplete DLL would not be reproducible evidence.

## Unexecuted evidence matrix

Because no rebuilt DLL with `dflowfm_get_water_balance_v1` was produced, the following required runtime evidence remains absent:

1. symbol and ABI validation in the real runtime;
2. closed/open/qext/lateral/mixed component closure;
3. provenance separation for CouplingLib-owned versus external lateral flow;
4. observed 30/60/120 second timestep behavior;
5. restart continuity or a complete checkpointed host-offset contract;
6. closure against independent `sum(vol1)` without deriving the forcing from storage.

## Decision

M274 stops at feasibility evidence. It does **not** add:

- a production D-Flow external-net provider;
- an `IDFlowFMEngine` method;
- a SimDriver hook;
- G27;
- G19 promotion;
- a tolerance change.

Real whole-system conservation remains `REVIEW_REQUIRED` and release claims depending on G19 remain blocked.

## Unblock requirements

At least one of these must be provided:

1. a governed D-Flow build environment with the configured Cygwin, Visual Studio, oneAPI, Conan dependencies and permission to compile the project-owned bridge into `dflowfm.dll`; or
2. an upstream D-Flow/BMI release exporting an equivalent versioned cumulative water-balance API.

After that, rerun the full M274 case matrix before implementing any production provider.
