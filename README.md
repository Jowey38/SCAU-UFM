# SCAU-UFM

SCAU urban flood model. C++20 numerical core (Anisotropic DPM HLLC) with embedded SWMM and D-Flow FM 1D engines, fully coupled by `CouplingLib`.

This repository is currently at **Milestone M1 (build chain + scaffolding)**. Subsequent milestones (M2 mesh+stcf, M3 surface2d, M4 coupling, M5 release gate) deliver actual numerics. See `superpowers/INDEX.md` for the authoritative documentation tree.

## Prerequisites

- CMake ≥ 3.27
- A C++20 compiler:
  - Linux: GCC ≥ 12 or Clang ≥ 14
  - Windows: Visual Studio 2022 (MSVC ≥ 19.32)
- Ninja (Linux) or the Visual Studio 17 2022 generator (Windows)
- A local clone of [vcpkg](https://github.com/microsoft/vcpkg); the path must be exported as `VCPKG_ROOT` before running CMake.

## Build & Test

Linux:
```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Windows (PowerShell):
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Both presets pass `SCAU_WARNINGS_AS_ERRORS=ON`. Override locally with `-DSCAU_WARNINGS_AS_ERRORS=OFF` while iterating.

## Documentation

- Authoritative entries: `superpowers/INDEX.md`
- Main spec: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md`
- Stability protocol: `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`
- Symbols & terms: `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`
- Project layout design: `superpowers/specs/project-layout-design.md`

## License

TBD.
