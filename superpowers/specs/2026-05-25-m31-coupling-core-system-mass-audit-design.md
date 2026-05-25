# M31 CouplingLib Core System Mass Audit Design

**Date:** 2026-05-25

## Goal

Lift the §5.1 v5.3.2 conservation contract — "**所有质量收支、耦合可提取水量、守恒审计一律以 `phi_t h A` 为准**" — and the §11.4 `M_ref` definition into pure, machine-asserted helpers in `libs/coupling/core`. This closes the system-wide conservation gap explicitly left open by M27 evidence ("M27 does not introduce system-wide `M_ref` accumulation; per-call only.").

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §5.1 (v5.3.2):
  > "所有质量收支、耦合可提取水量、守恒审计一律以 `phi_t h A` 为准"
  > "质量闭合不得再用纯几何体积 `hA` 代替物理储量 `phi_t h A`"

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §11.4:
  | 参数 | 默认值 | 单位 | 说明 |
  |---|---|---|---|
  | `M_ref` | 由算例 t=0 计算 | m³ | `sum(phi_t × h × A) over all cells with h >= h_wet` |
  | `epsilon_mass` | `max(1e-10, 1e-12 × M_ref)` | m³ | 总质量守恒误差容差 |
  | `h_wet` | 1e-4 | m | 湿单元判据 |

- `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`: machine-facing names are mandatory.

M31 implements `M_ref`-style accumulation as a pure helper. It does **not** yet hook into the correctness-path golden tests (which `epsilon_mass` is the tolerance for); the helper only **computes** the conservation diagnostic. Where to assert it (strict 0 on correctness path vs `epsilon_mass` on performance path) is a downstream call.

## Scope

M31 adds two value types and two pure functions in `libs/coupling/core`:

```cpp
struct SystemMassAudit {
    double surface_mass{0.0};   // sum_i phi_t_i * h_i * area_i over wet cells
    double deficit_mass{0.0};   // sum_i mass_deficit_account_i
    double total_mass{0.0};     // surface_mass + deficit_mass
    std::size_t wet_cell_count{0};
};

struct SystemMassDelta {
    SystemMassAudit baseline{};
    SystemMassAudit current{};
    double residual{0.0};       // current.total_mass - baseline.total_mass
    bool conserved{true};       // residual == 0.0 strictly
};

[[nodiscard]] SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet);

[[nodiscard]] SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet);
```

`compute_system_mass`:

- Iterates over every `cell` in `cells`.
- If `cell.h >= h_wet`, adds `cell.phi_t * cell.h * cell.area` to `surface_mass` and increments `wet_cell_count`.
- Always adds `cell.mass_deficit_account.volume` to `deficit_mass` (deficit ledger spans all cells regardless of wetness; §6.3 makes no wet-cell restriction on the ledger).
- Sets `total_mass = surface_mass + deficit_mass`.
- Throws `std::invalid_argument` if `h_wet <= 0.0`, or if any cell has `phi_t < 0`, `h < 0`, `area < 0`, or `mass_deficit_account.volume < 0`.

`audit_system_mass_against_reference`:

- Computes `current` via `compute_system_mass(current_cells, h_wet)`.
- Sets `residual = current.total_mass - baseline.total_mass`.
- Sets `conserved = (residual == 0.0)` — strict equality, no tolerance (§8.3 correctness path).
- Throws if `h_wet <= 0.0` or `baseline.total_mass < 0`.

Callers responsible for capturing the baseline once (e.g., at t=0 to obtain `M_ref`) and re-computing after each pipeline / replay cycle.

## Why `surface_mass + deficit_mass`

§5.1 v5.3.2 anchors **surface mass** to `phi_t h A`. §6.3 says the deficit ledger captures volume **not yet delivered** to 1D engines — i.e., volume that has left the surface accounting but is still owed by the system. For a closed audit over the CouplingLib core boundary (before any actual transfer to SWMM / D-Flow FM), the conserved quantity is the **sum**:

