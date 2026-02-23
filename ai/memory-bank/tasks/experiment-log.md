# RAK Sidewalk EVSE Monitor — Experiment Log

**Experiment Tracker**: Oliver
**Project**: RAK Sidewalk EVSE Monitor
**Analysis Date**: 2026-02-11
**Methodology**: Retrospective analysis from git history, commit messages, code artifacts, and branch lifecycle

---

## Part 1: Concluded Experiments

---

### EXP-001: Windowed Blast OTA Transfer Mode

## Hypothesis
**Problem Statement**: Per-chunk ACK mode for OTA over LoRa is slow — each 15B chunk requires a round-trip ACK before the next chunk can be sent, making full-image OTA take hours.
**Hypothesis**: Sending chunks on a timer within fixed-size windows (without waiting for per-chunk ACKs) will significantly reduce OTA transfer time by eliminating ACK-wait from the critical path.
**Success Metrics**: Reduction in total OTA transfer time for full-image updates.

## Experimental Design
**Type**: Feature implementation with before/after comparison
**Variants**:
- Control: Legacy per-chunk ACK mode (send chunk → wait ACK → send next)
- Variant: Windowed blast mode (send N chunks on timer → device reports gaps → gap-fill)
**Implementation**: Commit `e3f97e0` — device tracks received chunks via bitfield, reports gaps at window boundaries. Backward-compatible via `window_size=0` in OTA_START.

## Results
**Decision**: **REVERTED** (commit `78924b6`)
**Rationale**: Delta OTA mode (EXP-004) made blast mode obsolete before it could be fully validated in production. Delta sends 2-3 chunks for typical changes vs 277+ for full image. The complexity of windowed blast (gap tracking, window ACKs, timer scheduling) was no longer justified.
**Outcome**: Code removed. Delta mode + legacy per-chunk ACK retained.

## Key Insights
- The right abstraction level matters more than optimizing the wrong approach. Sending fewer chunks (delta) beats sending the same chunks faster (blast).
- Blast mode added significant protocol complexity for a problem that was better solved at a higher level.
- Backward-compatible protocol design (`window_size=0` fallback) was good practice — made revert clean.

## Status: CONCLUDED — Reverted, superseded by EXP-004

---

### EXP-002: OTA Chunk Size Optimization (12B → 15B)

## Hypothesis
**Problem Statement**: OTA chunks were using 12B data + 4B header = 16B, leaving 3 bytes of the 19B Sidewalk LoRa MTU unused.
**Hypothesis**: Increasing chunk data from 12B to 15B (using full 19B MTU) will reduce the number of chunks required and proportionally reduce OTA transfer time.
**Success Metrics**: Measurable reduction in chunk count for same firmware size.

## Experimental Design
**Type**: Parameter optimization
**Variants**:
- Control: 12B data chunks (325 chunks for ~3.9KB app)
- Variant: 15B data chunks (260 chunks for ~3.9KB app)

## Results
**Decision**: **GO** — Merged to main (commit `b8e62cd`)
**Primary Metric Impact**: 20% reduction in chunk count (260 vs 325)
**Verification**: Device-side code already handled variable chunk sizes via OTA_START parameters — zero device code changes needed.

## Key Insights
- Always use your full MTU. The 3 unused bytes were pure waste.
- The device-side protocol was well-designed — chunk size parameterized in OTA_START, not hardcoded.
- 20% fewer chunks means 20% fewer ACK round-trips, compounding the benefit.

## Status: CONCLUDED — Shipped, production

---

### EXP-003: OTA Retry Stale Threshold Reduction (300s → 30s)

## Hypothesis
**Problem Statement**: Lost LoRa ACKs caused 5-minute dead waits before the EventBridge retry timer would resend. This was the dominant source of OTA transfer latency.
**Hypothesis**: Reducing the stale session threshold from 300s to 30s will allow faster recovery from lost ACKs, significantly reducing total OTA transfer time.
**Success Metrics**: Reduction in worst-case stall duration per lost ACK.

## Experimental Design
**Type**: Parameter tuning
**Variants**:
- Control: 300s stale threshold (5-min dead wait per lost ACK)
- Variant: 30s stale threshold (caught on next timer tick, ~60s worst case)

## Results
**Decision**: **GO** — Merged to main (commit `3db368b`)
**Primary Metric Impact**: Worst-case stall reduced from ~300s to ~60s per lost ACK. Commit message states "roughly halving total OTA transfer time."
**Risk**: More aggressive retries could cause duplicate chunks, but device-side deduplication (bitfield tracking) handles this safely.

## Key Insights
- LoRa packet loss is a first-class concern, not an edge case. The system needs to be tuned for lossy links.
- 300s was overly conservative — likely a default that was never calibrated against real-world LoRa conditions.
- The EventBridge 1-minute timer granularity sets the floor. 30s threshold + 60s timer = ~60s worst case, which is a good balance.

## Status: CONCLUDED — Shipped, production

---

### EXP-004: Delta OTA Mode

## Hypothesis
**Problem Statement**: Full-image OTA over LoRa sends the entire firmware binary (~3.9KB, 260 chunks at 15B each) even when only a few bytes changed. At LoRa data rates with ACK round-trips, this takes hours.
**Hypothesis**: Comparing new firmware against a stored baseline and sending only changed chunks will reduce OTA transfer time from hours to seconds for typical app changes.
**Success Metrics**: Orders-of-magnitude reduction in chunk count and transfer time for incremental updates.

## Experimental Design
**Type**: New feature with before/after comparison
**Variants**:
- Control: Full-image OTA (260+ chunks, hours)
- Variant: Delta OTA (only changed chunks, seconds)
**Implementation** (commit `65e7389`):
- Cloud: Lambda computes chunk diff against S3 baseline, sends only changed chunks with absolute indexing, auto-saves successful firmware as new baseline
- Device: Delta receive mode with bitfield tracking, merged CRC validation (staging + primary), page-by-page apply with baseline overlay

## Results
**Decision**: **GO** — Merged to main (via `eed3aeb`)
**Primary Metric Impact**: 2-3 chunks (~seconds) vs 277+ chunks (~hours) for typical app changes. ~100x improvement.
**Secondary Impact**: Made windowed blast mode (EXP-001) obsolete — reverted.
**Verification**: Deploy CLI (`ota_deploy.py preview`) shows delta before sending.

## Key Insights
- This was the single most impactful experiment in the project. Changed OTA from "overnight operation" to "deploy while you watch."
- Baseline management is critical — S3 stores the last successful firmware as reference. If baseline drifts, delta fails gracefully (falls back to full).
- The `ota_deploy.py` tooling around delta (preview, baseline, deploy) made the feature practical, not just possible.

## Status: CONCLUDED — Shipped, production. Project's most significant optimization.

---

### EXP-005: On-Change Sensing vs Fixed-Interval Polling

## Hypothesis
**Problem Statement**: Fixed-interval sensor polling wastes LoRa airtime transmitting unchanged readings. Sidewalk LoRa is bandwidth-constrained (19B MTU, shared spectrum).
**Hypothesis**: Event-driven uplinks (transmit only on state change + periodic heartbeat) will dramatically reduce unnecessary transmissions while maintaining data freshness.
**Success Metrics**: Reduction in uplink count without missing state transitions.

## Experimental Design
**Type**: Architecture change with measurable traffic reduction
**Variants**:
- Control: Fixed-interval polling (transmit every N seconds regardless of change)
- Variant: Change detection (500ms ADC poll with state comparison, TX only on change, 60s heartbeat)
**Implementation** (commit `deb4007`):
- Thermostat GPIOs: hardware edge interrupts
- J1772 pilot + current clamp: 500ms ADC polling with state comparison
- 5s TX rate limiter to avoid flooding on rapid state changes
- 60s heartbeat for liveness

## Results
**Decision**: **GO** — Merged to main (via `170fbda`)
**Primary Metric Impact**: TX count drops to ~1/minute (heartbeat) during steady state, vs every-poll-cycle before. Bursts only on actual state transitions.
**Verification**: `sid test_change` shell command for on-device testing.

## Key Insights
- For a battery-constrained LoRa device, "don't transmit unless something changed" is the single most important power optimization.
- The 5s rate limiter was important — rapid J1772 pilot voltage transitions during plug-in could generate a burst of state changes.
- 60s heartbeat is a good liveness signal without being wasteful. Worth validating this interval (see Recommended Experiments).

