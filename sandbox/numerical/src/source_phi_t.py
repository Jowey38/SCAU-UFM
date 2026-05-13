from __future__ import annotations


def pressure_pairing(phi_left: float, phi_right: float, h: float, gravity: float = 9.81) -> float:
    return 0.5 * gravity * h * h * (phi_right - phi_left)


def s_phi_t_centered(phi_left: float, phi_right: float, h: float, gravity: float = 9.81) -> float:
    return -pressure_pairing(phi_left, phi_right, h, gravity)
