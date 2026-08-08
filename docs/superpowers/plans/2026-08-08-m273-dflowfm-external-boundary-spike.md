# M273 D-Flow FM External-Boundary Contract Spike

## Scope

M273 is evidence-only. It may extend the standalone `spikes/dflowfm` host and add authored diagnostic cases/traces, but it must not add a production external-net provider, widen `IDFlowFMEngine`, wire SimDriver audit hooks, or promote G19.

## Question

Can the real D-Flow FM BMI runtime provide a complete cumulative external net-volume observation with proven boundary identity, orientation, units, sampling point, and restart behavior?

## Method

1. Reuse the M258 `sum(vol1)` storage contract.
2. Run an authored 1D reach with upstream `dischargebnd=0.125 m3/s` and downstream `waterlevelbnd=1.0 m` for 10 x 60 s.
3. Run a zero-discharge control with the same downstream boundary.
4. Probe `vol1`, `qext`, `qextreal`, `vextcum`, and indexed `q1` after initialization and each update.
5. Accept a contract only if BMI observations independently identify every external boundary, define orientation and units, and integrate to storage change without using `delta vol1` as both the observed result and inferred forcing.

## Exit criteria

- **CONFIRMED:** a reproducible complete cumulative external-net contract is demonstrated.
- **FAILED / BLOCKED:** any boundary class is unavailable, unmapped, ambiguously oriented, or not time-integrable from the sampled BMI values. Record the failure and required upstream evidence; do not implement a provider.
