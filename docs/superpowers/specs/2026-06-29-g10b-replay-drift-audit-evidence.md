# G10b Replay Drift Audit Evidence

## Scope

G10b strengthens the existing G10 Golden by proving repeated replay zero drift inside the same CouplingLib core replay boundary.

Covered:

- aggregate repeated replay drift
- shared-endpoint repeated replay drift
- exact round-by-round replay signature equality

## Result

- repeated aggregate replay rounds remain identical to round 1
- repeated shared-endpoint replay rounds remain identical to round 1
- pending queues drain after each replay
- runtime counters remain stable across all rounds

## Boundary

G10b does not introduce a new manifest item and does not claim real 1D solver transient replay correctness.
