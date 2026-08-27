# M287-A gmsh mesh-generation spike

Standalone spike proving that a governed gmsh subprocess can turn the
synthetic D-5 input package (`samples/d5_gis_preproc_template/`) into a
validated STCF v5 / CF-UGRID case, with fail-closed handling of degenerate
geometry and runaway meshing. Spike-spec section 2 isolation applies: nothing
here links main-repo targets; the only downstream contract is the generated
`.stcf.nc`, certified by the authoritative validator CLI.

## Requirements

- Python 3.11+ with `gmsh` (4.15.2 pinned, see `third_party/manifest/gmsh.version`)
  and `netCDF4` from PyPI.

## Run

```bash
py -3 spikes/gmsh_meshgen/run_spike.py samples/d5_gis_preproc_template <work_dir>
```

Scenarios executed by the runner:

1. **nominal** — boundary + two building holes, lc=8 m, Blossom
   recombination; runs twice and requires bitwise-identical `.stcf.nc`
   (SHA-256) plus a quality report (min angle / edge-length ratio vs the
   10 deg / 20 review thresholds).
2. **degenerate** — `fixtures/degenerate_self_intersecting.geojson` (bowtie
   footprint) must be rejected BEFORE meshing: exit 2, diagnostic GeoJSON
   naming the feature, no output and no temporary file.
3. **timeout** — nominal geometry with lc=0.08 m under a 20 s policy-override
   timeout: the runner kills the subprocess, records the
   `MeshGenerationFailed` outcome, and requires that no partial output
   exists.

The runner writes `spike_summary.json` and exits non-zero unless all three
scenarios (including determinism) hold.

## Authoritative certification

```bash
scau_preproc validate --input <work_dir>/nominal_1.stcf.nc
```

`validate` loads the file through the same read path the solver bridge uses
(full dataset + topology validation), so spike output is certified against
the production rules without duplicating any validator logic in Python.

## Governance

- gmsh is GPL-2.0+; it is imported only inside the generation subprocess
  (file-interchange boundary; see `third_party/licenses/gmsh-LICENSE-NOTE.md`).
- Determinism pinning: Mesh.Algorithm=6, Mesh.RandomSeed=1, single thread,
  Blossom recombination.
- Field values beyond the mesh (uniform `phi_t`/tensor/edge fields, nearest
  DEM `z_b` sampling, landcover Manning/soil lookup) are spike-scope
  placeholders; the M287-B pipeline owns real field derivation.
