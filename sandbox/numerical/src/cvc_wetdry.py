"""M286 CVC wet-dry sandbox core (Phase D-4 entry, M282 plan).

A faithful 1D-strip port of the CURRENT first-order production kernels so the
M282 exit criteria can be proven or refuted before ANY solver wiring:

- ``reconstruct_hydrostatic_pair``  <- libs/surface2d/include/surface2d/reconstruction/hydrostatic.hpp
- ``estimate_hllc_wave_speeds`` / ``hllc_normal_flux``
                                    <- libs/surface2d/include/surface2d/riemann/hllc.hpp
- ``well_balanced_edge_pairing`` / ``well_balanced_boundary_pressure``
                                    <- libs/surface2d/include/surface2d/source_terms/well_balanced.hpp
- ``cvc_side_fluxes``               <- libs/surface2d/include/surface2d/dpm/cvc_augmented_flux.hpp
- ``nonnegative_depth_after_increment`` / ``apply_dry_cell_momentum_limit``
                                    <- libs/surface2d/include/surface2d/wetting_drying/limits.hpp
- step ordering (flux residuals -> WB pairing -> wall pressure -> depth clamp
  -> dry momentum limit -> momentum update)
                                    <- libs/surface2d/src/time_integration/step.cpp

The strip is a row of quad cells of width ``dx`` and unit height; every
internal edge normal is +x and every quantity is per unit edge length, which
matches the 2D kernel with ``normal = (1, 0)`` and ``edge.length = 1``.

Deviation from production, by design: the sandbox records the pre-clamp
negative depth mass that ``max(0, h + dh)`` silently truncates
(``clamp_storage_error``), because quantifying that conservation defect is an
M282 exit criterion. Production has no such diagnostic yet.

This module is sandbox-only evidence tooling. It changes no solver behavior
and must not be imported by production code.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace


@dataclass(frozen=True)
class CellState:
    h: float
    hu: float
    eta: float

    @property
    def u(self) -> float:
        return self.hu / self.h if self.h > 0.0 else 0.0

    @property
    def z_b(self) -> float:
        return self.eta - self.h


@dataclass(frozen=True)
class EdgeFlux:
    mass: float = 0.0
    momentum_n: float = 0.0


@dataclass(frozen=True)
class WaveSpeeds:
    s_l: float
    s_star: float
    s_r: float


# --- reconstruction/hydrostatic.hpp (Audusse, main-spec 5.4) ---------------


@dataclass(frozen=True)
class HydrostaticPair:
    left: CellState
    right: CellState


def reconstruct_hydrostatic_pair(left: CellState, right: CellState) -> HydrostaticPair:
    z_b_star = max(left.z_b, right.z_b)
    left_depth = max(0.0, left.eta - z_b_star)
    right_depth = max(0.0, right.eta - z_b_star)
    return HydrostaticPair(
        left=CellState(h=left_depth, hu=left_depth * left.u, eta=z_b_star + left_depth),
        right=CellState(h=right_depth, hu=right_depth * right.u, eta=z_b_star + right_depth),
    )


# --- riemann/hllc.hpp -------------------------------------------------------


def _wave_celerity(state: CellState, gravity: float) -> float:
    return math.sqrt(gravity * state.h)


def _physical_normal_mass_flux(state: CellState) -> float:
    return state.h * state.u


def _physical_normal_momentum_flux(state: CellState, gravity: float) -> float:
    un = state.u
    return state.h * un * un + 0.5 * gravity * state.h * state.h


def _advective_normal_momentum_flux(state: CellState) -> float:
    un = state.u
    return state.h * un * un


def _hllc_star_normal_mass_flux(state: CellState, s_k: float, s_star: float) -> float:
    un = state.u
    denominator = s_k - s_star
    if denominator == 0.0:
        return _physical_normal_mass_flux(state)
    h_star = state.h * (s_k - un) / denominator
    return _physical_normal_mass_flux(state) + s_k * (h_star - state.h)


def _hllc_star_normal_momentum_flux(state: CellState, s_k: float, s_star: float) -> float:
    un = state.u
    denominator = s_k - s_star
    if denominator == 0.0:
        return _advective_normal_momentum_flux(state)
    h_star = state.h * (s_k - un) / denominator
    momentum_star = h_star * s_star
    return _advective_normal_momentum_flux(state) + s_k * (momentum_star - state.h * un)


def estimate_hllc_wave_speeds(left: CellState, right: CellState, gravity: float = 9.81) -> WaveSpeeds:
    left_un = left.u
    right_un = right.u
    left_c = _wave_celerity(left, gravity)
    right_c = _wave_celerity(right, gravity)
    s_l = min(left_un - left_c, right_un - right_c)
    s_r = max(left_un + left_c, right_un + right_c)

    numerator = (
        right.h * right_un * (s_r - right_un)
        - left.h * left_un * (s_l - left_un)
        + _physical_normal_momentum_flux(left, gravity)
        - _physical_normal_momentum_flux(right, gravity)
    )
    denominator = right.h * (s_r - right_un) - left.h * (s_l - left_un)
    s_star = numerator / denominator if denominator != 0.0 else 0.0
    return WaveSpeeds(s_l=s_l, s_star=s_star, s_r=s_r)


def hllc_normal_flux(
    left: CellState,
    right: CellState,
    phi_e_n: float = 1.0,
    omega_edge: float = 1.0,
    h_min: float = 1.0e-8,
    gravity: float = 9.81,
) -> EdgeFlux:
    # dpm/edge_classification.hpp: hard-block edges zero the advective flux
    # (spec epsilon_omega = 1e-4, phi_edge_min = 0.01).
    if omega_edge <= 1.0e-4 or phi_e_n <= 0.01:
        return EdgeFlux()

    pair = reconstruct_hydrostatic_pair(left, right)
    # The production dry gate: either reconstructed side at or below h_min
    # zeroes the WHOLE advective edge flux.
    if pair.left.h <= h_min or pair.right.h <= h_min:
        return EdgeFlux()

    speeds = estimate_hllc_wave_speeds(pair.left, pair.right, gravity)

    if 0.0 <= speeds.s_l:
        mass = _physical_normal_mass_flux(pair.left)
        momentum_n = _advective_normal_momentum_flux(pair.left)
    elif speeds.s_l <= 0.0 <= speeds.s_star:
        mass = _hllc_star_normal_mass_flux(pair.left, speeds.s_l, speeds.s_star)
        momentum_n = _hllc_star_normal_momentum_flux(pair.left, speeds.s_l, speeds.s_star)
    elif speeds.s_star <= 0.0 <= speeds.s_r:
        mass = _hllc_star_normal_mass_flux(pair.right, speeds.s_r, speeds.s_star)
        momentum_n = _hllc_star_normal_momentum_flux(pair.right, speeds.s_r, speeds.s_star)
    else:
        mass = _physical_normal_mass_flux(pair.right)
        momentum_n = _advective_normal_momentum_flux(pair.right)

    return EdgeFlux(mass=phi_e_n * mass, momentum_n=phi_e_n * momentum_n)


# --- source_terms/well_balanced.hpp ----------------------------------------


@dataclass(frozen=True)
class WellBalancedEdgePairing:
    left_normal: float
    right_normal: float
    pressure_flux: float
    s_phi_t_left: float
    s_topo_left: float


def well_balanced_edge_pairing(
    phi_t_left: float,
    phi_t_right: float,
    h_left: float,
    h_right: float,
    h_left_star: float,
    h_right_star: float,
    gravity: float = 9.81,
) -> WellBalancedEdgePairing:
    h_bar = 0.5 * (h_left_star + h_right_star)
    h_bar_sq = h_bar * h_bar
    phi_t_e = 0.5 * (phi_t_left + phi_t_right)

    pressure_flux = 0.5 * gravity * phi_t_e * h_bar_sq
    s_phi_t_left = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_left)
    s_phi_t_right = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_right)
    s_topo_left = 0.5 * gravity * phi_t_left * (h_bar_sq - h_left * h_left)
    s_topo_right = 0.5 * gravity * phi_t_right * (h_bar_sq - h_right * h_right)

    return WellBalancedEdgePairing(
        left_normal=-pressure_flux + s_phi_t_left + s_topo_left,
        right_normal=-pressure_flux + s_phi_t_right + s_topo_right,
        pressure_flux=pressure_flux,
        s_phi_t_left=s_phi_t_left,
        s_topo_left=s_topo_left,
    )


def well_balanced_boundary_pressure(phi_t_inside: float, h_inside: float, gravity: float = 9.81) -> float:
    return 0.5 * gravity * phi_t_inside * h_inside * h_inside


# --- dpm/cvc_augmented_flux.hpp ---------------------------------------------


@dataclass(frozen=True)
class CvcSideFluxes:
    left_mass: float = 0.0
    right_mass: float = 0.0
    left_momentum_x: float = 0.0
    right_momentum_x: float = 0.0
    storage_residual_before: float = 0.0
    storage_residual_after: float = 0.0
    applied: bool = False


def cvc_side_fluxes(baseline: EdgeFlux, phi_t_left: float, phi_t_right: float) -> CvcSideFluxes:
    if not (math.isfinite(phi_t_left) and phi_t_left > 0.0):
        raise ValueError("CVC left phi_t must be finite and positive")
    if not (math.isfinite(phi_t_right) and phi_t_right > 0.0):
        raise ValueError("CVC right phi_t must be finite and positive")

    if phi_t_left == phi_t_right:
        return CvcSideFluxes(
            left_mass=baseline.mass,
            right_mass=baseline.mass,
            left_momentum_x=baseline.momentum_n,
            right_momentum_x=baseline.momentum_n,
        )

    if baseline.mass > 0.0:
        transport_phi = phi_t_left
    elif baseline.mass < 0.0:
        transport_phi = phi_t_right
    else:
        transport_phi = 0.5 * (phi_t_left + phi_t_right)

    mass_storage_flux = transport_phi * baseline.mass
    momentum_storage_flux = transport_phi * baseline.momentum_n

    left_mass = mass_storage_flux / phi_t_left
    right_mass = mass_storage_flux / phi_t_right
    left_momentum = momentum_storage_flux / phi_t_left
    right_momentum = momentum_storage_flux / phi_t_right

    return CvcSideFluxes(
        left_mass=left_mass,
        right_mass=right_mass,
        left_momentum_x=left_momentum,
        right_momentum_x=right_momentum,
        storage_residual_before=baseline.mass * (phi_t_right - phi_t_left),
        storage_residual_after=-phi_t_left * left_mass + phi_t_right * right_mass,
        applied=baseline.mass != 0.0 or baseline.momentum_n != 0.0,
    )


# --- wetting_drying/limits.hpp ----------------------------------------------


def nonnegative_depth_after_increment(h: float, depth_increment: float) -> float:
    return max(0.0, h + depth_increment)


def apply_dry_cell_momentum_limit(h: float, hu: float, h_min: float) -> float:
    return 0.0 if h <= h_min else hu


# --- 1D strip state and step (mirrors step.cpp ordering) --------------------


@dataclass
class Strip:
    """A 1D row of quad cells; edge i sits between cell i and cell i+1."""

    h: list[float]
    hu: list[float]
    z_b: list[float]
    phi_t: list[float]
    dx: float = 1.0
    gravity: float = 9.81
    h_min: float = 1.0e-8
    enable_cvc: bool = False

    def cell_state(self, i: int) -> CellState:
        return CellState(h=self.h[i], hu=self.hu[i], eta=self.z_b[i] + self.h[i])

    @property
    def n_cells(self) -> int:
        return len(self.h)

    def storage_total(self) -> float:
        # phi_t * h * A with A = dx * 1 (main-spec storage semantics).
        return sum(p * h * self.dx for p, h in zip(self.phi_t, self.h))

    def copy(self) -> "Strip":
        return Strip(
            h=list(self.h),
            hu=list(self.hu),
            z_b=list(self.z_b),
            phi_t=list(self.phi_t),
            dx=self.dx,
            gravity=self.gravity,
            h_min=self.h_min,
            enable_cvc=self.enable_cvc,
        )


@dataclass
class StepDiagnostics:
    mass_residual: list[float] = field(default_factory=list)
    momentum_residual: list[float] = field(default_factory=list)
    pre_clamp_negative_depth: list[float] = field(default_factory=list)
    clamp_storage_error: float = 0.0
    cvc_events: int = 0
    max_cvc_storage_residual_after: float = 0.0
    dry_gated_edges: int = 0


def step(strip: Strip, dt: float) -> StepDiagnostics:
    """One first-order step on the strip, wall-closed at both ends.

    Mirrors run_flux_core + apply_depth_update + apply_momentum_update from
    step.cpp with edge.length = 1 and area = dx. All edges are regular
    (omega_edge = 1, phi_e_n = 1) unless gated by the internal dry gate.
    """

    n = strip.n_cells
    diag = StepDiagnostics(
        mass_residual=[0.0] * n,
        momentum_residual=[0.0] * n,
        pre_clamp_negative_depth=[0.0] * n,
    )

    for i in range(n - 1):
        left = strip.cell_state(i)
        right = strip.cell_state(i + 1)
        flux = hllc_normal_flux(left, right, h_min=strip.h_min, gravity=strip.gravity)
        pair = reconstruct_hydrostatic_pair(left, right)
        if pair.left.h <= strip.h_min or pair.right.h <= strip.h_min:
            diag.dry_gated_edges += 1

        left_mass = flux.mass
        right_mass = flux.mass
        left_momentum = flux.momentum_n
        right_momentum = flux.momentum_n
        phi_l = strip.phi_t[i]
        phi_r = strip.phi_t[i + 1]
        if strip.enable_cvc and phi_l != phi_r:
            side = cvc_side_fluxes(flux, phi_l, phi_r)
            if side.applied:
                diag.cvc_events += 1
                diag.max_cvc_storage_residual_after = max(
                    diag.max_cvc_storage_residual_after, abs(side.storage_residual_after)
                )
            left_mass = side.left_mass
            right_mass = side.right_mass
            left_momentum = side.left_momentum_x
            right_momentum = side.right_momentum_x

        diag.mass_residual[i] -= left_mass
        diag.mass_residual[i + 1] += right_mass
        diag.momentum_residual[i] -= left_momentum
        diag.momentum_residual[i + 1] += right_momentum

        # WB pairing is assembled on every regular internal edge regardless of
        # the advective dry gate (step.cpp).
        wb = well_balanced_edge_pairing(
            phi_l, phi_r, left.h, right.h, pair.left.h, pair.right.h, strip.gravity
        )
        diag.momentum_residual[i] += wb.left_normal
        diag.momentum_residual[i + 1] += -wb.right_normal

    # Closed walls at both ends (step.cpp wall branch): pressure only.
    wall_left = well_balanced_boundary_pressure(strip.phi_t[0], strip.h[0], strip.gravity)
    wall_right = well_balanced_boundary_pressure(strip.phi_t[-1], strip.h[-1], strip.gravity)
    # Left wall normal points -x into cell 0: inside cell is the edge's RIGHT
    # cell, signed momentum = +wall_pressure along +x.
    diag.momentum_residual[0] += wall_left
    # Right wall normal points +x out of the last cell: signed = -wall_pressure.
    diag.momentum_residual[-1] -= wall_right

    # apply_depth_update: clamp negative depths, record the truncated storage.
    for i in range(n):
        dh = dt * diag.mass_residual[i] / strip.dx
        raw = strip.h[i] + dh
        if raw < 0.0:
            diag.pre_clamp_negative_depth[i] = -raw
            diag.clamp_storage_error += strip.phi_t[i] * (-raw) * strip.dx
        strip.h[i] = nonnegative_depth_after_increment(strip.h[i], dh)

    # apply_momentum_update: dry limit first, dry cells take no update.
    for i in range(n):
        strip.hu[i] = apply_dry_cell_momentum_limit(strip.h[i], strip.hu[i], strip.h_min)
        if strip.h[i] <= strip.h_min:
            continue
        strip.hu[i] += dt * diag.momentum_residual[i] / strip.dx

    return diag


def run(strip: Strip, dt: float, steps: int) -> list[StepDiagnostics]:
    return [step(strip, dt) for _ in range(steps)]
