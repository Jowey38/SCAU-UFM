# M262 PreProc CLI + G22 File-Driven Mixed Mesh Evidence

Date: 2026-07-29
Base: master `5e450b4`

## Result

The repository now has an executable PreProc path:

```text
scau_preproc generate --profile mixed-minimal --output <case.stcf.nc> [--force]
```

The CLI writes a strict STCF v5 CF/UGRID case atomically through a temporary
file and the same `validate_stcf_case` export gate used by library callers.
It rejects missing/unknown/duplicate arguments and refuses to overwrite an
existing file without `--force`; failures remove the temporary file.

`libs/stcf/case_profiles.*` owns the deterministic authored profile, keeping
CLI parsing separate from physical data construction. `mixed-minimal`
contains a quadrilateral + triangle mesh, varying `phi_t`, SPD `Phi_c`,
Manning roughness, sloping bed, two Green-Ampt soil entries, and explicit
edge conveyance. No premature JSON/YAML/GIS input schema was introduced;
future import subcommands feed the same `StcfCase` validation/export boundary.

## G22 `preproc_mixed_mesh_case`

Registered as `implemented, ci_gate:true` using the GoldenSuite four-touch
rule (manifest/checker/root CMake/per-test CMake).

CTest fixture setup invokes the built CLI twice to create independent
persistent `.stcf.nc` files. The Golden then verifies:

1. logical determinism: all topology and field arrays compare exactly across
   the two CLI outputs;
2. strict file load produces 5 nodes / 2 mixed faces / 6 ordered edges and
   the authored DPM/Manning/bed fields;
3. 20-step varying-phi_t, sloping-bed lake at rest preserves depth and
   momentum within 1e-12;
4. 10-step DischargeInflow audit: total storage gain and audit both equal
   `q * edge_length * dt * steps` within 1e-12.

## Verification

- G22 fixture setup + Golden: 2/2 CTest entries passed, 4/4 Golden assertions.
- CLI failure-path test: unknown profile, overwrite refusal, `--force`
  replacement, and no temporary-file residue.
- GoldenSuite manifest completeness: OK.
- windows-msvc Debug full repository suite: **133/133 passed**.
- Final build `LNK1168 == 0`.

## Scope

This is a reproducible authored profile generator and the first real tool
entry point. External GIS/UGRID import, projection handling, mesh quality
repair, and full production-city datasets remain subsequent PreProc slices.