## Status: CONCLUDED — Shipped, production

---

### EXP-006: Raw 8-Byte Payload vs sid_demo Protocol

## Hypothesis
**Problem Statement**: The inherited sid_demo protocol used a complex 3-state state machine with capability discovery, adding ~290 lines of code and protocol overhead that blocked reliable sensor data flow.
**Hypothesis**: Replacing the sid_demo protocol with a direct 8-byte raw payload format will improve reliability and reduce code complexity without losing functionality.
**Success Metrics**: Code reduction, elimination of protocol-related data flow blockages.

## Experimental Design
**Type**: Simplification experiment
**Variants**:
- Control: sid_demo protocol with SMF state machine, capability discovery, LED/button response handlers (~420 lines)
- Variant: Raw 8-byte format — magic(1) + version(1) + J1772(1) + voltage(2) + current(2) + thermo(1) (~130 lines)

## Results
**Decision**: **GO** — Merged to main (commit `550560f`)
**Primary Metric Impact**: 69% code reduction (420 → 130 lines). Eliminated protocol-related TX blockages.
**Payload Format**: `[0xE5, 0x01, state, volt_lo, volt_hi, curr_lo, curr_hi, thermo_flags]`
**Trade-off**: Lost backward compatibility with sid_demo decoders. Lambda updated to handle new format.

## Key Insights
- The sid_demo protocol was designed for a general-purpose demo, not for a single-purpose sensor. Stripping it was the right call.
- Magic byte (0xE5) + version byte (0x01) gives forward compatibility for payload evolution without protocol overhead.
- 8 bytes fits easily in the 19B MTU with room for future fields.

## Status: CONCLUDED — Shipped, production

---

### EXP-007: Split-Image Architecture (Platform + App)

## Hypothesis
**Problem Statement**: Full firmware builds take ~45 seconds and require the entire nRF Connect SDK / Zephyr toolchain. Iterating on application logic is slow.
**Hypothesis**: Splitting firmware into a stable platform image and a small independently flashable app image will dramatically reduce build/flash iteration time and enable app-only OTA.
**Success Metrics**: Build time reduction, successful API boundary operation.

## Experimental Design
**Type**: Architecture experiment
**Variants**:
- Control: Monolithic firmware (~45s build, full flash)
- Variant: Split image — platform (512KB @ 0x0) + app (~4KB @ 0x80000, later moved to 0x90000)
**Implementation** (commit `d6faff4`):
- Platform exposes function pointer table (`struct platform_api`) at fixed flash address
- App provides callback table (`struct app_callbacks`) with magic word + version
- Platform discovers app at boot, validates magic/version, calls `app->init()`

## Results
**Decision**: **GO** — Merged, then evolved further in EXP-008
**Primary Metric Impact**: App-only rebuild+flash ~2 seconds vs ~45s for full platform. ~22x improvement.
**Verification**: End-to-end verified — sensor read, Sidewalk TX, ACK callback, downlink RX all working across API boundary.
**Evolution**: Initial split had EVSE domain knowledge in platform. EXP-008 completed the separation.

## Key Insights
- Fixed flash addresses for API tables are simple and robust. No dynamic linking or symbol resolution needed.
- Magic word + version validation catches mismatched platform/app pairs at boot.
- The API boundary design (function pointers + callbacks) enables independent evolution of platform and app.
- This experiment was the foundation for everything that followed — OTA, delta OTA, and generic platform.

## Status: CONCLUDED — Shipped, evolved into EXP-008

---

### EXP-008: Generic Platform (Move All Domain Knowledge to App)

## Hypothesis
**Problem Statement**: The platform still contained EVSE-specific knowledge (sensor_monitor.c, PIN definitions, evse/hvac shell commands). This meant platform updates were needed for domain logic changes, defeating the purpose of app-only OTA.
**Hypothesis**: Moving ALL domain-specific code (sensor interpretation, pin assignments, change detection, polling, shell commands) to the app partition will make the platform truly generic and the app fully self-contained and OTA-updatable.
**Success Metrics**: Platform has zero EVSE knowledge. App is independently buildable and testable. All existing functionality preserved.

## Experimental Design
**Type**: Architecture completion experiment
**Implementation** (commit `e88d519`):
- Delete `sensor_monitor.c` from platform
- Add `set_timer_interval()` to platform API (app configures its own 500ms poll)
- Move PIN defines from platform to app modules
- Replace evse/hvac shell commands with generic "app" shell dispatch
- App does own change detection + 60s heartbeat in `on_timer()`
- Add host-side unit test harness (Grenning dual-target pattern, 32 tests)

## Results
**Decision**: **GO** — Completed on `feature/generic-platform`, merged to main
**Primary Metric Impact**: Platform is now fully domain-agnostic. App contains all EVSE logic.
**Test Coverage**: 32 host-side unit tests covering sensors, thermostat, charge control, TX, and on_timer change detection.
**API Contract**: Platform API v2 (magic 0x504C4154), App Callbacks v3 (magic 0x53415050)

## Key Insights
- The Grenning dual-target testing pattern was the key enabler — without host-side tests, this refactor would have been much riskier.
- `set_timer_interval()` was the crucial missing API — app needs to control its own polling rate without platform knowledge.
- Generic shell dispatch (`on_shell_cmd` callback) lets the app register arbitrary commands without platform changes.
- This completes the split-image vision from EXP-007: platform is now a reusable Sidewalk sensor runtime.

## Status: CONCLUDED — Shipped, branch merged

---

## Part 2: Recommended Experiments

---

### REC-001: Heartbeat Interval Optimization (Currently 60s)

## Hypothesis
**Problem Statement**: The 60s heartbeat interval was chosen without empirical validation. It may be too frequent (wasting battery/airtime) or too infrequent (delayed anomaly detection on the cloud side).
**Hypothesis**: There exists an optimal heartbeat interval that balances liveness detection latency against battery/airtime cost.

## Experimental Design
**Type**: Parameter sweep
**Variants**: 30s, 60s (current), 120s, 300s
**Metrics**:
- Primary: Cloud-side anomaly detection latency (time from device offline to alert)
- Secondary: Battery impact (mAh consumed by heartbeat TX per day)
- Guardrail: No missed state transitions (on-change detection is independent)
**Duration**: 1 week per variant (stable state, no OTA or config changes)
**Prerequisites**: Battery current measurement setup, cloud-side alerting to detect offline devices

## Priority: Medium
**Rationale**: Directly impacts battery life and operational visibility. Low implementation effort (single `#define` change, OTA-updatable).

---

### REC-002: ADC Polling Interval Optimization (Currently 500ms)

## Hypothesis
**Problem Statement**: The 500ms ADC polling interval for J1772 pilot and current clamp was chosen as a reasonable default but hasn't been validated against actual J1772 state transition speeds or battery impact.
**Hypothesis**: J1772 pilot state transitions happen on the order of seconds (relay switching). A longer polling interval (1s, 2s) may catch all transitions while reducing CPU wake-ups and battery drain.

## Experimental Design
**Type**: Parameter sweep with edge-case validation
**Variants**: 250ms, 500ms (current), 1000ms, 2000ms
**Metrics**:
- Primary: Missed state transitions (compare against reference J1772 pilot logger)
- Secondary: CPU active time / battery impact
- Guardrail: Never miss a State A→C transition (vehicle plugged in and ready)
**Duration**: 1 week per variant with daily plug/unplug cycles
**Prerequisites**: Reference J1772 pilot logger for ground truth, current measurement setup

## Priority: Medium
**Rationale**: 500ms polling means 2 wake-ups/sec. If 1s is sufficient, that's 50% fewer wake-ups. Significant for battery-powered deployment.

---

### REC-003: TX Rate Limiter Tuning (Currently 100ms)

## Hypothesis
**Problem Statement**: The TX rate limiter prevents sending more than one uplink per 100ms. This was set conservatively. On LoRa, actual TX + airtime may be longer, meaning the limiter may never trigger. Or it may be too aggressive during rapid state transitions.
**Hypothesis**: The rate limiter threshold can be calibrated to the actual Sidewalk LoRa TX cadence to avoid both wasted attempts and delayed state reports.

