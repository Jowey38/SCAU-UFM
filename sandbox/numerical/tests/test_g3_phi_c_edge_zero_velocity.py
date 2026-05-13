from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import unittest

from run_golden_sandbox import run_g3


class G3PhiCEdgeZeroVelocityTest(unittest.TestCase):
    def test_g3_phi_c_edge_zero_velocity(self) -> None:
        results = run_g3()
        for diagnostics in results.values():
            self.assertLess(diagnostics.max_velocity, 1.0e-12)
        mixed_e1 = next(edge for edge in results["mixed"].edges if edge.edge_id == "E1")
        self.assertEqual(mixed_e1.mass_flux, 0.0)


if __name__ == "__main__":
    unittest.main()
