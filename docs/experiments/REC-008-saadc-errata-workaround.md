# REC-008: SAADC Errata Workaround Retention Decision

**Status**: Proposed
**Verdict**: —
**Type**: Hardware diagnostic
**Priority**: High (elevated from Low, 2026-02-22)
**Owner**: Oliver
**Related**: EXP-012, TASK-104

---

## Problem Statement

EXP-010 added `nrf_saadc_disable(NRF_SAADC)` at boot as a workaround for the nRF52840 SAADC errata. The workaround did not fix EXP-010's issue (root cause was mechanical), but EXP-012 Phase 1 confirmed it DOES recover the SAADC mux latch. The question is whether to keep it permanently.

## Hypothesis

Keeping the SAADC disable-at-boot workaround as a defensive measure will prevent a potential future SAADC lockup, at negligible code/runtime cost.

**Success Metrics**:
- Primary: ADC accuracy after 1000 enable/disable cycles with and without the workaround

## Method

**Step 1**: Review the specific nRF52840 SAADC errata (anomaly 86, 87, or related) and determine if disable-at-boot is the recommended mitigation
**Step 2**: If applicable, keep the workaround; if not, remove to avoid cargo-cult code
**Step 3**: Stress test: rapid ADC enable/disable cycles + sleep/wake transitions, checking for stuck-at-ground readings
**Duration**: 1 hour

### Context from EXP-012

EXP-012 Phase 1 confirmed that the SAADC workaround **does** recover a grounded AIN1 pin caused by a board reset during an active ADC sample. This makes the workaround a **permanent production requirement** — without it, any board reset during an active ADC sample could permanently ground an analog pin until the next firmware flash.

## References

- Files: `app/rak4631_evse_monitor/src/app.c` (`platform_adc_init`)
- Tasks: TASK-104