## Experimental Design
**Type**: Observational measurement + parameter tuning
**Step 1**: Instrument actual TX timing (uplink queued → Sidewalk send callback → ACK) to establish baseline
**Step 2**: Set limiter to 1.5x actual TX cadence
**Metrics**:
- Primary: Dropped/queued uplinks per day
- Secondary: State transition report latency
**Prerequisites**: Shell logging of TX timestamps at each stage

## Priority: Low
**Rationale**: Likely the current value works fine, but there's no data to confirm. Quick to instrument and measure.

---

### REC-004: WattTime MOER Threshold Optimization (Currently 70%)

## Hypothesis
**Problem Statement**: The charge scheduler pauses charging when MOER exceeds 70%. This threshold was set without analysis of PSCO region MOER distributions. It may be too aggressive (excessive charging interruption) or too lenient (minimal carbon benefit).
**Hypothesis**: Analyzing historical MOER data for the PSCO region will reveal an optimal threshold that maximizes carbon displacement while minimizing charging interruption.

## Experimental Design
**Type**: Historical data analysis + A/B threshold comparison
**Step 1**: Pull 30 days of PSCO MOER data from WattTime API. Plot distribution. Identify natural break points.
**Step 2**: Simulate charging behavior at thresholds 50%, 60%, 70% (current), 80%, 90%
**Metrics**:
- Primary: Hours of charging interrupted per day at each threshold
- Secondary: Estimated CO2 displacement (lbs) at each threshold
- Guardrail: Vehicle must reach full charge by morning (enough off-peak hours)
**Duration**: Analysis phase (1 day), then 1-week A/B per threshold if warranted
**Prerequisites**: WattTime API access (already configured), historical data pull script

## Priority: High
**Rationale**: Directly impacts user experience (vehicle charged on time) and environmental benefit (the whole point of the scheduler). The 70% threshold is unvalidated.

---

### REC-005: OTA Reliability Under Real-World LoRa Conditions

## Hypothesis
**Problem Statement**: OTA has been tested in controlled lab conditions. Real-world LoRa conditions (distance, interference, weather) will have higher packet loss rates that could stress the retry/recovery mechanisms.
**Hypothesis**: Characterizing OTA success rate, transfer time, and retry count across varying RF conditions will validate the current reliability parameters (5 retries, 30s stale threshold, 1-min timer).

## Experimental Design
**Type**: Field measurement across RF conditions
**Variants** (distance/conditions):
- Baseline: Same room (~1m, near-zero loss)
- Indoor: Different floor (~10m, moderate loss)
- Outdoor: Across property (~50m, variable loss)
- Edge: Maximum range (~200m+, high loss)
**Metrics per variant**:
- Primary: OTA success rate (complete transfers / attempts)
- Secondary: Transfer time, retry count, chunks retransmitted
- Guardrail: Recovery from power-cycle during apply (already implemented, needs field validation)
**Duration**: 5 OTA cycles per variant
**Prerequisites**: Two firmware versions to alternate between, `ota_deploy.py status --watch` for monitoring

## Priority: High
**Rationale**: OTA is a safety-critical path. If it fails in the field, the device is bricked until physical access. Must validate before production deployment.

---

### REC-006: Charge Control Auto-Resume Timer Validation

## Hypothesis
**Problem Statement**: The charge control module has an auto-resume timer that re-enables charging after a scheduler-initiated pause. The timer duration and interaction with the cloud scheduler's periodic evaluation need validation to avoid conflicting commands.
**Hypothesis**: The auto-resume timer and scheduler evaluation frequency must be coordinated to avoid the device resuming charging right before the scheduler re-pauses it (unnecessary relay cycling) or the scheduler commanding a pause after the device has already resumed.

## Experimental Design
**Type**: Integration timing analysis
**Step 1**: Map the timing: scheduler rate (EventBridge, configurable), auto-resume delay (device-side), Sidewalk downlink latency
**Step 2**: Identify conflict windows where auto-resume and scheduler pause can race
**Step 3**: If conflicts exist, test with instrumented logging
**Metrics**:
- Primary: Relay cycle count per day (each pause/resume = 1 cycle)
- Secondary: Time-in-wrong-state (device charging when scheduler says pause, or vice versa)
**Prerequisites**: Shell logging of charge control state transitions, CloudWatch logs from scheduler Lambda

## Priority: Medium
**Rationale**: Relay cycling affects hardware longevity. State conflicts confuse cloud-side monitoring.

---

---

### EXP-009: clang-format CI Enforcement — EVALUATED, DECLINED

## Hypothesis
**Problem Statement**: Code formatting is enforced only by convention. A CI-enforced formatter would prevent style drift and reduce review friction.
**Hypothesis**: Adding clang-format to CI will keep C code consistently formatted without manual effort.

## Options Evaluated
1. **Add config + enforce in CI** — one reformatting commit, then CI keeps it consistent going forward
2. **Skip it** — code is already consistently styled by hand. cppcheck catches real bugs, which matters more than formatting
3. **Add config but warn-only** — run in CI as informational, don't fail the build

## Analysis
- Codebase uses **tabs for indentation** (standard in Zephyr/embedded)
- clang-format defaults expect spaces — every line would trigger a violation without config
- Even with correct config (`UseTab: ForIndentation`, `IndentWidth: 8`, `TabWidth: 8`), clang-format would likely reformat existing code (brace placement, `#define` alignment, etc.)
- Risk: a "format-only" commit would pollute git blame and touch files unnecessarily
- Current state: code is already consistently styled by hand across the codebase
- cppcheck is already in CI and catches real bugs — higher value per CI second

## Decision: **DECLINED — Option 2 (Skip it)**
**Rationale**: The cost (reformatting risk, config tuning, git blame pollution) outweighs the benefit for a single-developer embedded project with already-consistent style. cppcheck in CI provides the real value. Revisit if team grows or style drift becomes a problem.

## Status: CONCLUDED — Not implementing

---

### EXP-009b: Board #2 Potentiometer Overcurrent Incident — Partial Silicon Damage

## Incident Report (reconstructed 2026-02-22 by Oliver)

**Date**: ~2026-01-05
**Board**: Board #2 (RAK4631). Device ID from MFG certificate does not reliably identify physical hardware.
**Baseboard**: RAK19007 (only 1 analog pin on J11: AIN1/P0.31)
**Firmware at time of incident**: Original RAK SDK demo (rak1901 temperature/humidity). No custom ADC or GPIO configuration. All pins in nRF52840 default reset state (disconnected input, high impedance). The EVSE monitor firmware was not written until Feb 1 (commit `e35af07`).

## What Happened

A small potentiometer was connected to the RAK19007 J11 header with protection resistors on both sides of the pot. The pot was turned up and **started to smoke**. Shortly thereafter, the board became unable to flash via its built-in USB/DAPLink interface.

## Damage Assessment

| Capability | Status | Evidence |
|-----------|--------|----------|
| Flash memory | **Working** | MFG, platform, and app all flashed via NanoDAP (external SWD programmer) |
| SWD debug port | **Intermittent** | Responds but drops connection constantly. User confirms this is NOT loose wiring — stable wiring doesn't help. Flashing required dozens of retries; pyOCD's page-skip feature accumulated partial writes across attempts. |
| USB interface / DAPLink | **Dead** | Cannot flash or connect via USB. NanoDAP required for all programming. |
| AIN1 (P0.31) | **Possibly damaged** | A friend measured the pin as shorted to ground. Unconfirmed — may be floating input reading ~0V (indistinguishable without resistance measurement). |
| BLE registration | **Not attempted** | Board was flashed but never completed first-boot BLE registration with an Echo gateway. |
| Sidewalk key backup | **Not done** | Keys at `0xF8000` not backed up yet. |

## Root Cause Analysis

**The pot smoked because of excessive power dissipation through the pot's resistive element.** The MCU pin (P0.31, unconfigured high-impedance input on Jan 5) could not source or sink enough current to smoke anything (~5mA max from GPIO, and the pin wasn't even configured as output). The current path was the pot's power supply (3V3 from J11 header) → pot → GND.

For a pot to smoke at 3.3V, total resistance must be very low:
- 100Ω → 109mW (borderline for small trim pot rated 100-250mW)
- 50Ω → 218mW (smoking)
- 20Ω → 545mW (definitely smoking)

