# M271 Deficit Write-off Ledger Evidence

Date: 2026-08-07
Base: `origin/master@8f4eafc`

## Delivered

- Core-owned consecutive age per aggregate/shared `mass_deficit_account`.
- Explicit epoch-end `CouplingState::apply_deficit_writeoff`, default threshold
  `N_writeoff_steps=3`, pending-queue fail-closed guard, atomic copy/validate/
  commit implementation.
- Runtime counters: `count_writeoff_events` and canonical
  `count_writeoff_volume_total` (m3).
- Deterministic records include cell and optional engine/node endpoint identity,
  written-off volume, and pre-writeoff age.
- Snapshot/rollback includes account age and counters; M269 checkpoint hash now
  mixes account age and both write-off counters.
- SimDriver config adds `n_writeoff_steps`; run loop applies write-off after
  replay/write-back and before checkpoint snapshot; counters persist across
  per-epoch CouplingState rebuild; JSON summary emits run/epoch totals and
  endpoint IDs for stability-protocol WARN/operator evidence.

## Behavioral evidence

- Aggregate account age progresses 1 -> 2 -> third-epoch write-off; complete
  repayment resets age to zero.
- Shared drainage and river endpoints age independently and write off in stable
  order.
- A snapshot taken at age 2, followed by write-off, rollback, and replay yields
  a strictly identical report, counters, cell ledger, and checkpoint hash.
- Write-off changes only the obligation ledger; `phi_t*h*A` physical surface
  storage is unchanged.
- G24 integration now observes real write-off events: post-writeoff account age
  is always <3, the physical whole-system audit remains conserved, and
  cumulative write-off volume is positive.

## G25

G25 `deficit_writeoff_replay` is registered
`implemented, ci_gate:true, Phase 1+` and golden-labeled. It writes off one
aggregate plus two shared endpoint accounts (14 m3 total) on the third epoch,
then proves rollback/replay report and checkpoint-hash equality.

## Verification

MSVC Debug, clean `origin/master@8f4eafc` plus this change:

- clean full build: zero C/C++/link/MSBuild errors (a stale locked test exe was
  explicitly removed and re-linked; stale CTest output was not trusted);
- full CTest: **156/156 passed**;
- golden label: **26/26 passed**;
- manifest: **1/1 passed**;
- governed real-runtime gateway: **6/6 passed**.

## Consequence boundary

A write-off is a stability-protocol `WARN` requiring endpoint ID and operator
review. It is not physical mass loss and does not change M270 storage closure.
M272/M273 remain responsible for complete real-engine external-net scope;
G19 remains non-gating `REVIEW_REQUIRED`.
