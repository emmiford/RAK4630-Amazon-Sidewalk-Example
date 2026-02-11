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
```

## Cross-Reference
- **Malcolm's Task List**: `ai/memory-bank/tasks/rak-sid-tasklist.md`
- **Tasks informed by experiments**:
  - TASK-005 (OTA recovery tests) — validates EXP-004/EXP-007 recovery paths
  - TASK-007 (E2E test plan) — should include REC-005 field conditions
  - TASK-008 (OTA recovery docs) — documents EXP-007/EXP-008 recovery behavior

---

**Experiment Tracker**: Oliver
**Statistical Confidence**: Retrospective analysis — metrics quoted from commit messages and code artifacts. No controlled statistical trials were run; experiments were engineering feature flags with before/after comparisons.
**Decision Impact**: All GO decisions shipped to production. One REVERT executed cleanly.
