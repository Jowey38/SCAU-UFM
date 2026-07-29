# M260 STCF v5 UGRID Topology I/O Plan

Date: 2026-07-28
Base: master `f62e22a`
Branch: `feat/m260-stcf-ugrid-topology`

## Goal

Add a strict, self-contained STCF case contract that carries both Surface2D
fields and mixed triangle/quadrilateral mesh topology. Keep the existing
field-only `read_stcf` / `write_stcf` API intact for explicit sidecar-mesh
workflows and G21 compatibility.

## File contract

`write_stcf_case` first writes the validated field dataset, then adds CF-1.8 /
UGRID-1.0 topology:

- global `Conventions = "CF-1.8 UGRID-1.0"`, existing
  `schema_version = 5`;
- dimensions: existing `cell`, `edge`, `soil_type_entry`, plus
  `nMesh2_node`, `nMesh2_max_face_nodes = 4`, `Two = 2`;
- scalar `mesh2` with `cf_role=mesh_topology`, `topology_dimension=2`,
  connectivity/coordinate attributes, and explicit
  `face_dimension=cell`, `edge_dimension=edge`;
- `mesh2_node_x/y(nMesh2_node)`;
- `mesh2_face_nodes(cell,nMesh2_max_face_nodes)`, zero-based, fill -1;
- `mesh2_edge_nodes(edge,Two)`, zero-based;
- `mesh2_edge_faces(edge,Two)`, zero-based, fill -1;
- `mesh2_face_edges(cell,nMesh2_max_face_nodes)`, zero-based, fill -1;
- every STCF cell/edge variable gets UGRID `mesh=mesh2` and
  `location=face|edge` attributes.

Using existing `cell`/`edge` dimensions is UGRID-compliant (dimension names
are not prescribed) and preserves `read_stcf` compatibility for complete
case files. New generated identifiers (`N<i>`, `C<i>`, `E<i>`) belong to the
future mesh-loader seam, not the STCF storage contract.

## Validation

`validate_stcf_case` runs field validation plus topology checks:

- positive node/face/edge counts and exact field/topology count agreement;
- finite node coordinates;
- each face has contiguous 3 or 4 valid, distinct nodes then fill values;
- each edge has two valid, distinct nodes;
- each edge has one or two valid, distinct adjacent faces;
- each face has exactly its node-count valid, distinct edges;
- connectivity cross-consistency: every face edge references the face and
  its endpoint pair occurs consecutively in the face ring.

`read_stcf_case` is strict: a field-only v5 file is rejected. It verifies
required UGRID variables, dimensions, type, ordering, `start_index=0`, fill
value, topology attributes, and then validates the reconstructed case.

## Compatibility

- Existing `read_stcf` can read both old field-only v5 and new complete case
  files because `cell`/`edge` field dimensions remain unchanged.
- Existing `write_stcf` continues to produce field-only files.
- A complete case is only accepted through `read_stcf_case`; no silent
  fallback to an external mesh.
- G7 v4->v5 migration remains a later dedicated mapping slice.

## Verification

- topology validation accept/reject matrix;
- complete NetCDF round trip, including mixed 3/4-node faces;
- field-only file rejected by strict case reader but still readable by
  `read_stcf`;
- NetCDF attribute/connectivity tamper failures;
- full repository ctest under windows-msvc and hosted CI.
