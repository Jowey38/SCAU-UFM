# M253 CVC Spatial `phi_t` Dynamic Perturbation Evidence

## Scope

M253 implements Phase A from the M252 design: a failure-revealing GoldenSuite candidate for the known §5.5.2 blank zone.

It does not change solver behavior.

Added test:

```text
test_cvc_spatial_phi_t_dynamic_perturbation
LABELS: golden_candidate;cvc;surface2d
```

## Physical Scenario

The test constructs a minimal internal-edge dynamic perturbation:

- flat lake-at-rest depth baseline (`h=1`, `eta=1`);
- nonzero opposing normal momentum on the two cells across one internal edge;
- all non-target edges hard-blocked (`phi_e_n=0`, `omega_edge=0`);
- target internal edge regular (`phi_e_n=1`, `omega_edge=1`);
- comparison between:
  - uniform `phi_t = 1.0 / 1.0`;
  - spatial step `phi_t = 1.0 / 0.4`.

The audit quantity is physical storage:

```text
sum(phi_t * h * A)
```

## Evidence Meaning

For uniform `phi_t`, the current standard HLLC/WB path preserves the physical storage audit within `1e-12`.

For spatial-step `phi_t` with nonzero velocity, the test requires a measurable storage residual:

```text
jumped_residual > 1e-6
jumped_residual > 1000 * uniform_residual
```

This intentionally captures the blank-zone behavior rather than hiding it. The test is a candidate/evidence gate, not a production failure gate.

## Boundary

This test must not be interpreted as a regression. It documents current behavior that the main spec already identifies:

- static spatial `phi_t` jump at rest is covered by existing well-balanced tests;
- dynamic time-jump remap is covered by M250/M251;
- nonzero-velocity spatial `phi_t` discontinuity still needs future CVC/augmented-HLLC correction.

## Future Promotion

Suggested future GoldenSuite identity after PR #8 lands:

```json
{
  "test_id": "G14",
  "name": "cvc_spatial_phi_t_dynamic_perturbation",
  "test_path": "tests/golden/cvc_spatial_phi_t_dynamic_perturbation/",
  "applicable_phase": "Phase 1+",
  "reference_path": "tests/golden/reference/cvc_spatial_phi_t_dynamic_perturbation.md",
  "ci_gate": false,
  "status": "candidate"
}
```

`ci_gate` should remain false until an augmented-HLLC correction exists and the test can be inverted from failure-revealing evidence into a passing correctness gate.

## Validation

Validated locally with:

```text
cmake --build --preset windows-msvc --target test_cvc_spatial_phi_t_dynamic_perturbation
ctest --preset windows-msvc -R "^test_cvc_spatial_phi_t_dynamic_perturbation$" -V
```
