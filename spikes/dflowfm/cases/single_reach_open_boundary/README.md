# M273 authored open-boundary spike case

This is a project-authored diagnostic case for the D-Flow FM external-boundary contract spike. It reuses the authored `single_reach_1d/single_reach_net.nc` geometry and adds:

- upstream constant `dischargebnd`;
- downstream constant `waterlevelbnd`;
- a zero-discharge control with the same downstream stage.

The case is spike-only. It is not a production fixture, GoldenSuite test, or external-net provider contract.

Run from this directory with the release D-Flow FM `bin` directory on the Windows DLL search path:

```bash
H:/scau-vb273/Release/dflowfm_spike_host.exe single_reach_open.mdu \
  --steps 10 --dt 60 --skip-boundary-write --stage-var s1 \
  --probe-sum-vars vol1,qext,qextreal,vextcum,q1 \
  --probe-values-vars q1 \
  --trace-out m273_external_boundary_indexed.trace.txt
```

The committed traces are the raw compact BMI observations. Generated D-Flow FM map/history/restart output and cache files are ignored.
