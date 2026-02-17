# Self-Test E2E Results — TASK-048

**Date**: ____
**Firmware**: main @ commit ____
**Tester**: ____

## Boot Self-Test

| Check | Result | Notes |
|-------|--------|-------|
| adc_pilot | | |
| adc_current | | |
| gpio_heat | | |
| gpio_cool | | |
| charge_en | | |
| Error LED pattern | | Should NOT flash on healthy device |

Serial log excerpt:
```
(paste boot self-test output here)
```

## Shell Self-Test (`sid selftest`)

```
(paste sid selftest output here)
```

Result: ____

## Fault Flag Uplink

### Trigger: Clamp mismatch (State C, no load)
- Simulated via: `app evse c`
- Wait time: ____ seconds
- Uplink byte 7 hex: `0x____`
- DynamoDB fields:
  - `clamp_mismatch`: ____
  - `sensor_fault`: ____
  - `interlock_fault`: ____
  - `selftest_fail`: ____

### Clear: Return to State A
- Cleared via: `app evse a`
- DynamoDB after clear:
  - `clamp_mismatch`: ____
  - All fault fields zero: ____

## Stale Flash Erase (TASK-022)

- Large app flashed: ____ bytes
- Small app flashed: ____ bytes
- Partition dump non-erased bytes: ____
- Match small app size: ____

## Summary

| Test | Pass/Fail |
|------|-----------|
| Boot self-test | |
| Shell self-test | |
| Fault flags in uplink | |
| Fault flags in DynamoDB | |
| Fault flags clear on resolve | |
| Stale flash erase | |

**Overall**: ____
