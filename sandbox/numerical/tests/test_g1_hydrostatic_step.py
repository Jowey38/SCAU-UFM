from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import unittest

from run_golden_sandbox import run_g1


class G1HydrostaticStepTest(unittest.TestCase):
    def test_g1_hydrostatic_step(self) -> None:
        results = run_g1()
        self.assertEqual(set(results), {"mixed", "quad_only", "tri_only"})


if __name__ == "__main__":
    unittest.main()
