## Summary

- Enhance the D-Flow FM spike host so it emits a markdown BMI variable inventory table during a real runtime pass.
- Wire the spike evidence docs to that output so future G11 evidence collection can paste the host output directly into `spikes/dflowfm/evidence/var_inventory.md`.
- Keep the change scoped to spike/evidence tooling; no main-graph or GoldenSuite state changes.

## Validation

- Code/doc review only in this PR: the spike host change is exercised when a real external D-Flow FM runtime is available.
- Checked that the generated markdown table structure matches the inventory template fields (`index`, `name`, `type`, `rank`, `shape`, `units`, `read/write role`, `notes`).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
