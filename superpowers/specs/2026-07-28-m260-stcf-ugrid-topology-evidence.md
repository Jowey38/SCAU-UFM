# M260 STCF v5 UGRID Topology I/O Evidence

Date: 2026-07-28
Base: master `f62e22a`

## Result

`libs/stcf` now supports a strict, self-contained STCF v5 case carrying both
Surface2D fields and mixed triangle/quadrilateral UGRID topology.

- New `MeshTopology` / `StcfCase` DTOs and `validate_stcf_case`.
- New `write_stcf_case` / `read_stcf_case` strict APIs.
- Existing field-only `write_stcf` / `read_stcf` remain unchanged. A complete
  case stays readable as fields, while the strict reader rejects field-only
  files. This keeps explicit sidecar-mesh workflows compatible without
  silently treating them as self-contained cases.

## NetCDF/UGRID contract

- global `schema_version=5`, `Conventions="CF-1.8 UGRID-1.0"`;
- scalar `mesh2` (`cf_role=mesh_topology`, topology_dimension=2, standard
  coordinate/connectivity attributes, explicit face_dimension=`cell` and
  edge_dimension=`edge`);
- `nMesh2_node`, `nMesh2_max_face_nodes=4`, `Two=2` plus existing
  `cell`/`edge`/`soil_type_entry` dimensions;
- node coordinates, face-node, edge-node, edge-face, and face-edge
  connectivity; zero-based start_index; -1 fill on ragged/optional slots;
- all STCF face/edge fields carry `mesh=mesh2` and `location=face|edge`.

The project specs required UGRID + NetCDF but did not prescribe concrete
variable names/start_index/fill. These choices follow CF-1.8/UGRID-1.0 and
are recorded in the M260 plan.

## Validation

Fail-closed validation covers:

- field/topology count agreement and finite node coordinates;
- contiguous 3/4-node faces, valid/distinct indices;
- valid/distinct edge endpoints and no duplicate topology edges;
- one/two valid distinct adjacent faces;
- exact face-edge counts with fill values in unused slots;
- bidirectional face/edge adjacency and endpoint/ring consistency;
- every edge referenced exactly once (boundary) or twice (internal);
- strict NetCDF type/dimension order, start_index/fill values, topology
  attributes, and field mesh/location bindings.

## Verification

- New tests: `test_stcf_topology` and `test_stcf_io_ugrid`.
- Mixed quad+triangle round trip preserves every topology array and field.
- Tamper tests cover start_index, topology attributes, field location binding,
  and connectivity values.
- Writer rejects invalid topology before touching disk.
- windows-msvc Debug full repository suite: **129/129 passed**.
- Final build `LNK1168 == 0`.

## Next slice

M261 maps `StcfCase.topology` to `mesh::Mesh`, preserving edge order so STCF
edge fields remain index-aligned. M262 adds the PreProc CLI and persistent
file-driven Golden.