The pot was described as "small" — likely a trim pot with low power rating.

**The "coded as output" theory is ruled out.** On Jan 5, the firmware was the RAK SDK demo with no custom pin configuration. P0.31 was in nRF52840 default reset state (disconnected input). No firmware version in the git history ever configured P0.31 as a GPIO output.

## Why the Board Became Partially Unflashable

Two candidate damage mechanisms from the pot overcurrent:

**Theory 1 — LDO thermal damage**: The RAK19007's onboard voltage regulator was trying to supply the short-circuit current through the pot. Sustained overcurrent overheated the LDO, permanently degrading it. Now produces marginal voltage on power-up → nRF52840 operates at edge of voltage spec → intermittent SWD, dead USB.

**Theory 2 — nRF52840 latch-up**: If the 3V3 rail sagged during the overcurrent event and then recovered rapidly (when the pot burned open or was disconnected), the voltage transient could trigger CMOS latch-up in the nRF52840. Latch-up drives destructive current through chip internals. Damage pattern: flash memory survives (robust oxide), sensitive analog/digital peripherals (USB PHY, SWD timing, SAADC) degraded.

Both theories are consistent with the observed behavior: flash writes work, SWD is intermittent even with stable wiring, USB is completely dead.

## Recovery Path

Board #2 is partially functional but unreliable. Options:
1. **Continue with NanoDAP**: Accept intermittent SWD, complete BLE registration, back up Sidewalk keys, use as secondary test board
2. **Write off**: Treat as parts donor. The board may never be reliable enough for development
3. **Solder SWD wires**: Reduce connection resistance to see if SWD reliability improves (tests whether the issue is marginal signal integrity vs dead silicon)

## Key Insight: nRF52840 AIN Naming Confusion

A critical naming confusion was identified that affected the entire project from Feb 1 through Feb 19:

| Label | nRF52840 SAADC | Physical Pin | WisBlock Connector (RAK4631) |
|-------|---------------|-------------|------------------------------|
| `NRF_SAADC_AIN0` | AIN0 | P0.02 | **Not on WisBlock connector** |
| `NRF_SAADC_AIN1` | AIN1 | P0.03 | **Not on WisBlock connector** |
| `NRF_SAADC_AIN7` | AIN7 | P0.31 | Pin 22, labeled **"AIN1"** on J11 |
| WisBlock "AIN0" | AIN3 | P0.05 | Pin 21 |
| WisBlock "AIN1" | AIN7 | P0.31 | Pin 22 |

The original firmware (commit `e35af07`, Feb 1) used `NRF_SAADC_AIN0` (P0.02) for pilot voltage — a pin that is NOT routed to the WisBlock connector. All telemetry data from Feb 4–19 was from a floating, unconnected internal pin. The firmware was not corrected to read from P0.31 (the actual WisBlock AIN1 pad) until TASK-100 (commit `41ea3fe`, Feb 19).

An intermediate commit on Feb 19 (`59102c3`) swapped pilot to `NRF_SAADC_AIN1` (P0.03) — also not on the WisBlock connector — likely due to the same naming confusion. This was superseded hours later by TASK-100.

This naming mismatch means: **no real analog sensor data was ever collected before Feb 20 11:00 AM.** All prior telemetry (documented in `j1772-telemetry-analysis-2026-02.md`) was noise from floating pins. See that document's addendum for the full revised interpretation.

## Status: DOCUMENTED — Board #2 is damaged, root cause recorded. Naming confusion identified and corrected in TASK-100. Board numbering corrected 2026-02-22 (was previously mislabeled as Board #1).

---

### EXP-010: RAK19001 Baseboard Pin Mapping Validation — Connector Seating Failure

## Hypothesis
**Problem Statement**: After switching from RAK19007 to RAK19001 WisBlock Dual IO Base Board, AIN1 (P0.31, pin 22) measured as grounded with a voltmeter. The RAK19001 was chosen to gain access to more analog pins. The question was whether the pin failure was caused by an nRF52840 silicon errata (SAADC analog mux latching), an NFC pin conflict, a bad baseboard, or something mechanical.
**Hypothesis**: The pilot ADC failure on P0.31 is caused by an nRF52840 SAADC errata that latches analog inputs to ground. Switching to AIN2 (P0.04/IO4) and adding the SAADC disable workaround at boot will restore analog input functionality.
**Success Metrics**: Successful ADC reading of pilot voltage on the new pin. Successful GPIO output and input on all available IO pins.

## Experimental Design
**Type**: Systematic hardware bring-up — pin-by-pin validation of the RAK19001 baseboard using GPIO output (LED toggle), GPIO input (button/wire short), and ADC tests.
**Branch**: `task/ain2-swap` (worktree)
**Firmware Changes Made**:
1. ADC input changed from `NRF_SAADC_AIN7` (P0.31) to `NRF_SAADC_AIN2` (P0.04/IO4) in devicetree overlay
2. Added `#include <hal/nrf_saadc.h>` and `nrf_saadc_disable(NRF_SAADC)` call at start of `platform_adc_init()` as SAADC errata workaround
3. Added `CONFIG_NFCT_PINS_AS_GPIOS=y` to `prj.conf` (required for IO5/IO6 which share NFC pins)
4. Various pin swaps for `charge_block` and `cool_call` GPIOs during iterative testing

**Test Methodology**:
- **GPIO Output**: Configure pin as output, toggle LED via `charge_block` GPIO assignment. Visual confirmation of LED state change.
- **GPIO Input**: Configure pin as input with pull-up, test with button press and direct wire-short to GND via `cool_call` GPIO assignment. Shell readback of state.
- **ADC**: Configure SAADC channel, read voltage via `app evse status`. Compare against known applied voltage.

## Results

### GPIO Output (charge_block / LED toggle test)

| Connector Pin | WisBlock Label | nRF52840 GPIO  | Output Result | Pin Row |
|:-------------:|:--------------:|:--------------:|:-------------:|:-------:|
| 29            | IO1            | P0.17 (gpio0 17) | **PASS**   | Odd     |
| 30            | IO2            | P1.02 (gpio1 2)  | **FAIL**   | Even    |
| 31            | IO3            | P0.21 (gpio0 21) | **PASS**   | Odd     |
| 32            | IO4            | P0.04 (gpio0 4)  | **FAIL**   | Even    |
| 37            | IO5            | P0.09 (gpio0 9)  | **PASS**   | Odd     |
| 38            | IO6            | P0.10 (gpio0 10) | **FAIL**   | Even    |
| 24            | IO7            | NC               | **FAIL** (expected — not connected) | Even |

### GPIO Input (cool_call / button + wire short test)

| Connector Pin | WisBlock Label | nRF52840 GPIO  | Input Result | Method | Pin Row |
|:-------------:|:--------------:|:--------------:|:------------:|:------:|:-------:|
| 31            | IO3            | P0.21 (gpio0 21) | **PASS**  | Button | Odd     |
| 37            | IO5            | P0.09 (gpio0 9)  | **PASS**  | Wire short to GND | Odd |
| 30            | IO2            | P1.02 (gpio1 2)  | **FAIL**  | Button | Even    |
| 32            | IO4            | P0.04 (gpio0 4)  | **FAIL**  | Button | Even    |
| 38            | IO6            | P0.10 (gpio0 10) | **FAIL**  | Wire short to GND | Even |

**Important discovery**: Initial input tests all appeared to fail (including odd pins). The breakthrough came when IO5 was shorted directly to GND with a wire — `cool=1` appeared immediately. This proved the pin was working and the external button/potentiometer assembly was broken or miswired. Subsequent retesting with wire shorts confirmed the odd/even pattern.

### ADC Test

| Connector Pin | WisBlock Label | nRF52840 Analog  | ADC Result | Pin Row |
|:-------------:|:--------------:|:----------------:|:----------:|:-------:|
| 22            | AIN1           | P0.31 / AIN7     | **FAIL** — 0mV, pin grounded | Even |
| 32            | IO4            | P0.04 / AIN2     | **FAIL** — 0mV, no response to applied voltage | Even |

The SAADC errata workaround (`nrf_saadc_disable(NRF_SAADC)` at boot) was applied but did **not** resolve the ADC failure. Both analog pins tested are on even-numbered connector pins.

