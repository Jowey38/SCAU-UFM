# M247 Runoff GoldenSuite Gate Evidence

## Scope

This evidence records the post-M247-F gate promotion for the urban runoff golden case.

M247-F is already merged into `origin/master` at merge commit `694a430`. This gate promotion adds a formal CTest/manifest gate for the M247-E/F urban-block runoff golden without changing the runoff implementation.

## Files

- `tests/CMakeLists.txt`
- `tests/golden/CMakeLists.txt`
- `tests/golden/runoff_urban_block/CMakeLists.txt`
- `tests/golden/suite_manifest/goldensuite.json`
- `tests/golden/suite_manifest/check_manifest.py`

## Gate semantics

The gate registers the existing `tests/unit/surface2d/test_runoff_golden_urban_block.cpp` golden as a dedicated GoldenSuite test executable:

- CTest name: `test_golden_runoff_urban_block`
- labels: `golden;runoff;m247`
- manifest id: `M247-EF`
- manifest status: `implemented`
- manifest `ci_gate`: `true`

The manifest check is also registered as CTest:

- CTest name: `test_goldensuite_manifest`
- labels: `golden;manifest`

This creates a runnable gate via:

```bash
ctest --preset windows-msvc -L golden --output-on-failure
```

## Verification

Executed in clean worktree `H:/githubcode/SCAU-UFM/.worktrees/m247-postmerge-gate` on branch `feat/m247-runoff-golden-gate`, based on `origin/master` `694a430`.

### M247-F post-merge runoff checks

```text
ctest --preset windows-msvc -R "test_runoff_(ground|step|golden_urban_block)$" --output-on-failure
```

Result:

```text
3/3 passed, 0 failed
```

### GoldenSuite label gate

```text
ctest --preset windows-msvc -L golden --output-on-failure
```

Result:

```text
2/2 passed, 0 failed
```

The two tests were:

- `test_goldensuite_manifest`
- `test_golden_runoff_urban_block`

### Full suite

```text
ctest --preset windows-msvc --output-on-failure
```

Result:

```text
55/55 passed, 0 failed
```

## Completion assessment

The M247-E/F urban runoff golden is now represented as an explicit GoldenSuite gate on this branch. The gate is manifest-backed, label-selectable, and passes together with the full suite.
