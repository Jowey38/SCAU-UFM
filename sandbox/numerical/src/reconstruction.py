from __future__ import annotations

from dataclasses import dataclass

from dpm_state import CellState


@dataclass(frozen=True)
class HydrostaticPair:
    left: CellState
    right: CellState


def reconstruct_hydrostatic_pair(left: CellState, right: CellState) -> HydrostaticPair:
    eta = min(left.eta, right.eta)
    left_depth = max(0.0, left.h - (left.eta - eta))
    right_depth = max(0.0, right.h - (right.eta - eta))
    return HydrostaticPair(
        left=CellState(h=left_depth, hu=0.0, hv=0.0, eta=eta),
        right=CellState(h=right_depth, hu=0.0, hv=0.0, eta=eta),
    )