- Before any exchange: deficit_mass = 0, surface_mass = M_ref → total_mass = M_ref.
- After an exchange that fully transfers a request out of the surface: surface_mass drops by v_granted; deficit_mass picks up v_unmet (the remainder that should still leave but couldn't). If both v_granted and v_unmet are accounted for elsewhere (1D engine accepted v_granted; deficit ledger holds v_unmet), `surface_mass + deficit_mass` decreases by exactly `q_request * dt_sub` — the volume that left the core's control. So conservation is checked **against a re-computed baseline that accounts for known external transfers**, not against a static M_ref.

M31's helpers expose the raw quantity; **interpretation** (whether residual == 0 is correct in a given context) belongs to the caller. M27 already audits the per-call request-side identity; M31 audits the surface+ledger snapshot.

## What M31 Does NOT Do

- No `epsilon_mass`-based comparison. §8.3 correctness path requires strict 0; `epsilon_mass` is for the performance path (see §11.4 row 2 vs row 3). Performance-path acceptance is out of scope until a performance-path entry exists.
- No integration with `CouplingState::snapshot()` / `rollback()`. The audit is a read-only helper; callers feed `state.cells()` into it.
- No automatic baseline capture inside `CouplingState`. Adding a `baseline_mass_ref_` field belongs to a downstream milestone once the policy for when to capture M_ref is decided (per-simulation, per-restart, per-rollback?).
- No multi-donor weighting (`priority_weight`, `V_limit_k`).
- No write-off accounting (§6.3 mentions `count_writeoff_volume_total` but write-off is a separate ledger; M31 ignores it).
- No connection to `Surface2DCore` `h/hu/hv` audit; M31 only sees the `ExchangeCellState::h` exposed through the coupling DTO.
- No floating-point tolerance, no Kahan summation, no determinism analysis on summation order (current `std::vector` iteration order is the de facto contract; downstream may need stricter rules but M31 doesn't dictate them).
- No IO, no schema, no persistence.

## Tests

Create `tests/unit/coupling/test_coupling_system_mass_audit.cpp` with:

- `EmptyCellListProducesZeroMass` — empty vector → all fields 0; `wet_cell_count == 0`; throws never.
- `SingleWetCellComputesPhiTHAreaProduct` — one cell with `phi_t=0.4, h=2.0, area=50.0, deficit=0` → `surface_mass = 40.0`, `deficit_mass = 0.0`, `total_mass = 40.0`, `wet_cell_count = 1`.
- `DryCellsAreExcludedFromSurfaceMassButDeficitStillCounted` — two cells: one wet (`h=2.0`), one with `h = h_wet/2` (dry); the dry cell still contributes its deficit volume to `deficit_mass` but contributes 0 to `surface_mass`; `wet_cell_count == 1`.
- `DeficitVolumesAccumulateAcrossAllCells` — multiple cells with various deficits, including dry cells; `deficit_mass == sum`.
- `BoundaryHEqualsHWetIsCountedAsWet` — cell with `h == h_wet` exactly is included in surface_mass (per spec inclusive `h >= h_wet`).
- `RejectsInvalidInputs` — `h_wet <= 0`, `phi_t < 0`, `h < 0`, `area < 0`, `deficit < 0` each throw `std::invalid_argument`.
- `AuditDetectsZeroResidualWhenStateUnchanged` — snapshot baseline, do nothing, compute audit → `residual == 0.0`, `conserved == true`.
- `AuditDetectsNonZeroResidualWhenMassMissing` — baseline; then mutate one cell's `h` downward without corresponding deficit increase; audit → `residual < 0`, `conserved == false`.
- `AuditPreservesConservationWhenGrantedMatchedByDeficit` — baseline; then drop a cell's `h` by `dh` and add `phi_t * dh * area` to its deficit; audit → `residual == 0`, `conserved == true`. (This proves the surface+ledger sum is the right invariant.)
- `RejectsNegativeBaseline` — `baseline.total_mass < 0` throws.

Register `test_coupling_system_mass_audit` in `tests/unit/coupling/CMakeLists.txt`.

## Boundaries

- Pure helpers; no `CouplingState` mutation; no snapshot/rollback wiring.
- Strict `==` equality on conservation residual; no tolerance.
- No multi-donor, `priority_weight`, `V_limit_k`, or DTO refactor.
- No automatic baseline capture or persistence; caller manages baseline.
- No connection to SWMM / D-Flow FM / `Surface2DCore`.
- No write-off ledger.
- No determinism / summation-order policy.
- No CMake target outside `tests/unit/coupling/`.
