## Summary

- Extend the D-Flow FM spike host with CLI-controlled evidence output options.
- Add `--steps`, `--dt`, `--inventory-out`, and `--trace-out` so a real runtime spike can archive inventory and step traces directly to files.
- Emit a machine-readable trace summary with completed steps, requested steps, last time, expected last time, and maximum observed `dt` absolute error.
- Make the host exit non-zero when `update()` or `finalize()` fails so partial traces are not mistaken for successful evidence.
- Update the spike evidence docs to explain how the new outputs should be consumed.

## Validation

- `git diff --check -- spikes/dflowfm/host/dflowfm_spike_host.cpp spikes/dflowfm/evidence/var_inventory.md spikes/dflowfm/evidence/spike_report.md`
- Reviewed the generated inventory and trace formats for consistency with the evidence templates.

## Note

This PR improves spike/evidence tooling only. It does not claim a real D-Flow FM runtime run has been completed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
