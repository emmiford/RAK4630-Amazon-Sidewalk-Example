# ADR-002: Charge Now Cancels Demand Response Window

## Status

Accepted (2026-02-16)

## Context

The charge scheduler sends demand response commands (TOU peak pause, MOER
curtailment) to the device via LoRa downlinks. The "Charge Now" button lets the
user override AC priority and charge for 30 minutes.

The question is what happens to the active demand response window when Charge Now
is pressed, and what happens after the 30-minute AC override expires.

Three behaviors were considered for the post-30-minute period:

1. **Reinstate the pause** — the delay window resumes and charging is paused again
2. **Normal load-sharing** — the delay window is cancelled; EVSE and AC share the
   breaker with AC priority until peak ends
3. **Indefinite override** — Charge Now cancels demand response permanently

The user's intent when pressing Charge Now is "I need to charge my car now, and
I'm willing to accept the higher rate / grid impact." Reinstating the pause after
30 minutes frustrates this intent — the user gets 30 minutes of charging, then the
car stops again, and they'd have to press the button repeatedly through a 4-hour
peak window. That's a bad experience.

A secondary concern is the fire-and-forget nature of LoRa downlinks. The scheduler
sentinel (`DynamoDB timestamp=0`) records the last command sent but has no device
ACK. If the scheduler re-sends pause commands on a heartbeat to compensate for
lost downlinks, those re-sends would also stomp a Charge Now override — requiring
the cloud to know about the override regardless.

## Decision

**Charge Now cancels the active demand response window for the remainder of the
current peak period.** Specifically:

### Device side
- Charge Now activates the 30-minute AC override (EVSE charges, AC suppressed)
- The stored delay window is **deleted**, not paused
- `FLAG_CHARGE_NOW` (bit 3) is set in uplinks for the 30-minute duration
- After 30 minutes: AC priority restored, normal load-sharing, no delay window

### Cloud side
- `decode_evse_lambda` detects `FLAG_CHARGE_NOW=1` in an uplink and writes
  `charge_now_override_until` to the scheduler sentinel (`timestamp=0`), set
  to the end of the current peak window
- `charge_scheduler_lambda` checks this field before sending a pause — if
  `now < charge_now_override_until`, it suppresses the downlink
- After the peak window ends, the field expires and normal scheduling resumes

### Reliability
This also addresses the sentinel staleness bug: the scheduler can safely re-send
pause commands on a periodic heartbeat (to cover lost LoRa downlinks) because
the `charge_now_override_until` guard prevents those heartbeats from stomping
a user override.

## Consequences

**What becomes easier:**
- User experience is intuitive: press button, charge your car, done
- Scheduler heartbeat can be added without conflicting with local overrides
- No complex state synchronization between device and cloud — one uplink bit
  is sufficient

**What becomes harder:**
- `decode_evse_lambda` needs to know peak window boundaries (TOU schedule) to
  compute `charge_now_override_until`. This couples it to the scheduler's
  rate schedule data. Mitigation: read the TOU config from the device registry
  or use a conservative default (e.g., 4 hours from now)
- A user who presses Charge Now at 5:01 PM opts out of demand response for
  nearly 4 hours. This is by design — the user made a conscious choice — but
  it reduces demand response effectiveness

## Alternatives Considered

1. **Reinstate pause after 30 minutes (original PRD text):** The delay window
   resumes when the AC override expires. Rejected because it frustrates user
   intent and would require repeated button presses through a peak window.

2. **Scheduler heartbeat without Charge Now awareness:** Re-send pause every
   N minutes to cover lost downlinks. Rejected because it would stomp Charge
   Now overrides and auto-resume timers — any device-side state change would
   be overwritten.

3. **Decode Lambda re-sends charge command directly on mismatch:** Instead of
   updating the sentinel, the decode Lambda would detect charge_allowed mismatch
   and re-send the command. Rejected because it adds charge-control logic to the
   decode Lambda and could race with the scheduler.

4. **Indefinite override (no expiry):** Charge Now permanently disables demand
   response until manually re-enabled. Rejected because it effectively opts the
   user out of all future demand response with a single button press.

## References

- PRD section 2.0.1.1 — "Charge Now" Override: Duration Options
- PRD section 4.4.5 — Interaction with "Charge Now" Override
- TDD section 6.3 — Charge Control command sources
- TDD section 8.2 — charge_scheduler_lambda
- TDD section 8.4 — DynamoDB sentinel keys
