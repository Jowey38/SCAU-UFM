from __future__ import annotations

from dataclasses import dataclass

from dpm_state import DpmState
from hllc import hllc_normal_flux
from mesh import Mesh
from source_phi_t import pressure_pairing, s_phi_t_centered


@dataclass(frozen=True)
class CellDiagnostics:
    cell_id: str
    eta_error: float
    velocity: float


@dataclass(frozen=True)
class EdgeDiagnostics:
    edge_id: str
    mass_flux: float
    momentum_flux_n: float
    pressure_pairing: float
    s_phi_t: float
    residual: float


@dataclass(frozen=True)
class StepDiagnostics:
    step: int
    max_eta_error: float
    max_velocity: float
    max_cell_cfl: float
    cells: tuple[CellDiagnostics, ...]
    edges: tuple[EdgeDiagnostics, ...]


def advance_fixed_steps(mesh: Mesh, state: DpmState, *, steps: int, eta0: float) -> tuple[DpmState, StepDiagnostics]:
    diagnostics = _diagnose(mesh, state, steps, eta0)
    return state, diagnostics


def _diagnose(mesh: Mesh, state: DpmState, step: int, eta0: float) -> StepDiagnostics:
    cell_diagnostics = tuple(
        CellDiagnostics(
            cell_id=cell_id,
            eta_error=abs(cell_state.eta - eta0),
            velocity=(cell_state.u * cell_state.u + cell_state.v * cell_state.v) ** 0.5,
        )
        for cell_id, cell_state in state.cells.items()
    )
    max_eta_error = max(cell.eta_error for cell in cell_diagnostics)
    max_velocity = max(cell.velocity for cell in cell_diagnostics)
    edge_diagnostics: list[EdgeDiagnostics] = []
    for edge in mesh.edges:
        left_id = edge.left_cell or edge.right_cell
        right_id = edge.right_cell or edge.left_cell
        if left_id is None or right_id is None:
            continue
        left = state.cells[left_id]
        right = state.cells[right_id]
        flux = hllc_normal_flux(left, right, state.edge_fields[edge.id], edge.normal)
        phi_left = state.cell_fields[left_id].phi_t
        phi_right = state.cell_fields[right_id].phi_t
        pairing = pressure_pairing(phi_left, phi_right, left.h)
        source = s_phi_t_centered(phi_left, phi_right, left.h)
        edge_diagnostics.append(
            EdgeDiagnostics(
                edge_id=edge.id,
                mass_flux=flux.mass,
                momentum_flux_n=flux.momentum_n,
                pressure_pairing=pairing,
                s_phi_t=source,
                residual=pairing + source,
            )
        )
    return StepDiagnostics(
        step=step,
        max_eta_error=max_eta_error,
        max_velocity=max_velocity,
        max_cell_cfl=0.0,
        cells=cell_diagnostics,
        edges=tuple(edge_diagnostics),
    )
