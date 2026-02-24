# EXP-007: Split-Image Architecture (Platform + App)

**Status**: Concluded
**Verdict**: GO
**Type**: Architecture change
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-008 (evolution), ADR-001

---

## Problem Statement

Full firmware builds take ~45 seconds and require the entire nRF Connect SDK / Zephyr toolchain. Iterating on application logic is slow.

## Hypothesis

Splitting firmware into a stable platform image and a small independently flashable app image will dramatically reduce build/flash iteration time and enable app-only OTA.

**Success Metrics**:
- Primary: build time reduction, successful API boundary operation

## Method

**Variants**:
- Control: Monolithic firmware (~45s build, full flash)
- Variant: Split image — platform (512KB @ 0x0) + app (~4KB @ 0x80000, later moved to 0x90000)

**Implementation** (commit `d6faff4`):
- Platform exposes function pointer table (`struct platform_api`) at fixed flash address
- App provides callback table (`struct app_callbacks`) with magic word + version
- Platform discovers app at boot, validates magic/version, calls `app->init()`

## Results

**Decision**: GO — merged, then evolved further in EXP-008
**Primary Metric Impact**: App-only rebuild+flash ~2 seconds vs ~45s for full platform. ~22x improvement.
**Verification**: End-to-end verified — sensor read, Sidewalk TX, ACK callback, downlink RX all working across API boundary.
**Evolution**: Initial split had EVSE domain knowledge in platform. EXP-008 completed the separation.

## Key Insights

- Fixed flash addresses for API tables are simple and robust. No dynamic linking or symbol resolution needed.
- Magic word + version validation catches mismatched platform/app pairs at boot.
- The API boundary design (function pointers + callbacks) enables independent evolution of platform and app.
- This experiment was the foundation for everything that followed — OTA, delta OTA, and generic platform.
