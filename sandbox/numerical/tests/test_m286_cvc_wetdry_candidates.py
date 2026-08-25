"""M286 CVC wet-dry sandbox: failure-revealing candidates + exactness proofs.

Per the M282 plan (Phase D-4 entry), each M282 exit criterion is proven or
refuted on the CURRENT first-order closure BEFORE any solver change. Tests
named ``*_candidate_*`` are failure-revealing in the M253/G-candidate sense:
they assert that the documented defect IS present at a quantified magnitude,
so any future high-order CVC / wet-dry closure that repairs the defect must
flip them consciously (never silently).

Criterion map (M282 plan):
 1. positivity            -> PositivityClampTruncation (+ donor lemma)
 2. near-dry              -> NearDrySpuriousVelocity (REFUTED, quantified)
 3. arbitrary wet/dry     -> DamBreakDryFrontStall (REFUTED: front stall)
                             LakeAtRestDryIsland (EXACT: proof-style)
 4. replay                -> ReplayBitwise (bitwise)
 5. candidates before fix -> this file itself
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from cvc_wetdry import (  # noqa: E402
    CellState,
    EdgeFlux,
    Strip,
    cvc_side_fluxes,
    run,
    step,
)


class DonorInvarianceLemma(unittest.TestCase):
    """CVC never amplifies the donor side; only the receiver is rescaled."""

    def test_positive_flux_donor_is_left_and_unamplified(self) -> None:
        baseline = EdgeFlux(mass=0.3, momentum_n=0.1)
        side = cvc_side_fluxes(baseline, 1.0, 0.2)
        self.assertEqual(side.left_mass, baseline.mass)
        self.assertEqual(side.left_momentum_x, baseline.momentum_n)
        # Receiver amplified by phi_up / phi_receiver = 1.0 / 0.2.
        self.assertAlmostEqual(side.right_mass, baseline.mass / 0.2, places=12)

    def test_negative_flux_donor_is_right_and_unamplified(self) -> None:
        baseline = EdgeFlux(mass=-0.3, momentum_n=0.1)
        side = cvc_side_fluxes(baseline, 0.2, 1.0)
        self.assertEqual(side.right_mass, baseline.mass)
        self.assertAlmostEqual(side.left_mass, baseline.mass / 0.2, places=12)

    def test_storage_residual_after_is_exactly_zero(self) -> None:
        side = cvc_side_fluxes(EdgeFlux(mass=0.7, momentum_n=-0.4), 0.9, 0.1)
        self.assertEqual(side.storage_residual_after, 0.0)


class StorageClosure(unittest.TestCase):
    """CVC repairs the baseline phi_t-weighted storage drift (wall-closed)."""

    @staticmethod
    def _strip(enable_cvc: bool) -> Strip:
        return Strip(
            h=[1.0, 0.01, 1.0],
            hu=[-3.0, 0.0, 3.0],
            z_b=[0.0, 0.0, 0.0],
            phi_t=[1.0, 0.05, 1.0],
            enable_cvc=enable_cvc,
            dx=0.5,
        )

    def test_cvc_on_single_step_storage_drift_is_zero(self) -> None:
        strip = self._strip(enable_cvc=True)
        before = strip.storage_total()
        diagnostics = step(strip, dt=0.01)
        self.assertEqual(diagnostics.clamp_storage_error, 0.0)
        self.assertAlmostEqual(strip.storage_total() - before, 0.0, delta=1.0e-15)

    def test_candidate_cvc_off_baseline_drift_is_macroscopic(self) -> None:
        strip = self._strip(enable_cvc=False)
        before = strip.storage_total()
        step(strip, dt=0.01)
        drift = strip.storage_total() - before
        # Known G23-class defect of the uncorrected baseline across a
        # phi_t jump: ~1.1e-2 storage loss in ONE step on this fixture.
        self.assertLess(drift, -1.0e-3)


class LakeAtRestDryIsland(unittest.TestCase):
    """M282 criterion 3 (partial-dry rest): EXACT on the current kernel.

    A wet basin at rest around a dry island (bed above the free surface)
    with a phi_t jump at the island edges stays bitwise at rest: the Audusse
    S_topo term at an h_bar = 0 edge exactly closes the cell pressure
    polygon (see proofs/cvc_wetdry_derivations.py, derivation 4).
    """

    def test_dry_island_with_phi_jump_stays_bitwise_at_rest(self) -> None:
        strip = Strip(
            h=[1.0, 1.0, 0.0, 1.0, 1.0],
            hu=[0.0] * 5,
            z_b=[0.0, 0.0, 2.0, 0.0, 0.0],
            phi_t=[1.0, 1.0, 0.4, 0.4, 0.4],
            enable_cvc=True,
        )
        h_before = list(strip.h)
        storage_before = strip.storage_total()
        run(strip, dt=0.01, steps=50)
        self.assertEqual(strip.h, h_before)
        self.assertEqual(strip.hu, [0.0] * 5)
        self.assertEqual(strip.storage_total() - storage_before, 0.0)


class DamBreakDryFrontStall(unittest.TestCase):
    """M282 criterion 3 (arbitrary wet/dry): REFUTED on the current kernel.

    The production dry gate (hllc.hpp: zero flux when either reconstructed
    depth <= h_min) means an advective front can NEVER wet a dry cell. The
    dam-break front error against any physical reference is O(1) at every
    resolution, so no convergence study is meaningful until a wetting-capable
    closure exists.
    """

    def test_candidate_flat_bed_dam_break_front_never_advances(self) -> None:
        strip = Strip(
            h=[1.0] * 5 + [0.0] * 5,
            hu=[0.0] * 10,
            z_b=[0.0] * 10,
            phi_t=[1.0] * 5 + [0.3] * 5,
            enable_cvc=True,
            dx=0.1,
        )
        diagnostics = run(strip, dt=0.001, steps=200)
        self.assertEqual(strip.h[5:], [0.0] * 5)
        self.assertEqual(strip.h[:5], [1.0] * 5)
        # The wet/dry interface edge plus the 4 dry-dry edges stay gated.
        self.assertEqual(diagnostics[-1].dry_gated_edges, 5)

    def test_candidate_submerged_step_flood_stalls(self) -> None:
        # eta_wet = 1.0 clears the dry bed z_b = 0.5; water must flood the
        # dry cell physically, but the gate zeroes the edge forever.
        strip = Strip(
            h=[1.0, 0.0],
            hu=[0.0, 0.0],
            z_b=[0.0, 0.5],
            phi_t=[1.0, 0.3],
            enable_cvc=True,
        )
        run(strip, dt=0.001, steps=200)
        self.assertEqual(strip.h[1], 0.0)
        self.assertEqual(strip.h[0], 1.0)


class NearDrySpuriousVelocity(unittest.TestCase):
    """M282 criterion 2 (near-dry): REFUTED for the current CVC closure.

    Filling a just-wet cell across a phi_t jump, the CVC-rescaled momentum
    (amplified by phi_up / phi_receiver) is not paired with any rescaling of
    the WB pressure/source terms, so the receiving near-dry cell acquires a
    spurious back-jet at ~0.9x the deep-water celerity sqrt(g*h_wet). With
    the correction off the same fixture keeps |u| at machine-noise level.
    """

    @staticmethod
    def _fill_thin_cell(enable_cvc: bool) -> float:
        strip = Strip(
            h=[1.0, 1.5e-8, 1.0],
            hu=[0.0] * 3,
            z_b=[0.0] * 3,
            phi_t=[1.0, 0.05, 0.05],
            enable_cvc=enable_cvc,
        )
        max_abs_u = 0.0
        for _ in range(8):
            step(strip, dt=0.0005)
            if strip.h[1] > 0.0:
                max_abs_u = max(max_abs_u, abs(strip.hu[1] / strip.h[1]))
        return max_abs_u

    def test_candidate_cvc_on_back_jet_near_celerity(self) -> None:
        max_abs_u = self._fill_thin_cell(enable_cvc=True)
        celerity = (9.81 * 1.0) ** 0.5
        self.assertGreater(max_abs_u, 0.8 * celerity)

    def test_cvc_off_velocity_stays_machine_noise(self) -> None:
        self.assertLess(self._fill_thin_cell(enable_cvc=False), 1.0e-12)


class PositivityClampTruncation(unittest.TestCase):
    """M282 criterion 1 (positivity): updated h >= 0 holds only by silent
    truncation; with CVC on the wall-closed storage drift equals the clamped
    truncation exactly (closure identity), so the clamp is the sole
    conservation leak.

    The fixture deliberately violates CFL so the truncation path fires
    deterministically; in production such a step trips C_rollback, but the
    clamp is the last-resort guard the criterion targets.
    """

    @staticmethod
    def _violent_strip(enable_cvc: bool) -> Strip:
        return Strip(
            h=[1.0, 0.01, 1.0],
            hu=[0.0, 0.05, 0.0],
            z_b=[0.0] * 3,
            phi_t=[0.05, 0.05, 1.0],
            enable_cvc=enable_cvc,
            dx=0.05,
        )

    def test_candidate_clamp_truncates_macroscopic_storage(self) -> None:
        strip = self._violent_strip(enable_cvc=True)
        diagnostics = step(strip, dt=0.1)
        self.assertGreater(diagnostics.clamp_storage_error, 1.0e-2)
        self.assertTrue(all(h >= 0.0 for h in strip.h))

    def test_cvc_on_drift_equals_clamp_error_identity(self) -> None:
        strip = self._violent_strip(enable_cvc=True)
        before = strip.storage_total()
        diagnostics = step(strip, dt=0.1)
        drift = strip.storage_total() - before
        self.assertAlmostEqual(drift, diagnostics.clamp_storage_error, places=12)

    def test_candidate_cvc_off_drift_breaks_the_identity(self) -> None:
        strip = self._violent_strip(enable_cvc=False)
        before = strip.storage_total()
        diagnostics = step(strip, dt=0.1)
        drift = strip.storage_total() - before
        self.assertGreater(abs(drift - diagnostics.clamp_storage_error), 1.0e-2)


class ReplayBitwise(unittest.TestCase):
    """M282 criterion 4: bitwise snapshot/replay with the correction on."""

    def test_400_step_mixed_wet_dry_replay_is_bitwise(self) -> None:
        strip = Strip(
            h=[1.0, 0.6, 0.2, 1.0e-6, 0.0, 0.4],
            hu=[0.1, -0.2, 0.05, 0.0, 0.0, 0.3],
            z_b=[0.0, 0.1, 0.3, 0.5, 0.6, 0.0],
            phi_t=[1.0, 0.7, 0.4, 0.2, 0.2, 1.0],
            enable_cvc=True,
            dx=0.2,
        )
        snapshot = strip.copy()
        run(strip, dt=0.002, steps=400)
        replay = snapshot.copy()
        run(replay, dt=0.002, steps=400)
        self.assertEqual(strip.h, replay.h)
        self.assertEqual(strip.hu, replay.hu)


if __name__ == "__main__":
    unittest.main()
