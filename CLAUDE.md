# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository State

This repository is currently a documentation/specification repository for SCAU-UFM rather than an implemented codebase. Do not invent build, lint, test, or single-test commands unless they are later added to repository documentation or build files.

## Authoritative Documentation

Use `superpowers/INDEX.md` as the entry point for normative project documentation. Treat historical review, status, plan, design, and conversation-export documents as non-normative when they conflict with the indexed sources.

Normative sources currently identified by the index:

| Entry | Authority Boundary |
|---|---|
| `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` | Main architecture spec; defines physical semantics, interface contracts, system invariants, coupling semantics, and defaults. |
| `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` | Stability/reliability protocol; defines gates, trigger thresholds, consequences, evidence requirements, and operator responsibilities. |
| `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` | Canonical symbols and terms; defines machine-facing names, units, deprecated aliases, and naming constraints. |
| `superpowers/specs/project-layout-design.md` | Target directory layout design; defines the target engineering placement for the 2D surface core, `CouplingLib`, SWMM/D-Flow FM adapters, third-party governance, validation system, and Phase evolution boundaries. |

In addition, `superpowers/INDEX.md` lists conditional-authority specs (currently the third-party engine spike plan and the numerical sandbox plan) that act as Phase 1 entry conditions. Treat them as authoritative for the corresponding spike/sandbox activity until their exit criteria are met and their evidence is archived.

## Commands

No concrete project commands are currently documented in the repository for build, lint, tests, development servers, or single-test execution. The specs mention a CMake/vcpkg-oriented target architecture and GoldenSuite/GoldenTest gates, but do not define runnable command lines.

When implementation files are added, document real commands here only after verifying them against repository build/test configuration.

## Architecture Overview

SCAU-UFM targets a fully coupled urban flood model whose model core is developed in C++ and composed of:

- `Surface2DCore` — owns C++ two-dimensional surface numerical advancement on an unstructured mixed triangular/quadrilateral mesh using STCF-compatible fields and an anisotropic double porosity model.
- Anisotropic DPM fields — include `phi_t`, `Phi_c`, `phi_e_n`, and `omega_edge`; preserve their distinct semantics in schemas, logs, audits, and code.
- HLLC-based 2D solver — includes Riemann fluxes, source terms, wetting/drying, CFL diagnostics, CPU reference backend, and future CUDA backend concerns.
- `CouplingLib` — owns exchange decisions, arbitration, scheduling, conservation, deficit ledger semantics, snapshot/rollback/replay, and fault handling.
- `SWMMAdapter` — connects the embedded SWMM 5.2.x drainage/pipe-network engine to `CouplingLib` through shared DTOs.
- `DFlowFMAdapter` — connects the embedded D-Flow FM/BMI river-network engine to `CouplingLib` through shared DTOs.
- `PreProc` / STCF pipeline — produces mesh, subgrid, DPM, tensor, roughness, soil, and landcover fields consumed by the solver.
- Golden correctness system — `GoldenTest` is a single correctness baseline; `GoldenSuite` is the release/merge gate regression collection.

## Boundaries And Invariants

- `Surface2DCore` must not depend on SWMM, D-Flow FM, or any 1D engine ABI.
- SWMM and D-Flow FM are parallel 1D engines; their native objects must not see each other.
- Cross-engine exchange must go through shared DTOs and `CouplingLib` ledger semantics.
- `IDFlowFMEngine` wraps only lifecycle and state read/write. It must not carry `Q_limit`, `V_limit`, deficit, rollback, replay, or arbitration semantics.
- Phase 1/2 coupling core, drainage, and river components are intended to remain same-process/same-compile-unit with static linking and one standard library boundary.
- Third-party source, patches, licenses, version manifests, compatibility policy, and ABI boundary policy should remain separated under the target `extern/` and `third_party/` structure.

## Canonical Names

Follow `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` for machine-facing names.

- Use `Q_limit` as the only machine-facing name for coupling hard-gate flow limit; do not introduce `Q_max_safe` in audit, CI, log, schema, or machine-readable interfaces.
- Preserve the two-step volume/flow expression: `V_limit = 0.9 * phi_t * h * A`, then `Q_limit = V_limit / dt_sub`.
- Keep `phi_e_n` and `omega_edge` as separate fields. `phi_e_n` is edge-normal effective conveyance; `omega_edge` is edge connectivity/open-close weight.
- Treat `max_cell_cfl` as the raw physical CFL diagnostic. Do not scale it by `CFL_safety`; compare rollback decisions against `C_rollback` separately.
- Use `mass_deficit_account`, `epsilon_deficit`, `snapshot`, `rollback`, and `replay` with the meanings defined in the symbols reference and main spec.

## Reliability Requirements

The stability protocol applies from Phase 1 onward. Do not claim release/merge readiness without fresh evidence for the applicable gates.

- GoldenSuite must be represented in CI before it can be used as merge/release evidence.
- Missing required GoldenTest scripts, reference paths, or reproducible evidence means the GoldenSuite is incomplete.
- GoldenTest failures block merge according to the stability protocol.
- Coupling evidence must include `Q_limit` behavior, deficit update/repayment/write-off/replay evidence, and shared-cell arbitration evidence when both 1D engines are active.
- Runtime/fault evidence should preserve protocol states such as `ABORT`, `REVIEW_REQUIRED`, `BLOCK_MERGE`, and `BLOCK_RELEASE`.
