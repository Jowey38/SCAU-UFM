# M261 STCF Case -> Surface2D Mesh Loader Evidence

Date: 2026-07-29
Base: master `1aab274` (M260 UGRID topology merged)

## Result

A strict STCF v5 UGRID case can now be loaded into a solver-ready
`LoadedSurface2DCase` containing:

- `mesh::Mesh` built from file topology;
- `DpmFields` with file-carried `phi_e_n`/`omega_edge` index-aligned to mesh
  edges;
- `SourceTermFields` with file-carried Manning roughness and zero exchange;
- per-cell `z_b` bed elevations.

The implementation lives in `libs/surface2d/stcf_bridge/` so dependency
ownership remains `surface2d -> stcf + mesh`; `libs/stcf` does not reverse-
depend on mesh.

## Orientation and ordering contract

- deterministic generated IDs: `N<i>`, `C<i>`, `E<i>`;
- face rings are normalized to counter-clockwise coordinate order;
- UGRID edges are iterated in exact file order;
- `edge_faces[e][0]` is loaded as left cell, optional `[1]` as right;
- edge endpoints are reordered to match the normalized left-face ring,
  satisfying `mesh::build_mesh` left-orientation validation;
- resulting normals point left -> right for internal edges and outward for
  left-only boundary edges;
- returned edge IDs/order are asserted unchanged before field assembly.

## Verification

`test_stcf_case_loader` covers:

1. mixed quadrilateral + triangle counts/types;
2. exact edge ID/order and `phi_e_n`/`omega_edge` index alignment;
3. clockwise face normalization;
4. left/right and outward normal direction;
5. zero-area geometry fail-closed;
6. strict NetCDF file -> read_stcf_case -> loader entry point;
7. loaded varying-phi_t, sloping-bed lake at rest: 10 steps, depth and momentum
   preserved within 1e-12.

Full windows-msvc Debug suite: **130/130 passed**; final `LNK1168 == 0`.

## Scope

The loader rejects zero-area or mesh-invalid geometry through the existing
`mesh::build_mesh` validation. A future PreProc/mesh-quality slice should
make convexity/skew/aspect quality gates explicit before export; this loader
does not repair geometry.
