# G2 phi_t jump hydrostatic reference output

**来源**: `sandbox/numerical/proofs/phi_t_jump_pressure_pairing.ipynb`

For every edge:

```text
pressure_pairing = 0.5 * g * h^2 * (phi_t,R - phi_t,L)
S_phi_t = -0.5 * g * h^2 * (phi_t,R - phi_t,L)
pressure_pairing + S_phi_t = 0
mass_flux = 0
```

For the mixed minimal mesh with `h = 1`, `g = 9.81`, and adjacent `phi_t` jump `0.25`, E1 and E3 have:

```text
pressure_pairing = 1.22625
S_phi_t = -1.22625
residual = 0
```
