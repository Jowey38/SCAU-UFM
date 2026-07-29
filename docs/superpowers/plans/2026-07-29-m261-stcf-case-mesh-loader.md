# M261 STCF Case -> Surface2D Mesh Loader Plan

Date: 2026-07-29
Base: master `1aab274` (M260 merged)

## Goal

Build `mesh::Mesh` directly from strict `stcf::StcfCase` topology while
preserving NetCDF edge order, so every `EdgeStcfFields[e]` remains aligned
with `mesh.edges[e]`. Provide one load-time aggregate containing the mesh and
solver-facing DPM/source/bed fields.

## Ownership

Implementation lives in `libs/surface2d/stcf_bridge/`, not `libs/stcf`:
Surface2D already depends on both stcf and mesh, while stcf must not reverse-
depend on mesh. The bridge owns only loading/assembly, never solver advance
or coupling decisions.

## Algorithm

1. Validate the strict `StcfCase`.
2. Generate deterministic IDs `N<i>`, `C<i>`, `E<i>`.
3. Normalize each face ring to counter-clockwise coordinate order (reverse
   node indices when signed area is negative). Reject zero signed area and
   self-intersecting/invalid geometry through `mesh::build_mesh`.
4. Create triangle/quadrilateral cells from 3/4 face-node counts.
5. Iterate UGRID edges in file order. Use `edge_faces[e][0]` as the left cell
   and optional `[1]` as right. Reorder the two edge nodes so they match the
   left cell's normalized ring direction. This satisfies mesh::build_mesh's
   explicit left-orientation rule and yields normals from left to right; a
   boundary edge is left-only and its normal points outward.
6. Call `mesh::build_mesh(nodes,cells,edge_specs)`. Assert returned edge count
   and deterministic IDs/order match the topology.
7. Assemble DPM fields, source fields and z_b with the existing stcf_bridge
   functions. Return a `LoadedSurface2DCase` aggregate.

## Tests

- mixed quad+triangle case preserves node/cell/edge count and edge order;
- every STCF edge field remains on the same loaded mesh edge index;
- clockwise faces are normalized and produce valid outward/internal normals;
- invalid zero-area geometry fails closed even if topology indices are valid;
- full NetCDF `read_stcf_case -> load_surface2d_case` round trip;
- loaded sloping-bed lake at rest advances without mass/momentum drift.
