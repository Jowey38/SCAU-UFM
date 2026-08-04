# M268 SimDriver Minimal Executable Tri-Model Run Loop Evidence

Date: 2026-08-04
Base: `origin/master@8eafd23`

## Delivered

- `libs/coupling/driver/surface2d_coupling_map`: the Surface2D <->
  CouplingState adapter seam. `build_exchange_cells` projects the current
  surface state into fresh exchange cells (`V = phi_t * h * A`) with aggregate
  and endpoint-owned deficit carry-over across the per-epoch rebuild;
  `apply_exchange_write_back` pushes post-replay volumes back
  (`h = V / (phi_t * A)`, `eta = z_b + h`) with the recorded momentum
  convention: drains scale `hu/hv` by `h_new/h_old` (velocity-preserving),
  returns enter with zero momentum. Fail-closed on size mismatches, non-empty
  pending event queues, and non-finite/negative depths.
- `apps/sim_driver` RuntimeConfig v2: `initial_eta` (required; STCF carries no
  hydrodynamic initial state), CFL knobs, opt-in
  `enable_cvc_spatial_phi_t_correction`, engine mode, head-driven link
  configs, strict divisibility and `dt_swmm = dt_dflowfm = dt_couple`
  contracts, single-writer rule across links.
- Strict fail-closed `key=value` config parser (no new dependency), epoch run
  loop `run_simulation`, hand-rolled JSON run summary, real `scau_sim` CLI
  with mock and real engine modes (real mode compiled under `SCAU_HAS_SWMM5`
  and `SCAU_HAS_DFLOWFM_BMI_RUNTIME`).
- Epoch ordering invariant: all surface substeps are accepted BEFORE engines
  advance; a CFL rollback lands in `review_required` with zero engine
  advancement (SWMM cannot rewind).

## G19 registration

G19 `surface2d_tri_coupling_real` is registered `implemented, ci_gate:false`
with the `candidate_non_gating` label (G15 precedent), added to the
self-hosted gateway (`tools/dflowfm/run_real_goldens.sh`, now 6/6) and the
`real-dflowfm-golden` CI build targets. It drives the REAL Surface2D solver,
REAL embedded SWMM, and REAL D-Flow FM BMI runtime through `run_simulation`
over the locked mixed-minimal strict STCF case. Promotion to `ci_gate:true`
is planned with the M270 whole-system mass audit assertion.

## CVC finding (bug-186)

The first demo run with default-off CVC showed a 0.0597 m3 (4.4%) whole-domain
`phi_t*h*A` closure gap over 10 coupled epochs; per-epoch analysis attributed
all of it to surface-substep drift across the mixed-minimal internal edge
(phi_t 1.0/0.8 jump under nonzero velocity) — the documented M253 limitation.
With the opt-in G23 correction enabled the same run closes to 2.2e-16 with the
deficit ledger fully repaid. The knob therefore surfaced in RuntimeConfig v2;
the project-wide default remains OFF.

## Runtime netcdf deployment finding (bug-188)

G19 is the first golden whose executable statically imports the vcpkg
`netcdf.dll` (STCF I/O, deployed app-local beside the executable) AND loads
the real D-Flow FM runtime. The Windows loader resolves `netcdff.dll`'s
`netcdf.dll` dependency against the already-loaded module by base name, and
the vcpkg build lacks the `nc_def_var_chunking_ints` /
`nc_inq_var_chunking_ints` exports the runtime needs, so `LoadLibrary`
failed with error 127 on the governed runner. Fix: the gateway script deploys
the runtime's `netcdf.dll` beside the G19 executable before running it;
`dumpbin` verified the runtime build is a superset of all 24 `nc_*` imports
the executable needs. The same deployment rule applies to `scau_sim` in real
engine mode. Hosted CI is unaffected (G19 skips without the runtime env).

## Real gateway evidence (governed runner environment, local execution)

`tools/dflowfm/run_real_goldens.sh H:/scau-b-m268 Debug` with the production
runtime (`SCAU_DFLOWFM_LIBRARY` -> Delft3D install): **6/6 goldens passed**
(G11, G18, G16, G17, checkpoint reload, G19). G19 drove the real Surface2D
solver, real embedded SWMM, and real D-Flow FM BMI runtime through
`run_simulation` for 3 coupled epochs with the analytic `phi_t*h*A` closure
holding to 1e-9; the real SWMM engine overshoots the requested elapsed target
by less than one ROUTING_STEP (asserted as an interval, not an equality).

## Verification (MSVC Debug, clean master@8eafd23 + this change only)

- Build: no C/C++/link/MSBuild errors.
- Full CTest: 147/147 passed (142 baseline + adapter unit, config parser
  unit, fixture generator, run-loop integration, G19 env-gated skip).
- `ctest -L golden`: 23/23 passed (G19 is non-gating and not golden-labeled).
- `ctest -L manifest`: GoldenSuite completeness checker passed with the G19
  entry.
- Executable demo (`scau_sim demo_run.cfg`, mock engines, mixed-minimal,
  10 epochs, CVC on): exit 0, `completed`, committed_epochs=10, closure gap
  vs the analytic 1.36 m3 initial storage = 2.2e-16, final deficit 0.
- Integration test asserts: completion, per-epoch records, engines advance
  exactly once per committed epoch, analytic closure to 1e-9, CFL-rollback
  fail-closed path with zero engine advancement, out-of-range coupled cell
  rejected before any engine write.

## Non-claims

- No engine sub-stepping; no dt-halving retry after CFL rollback.
- No roof path in the loop (double-writer hazard on SWMM lateral inflow is a
  separate arbitration milestone).
- No checkpoint/rollback integration (M269); no whole-system engine-storage
  mass audit (M270); the closure evidence covers the surface + coupling
  ledger with wall-only boundaries.
- G19 is not a merge gate yet; real-runner evidence lands through the
  self-hosted gateway.
