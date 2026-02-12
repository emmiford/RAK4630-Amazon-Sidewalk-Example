# ADR-001: API Version Mismatch Is a Hard Stop

## Status

Accepted (2026-02-11)

## Context

The platform and app communicate through versioned function pointer tables at fixed flash addresses (`platform_api` at 0x8FF00, `app_callbacks` at 0x90000). Each table carries a magic word and a version number.

Originally, `discover_app_image()` logged a **warning** on version mismatch but continued to load the app. This was convenient during development — you could iterate on the platform or app independently without bumping both sides in lockstep.

However, on bare metal with no MMU, a mismatched function pointer table means calling the wrong function at the wrong offset. The consequences range from hard faults (best case — obvious crash) to silent memory corruption (worst case — data loss, incorrect relay control, bricked device requiring physical reflash).

## Decision

Version mismatch is a **hard stop**. If `app_callbacks.version != APP_CALLBACK_VERSION`, the platform:

1. Logs an error (not warning) with expected vs actual version
2. Sets `app_cb = NULL` — no app callbacks will be invoked
3. Records the rejection reason for shell diagnostics (`sid status`)
4. Boots in platform-only mode (Sidewalk stack + OTA engine operational, no app logic)

The version number should **only** be bumped when the function pointer table layout changes (add, remove, or reorder pointers) — not on every build. This keeps the version stable across most development cycles.

## Consequences

**What becomes easier:**
- No risk of silent memory corruption from mismatched tables
- Failure mode is obvious and diagnosable (`sid status` shows "NOT LOADED (version mismatch)")
- OTA engine still works in degraded mode, so a corrected app can be pushed OTA

**What becomes harder:**
- Developers must flash matching platform + app versions together
- If you bump the version on one side, the other side must be updated before the device is functional
- During development, changing the table layout requires reflashing both images

**Known issue:** See KI-001 — version mismatch leaves device in platform-only mode until matching images are flashed.

## Alternatives Considered

1. **Keep the warning (status quo)**: Too dangerous on bare metal. Development convenience doesn't justify silent corruption risk.

2. **Forward-compatible versioning**: Add new pointers at the end only, check `app.version >= MINIMUM_SUPPORTED`. Rejected because even "compatible" assumptions are risky when the contract is raw function pointers — no type safety, no runtime dispatch.

3. **Magic-only validation (no version field)**: Magic word catches "no app" but not "wrong version of app." Insufficient.

## References

- `app/rak4631_evse_monitor/src/app.c` — `discover_app_image()`
- `app/rak4631_evse_monitor/include/platform_api.h` — version and magic constants
- `docs/architecture.md` — Versioning Rules section
