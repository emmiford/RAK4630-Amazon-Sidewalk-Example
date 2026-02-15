# TASK-051: Move EVSE payload struct to app layer

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
The `evse_payload_t` struct in `include/rak_sidewalk.h` defines EVSE-specific payload formats in the platform include directory. The platform sends raw bytes via `send_msg()` and has no business knowing EVSE payload structure.

Move the payload struct definition out of `rak_sidewalk.h` and into the app layer. If nothing outside `src/app_evse/` references the struct, it can live in a local header or be inlined into `rak_sidewalk.c`. If `app_tx.c` also uses it, create `include/evse_payload.h`.

Check what remains in `rak_sidewalk.h` after the struct moves — if it's empty or trivial, mark it for removal in TASK-052.

Reference: `docs/technical-design-rak-firmware.md`, Change 2.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-052

## Acceptance Criteria
- [ ] `evse_payload_t` no longer defined in a platform-visible header
- [ ] Platform build succeeds without EVSE payload knowledge
- [ ] App build succeeds
- [ ] Host tests pass

## Testing Requirements
- [ ] Platform build succeeds
- [ ] App build succeeds
- [ ] 57 host-side tests pass

## Deliverables
- Payload struct moved to app-layer header or source
- `include/rak_sidewalk.h` cleaned up or marked for removal
