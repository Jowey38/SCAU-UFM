# M3 Surface2D Numerical Minimal Slice Evidence

**Date:** 2026-05-13
**Plan:** `superpowers/plans/2026-05-12-plan-m3-surface2d-numerical-minimal-slice.md`
**Sandbox B Entry Evidence:** `sandbox/numerical/evidence/sandbox_report.md`

## Scope

M3 implements the formal C++ minimal slice for `Surface2DCore` DPM fields, hydrostatic reconstruction, minimal HLLC normal flux, `phi_t` pressure-source pairing, and CPU step diagnostics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 11/11 tests passed
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal)$"`: PASS, 7/7 Surface2D tests passed

## Coverage

- `test_dpm_fields`: verifies mesh-aligned defaults for `phi_t`, `Phi_c`, `phi_e_n`, and `omega_edge`.
- `test_hydrostatic_reconstruction`: verifies zero-velocity hydrostatic reconstruction preserves equal-eta depth.
- `test_hllc_flux`: verifies normal projection plus hard-block, soft-block, and zero-velocity zero mass flux.
- `test_phi_t_source`: verifies pressure pairing and `S_phi_t` cancel exactly for the sandbox B G2 stencil.
- `test_surface_step`: verifies CPU step diagnostics preserve M2 behavior and report DPM edge diagnostics.
- `test_surface2d_golden_minimal`: mirrors sandbox B G1/G2/G3 with mixed, quad-only, and tri-only mesh coverage.

## Sandbox B Archive Scope

- Archive source and tests under `sandbox/numerical/src/`, `sandbox/numerical/tests/`, and `sandbox/numerical/proofs/` as the Python reference basis for M3.
- Archive case inputs and expected outputs under `sandbox/numerical/cases/`, including G1/G2/G3 reference outputs.
- Archive evidence under `sandbox/numerical/evidence/`, especially `g1_run.md`, `g2_run.md`, `g3_run.md`, `well_balanced_proof.md`, and `sandbox_report.md`.
- Archive `sandbox/numerical/README.md` and `sandbox/numerical/pyproject.toml` to preserve validated commands and notebook dependencies.
- Do not archive generated Python caches such as `__pycache__/` or `*.pyc`; they are already excluded by `.gitignore`.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- `max_cell_cfl` remains the raw physical CFL diagnostic; rollback comparison uses `C_rollback` separately.
- This evidence does not claim GoldenSuite release readiness.