### The Pattern

**Every failing pin is on an even-numbered connector position. Every passing pin is on an odd-numbered connector position.**

On the RAK19001's 40-pin board-to-board connector, odd and even pins sit on opposite physical rows. This means one entire row of the connector has no electrical contact.

**Decision**: **ROOT CAUSE IDENTIFIED — Mechanical connector seating failure**

The RAK4631 module is not fully seated in the RAK19001 baseboard's 40-pin board-to-board connector. One row of pins (the even-numbered row) has no electrical contact.

## Theories Investigated and Ruled Out

| # | Theory | Evidence Against |
|---|--------|-----------------|
| 1 | **nRF52840 SAADC errata** — analog mux latches pins to ground | Applied `nrf_saadc_disable()` workaround; did not fix. Also, the failure affects digital GPIO pins (IO2, IO4, IO6), not just analog. |
| 2 | **NFC pin conflict** — IO5/IO6 share NFC pins P0.09/P0.10 | Added `CONFIG_NFCT_PINS_AS_GPIOS=y`; IO5 (odd) worked, IO6 (even) did not. The NFC config was correct but irrelevant to the root cause. |
| 3 | **Individual dead pins on nRF52840** — silicon defect | The perfect odd/even split across 7 tested pins rules out random pin failure. The failing pins span both gpio0 and gpio1 ports. |
| 4 | **Bad RAK19001 baseboard traces** — manufacturing defect | A trace defect would not produce a perfect odd/even pattern aligned with connector row geometry. |
| 5 | **Damaged potentiometer/button** — external wiring fault | Partially true (the button was indeed broken/miswired), but this was a secondary issue. The primary failure was connector seating, proven by wire-short tests bypassing the button. |

## Resolution
1. Reseat the RAK4631 module firmly in the RAK19001 baseboard, ensuring both rows of the 40-pin connector click into place.
2. Revert all firmware changes back to original pin assignments:
   - `charge_block` on IO1/P0.17
   - `cool_call` on IO2/P1.02
   - Pilot ADC on AIN7/P0.31
3. Re-test all even pins (IO2, IO4, IO6, AIN1) to confirm the connector fix.

## Key Insights
- **Mechanical failures masquerade as electrical/software bugs.** The original symptom ("AIN1 reads 0V") looked exactly like an SAADC errata or pin configuration problem. Hours were spent on firmware workarounds before the physical root cause was identified.
- **Systematic pin-by-pin testing reveals patterns that point-testing misses.** Testing a single pin (AIN1) gave no information about root cause. Testing seven pins across both connector rows revealed the odd/even pattern that immediately pointed to connector seating.
- **The "broken button" red herring was costly.** When all input tests initially failed, it looked like a firmware or pull-up configuration issue. The breakthrough of wire-shorting IO5 directly to GND, bypassing the button, was the critical debugging step that separated the button problem from the connector problem.
- **Board-to-board connectors are fragile.** The RAK WisBlock system uses Hirose DF40C-series connectors with 0.4mm pitch. These require significant, even pressure to seat fully. Partial seating can leave one row of contacts disconnected while the module appears physically attached.
- **Always validate assumptions on new hardware before writing software workarounds.** The switch from RAK19007 to RAK19001 was assumed to be a drop-in replacement. A 5-minute continuity check on the connector would have caught this before any firmware changes were made.

## Addendum (2026-02-22)

Subsequent investigation revealed that the telemetry data from Feb 4–19 (analyzed in `j1772-telemetry-analysis-2026-02.md`) was **not** related to the RAK19001 connector seating failure. The firmware was reading from `NRF_SAADC_AIN0` (P0.02) — a pin not routed to the WisBlock connector — until TASK-100 remapped to `NRF_SAADC_AIN7` (P0.31) on Feb 19. All data before Feb 20 was floating-pin noise, not a hardware fault. The potentiometer was not connected until ~11 AM Feb 20.

The connector seating failure (even-row pin pattern) remains valid and confirmed by EXP-010's pin-by-pin tests. However, AIN1 (P0.31, pin 22) may have an additional, independent failure: on Feb 20, connecting a voltmeter caused a board reset, after which P0.31 read 0mV permanently. This could be SAADC mux latch (recoverable via firmware) or ESD damage (permanent). See EXP-012 for the diagnostic protocol.

See also: `j1772-telemetry-analysis-2026-02.md` addendum for the full revised interpretation.

## Status: CONCLUDED — Root cause identified (mechanical), firmware changes to be reverted

---

### REC-007: Post-Reseat RAK19001 Pin Validation

## Hypothesis
**Problem Statement**: EXP-010 identified that one row of the RAK4631-to-RAK19001 connector is not making contact. After reseating the module, all even-row pins need validation before resuming normal development.
**Hypothesis**: Firmly reseating the RAK4631 in the RAK19001 will restore electrical contact on all even-row pins (IO2, IO4, IO6, AIN1).

## Experimental Design
**Type**: Validation test (confirm fix for EXP-010)
**Test Plan**:
1. Reseat RAK4631 module with firm, even pressure until both connector rows click
2. Before any firmware changes: multimeter continuity check on all even pins (22, 30, 32, 38) between module pad and baseboard test point
3. Flash original firmware (charge_block=IO1, cool_call=IO2, ADC=AIN7)
4. Test IO2 GPIO output (LED toggle) — was FAIL in EXP-010
5. Test IO2 GPIO input (wire short to GND) — was FAIL in EXP-010
6. Test AIN1/P0.31 ADC (apply known voltage from potentiometer) — was 0mV in EXP-010
7. Optionally test IO4 and IO6 for completeness
**Success Metrics**: All previously-failing even pins respond correctly.
**Duration**: 30 minutes

## Priority: **Critical** — Blocks all further hardware development on RAK19001

---

### REC-008: SAADC Errata Workaround Retention Decision

## Hypothesis
**Problem Statement**: EXP-010 added `nrf_saadc_disable(NRF_SAADC)` at boot as a workaround for the nRF52840 SAADC errata. The workaround did not fix the issue (root cause was mechanical), but the underlying SAADC errata is real and could manifest in other scenarios (warm reboot, long uptime, sleep/wake cycles).
**Hypothesis**: Keeping the SAADC disable-at-boot workaround as a defensive measure will prevent a potential future SAADC lockup, at negligible code/runtime cost.

## Experimental Design
**Type**: Risk/benefit analysis + targeted stress test
**Step 1**: Review the specific nRF52840 SAADC errata (anomaly 86, 87, or related) and determine if the disable-at-boot pattern is the recommended mitigation
**Step 2**: If applicable, keep the workaround; if not, remove it to avoid cargo-cult code
**Step 3**: Stress test: rapid ADC enable/disable cycles + sleep/wake transitions, checking for stuck-at-ground readings
**Metrics**: ADC accuracy after 1000 enable/disable cycles with and without the workaround
**Duration**: 1 hour

## Priority: **High** (elevated from Low, 2026-02-22)
**Rationale**: EXP-012 Phase 1 will test whether the SAADC workaround recovers Board #1's grounded AIN1 pin. If Hypothesis A (SAADC mux latch) is confirmed, this workaround becomes a **permanent production requirement** — without it, any board reset during an active ADC sample could permanently ground an analog pin until the next firmware flash. The Feb 20 voltmeter incident on Board #1 is a plausible real-world trigger for this exact failure mode. See EXP-012 Phase 1 for the diagnostic protocol. Resolves jointly with TASK-104.

---

### REC-009: External Button/Potentiometer Assembly Validation

## Hypothesis
**Problem Statement**: During EXP-010 input testing, all button-based input tests initially appeared to fail. The button/potentiometer assembly may be miswired, damaged, or require a different pull-up configuration.
**Hypothesis**: The external button/potentiometer assembly has a wiring fault independent of the RAK19001 connector issue.

## Experimental Design
**Type**: Component isolation test
**Step 1**: Multimeter continuity test on the button assembly (press/release)
**Step 2**: Measure potentiometer resistance range (wiper to each end)
**Step 3**: Connect button to a known-good IO pin (e.g., IO1/P0.17, confirmed working in EXP-010) and test input
**Step 4**: Connect potentiometer output to AIN1 after connector reseat, sweep voltage range
**Metrics**: Button produces clean low/high transitions; potentiometer produces linear 0-3.3V sweep
**Duration**: 15 minutes

