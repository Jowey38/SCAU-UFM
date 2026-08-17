# M280 CUDA determinism spike

Standalone spike (NOT in the main graph) proving the deterministic-CUDA
pattern required by the M266 backend contract before any CUDA code enters
the Surface2D core:

- gather-by-owner per-cell update (fixed per-cell edge order, no atomics);
- fixed-order block-tree reduction with sequential host combination;
- double precision throughout.

Build and run (governed host, CUDA 12.8, VS2022):

```bat
spikes\cuda\determinism\build.cmd
spikes\cuda\determinism\cuda_determinism_spike.exe
```

PASS criteria: repeated device runs bitwise identical AND bitwise equal to a
CPU reference that replicates the same association order, for both the full
state vector and the whole-domain storage reduction.
