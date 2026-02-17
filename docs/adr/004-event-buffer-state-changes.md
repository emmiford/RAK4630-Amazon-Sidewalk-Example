# ADR-004: Event Buffer Records State Changes, Not Poll Cycles

## Status

Accepted (2026-02-16)

## Context

The EVSE monitor polls sensors every 500ms. We need a RAM-resident event log to retain recent device history for diagnostics and for the ACK watermark trimming protocol.

The original implementation wrote a snapshot on every 500ms poll cycle. At that rate, 50 entries covered only **25 seconds** of history — far too short to survive any meaningful connectivity gap. The TDD incorrectly claimed 12.5 hours of coverage by conflating the 15-minute heartbeat interval with the buffer write interval.

The app layer runs in a fixed **8KB RAM budget**. The buffer must balance coverage duration against RAM cost.

## Decision

The event buffer records **state changes only**, not every poll cycle. A snapshot is written when any of these change:

- J1772 pilot state (vehicle plug/unplug, charging start/stop)
- Charge control state (pause/allow command received)
- Thermostat flags (heat/cool call transitions)
- Current flow (on/off transitions)

The buffer capacity remains **50 entries** at 12 bytes each = **600 bytes** (7.3% of 8KB budget). Pilot voltage is retained alongside the J1772 state enum — the raw voltage is valuable for field debugging (e.g., readings near a threshold boundary indicate a marginal connection).

On a typical day, an EVSE charger sees a handful of state transitions (vehicle arrives, charging begins, TOU pause, resume, vehicle departs). At ~5-10 events per day, 50 entries comfortably covers **multiple days** of history.

The buffer is a ring buffer: when full, new writes overwrite the oldest entry. In pathological cases (rapid state bouncing from a wiring fault or software bug), the buffer could fill quickly and lose older events. This is acceptable — the most recent state transitions are more diagnostically valuable than older ones, and rapid bouncing is itself the signal.

## Consequences

**What becomes easier:**
- Multi-day coverage under normal operation (vs. 25 seconds before)
- Each entry is a meaningful event, not redundant steady-state noise
- ACK watermark trimming becomes useful — events persist long enough for the cloud to ACK them
- Shell command `app evse buffer` shows an actual event log, not a 25-second sample

**What becomes harder:**
- Coverage duration is unpredictable — depends on how often state changes (could be days or minutes)
- Rapid state bouncing (fault condition) fills the buffer quickly, losing older context
- Requires tracking "previous state" to detect changes (already done in the poll loop via `changed` flag)

## Alternatives Considered

1. **Keep 500ms writes (status quo)**: 50 entries = 25 seconds. Useless for connectivity gaps. ACK watermark trimming is dead code since entries are overwritten before any ACK arrives.

2. **Write once per heartbeat (every 15 minutes)**: 50 entries = 12.5 hours. Predictable coverage, but misses state changes between heartbeats — the most interesting events happen mid-interval.

3. **Larger buffer for 500ms writes**: Would need ~172,800 entries for 24 hours at 500ms. At 12 bytes each, that's 2MB — impossible in 8KB.

4. **Change-based + periodic heartbeat entries**: Write on state change AND every Nth heartbeat as a "still alive" marker. Adds complexity for marginal value — the regular uplinks already serve as heartbeats.

## References

- `app/rak4631_evse_monitor/include/event_buffer.h` — capacity define and snapshot struct
- `app/rak4631_evse_monitor/src/app_evse/event_buffer.c` — ring buffer implementation
- `app/rak4631_evse_monitor/src/app_evse/app_entry.c` — poll loop with `changed` flag
- `docs/technical-design.md` §6.6 — Event Buffer design
- `docs/PRD.md` §3.2.2 — Device-Side Event Buffer
