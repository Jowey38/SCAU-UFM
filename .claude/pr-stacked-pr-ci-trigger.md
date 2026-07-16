## Summary

- Add `feat/m230-stage-record` to the CI `pull_request` branch filter.
- This lets stacked feature PRs targeting the active stage branch run CI automatically instead of requiring `workflow_dispatch`.

## Validation

- `gh workflow view CI --repo Jowey38/SCAU-UFM --yaml >/dev/null`
- Confirmed diff only changes the `pull_request.branches` list.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
