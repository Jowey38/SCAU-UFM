## Summary

- Add a deliberately incomplete fake D-Flow FM BMI shared library for tests.
- Extend `test_coupling_dflowfm_engine` to verify `DFlowFMEngine` fails closed when a runtime library loads but is missing required BMI symbols.
- Keep this as a narrow loader-boundary hardening change with no GoldenSuite or gate-state changes.

## Validation

- `cmake --build H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc --config Debug --target test_coupling_dflowfm_engine`
- `ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "^test_coupling_dflowfm_engine$" --output-on-failure`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
