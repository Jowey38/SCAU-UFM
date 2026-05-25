# SWMM spike test cases

This directory will hold `.inp` files driving the SWMM 5.2.4 spike host.
M29 does NOT place real `.inp` files because EPA SWMM5 example licensing must
be verified first (deferred to M30+ placement decision).

Required cases per third-party-spike-design §5.1:

- `single_pipe.inp`: upstream constant inflow → single pipe → free outfall.
  Minimal sanity case for §3.1 lifecycle + §3.2 time advance + §3.3 state IO.
- `manhole_overflow.inp`: case with one surcharging manhole. Backs the
  §3.3 read assumption and the G8 GoldenTest physical basis.

Until those files are placed, the spike host can still be compiled to verify
ABI shape; running requires real `.inp` inputs.
