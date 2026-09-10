# B1/B2' surface + SWMM execution evidence

Decision: enable the existing export package's surface+SWMM mode independently of C3 river-provider readiness, as recommended by the mapper master plan. The existing enable_dflowfm flag selects participation; tri-model behavior remains the default.

## Implementation boundary

- CouplingLib still owns arbitration, Q_limit, deficit/replay, and write-back.
- A disabled D-Flow engine receives no initialization, reads, writes, update, clock-provider, storage-provider, external-net-provider, or finalization calls.
- River links with the river disabled are rejected. The checkpoint coordinator requires only active modules.
- A disabled river contributes zero physical storage/external flux. Enabled SWMM still requires complete storage and external-net scope; absence yields review_required.
- The real CLI currently requires compilation with SWMM and the BMI runtime adapter, but a surface+SWMM run does not require an installed D-Flow DLL or MDU.

## Fresh local evidence

MSVC Debug build at H:/scau-close-build. Command:

```sh
ctest --test-dir H:/scau-close-build -C Debug -R 'test_sim_driver_(drainage_only_real|run_loop|whole_system_mass_audit|checkpoint_rollback)$' --output-on-failure
```

Result: 5/5 CTest registrations passed, including fixture generation.

- Mock dual-mode integration: 10 committed epochs, conservative surface write-back, river left uninitialized, forbidden river clock hook never called.
- Dual-mode CFL rollback: zero committed epochs and zero SWMM advancement.
- Deterministic complete-scope audit: 20 epochs conserved; missing SWMM external-net scope rejected even with river disabled. Forbidden river provider hooks never called.
- Real SWMM CLI: original exported synthetic package STCF and copied SWMM INP; 180 seconds / 3 committed epochs. SCAU_DFLOWFM_LIBRARY deliberately points to a missing DLL. Test uses build-directory outputs and requires completed summary and expected epoch count.
- Existing tri-model integration and checkpoint/rollback tests passed in the same invocation.

The real CLI test keeps the source fixture's whole-system audit disabled. Its result proves executable dual-mode integration, not real-engine physical closure. The deterministic storage-tracking test supplies separate conservation evidence.

## Remaining gates

Full fresh GoldenSuite and cross-platform CI on this implementation remain required before merge. C3 hydraulic boundary contract, M288 output/results pipeline, M281-authorized real inputs, and QGIS 3.40 verification remain distinct incomplete tasks. No project-wide release-ready claim is made.
