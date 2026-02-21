# ADR-005: Simulation Mode Stays in evse_sensors.c

## Status

Accepted (2026-02-21)

## Context

The EVSE sensor module (`evse_sensors.c`) contains a simulation mode that overrides the real ADC pilot voltage reading with a synthetic value for a chosen J1772 state (A/B/C/D/E). It is triggered from shell commands (`app evse a`, `app evse b`, `app evse c`) and runs for 10 seconds before reverting to real sensor readings. It exists so developers can exercise the full data pipeline — sensor read → event buffer → Sidewalk uplink → Lambda decode → DynamoDB — without EVSE hardware attached.

The simulation is ~30 lines: three static variables, one if-block in `evse_j1772_state_get()`, and two public functions (`evse_sensors_simulate_state()`, `evse_sensors_is_simulating()`).

The question was whether to extract this into a separate module (e.g., `evse_sim.c`).

## Decision

Keep simulation mode inline in `evse_sensors.c`. Do not extract it into a separate module.

The simulation is tightly coupled to the sensor read path — it intercepts `evse_j1772_state_get()` by returning early with fake values. Extracting it would require either:

1. Exposing sensor internals so a sim module can inject values, or
2. Adding a function pointer indirection ("sensor backend" abstraction) to swap real vs. fake reads

Both add complexity disproportionate to the ~30 lines of simulation code.

## Consequences

**What becomes easier:**
- No extra files, headers, or build plumbing for a trivial feature
- Simulation logic is co-located with the thresholds and state machine it's faking, making it easy to keep consistent

**What becomes harder:**
- Cannot `#ifdef` out simulation without touching the sensor module (acceptable — simulation has no RAM cost when inactive, just three static variables)
- If simulation grows to cover more targets (current clamp, HVAC, Sidewalk connectivity), the decision should be revisited

**Known gap:** Simulated uplinks are indistinguishable from real ones in the wire format and DynamoDB. There is no simulation flag in the v0x0A payload. The only heuristic clues are the exact synthetic voltages (2980, 2234, 1489, 745 mV) which lack ADC noise. If simulation visibility becomes important, a flag bit could be added to the payload flags byte.

## Alternatives Considered

1. **Separate `evse_sim.c` module**: Clean separation of concerns, easier to compile out. But requires exposing sensor internals or adding indirection for a 30-line feature. Not worth it at current scope.

2. **Function pointer backend (real vs. sim)**: Maximum flexibility, testable. But over-engineered for a single if-block. Would revisit if multiple simulation targets are needed.

3. **Compile-time `#ifdef CONFIG_SIMULATION`**: Could gate the simulation code. Not needed — the three static variables cost zero when inactive, and the feature is useful in production builds for field debugging.

## Revisit If

- Simulation expands to cover current clamp, HVAC inputs, or Sidewalk connectivity
- A cloud-triggered simulation mode is added (would need a cleaner API boundary)
- Production builds need to exclude simulation for certification or code size

## References

- `app/rak4631_evse_monitor/src/app_evse/evse_sensors.c` — simulation variables and intercept logic
- `app/rak4631_evse_monitor/include/evse_sensors.h` — public simulation API
- `app/rak4631_evse_monitor/src/app_evse/app_entry.c:316-330` — shell commands that trigger simulation
