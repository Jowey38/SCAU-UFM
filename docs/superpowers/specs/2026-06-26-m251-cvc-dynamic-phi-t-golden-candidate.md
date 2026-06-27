# M251 CVC Dynamic `phi_t` GoldenSuite Candidate

## Status

Candidate gate implemented locally as a CTest-labeled unit gate:

```text
test_dpm_phi_t_remap
LABELS: golden_candidate;cvc;surface2d
```

This deliberately does not edit `tests/golden/suite_manifest/goldensuite.json` yet because the GoldenSuite manifest work is still isolated in PR #8 (`feat/goldensuite-manifest` -> `feat/m230-stage-record`). The candidate can be migrated mechanically into `tests/golden/` once that PR lands on the selected base branch.

## Candidate Identity

Suggested GoldenSuite entry after PR #8 lands:

```json
{
  "test_id": "G13",
  "name": "cvc_dynamic_phi_t_remap",
  "test_path": "tests/golden/cvc_dynamic_phi_t_remap/",
  "applicable_phase": "Phase 1+",
  "reference_path": "tests/golden/reference/cvc_dynamic_phi_t_remap.md",
  "ci_gate": true,
  "status": "implemented"
}
```

`G13` is proposed to avoid overloading existing `G2 phi_t_jump_hydrostatic` semantics. `G2` is a static spatial `phi_t`-jump well-balanced test. This candidate is a dynamic/time-jump `phi_t` remap test.

## Physical Boundary

The M250/M251 path verifies the time-jump remap seam:

```text
phi_t_old, h_old, hu_old, hv_old -> phi_t_new, h_new, hu_new, hv_new
```

It preserves:

- `sum(phi_t * h * A)`;
- `sum(phi_t * hu * A)`;
- `sum(phi_t * hv * A)`;
- bed elevation `z_b = eta - h`;
- velocity where both old and new states are wet.

It is not a full CVC augmented-HLLC implementation for nonzero-velocity spatial porosity discontinuities. Static spatial jumps remain covered by the existing Audusse well-balanced pressure/source path and the `phi_t_jump_hydrostatic` family.

## Current Test Coverage

`tests/unit/surface2d/test_dpm_phi_t_remap.cpp` covers:

1. no-op same-`phi_t` remap;
2. per-cell and total storage conservation under nonuniform `phi_t` changes;
3. velocity preservation and stored-momentum conservation;
4. bed elevation preservation through `eta - h`;
5. invalid `phi_t` fail-closed without mutation;
6. uniform dynamic `phi_t` jump followed by a normal WB step keeping lake-at-rest momentum at zero.

## Migration Steps After PR #8

1. Create `tests/golden/cvc_dynamic_phi_t_remap/`.
2. Move or wrap the current unit test into `test_cvc_dynamic_phi_t_remap.cpp`.
3. Add `set_tests_properties(test_cvc_dynamic_phi_t_remap PROPERTIES LABELS golden)`.
4. Add reference documentation under `tests/golden/reference/cvc_dynamic_phi_t_remap.md`.
5. Append the proposed `G13` row to `goldensuite.json`.
6. Run `python tests/golden/suite_manifest/check_manifest.py` and `ctest -L golden`.

## Validation

Local validation for this candidate should include:

```text
ctest --preset windows-msvc -L golden_candidate
ctest --preset windows-msvc --timeout 120
```

