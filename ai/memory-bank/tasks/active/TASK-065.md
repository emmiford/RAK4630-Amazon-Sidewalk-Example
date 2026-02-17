# TASK-065: AC-priority software interlock — firmware + doc corrections

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: M (5 points)

## Description

The hardware circuit enforces EVSE/HVAC mutual exclusion at the relay level, but the
firmware has no corresponding software interlock. The PRD §2.0 incorrectly labels three
interlock requirements as `IMPLEMENTED (SW+HW)` when only the HW side exists. The TDD
§6.3 (Charge Control) documents command sources but never mentions thermostat state.
The PRD §2.4.1 boot sequence (read cool_call before enabling charge) is designed but
not coded.

This task adds the baseline AC-priority logic and corrects the documentation. It is a
prerequisite for TASK-048b (Charge Now 30-min latch), which *overrides* AC priority —
you can't override something that doesn't exist.

### Rename `charge_enable` → `charge_block` + polarity inversion

The signal is renamed from `charge_enable` (active HIGH = allow) to `charge_block`
(active HIGH = block). This makes the name match the physical behavior: when the MCU
drives the pin HIGH, charging is blocked; when LOW (or MCU power loss), the hardware
safety gate controls charging independently.

**Rationale**: The MCU GPIO feeds into a hardware safety gate that independently
enforces AC/EVSE mutual exclusion. When the MCU loses power, the GPIO floats LOW.
With `charge_block` LOW = not blocking, the hardware safety gate remains in control —
it allows charging when AC is off and blocks it when AC is on. The current
`charge_enable` HIGH = allow design makes MCU power loss force GPIO LOW = "don't
enable", rendering the hardware safety gate useless and bricking all charging.

Rename scope:
- **DT overlay** (`rak4631_nrf52840.overlay`): rename node `charge_enable` →
  `charge_block`, keep `GPIO_ACTIVE_HIGH` (HIGH = actively blocking)
- **platform_api_impl.c**: rename `charge_en_gpio` → `charge_block_gpio`, init as
  `GPIO_OUTPUT_INACTIVE` (LOW = not blocking, hardware safety gate decides)
- **charge_control.c**: rename `EVSE_PIN_CHARGE_EN` → `EVSE_PIN_CHARGE_BLOCK`,
  invert write logic: `gpio_set(pin, !charging_allowed)`
- **selftest.c**: rename pin define, update toggle-and-verify
- **selftest.h**: rename `charge_en_ok` → `charge_block_ok`
- **All unit tests**: update references
- **Docs**: PRD, TDD, project plan — replace all `charge_enable` / `charge_en`
  references with `charge_block`

### What the firmware must do

1. **Boot sequence** (`charge_control_init`):
   - Read `thermostat_cool_call_get()` before setting `charge_block` GPIO
   - If cool_call HIGH → set `charge_block` HIGH (block), log "Boot: AC active, EV blocked"
   - If cool_call LOW → leave `charge_block` LOW (not blocking, boot default), log "Boot: AC idle, EV allowed"
   - Platform must init `charge_block` GPIO as `GPIO_OUTPUT_INACTIVE` (LOW = not blocking,
     hardware safety gate handles mutual exclusion during boot)

