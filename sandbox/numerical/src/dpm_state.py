from __future__ import annotations

from dataclasses import dataclass

from mesh import Mesh


@dataclass(frozen=True)
class CellDpmFields:
    phi_t: float
    Phi_c: float


@dataclass(frozen=True)
class EdgeDpmFields:
    phi_e_n: float
    omega_edge: float


@dataclass(frozen=True)
class CellState:
    h: float
    hu: float
    hv: float
    eta: float

    @property
    def u(self) -> float:
        return self.hu / self.h if self.h > 0.0 else 0.0

    @property
    def v(self) -> float:
        return self.hv / self.h if self.h > 0.0 else 0.0


@dataclass(frozen=True)
class DpmState:
    cells: dict[str, CellState]
    cell_fields: dict[str, CellDpmFields]
    edge_fields: dict[str, EdgeDpmFields]


def hydrostatic_state(
    mesh: Mesh,
    *,
    eta: float = 1.0,
    depth: float = 1.0,
    phi_t: dict[str, float] | None = None,
    Phi_c: dict[str, float] | None = None,
    phi_e_n: dict[str, float] | None = None,
    omega_edge: dict[str, float] | None = None,
) -> DpmState:
    cells = {cell.id: CellState(h=depth, hu=0.0, hv=0.0, eta=eta) for cell in mesh.cells}
    cell_fields = {
        cell.id: CellDpmFields(
            phi_t=phi_t[cell.id] if phi_t and cell.id in phi_t else 1.0,
            Phi_c=Phi_c[cell.id] if Phi_c and cell.id in Phi_c else 1.0,
        )
        for cell in mesh.cells
    }
    edge_fields = {
        edge.id: EdgeDpmFields(
            phi_e_n=phi_e_n[edge.id] if phi_e_n and edge.id in phi_e_n else 1.0,
            omega_edge=omega_edge[edge.id] if omega_edge and edge.id in omega_edge else 1.0,
        )
        for edge in mesh.edges
    }
    return DpmState(cells=cells, cell_fields=cell_fields, edge_fields=edge_fields)
