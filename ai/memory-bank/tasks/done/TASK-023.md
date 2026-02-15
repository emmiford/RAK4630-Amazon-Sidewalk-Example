# TASK-023: BUG — PSA crypto AEAD error -149 after platform re-flash

**Status**: MERGED DONE (2026-02-11, Claude + Eero)
**Branch**: `main`

## Summary
Root cause confirmed: HUK regenerated on platform flash, MFG keys still present, so `mfg_key_health_check()` passes but PSA derives wrong keys. `flash.sh platform` now warns about HUK invalidation and requires confirmation. KI-002 updated. Remaining gap: no runtime HUK mismatch detection at boot (deferred — needs test decryption in PSA).

## Deliverables
- Updated `flash.sh` (platform subcommand warning)
- Updated `docs/known-issues.md` (KI-002 resolution)