## Priority: Medium
**Rationale**: The button failure was initially conflated with the connector issue. It needs independent validation to confirm the EVSE simulator hardware is fully functional.

---

### EXP-011: Dashboard Data Generation — Lambda Replay + Serial Automation

## Hypothesis
**Problem Statement**: The EVSE Fleet Dashboard needed realistic multi-day telemetry data for development and testing, but accumulating real device data over LoRa would take weeks. Without representative data (charge sessions, faults, idle periods, OTA events, Charge Now overrides), dashboard features couldn't be validated.
**Hypothesis**: Invoking the decode Lambda directly with firmware-format binary payloads and backdated timestamps will generate realistic dashboard data that exercises the full decode pipeline — bypassing only the LoRa radio hop.
**Success Metrics**: DynamoDB populated with 14 days of realistic telemetry. Dashboard renders all event types correctly. Full end-to-end pipeline verified via serial automation.

## Experimental Design
**Type**: Tooling experiment — two complementary data generation approaches
**Branch**: Part of `task/106-dashboard-migration` work
**Implementation** (commit `cd08a27`):

**Variant A — Lambda replay** (`aws/seed_dashboard_data.py`):
- Builds firmware-format binary payloads (magic byte, version, J1772 state, pilot voltage, current, thermostat flags, device epoch)
- Invokes the `uplink-decoder` Lambda directly with `timestamp_override_ms` for backdating
- Generates 14 days of scenarios: idle, charge sessions, faults, wandering pilot, recovery, OTA, Charge Now overrides
- Small Lambda change: `decode_evse_lambda.py` accepts optional `timestamp_override_ms` field (production uplinks don't include this, so behavior unchanged)

**Variant B — Serial automation** (`aws/serial_data_gen.py`):
- Connects to device serial port (`/dev/tty.usbmodem101`)
- Cycles through J1772 simulation states via shell commands (`app evse a/b/c`, `app evse allow/pause`, `app sid send`)
- Each uplink travels the full path: device → LoRa → Sidewalk → IoT Rule → decode Lambda → DynamoDB
- Respects the 5s TX rate limiter

## Results
**Decision**: **GO** — Both approaches work, committed to main
**Primary Metric Impact**: 14 days of realistic telemetry populated in DynamoDB in ~2 minutes (Lambda replay) vs real-time generation (serial automation).
**Bug discovered**: The seed script exposed TASK-108 — `ev.timestamp` vs `ev.timestamp_mt` field mismatch in the dashboard frontend. The event table was showing "Invalid Date" because the frontend expected a field name that didn't match what the Lambda wrote.
**Secondary benefit**: The `timestamp_override_ms` approach keeps the Lambda as the single source of truth for payload decoding. No separate data-import path needed.

## Key Insights
- Lambda replay that uses the real decode path is strictly better than writing DynamoDB records directly — it catches schema mismatches, decode bugs, and interlock logic issues that a raw DynamoDB put would miss.
- The seed script's scenario library (idle → charge → fault → recovery) doubles as a regression suite for the decode pipeline.
- Serial automation is slower but validates the full radio path. Use Lambda replay for volume, serial for end-to-end confidence.
- The `timestamp_override_ms` pattern is a clean way to enable historical data injection without polluting the production code path.

## Status: CONCLUDED — Shipped, both scripts in `aws/`

---

### EXP-012: Board #1 AIN1 Pin Recovery + RAK19001 Connector Validation

> **Board numbering correction (2026-02-22)**: Board #1 and Board #2 labels were swapped in all prior documentation. Board #1 is physically marked "1" on the module. It is the board with (a) AIN1 grounded after the Feb 20 voltmeter incident, and (b) dead DAPLink (flashable via NanoDAP only). Board #2 is the board where the pot smoked ~Jan 5. This correction was applied across EXP-009b, EXP-012, and TASK-104.

## Hypothesis
**Problem Statement**: Board #1 (physically marked "1") has two overlapping pin problems:
1. **P0.31 (AIN1) reads 0mV permanently** — after a voltmeter was connected on Feb 20, the board reset and AIN1 was grounded thereafter. This may be an SAADC analog mux latch (recoverable via firmware) or ESD damage to the pin's protection diode (permanent).
2. **RAK19001 even-row connector seating failure** (EXP-010) — one row of the 40-pin Hirose DF40C connector has no electrical contact.

These problems must be separated and addressed independently.

**Hypothesis A (SAADC latch)**: The board reset on Feb 20 interrupted an active SAADC sample, latching the analog mux in a state that grounds P0.31 internally. Adding `nrf_saadc_disable(NRF_SAADC)` at boot will release the latch and restore AIN1. This is a known nRF52840 SAADC errata behavior.

**Hypothesis B (ESD damage)**: The voltmeter probe injected an ESD pulse that permanently damaged P0.31's internal protection diode, creating a low-impedance path to ground. The pin is physically dead.

**Success Metrics**: Determine which hypothesis is correct. If A: recover AIN1 via firmware. If B: confirm pin is dead and plan for alternate pin (AIN2/P0.04 on IO4). Then validate RAK19001 connector seating independently.

## Hardware Inventory

| Item | Status | Device ID | Notes |
|------|--------|-----------|-------|
| **Board #1** (RAK4631, marked "1") | Active dev board | (cert-based, not reliable HW ID) | Voltmeter incident Feb 20 → P0.31/AIN1 grounded (recovered via SAADC workaround). **DAPLink dead**. Flashable via NanoDAP only. |
| **Board #2** (RAK4631) | Active dev board | (cert-based, not reliable HW ID) | Pot overcurrent ~Jan 5. **DAPLink dead**. Flashable via NanoDAP only. Intermittent SWD failures during flash (see KI below). |
| **RAK19007 baseboard ×2** | Working | — | 3 pins on J11: AIN1 (P0.31), IO1 (P0.17), IO2 (P1.02) |
| **RAK19001 baseboard** | Bad solder joints on even-row headers | — | 7+ IO pins. Even-row GPIOs work via back-side probe; front-side header pins have cold joints. Needs reflow. |
| **RAK19011 baseboard** | Untested | — | Alternate baseboard. |
| **NanoDAP** (external SWD programmer) | Working | `0700000100...` | Required for both boards (both onboard DAPLinks are dead). |

### Known Issue: Both DAPLinks Dead
Neither Board #1 nor Board #2 has a working onboard DAPLink programmer. Board #1 was known; Board #2 was discovered during EXP-012 when `pyocd list` showed no probes with only Board #2 connected. Both boards require the external NanoDAP for all flash operations.

### Known Issue: Power Brownout During Flash (Board #2)
Board #2 exhibits intermittent SWD communication failures during flash operations via NanoDAP. **Observed behavior**: the red LED on the baseboard dims and flashes rapidly, after which pyOCD reports "SWD/JTAG communication failure (No ACK)." The operator must physically unsnap the RAK4631 module from the baseboard and resnap it to restore communication. This happens during most flash operations and occasionally at other times.

**Likely cause**: Flash erase/write operations draw significantly more current than normal operation. If the baseboard power supply cannot sustain the peak current, the module browns out, resetting the SWD debug interface. The snap-unsnap cycle power-cycles the module and re-establishes the SWD connection.

**Impact**: Multi-step flash sequences (MFG → platform → app) are unreliable — each step may require a module reseat. Single-step flashes sometimes succeed on first attempt. The `--frequency 1000000` (1MHz) flag helps but does not eliminate the issue.

**Workaround**: After any SWD failure, unsnap and resnap the module, then retry. Verify each flash step with `sid mfg` / `sid status` before proceeding to the next.

### Potentiometer wiring note
The external pot has **protection resistors on both sides** (series resistors between pot and MCU pin, and between pot and supply). This limits fault current through the analog pin but does not protect against ESD (nanosecond pulse passes before resistor can limit).

## Experimental Design

**Type**: Multi-phase diagnostic — firmware test, then resistance measurement, then connector validation

### Phase 1 — SAADC Latch Test (Board #1 on RAK19007, non-invasive)

This tests Hypothesis A. Can be done on the current RAK19007 setup before touching the RAK19001.

**Step 1.1 — Resistance measurement (board powered OFF)** — SKIPPED (2026-02-22, no multimeter available)
- Remove Board #1 from baseboard
- Multimeter: measure resistance from P0.31 pad to GND pad on the module itself
- **If near 0Ω**: ESD diode is blown → Hypothesis B confirmed → P0.31 is dead → skip to Phase 2
- **If high impedance (MΩ)**: No physical damage → Hypothesis A likely → proceed to Step 1.2
- **Decision**: Proceed directly to Step 1.2. The firmware test is non-destructive and will give us diagnostic signal either way — if AIN1 recovers, Theory A is confirmed regardless of the resistance measurement.

**Step 1.2 — Firmware SAADC workaround build** — DONE (2026-02-22)
Build and flash firmware with the SAADC disable workaround (from TASK-104 implementation reference):
```c
#include <hal/nrf_saadc.h>
// At top of platform_adc_init(), before adc_initialized check:
nrf_saadc_disable(NRF_SAADC);
```
Flashed to Board #1 on RAK19007 via NanoDAP. Branch `task/104-saadc-errata-workaround`, 454KB programmed successfully. Also fixed pre-existing `leds_id_t` build error. Board boots, serial shell responds, Sidewalk init OK.
Initial `app evse status` with nothing connected to J11: **Pilot voltage: 0 mV**. This is expected with no pot connected — need Step 1.3 to determine if pin is alive.

**Step 1.3 — AIN1 ADC test** — DONE (2026-02-22) — **AIN1 IS ALIVE**
- Touched 3V3 (pin 5) to AIN1 (pin 22) on RAK19001 J10/J15 headers
- `app evse status` reported: **Pilot voltage: 3301–3343 mV**
- Pin bounced between 3300 mV and 0 mV as wire was touched/released — expected behavior
- **RESULT: Hypothesis A confirmed** → SAADC analog mux latch was the cause → `nrf_saadc_disable(NRF_SAADC)` at boot released the latch → AIN1 (P0.31) is fully recovered
- **Partial finding**: Pin 22 (even row) works — but see Step 1.5 below for the full picture
- **Decision**: Keep `nrf_saadc_disable()` workaround permanently as a production safety measure. Merge TASK-104 to main.
- **Complete Phase 1 picture (2026-02-22)**: The AIN1 recovery via SAADC workaround is definitive. Combined with Step 1.6 (back-side probe confirming all even-row GPIOs work), Phase 1 establishes: (a) the nRF52840 silicon is fine on both boards, (b) the Hirose DF40C board-to-board connector is fine, (c) the only hardware fault is bad solder joints on the RAK19001's hand-soldered header pins. Additionally, both boards have dead onboard DAPLink programmers (Board #1 from the Feb 20 voltmeter incident brownout, Board #2 from the Jan 5 pot overcurrent incident), requiring the external NanoDAP for all flash operations. Board #2 also exhibits intermittent SWD brownout during flash (red LED dims, "No ACK" errors, requires module reseat).

**Step 1.5 — Even-row GPIO test (all at once)** — DONE (2026-02-22)
- Built throwaway firmware on branch `exp/012-even-pin-test` (from task-104)
- Configured IO2 (P1.02), IO4 (P0.04), IO6 (P0.10) as pull-down inputs
- Added `app io read` shell command to read all three GPIOs + AIN1 ADC simultaneously
- Overlay: changed cool_call from PULL_UP|ACTIVE_LOW to PULL_DOWN|ACTIVE_HIGH; added io4_test and io6_test nodes
- Platform: added gpio_dt_spec structs, GPIO_PIN_4/GPIO_PIN_6 indices, init + read cases
- App: added shell_io_read() function, `app io read` dispatch
- Touched 3V3 to each even-row pin:

| Pin | WisBlock Label | nRF52840 | Connector Pin | Result |
|-----|---------------|----------|---------------|--------|
| 22  | AIN1          | P0.31    | even          | **3301–3343 mV** — PASS |
| 30  | IO2           | P1.02    | even          | LOW — **FAIL** |
| 32  | IO4           | P0.04    | even          | LOW — **FAIL** |
| 38  | IO6           | P0.10    | even          | LOW — **FAIL** |

- **RESULT**: Only pin 22 makes contact via front-side headers. Pins 30, 32, 38 fail via front.

**Step 1.6 — Back-side probe of even-row pins** — DONE (2026-02-22)
- Probed even-row pins from the **back side** of the RAK19001 PCB (bypassing soldered header pins)
- **All even-row pins work** when probed from the back — IO2, IO4, IO6 all read HIGH with 3V3 applied
- **ROOT CAUSE IDENTIFIED**: The hand-soldered header pin row on the RAK19001 has **cold/bad solder joints** on the even row. The Hirose board-to-board connector and the nRF52840 GPIOs are both fine.
- This fully explains EXP-010's even-row failure pattern — it was never a connector seating issue, it was bad header solder joints on the baseboard.
- **Fix**: Reflow or resolder the even-row header pins on the RAK19001. No firmware or connector work needed.

**Step 1.4 — If AIN1 is dead, test AIN2 fallback**
- Remap pilot ADC to `NRF_SAADC_AIN2` (P0.04 / IO4) — the ain2-swap branch already has this change
- Requires RAK19001 or RAK19011 baseboard (IO4 is not on RAK19007 J11)
- This consumes an IO pin but provides a working analog input

### Phase 2 — RAK19001 Connector Validation (Board #1)

> **SUPERSEDED (2026-02-22)**: Steps 2.1–2.6 below were designed to diagnose a suspected Hirose DF40C connector seating failure. Step 1.6 (back-side probe) identified the actual root cause: **cold/bad solder joints on the hand-soldered even-row header pins** on the RAK19001 baseboard. The Hirose board-to-board connector is fine — all even-row GPIOs respond correctly when probed from the back side of the PCB, bypassing the header pins. The fix is straightforward: reflow/resolder the even-row header pins on the RAK19001. No connector reseat, baseboard swap, or firmware changes are needed.
>
> **Summary of Phase 2 resolution**:
> - **Root cause**: Cold/bad solder joints on hand-soldered even-row header pins on RAK19001
> - **Evidence**: All even-row GPIOs (IO2, IO4, IO6) read correctly via back-side PCB probe; fail only via front-side header pins
> - **Fix**: Reflow or resolder the even-row header pins on the RAK19001
> - **Hirose DF40C connector**: Confirmed working — not the source of EXP-010's even-row failures
> - **Steps 2.1–2.6**: No longer needed. Retained below as historical record.

<details>
<summary>Original Phase 2 steps (superseded by Step 1.6 findings — click to expand)</summary>

Proceed regardless of Phase 1 outcome — the connector issue is independent.

**Step 2.1 — Physical reseat**
- Remove Board #1 from RAK19001 baseboard completely
- Inspect both connector rows for bent pins, debris, or oxidation
- Reseat with firm, even pressure until both rows of the Hirose DF40C click into place
- Visual check: module sits flat and level on baseboard, no tilt

**Step 2.2 — Continuity check (before firmware)**
Multimeter continuity test on all even-row pins:
| Pin | WisBlock Label | nRF52840 | EXP-010 Result |
|-----|---------------|----------|---------------|
| 22 | AIN1 | P0.31 | FAIL |
| 30 | IO2 | P1.02 | FAIL |
| 32 | IO4 | P0.04 | FAIL |
| 38 | IO6 | P0.10 | FAIL |

Also verify odd-row pins (sanity check): pins 29, 31, 37.
**STOP if continuity fails** — swap to RAK19011 baseboard and retest (isolates baseboard vs connector).

**Step 2.3 — GPIO output test (charge_block LED toggle)**
| Test | Pin | nRF52840 GPIO | Expected |
|------|-----|---------------|----------|
| 2.3a | IO2 / pin 30 | P1.02 (gpio1 2) | PASS |
| 2.3b | IO4 / pin 32 | P0.04 (gpio0 4) | PASS |
| 2.3c | IO6 / pin 38 | P0.10 (gpio0 10) | PASS |

Method: Assign `charge_block` to each pin, `app evse allow` / `app evse pause` — visual LED toggle.

**Step 2.4 — GPIO input test (wire short to GND)**
| Test | Pin | nRF52840 GPIO | Expected |
|------|-----|---------------|----------|
| 2.4a | IO2 / pin 30 | P1.02 (gpio1 2) | PASS |
| 2.4b | IO4 / pin 32 | P0.04 (gpio0 4) | PASS |
| 2.4c | IO6 / pin 38 | P0.10 (gpio0 10) | PASS |

Method: Wire short pin to GND, verify `app hvac status` shows `cool=1`. Remove wire, verify `cool=0`.

**Step 2.5 — ADC test (AIN1 + AIN2)**
| Test | Pin | nRF52840 Analog | Expected |
|------|-----|-----------------|----------|
| 2.5a | AIN1 / pin 22 | P0.31 / AIN7 | Correct voltage (if Phase 1 recovered it) or 0mV (if dead) |
| 2.5b | IO4 / pin 32 | P0.04 / AIN2 | Correct voltage |

Method: Potentiometer (with protection resistors) to pin, sweep range, compare against multimeter reference.

**Step 2.6 — Baseboard swap (if Step 2.2 fails)**
If continuity fails on even pins after reseat:
- Move Board #1 to **RAK19011** baseboard
- Repeat Step 2.2 continuity check
- **If RAK19011 passes**: RAK19001 baseboard is faulty
- **If RAK19011 also fails**: Board #1 module connector is damaged

</details>

### Phase 3 — Final Pin Assignment Decision

**Outcome (2026-02-22): AIN1 recovered + Connector fine (bad solder)**

This matches the first row of the decision matrix below. Decisions:

1. **Use original pin map** — AIN7/P0.31 for pilot voltage. No pin reassignment needed.
2. **Keep SAADC workaround permanently** — `nrf_saadc_disable(NRF_SAADC)` at boot is now a production requirement. Without it, any board reset during an active ADC sample could re-latch the analog mux and ground the pin until next firmware flash. Merge TASK-104 to main.
3. **Reflow RAK19001 header pins when needed** — the even-row solder joints need rework before the RAK19001 can be used for v1.1 features (TASK-095/096/097/098). This is a 10-minute soldering task, not a blocker for current development.
4. **RAK19007 is sufficient for now** — it has AIN1 on J11 (P0.31), IO1 (P0.17), and IO2 (P1.02). Current v1.0 firmware only needs pilot ADC + charge_block GPIO + cool_call GPIO, all of which are available on RAK19007.

Original decision matrix (retained for reference):

| Phase 1 Result | Phase 2 Result | Action |
|---------------|---------------|--------|
| **AIN1 recovered (latch)** | **Connector fine (bad solder)** | **<-- THIS OUTCOME.** Use original pin map (AIN7 pilot). Keep SAADC workaround permanently. Reflow header pins when RAK19001 needed. |
| AIN1 recovered (latch) | Connector still broken | Stay on RAK19007 (3 pins) or try RAK19011. Keep SAADC workaround. |
| AIN1 dead (ESD) | Connector fixed | Use AIN2 (P0.04/IO4) for pilot on RAK19001. Costs one IO pin. |
| AIN1 dead (ESD) | Connector still broken | Stay on RAK19007, no analog input available. Escalate to new module. |

## Metrics
- **Primary**: Is P0.31 recoverable? (Yes/No — determines v1.1 pin budget)
- **Secondary**: Pass/fail on each even-row pin after connector reseat (4 pins × output + input)
- **Tertiary**: Voltage accuracy on working ADC pins (±50mV of multimeter reference)
- **Guardrail**: No regressions on odd-row pins (IO1, IO3, IO5)

## Priority: **Critical** — Blocks all v1.1 hardware features (TASK-095 → 096 → 097 → 098) and determines whether TASK-104 workaround ships permanently

## Status: Phase 1 COMPLETE, Phase 2 SUPERSEDED, Phase 3 DECIDED — Hypothesis A confirmed (SAADC mux latch). AIN1 recovered via `nrf_saadc_disable()` workaround. Phase 2 superseded by Step 1.6 root cause: bad solder joints on RAK19001 even-row header pins (not a connector seating issue). Remaining action: reflow RAK19001 header pins when needed for v1.1 features. Current development continues on RAK19007 with original pin map.

---

### REC-010: Connector Seating Force Measurement

## Hypothesis
**Problem Statement**: EXP-010 revealed that the Hirose DF40C 40-pin connector can appear physically attached while leaving one entire row disconnected. This is a production risk — field installers may not apply sufficient seating force.
**Hypothesis**: Measuring the force required for full engagement and defining a go/no-go tactile or visual indicator will prevent connector seating failures in production units.

## Experimental Design
**Type**: Process validation
**Step 1**: With a spring scale, measure the insertion force required for the RAK4631 to fully seat (both rows click)
**Step 2**: Identify the tactile/audible feedback that distinguishes partial vs full seating
**Step 3**: Document a commissioning checklist step: "apply X grams of force until Y click/flush indicator"
**Step 4**: Consider a simple electrical continuity test as part of commissioning self-test (e.g., loopback between an odd and even pin)

## Priority: Medium
**Rationale**: Important for production reliability, but only relevant once the PCB design (TASK-095) is finalized.

---

## Appendix: Experiment Evolution Timeline

```
Commit     Date         Experiment                          Outcome
───────────────────────────────────────────────────────────────────────
e35af07    —            EVSE monitor app created            Foundation
550560f    —            EXP-006: Raw payload (69% reduction)  GO
d6faff4    —            EXP-007: Split-image architecture   GO
deb4007    —            EXP-005: On-change sensing          GO
b8e62cd    —            EXP-002: Chunk size 12→15B (20%)    GO
3db368b    —            EXP-003: Stale threshold 300→30s    GO
e3f97e0    —            EXP-001: Windowed blast mode        REVERTED
65e7389    —            EXP-004: Delta OTA (~100x faster)   GO
78924b6    —            EXP-001 reverted (delta made it moot)
7dab212    —            OTA reliability fixes (supporting)  GO
e88d519    —            EXP-008: Generic platform           GO
—          ~2026-01-05  EXP-009b: Board #2 pot overcurrent  DAMAGED (partial silicon)  [board numbering corrected 2026-02-22]
—          2026-02-21   EXP-010: RAK19001 pin validation    MECHANICAL FAILURE
cd08a27    2026-02-22   EXP-011: Dashboard data generation  GO
—          2026-02-22   EXP-012: AIN1 recovery + connector   RESOLVED (SAADC latch + bad solder)
```

## Cross-Reference
- **Task Index**: `ai/memory-bank/tasks/INDEX.md`
- **Tasks informed by experiments**:
  - TASK-005 (OTA recovery tests) — validates EXP-004/EXP-007 recovery paths
  - TASK-007 (E2E test plan) — should include REC-005 field conditions
  - TASK-008 (OTA recovery docs) — documents EXP-007/EXP-008 recovery behavior
  - EXP-010 (RAK19001 pin validation) — informs REC-007 (post-reseat validation, now EXP-012), REC-008 (SAADC errata retention), REC-009 (button/pot validation)
  - EXP-011 (dashboard data generation) — discovered TASK-108 (timestamp field mismatch)
  - EXP-009b (Board #2 pot overcurrent) — documents hardware damage, naming confusion discovery. Board numbering corrected 2026-02-22.
  - EXP-012 (AIN1 recovery + connector validation) — resolved: Hypothesis A confirmed (SAADC latch), AIN1 recovered, connector fine (bad solder on header pins). Resolves TASK-104. RAK19001 header reflow needed before v1.1 features (TASK-095/096/097/098).

---

**Experiment Tracker**: Oliver
**Statistical Confidence**: Retrospective analysis — metrics quoted from commit messages and code artifacts. No controlled statistical trials were run; experiments were engineering feature flags with before/after comparisons. EXP-010 is a systematic hardware bring-up with deterministic pass/fail criteria (no statistical inference needed). EXP-011 is tooling validation (deterministic). EXP-012 is a planned physical validation with pass/fail criteria.
**Decision Impact**: All GO decisions shipped to production. One REVERT executed cleanly. One MECHANICAL FAILURE root-caused. EXP-012 resolved (Phase 1 complete, Phase 2 superseded, Phase 3 decided — remaining action is hardware rework only).
