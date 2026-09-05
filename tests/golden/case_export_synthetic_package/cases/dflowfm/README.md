# Optional D-Flow FM input contract

This directory is intentionally a placeholder in the synthetic package. No fake D-Flow FM runtime, DLL, or native network is committed.

If river coupling is in scope, the data provider must supply an authorized and runnable D-Flow FM case, normally including:

```text
model.mdu
model.ext
model.bc (when referenced)
UGRID/NetCDF network files
restart/initial files (when required)
all other files referenced by the case
```

The provider must also supply `boundary_mapping.csv` with stable native boundary IDs, river geometry, 2D boundary relation, boundary type, direction, units, and confirmation status.

Required decisions:

- native D-Flow FM version/build;
- BMI or native runtime mode;
- source and target CRS;
- vertical datum and units;
- boundary IDs and their meaning;
- inflow/outflow/water-level/internal-exchange classification;
- left/right bank and normal direction;
- restart and timestep contract;
- authorized non-production runtime path for validation.

A river centerline alone is not sufficient to establish a production boundary contract. This package does not claim that D-Flow FM scope is available.
