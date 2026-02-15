# TASK-001: Merge feature/generic-platform to main

**Status**: not started
**Priority**: P2
**Owner**: Oliver
**Branch**: —
**Size**: S (2 points)

## Description
The generic platform refactor is complete (2 commits: `d5a84a5`, `e88d519`). All EVSE domain logic has been moved from platform to app layer. This needs to be merged to main so subsequent work builds on the new architecture.

## Dependencies
**Blocked by**: none
**Blocks**: all other tasks (new work should be based on the generic platform architecture)

## Acceptance Criteria
- [ ] Host-side unit tests pass (`make -C app/rak4631_evse_monitor/tests/ clean test`)
- [ ] Full firmware builds successfully (`west build -p -b rak4631 app/rak4631_evse_monitor/ -- -DOVERLAY_CONFIG="lora.conf"`)
- [ ] App-only build succeeds (CMake standalone build in `app_evse/`)
- [ ] Git log reviewed — no accidental files, clean commit messages
- [ ] Merged to main (fast-forward or merge commit per team convention)
- [ ] Feature branch deleted

## Testing Requirements
- [ ] Existing 32 unit tests pass
- [ ] Build verification (platform + app-only)

## Deliverables
- Clean `main` branch with generic platform architecture
