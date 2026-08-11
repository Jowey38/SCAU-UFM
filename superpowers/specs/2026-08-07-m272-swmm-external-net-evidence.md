# M272 SWMM Complete External-Net Provider Evidence

## Implementation

The vendored SWMM 5.2.4 solver now exports the governed `massbal_getRoutingTotals` bridge. It copies routing continuity components, initial/final storage, and cumulative API lateral inflow into an ABI-owned DTO without exposing `TRoutingTotals` through the public C++ boundary. `SwmmEngine::observe_external_net_volume()` converts internal ft3 to m3 and returns component provenance, the raw routing external net, the API lateral component, the deduplicated audit external net, and `scope_complete`.

SimDriver's real-mode hook binds the deduplicated observation directly. The adapter subtracts API lateral exactly once because accepted surface-to-SWMM volume is already represented in Surface2D/CouplingLib storage and must not be counted as an external source. Baseline and per-epoch samples therefore use the same atomic observation contract. `ISwmmEngine` remains unchanged.

## Evidence

- Windows MSVC Debug configure/build with warnings-as-errors: passed in `H:/b272`.
- Full CTest: 158/158 passed after the bridge and audit wiring.
- Golden label suite: 26/26 passed; G26 is absent from the `golden` gate label.
- Non-gating evidence label: 4/4 passed, including G26 `swmm_external_net`.
- Manifest check: passed and rejects any implemented `ci_gate:false` entry carrying `LABELS golden`.
- G26 confirms finite routing components/storage, positive API-lateral accounting, the raw routing-net identity, and exact single-point API-lateral deduplication.

## Limits

This evidence does not promote G19. D-Flow FM external-net scope remains independently required before real whole-system conservation can become gate-eligible.