2. **Runtime AC-priority** (`charge_control_tick`, every 500ms):
   - If cool_call transitions LOW→HIGH: pause charging, log transition
   - If cool_call transitions HIGH→LOW **and** no cloud pause active **and** no
     delay window active: resume charging, log transition
   - Track previous cool_call state to detect edges (don't re-pause every tick)

3. **Uplink reporting**:
   - `charge_allowed` state is already in uplinks (byte 6 bit 0)
   - Interlock transition events are a separate concern (PRD §2.0.6, NOT STARTED) —
     not in scope here, but the state must be correct

### What the docs must fix

4. **PRD §2.0 status table** (lines 124-126):
   - Change "Mutual exclusion" from `IMPLEMENTED (SW+HW)` → `IMPLEMENTED (HW) / NOT STARTED (SW)`
   - Change "AC priority" from `IMPLEMENTED (SW+HW)` → `IMPLEMENTED (HW) / NOT STARTED (SW)`
   - Change "EV lockout" from `IMPLEMENTED (SW+HW)` → `IMPLEMENTED (HW) / NOT STARTED (SW)`
   - After this task is done, update all three back to `IMPLEMENTED (SW+HW)`

5. **PRD §2.4.1 implementation table** (lines 394-398):
   - Update status of the three items from NOT STARTED → IMPLEMENTED as each is coded

6. **TDD §6.3 Charge Control**:
   - Add AC-priority as command source #2 (between Charge Now and Cloud downlink)
   - Document the cool_call check in `charge_control_tick`
   - Document the boot-time cool_call read in `charge_control_init`
   - Cross-reference §6.4 (Thermostat Inputs) for GPIO details

7. **Project plan** Feature 1.2.1:
   - Update the three `NOT STARTED` items as they are completed

## Dependencies

**Blocked by**: none
**Blocks**: TASK-048b (Charge Now 30-min latch — needs AC-priority as the baseline to override)

## Acceptance Criteria

- [ ] Renamed: `charge_enable` → `charge_block` across DT overlay, C code, tests, docs
- [ ] GPIO polarity: `GPIO_ACTIVE_HIGH` on `charge_block` — HIGH = blocking, LOW = not blocking
- [ ] MCU power loss → GPIO floats LOW → hardware safety gate controls EVSE (not bricked)
- [ ] `charge_control_init()` reads cool_call GPIO before setting `charge_block`
- [ ] Platform inits `charge_block` as `GPIO_OUTPUT_INACTIVE` (LOW = not blocking by default)
- [ ] Runtime: cool_call HIGH → charging pauses within one poll cycle (500ms)
- [ ] Runtime: cool_call LOW → charging resumes (if no cloud pause / delay window)
- [ ] Edge detection: repeated cool_call HIGH does not re-log or re-trigger pause
- [ ] Cloud pause + cool_call LOW: charging stays paused (cloud takes precedence)
- [ ] Cloud pause cleared + cool_call HIGH: charging stays paused (AC takes precedence)
- [ ] Power loss during AC call: boot reads cool_call → stays paused (safe default)
- [ ] Selftest toggle-and-verify updated for `charge_block` name and polarity
- [ ] PRD §2.0 status labels corrected (HW-only until SW ships, then SW+HW)
- [ ] PRD §2.4.1 implementation table updated
- [ ] TDD §6.3 documents AC-priority as a command source with cool_call logic
- [ ] Project plan Feature 1.2.1 items updated

## Testing Requirements

- [ ] C unit test: boot with cool_call HIGH → charge_allowed = false
- [ ] C unit test: boot with cool_call LOW → charge_allowed = true
- [ ] C unit test: runtime cool_call LOW→HIGH → charge_allowed transitions to false
- [ ] C unit test: runtime cool_call HIGH→LOW → charge_allowed transitions to true
- [ ] C unit test: cool_call LOW + cloud pause active → charge_allowed stays false
- [ ] C unit test: cool_call HIGH + cloud pause cleared → charge_allowed stays false
- [ ] C unit test: cool_call LOW + cloud pause cleared → charge_allowed transitions to true
- [ ] C unit test: repeated cool_call HIGH (no edge) → no state change, no redundant GPIO write
- [ ] All existing charge_control tests still pass (no regression)
- [ ] On-device verification: simulate AC call with `app hvac` shell commands, observe charge relay

## Deliverables

- Modified `rak4631_nrf52840.overlay` — rename `charge_enable` → `charge_block`, `GPIO_ACTIVE_HIGH`
- Modified `platform_api_impl.c` — rename `charge_en_gpio` → `charge_block_gpio`, init `GPIO_OUTPUT_INACTIVE`
- Modified `charge_control.c` — rename `EVSE_PIN_CHARGE_EN` → `EVSE_PIN_CHARGE_BLOCK`, invert write logic, boot-time cool_call read, runtime AC-priority logic
- Modified `charge_control.h` — add `cool_call_active` to state struct if needed
- Modified `selftest.c` — rename pin define + `charge_en_ok` → `charge_block_ok`, update toggle-and-verify
- Modified `selftest.h` — rename `charge_en_ok` → `charge_block_ok`
- New/modified unit tests in `tests/`
- Updated `docs/PRD.md` — §2.0 status corrections, §2.4.1 tracking, `charge_block` rename + polarity rationale
- Updated `docs/technical-design.md` — §6.3 AC-priority documentation, `charge_block` rename
- Updated `docs/project-plan.md` — Feature 1.2.1 status
