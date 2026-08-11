# M272 SWMM Complete External-Net Provider

## Scope

Expose a governed, read-only SWMM 5.2.4 routing-totals observation at the concrete `SwmmEngine` boundary and feed it into the M270 whole-system audit without widening `ISwmmEngine` or moving arbitration semantics into the engine adapter.

## Contract

The vendored bridge copies raw `TRoutingTotals` fields and cumulative API lateral inflow in internal ft3. `SwmmEngine::observe_external_net_volume()` converts to m3, retains the raw routing net and component provenance, and exposes a complete audit net with API lateral removed exactly once. SimDriver consumes only that deduplicated value because the accepted API lateral volume is already represented by the CouplingLib/Surface2D exchange path.

## Verification

- Embedded SWMM unit case confirms finite raw components, storage values, and positive API-lateral accounting after a routed step.
- G26 records the concrete provider as an implemented, non-gating GoldenSuite candidate.
- Existing M270 mock and real-candidate tests remain unchanged in behavior.
- Third-party version manifest and patch evidence identify the SWMM 5.2.4 ABI and revalidation trigger.

## Boundary

This milestone does not claim G19 promotion or complete D-Flow external flux. G19 remains non-gating and whole-system conservation remains dependent on a governed D-Flow provider.
