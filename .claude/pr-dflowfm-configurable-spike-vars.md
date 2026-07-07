## Summary

- Make the D-Flow FM spike host accept CLI overrides for the boundary discharge and stage variable names.
- Keep the existing defaults (`boundary_discharge`, `stage_at_section`) so current usage does not change.
- Update the spike runbook so a real runtime pass can switch to the actual BMI variable names without recompiling the host.

## Validation

- `git diff --check -- spikes/dflowfm/host/dflowfm_spike_host.cpp spikes/dflowfm/evidence/runbook.md`
- Reviewed that the new `--boundary-var` and `--stage-var` flags match the updated runbook command.

## Note

This PR improves spike capture flexibility only. It does not claim that a real D-Flow FM runtime run has been completed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
