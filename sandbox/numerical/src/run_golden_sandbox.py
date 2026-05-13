from __future__ import annotations

import sys
from pathlib import Path

from dpm_state import hydrostatic_state
from mesh import build_mixed_minimal_mesh, build_quad_only_control_mesh, build_tri_only_control_mesh
from time_integration import StepDiagnostics, advance_fixed_steps

ETA_TOL = 1.0e-12
U_HYDRO_TOL = 1.0e-12
STEPS = 1000


def _assert_pass(diagnostics: StepDiagnostics) -> None:
    if diagnostics.max_eta_error >= ETA_TOL:
        raise AssertionError(f"eta error {diagnostics.max_eta_error} exceeds {ETA_TOL}")
    if diagnostics.max_velocity >= U_HYDRO_TOL:
        raise AssertionError(f"velocity {diagnostics.max_velocity} exceeds {U_HYDRO_TOL}")
    for edge in diagnostics.edges:
        if abs(edge.mass_flux) >= U_HYDRO_TOL:
            raise AssertionError(f"edge {edge.edge_id} mass flux {edge.mass_flux} exceeds {U_HYDRO_TOL}")
        if abs(edge.residual) >= ETA_TOL:
            raise AssertionError(f"edge {edge.edge_id} phi_t residual {edge.residual} exceeds {ETA_TOL}")


def _run_case(name: str, mesh_builder, state_builder) -> StepDiagnostics:
    mesh = mesh_builder()
    state = state_builder(mesh)
    _, diagnostics = advance_fixed_steps(mesh, state, steps=STEPS, eta0=1.0)
    _assert_pass(diagnostics)
    print(f"{name}: PASS max_eta_error={diagnostics.max_eta_error:.3e} max_velocity={diagnostics.max_velocity:.3e} max_cell_cfl={diagnostics.max_cell_cfl:.3e}")
    for edge in diagnostics.edges:
        print(
            f"  {edge.edge_id}: mass_flux={edge.mass_flux:.3e} momentum_flux_n={edge.momentum_flux_n:.3e} "
            f"pressure_pairing={edge.pressure_pairing:.3e} s_phi_t={edge.s_phi_t:.3e} residual={edge.residual:.3e}"
        )
    return diagnostics


def run_g1() -> dict[str, StepDiagnostics]:
    return {
        "mixed": _run_case("G1 mixed", build_mixed_minimal_mesh, lambda mesh: hydrostatic_state(mesh)),
        "quad_only": _run_case("G1 quad-only", build_quad_only_control_mesh, lambda mesh: hydrostatic_state(mesh)),
        "tri_only": _run_case("G1 tri-only", build_tri_only_control_mesh, lambda mesh: hydrostatic_state(mesh)),
    }


def run_g2() -> dict[str, StepDiagnostics]:
    def state_builder(mesh):
        phi_t = {cell.id: 1.0 + 0.25 * index for index, cell in enumerate(mesh.cells)}
        return hydrostatic_state(mesh, phi_t=phi_t)

    return {
        "mixed": _run_case("G2 mixed", build_mixed_minimal_mesh, state_builder),
        "quad_only": _run_case("G2 quad-only", build_quad_only_control_mesh, state_builder),
        "tri_only": _run_case("G2 tri-only", build_tri_only_control_mesh, state_builder),
    }


def run_g3() -> dict[str, StepDiagnostics]:
    def state_builder(mesh):
        Phi_c = {cell.id: 1.0 + 0.1 * index for index, cell in enumerate(mesh.cells)}
        phi_e_n = {edge.id: (0.0 if edge.id == "E1" else 1.0) for edge in mesh.edges}
        omega_edge = {edge.id: (0.0 if edge.id == "E1" else 1.0) for edge in mesh.edges}
        return hydrostatic_state(mesh, Phi_c=Phi_c, phi_e_n=phi_e_n, omega_edge=omega_edge)

    return {
        "mixed": _run_case("G3 mixed", build_mixed_minimal_mesh, state_builder),
        "quad_only": _run_case("G3 quad-only", build_quad_only_control_mesh, state_builder),
        "tri_only": _run_case("G3 tri-only", build_tri_only_control_mesh, state_builder),
    }


def _write_evidence(case: str, results: dict[str, StepDiagnostics]) -> None:
    evidence_dir = Path(__file__).resolve().parents[1] / "evidence"
    lines = [
        f"# {case.upper()} sandbox run evidence",
        "",
        "**日期**: 2026-05-12",
        "**步数**: 1000 fixed steps",
        "",
        "## 结果",
        "",
    ]
    for mesh_name, diagnostics in results.items():
        lines.extend(
            [
                f"### {mesh_name}",
                "",
                f"- max_eta_error: `{diagnostics.max_eta_error:.3e}`",
                f"- max_velocity: `{diagnostics.max_velocity:.3e}`",
                f"- max_cell_cfl: `{diagnostics.max_cell_cfl:.3e}`",
                "- conclusion: `PASS`",
                "",
                "| cell | eta_error | velocity |",
                "|---|---:|---:|",
            ]
        )
        for cell in diagnostics.cells:
            lines.append(f"| `{cell.cell_id}` | `{cell.eta_error:.3e}` | `{cell.velocity:.3e}` |")
        lines.extend(
            [
                "",
                "| edge | mass_flux | momentum_flux_n | pressure_pairing | S_phi_t | residual |",
                "|---|---:|---:|---:|---:|---:|",
            ]
        )
        for edge in diagnostics.edges:
            lines.append(
                f"| `{edge.edge_id}` | `{edge.mass_flux:.3e}` | `{edge.momentum_flux_n:.3e}` | "
                f"`{edge.pressure_pairing:.3e}` | `{edge.s_phi_t:.3e}` | `{edge.residual:.3e}` |"
            )
        lines.append("")
    (evidence_dir / f"{case}_run.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    selected = sys.argv[1:] or ["g1", "g2", "g3"]
    runners = {"g1": run_g1, "g2": run_g2, "g3": run_g3}
    for case in selected:
        results = runners[case]()
        _write_evidence(case, results)


if __name__ == "__main__":
    main()
