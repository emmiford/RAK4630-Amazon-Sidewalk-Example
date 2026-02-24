# REC-006: Charge Control Auto-Resume Timer Validation

**Status**: Proposed
**Verdict**: —
**Type**: Field measurement
**Priority**: Medium
**Owner**: Oliver

---

## Problem Statement

The charge control module has an auto-resume timer that re-enables charging after a scheduler-initiated pause. The timer duration and interaction with the cloud scheduler's periodic evaluation need validation to avoid conflicting commands.

## Hypothesis

The auto-resume timer and scheduler evaluation frequency must be coordinated to avoid the device resuming charging right before the scheduler re-pauses it (unnecessary relay cycling) or the scheduler commanding a pause after the device has already resumed.

**Success Metrics**:
- Primary: relay cycle count per day (each pause/resume = 1 cycle)
- Secondary: time-in-wrong-state (device charging when scheduler says pause, or vice versa)

## Method

**Step 1**: Map the timing: scheduler rate (EventBridge, configurable), auto-resume delay (device-side), Sidewalk downlink latency
**Step 2**: Identify conflict windows where auto-resume and scheduler pause can race
**Step 3**: If conflicts exist, test with instrumented logging
**Prerequisites**: Shell logging of charge control state transitions, CloudWatch logs from scheduler Lambda
