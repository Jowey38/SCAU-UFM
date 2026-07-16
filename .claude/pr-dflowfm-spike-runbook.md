## Summary

- Add a D-Flow FM spike runbook describing the standard evidence-capture workflow once a real DLL and authored `.mdu` case are available.
- Link the current spike evidence docs to that runbook so the inventory and trace outputs from the spike host are consumed consistently.
- Keep this as a documentation-only capture-procedure PR with no runtime or GoldenSuite state changes.

## Validation

- `git diff --check -- spikes/dflowfm/evidence/runbook.md spikes/dflowfm/evidence/spike_report.md spikes/dflowfm/evidence/var_inventory.md`
- Reviewed that the runbook command line matches the currently supported spike-host options.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
