from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import unittest

from run_golden_sandbox import run_g2


class G2PhiTJumpHydrostaticTest(unittest.TestCase):
    def test_g2_phi_t_jump_hydrostatic(self) -> None:
        results = run_g2()
        mixed_e1 = next(edge for edge in results["mixed"].edges if edge.edge_id == "E1")
        self.assertEqual(mixed_e1.mass_flux, 0.0)
        self.assertLess(abs(mixed_e1.residual), 1.0e-12)


if __name__ == "__main__":
    unittest.main()
