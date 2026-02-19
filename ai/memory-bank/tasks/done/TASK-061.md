# TASK-061: Event buffer — write on state change, not every poll cycle

**Status**: MERGED DONE
**Priority**: P2
**Owner**: Eliel
**Completed**: 2026-02-19
**Branch**: `task/061-event-buffer-change-detect`

## Summary
Added change-detection logic so event buffer entries are written only on meaningful state changes (J1772 transitions, voltage threshold crossings, charge control changes) instead of every 500ms poll cycle. Includes periodic heartbeat entry and voltage noise filtering. Significantly extends buffer lifetime and reduces uplink bandwidth.
