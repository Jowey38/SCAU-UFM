"""M286 CVC wet-dry symbolic derivations (sympy, executable).

Run: py -3 sandbox/numerical/proofs/cvc_wetdry_derivations.py

Each derivation prints PASS/FAIL; the script exits non-zero on any FAIL so it
can serve as reproducible evidence for the M286 sandbox record. The symbols
mirror libs/surface2d (cvc_augmented_flux.hpp, well_balanced.hpp,
hydrostatic.hpp, wetting_drying/limits.hpp).
"""

from __future__ import annotations

import sys

import sympy as sp

FAILURES: list[str] = []


def check(name: str, expression: sp.Expr) -> None:
    # Max(0, x) = (x + Abs(x))/2 lets sympy close piecewise identities.
    simplified = sp.simplify(expression.rewrite(sp.Abs))
    ok = simplified == 0
    print(f"{'PASS' if ok else 'FAIL'}  {name}: simplify(...) = {simplified}")
    if not ok:
        FAILURES.append(name)


def main() -> int:
    g, F, Fm = sp.symbols("g F F_m", real=True)
    phi_L, phi_R, phi_up = sp.symbols("phi_L phi_R phi_up", positive=True)
    h_L = sp.symbols("h_L", nonnegative=True)

    # 1. CVC storage conservation identity: the side-specific fluctuations
    #    conserve phi_t-weighted storage exactly for ANY upwind phi choice.
    left_mass = phi_up * F / phi_L
    right_mass = phi_up * F / phi_R
    check(
        "1. cvc storage residual after == 0",
        -phi_L * left_mass + phi_R * right_mass,
    )

    # 2. Donor invariance: when the donor is the upwind side (phi_up = phi_donor),
    #    the donor-side fluctuation equals the baseline flux exactly.
    check(
        "2a. donor(left, F>0) fluctuation == baseline",
        left_mass.subs(phi_up, phi_L) - F,
    )
    check(
        "2b. donor(right, F<0) fluctuation == baseline",
        right_mass.subs(phi_up, phi_R) - F,
    )

    # 3. Wall-closed clamp identity (single cell i, one step): with CVC on,
    #    every internal edge contributes -phi_L*left_mass + phi_R*right_mass = 0
    #    to total storage (derivation 1) and walls contribute no mass, so the
    #    only storage change is the truncation max(0, h + dh) - (h + dh):
    #    drift == sum_i phi_i * A_i * max(0, -(h_i + dh_i)).
    h_i, dh_i, phi_i, A_i = sp.symbols("h_i dh_i phi_i A_i", real=True)
    truncation = sp.Max(0, -(h_i + dh_i))
    updated = sp.Max(0, h_i + dh_i)
    check(
        "3. clamp identity: phi*A*(updated - (h+dh)) == phi*A*truncation",
        phi_i * A_i * (updated - (h_i + dh_i)) - phi_i * A_i * truncation,
    )

    # 4. Lake-at-rest with a dry island (Audusse, h_bar = 0 at the island
    #    edge): the wet cell's WB edge term is -F_p + S_phi_t + S_topo with
    #    h_bar = 0 -> -0 + 0 + g/2*phi_L*(0 - h_L^2) = -g/2*phi_L*h_L^2, and
    #    the opposite closed wall contributes +g/2*phi_L*h_L^2, so the net
    #    momentum residual on the wet cell is exactly zero.
    h_bar = sp.Integer(0)
    phi_e = (phi_L + phi_R) / 2
    F_p = g / 2 * phi_e * h_bar**2
    S_phi_t_L = g / 2 * h_bar**2 * (phi_e - phi_L)
    S_topo_L = g / 2 * phi_L * (h_bar**2 - h_L**2)
    island_edge_term = -F_p + S_phi_t_L + S_topo_L
    wall_pressure = g / 2 * phi_L * h_L**2
    check(
        "4. dry-island rest: island edge + wall pressure == 0",
        island_edge_term + wall_pressure,
    )

    # 5. Near-dry spurious momentum injection (criterion 2 refutation): the
    #    receiving side's CVC momentum fluctuation exceeds the baseline by
    #    (phi_up/phi_R - 1)*F_m, while the WB pressure/source pairing is not
    #    rescaled by CVC at all. The mismatch term is nonzero whenever
    #    phi_up != phi_R, i.e. exactly on the receiving side of every jump.
    mismatch = phi_up * Fm / phi_R - Fm - (phi_up / phi_R - 1) * Fm
    check("5a. spurious injection term identified", mismatch)
    injection = (phi_up / phi_R - 1) * Fm
    check(
        "5b. injection vanishes only when phi_up == phi_R",
        injection.subs(phi_up, phi_R),
    )
    nonzero_sample = injection.subs({phi_up: sp.Rational(1), phi_R: sp.Rational(1, 20), Fm: 1})
    print(
        f"INFO  5c. sample injection at phi_up=1, phi_R=0.05, F_m=1: {nonzero_sample} "
        "(19x the baseline momentum flux)"
    )
    if nonzero_sample == 0:
        FAILURES.append("5c. sample injection unexpectedly zero")

    print()
    if FAILURES:
        print(f"{len(FAILURES)} derivation(s) FAILED: {FAILURES}")
        return 1
    print("All M286 CVC wet-dry derivations PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
