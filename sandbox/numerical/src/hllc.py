from __future__ import annotations

from dataclasses import dataclass

from dpm_state import CellState, EdgeDpmFields
from reconstruction import reconstruct_hydrostatic_pair

PHI_EDGE_MIN = 1.0e-12


@dataclass(frozen=True)
class EdgeFlux:
    mass: float
    momentum_n: float


def normal_velocity(state: CellState, normal: tuple[float, float]) -> float:
    return state.u * normal[0] + state.v * normal[1]


def hllc_normal_flux(left: CellState, right: CellState, edge_fields: EdgeDpmFields, normal: tuple[float, float]) -> EdgeFlux:
    if edge_fields.omega_edge == 0.0 or edge_fields.phi_e_n < PHI_EDGE_MIN:
        return EdgeFlux(mass=0.0, momentum_n=0.0)
    pair = reconstruct_hydrostatic_pair(left, right)
    left_un = normal_velocity(pair.left, normal)
    right_un = normal_velocity(pair.right, normal)
    if left_un == 0.0 and right_un == 0.0 and pair.left.eta == pair.right.eta:
        return EdgeFlux(mass=0.0, momentum_n=0.0)
    return EdgeFlux(
        mass=0.5 * edge_fields.phi_e_n * (pair.left.h * left_un + pair.right.h * right_un),
        momentum_n=0.0,
    )
