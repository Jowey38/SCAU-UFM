## Summary

- Add the first author-authored `single_reach.mdu` skeleton under `spikes/dflowfm/cases/`.
- Add `README_single_reach.md` documenting the boundary: this is a runtime-entry skeleton, not a validated hydraulic case or G11 completion claim.
- Update the D-Flow FM case index and spike runbook so the first sanity pass targets `single_reach.mdu` and archives `single_reach.trace.txt`.

## Test Plan

- [x] `git diff --check -- spikes/dflowfm/cases/single_reach.mdu spikes/dflowfm/cases/README_single_reach.md spikes/dflowfm/cases/README.md spikes/dflowfm/evidence/runbook.md`
- [ ] Run the D-Flow FM spike host against a real DLL and authored companion files when the external runtime is available

## Note

This PR does not claim that the D-Flow FM runtime is currently available or that the case is hydraulically validated. It only adds the repository-owned skeleton entry point for the first real G11 sanity pass.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
