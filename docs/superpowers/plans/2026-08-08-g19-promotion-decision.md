# G19 Promotion Decision After M272/M273

## Decision

Status: **BLOCKED — do not rerun for promotion**

The real three-model whole-system audit may be rerun as promotion evidence only after both one-dimensional engines expose complete, governed cumulative external-net scope. M272 satisfies the SWMM side. M273 explicitly does not satisfy the D-Flow FM side.

## Entry-gate evaluation

| Requirement | Evidence | Verdict |
|---|---|---|
| Complete SWMM external net | M272 routing totals bridge; API lateral removed exactly once | PASS |
| Complete D-Flow FM external net | M273 real open-boundary spike | FAIL / BLOCKED |
| Independent external terms | `qext/qextreal/vextcum` unavailable; `q1` lacks governed boundary mapping/orientation/integration/restart semantics | FAIL |
| Scope-complete audit input | Both providers required | FAIL |

Because the entry gate fails, the previous G19 raw residual (`+141.801 m3` under zero-external assumption) remains failure-revealing scope-incomplete evidence. It is not a valid tolerance measurement and must not be compared with the legal engine tolerance for promotion.

## Requested decisions

### Residual within legal tolerance

**Not decidable.** A residual is tolerance-eligible only when every physical storage and cumulative external source/sink term is independently observed with complete scope. Missing D-Flow FM external flux dominates the equation; widening tolerance or inferring the term from `delta sum(vol1)` is prohibited.

### G19 promotion

**No.** Keep `implemented, ci_gate:false`. Its current purpose is to expose `scope_complete=false` and `REVIEW_REQUIRED`, not to claim real-engine conservation.

### G27 external-flux Golden

**Required as a future promotion prerequisite, but do not register it yet.** G27 should be created only after the D-Flow contract is proven. It must independently lock:

1. boundary ID to native flow location mapping;
2. external-inflow-positive orientation;
3. units and sampling point;
4. time integration over variable engine steps;
5. all active external boundary/source classes;
6. cumulative state across checkpoint reload/replay;
7. closure against `delta sum(vol1)` without deriving forcing from that storage difference.

Until those prerequisites exist, adding a pending or mock G27 entry would create the appearance of a contract that does not yet exist.

### Stability protocol and manifest

**No normative update now.** The existing protocol already requires complete reproducible evidence and blocks incomplete GoldenSuite/release claims. The current manifest correctly keeps G19 non-gating. Update the stability protocol and manifest atomically only when:

- G27 has real failure-revealing evidence and a governed implementation;
- the real G19 audit becomes scope-complete;
- the measured residual passes the pre-existing legal tolerance without widening it;
- fresh hosted and self-hosted CI evidence is green.

## Release consequence

Current consequence is `REVIEW_REQUIRED` for real whole-system conservation and `BLOCK_RELEASE` for any claim depending on G19 promotion. This does not block unrelated Phase 1/Phase 2 gates already supported by independent evidence.
