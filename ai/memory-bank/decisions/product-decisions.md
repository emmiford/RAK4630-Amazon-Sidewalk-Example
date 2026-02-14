# SideCharge Product Decision Log

Decisions about product direction, feature design, and architecture. Managed by the product owner. Referenced from PRD.md and task backlog.

## Format

Each decision has: ID, date, decision summary, context, alternatives considered, status.

---

## Decided

### PDL-001: Physical "Charge Now" button replaces double-tap J1772 override
- **Date**: 2026-02-12
- **Decision**: Use a physical button on the device for user override (force-charge when AC has priority), replacing the earlier concept of double-tapping the J1772 connector.
- **Context**: The double-tap concept required the user to go to the car, disconnect and reconnect the J1772 plug quickly. A physical button on the SideCharge device is more intuitive, doesn't require going to the car, and is a standard UX pattern.
- **Alternatives considered**: (1) Double-tap J1772 connector — rejected (requires going to car, easy to accidentally trigger, hard to communicate to users). (2) App/phone-based override — rejected for v1.0 (no user-facing app in scope).
- **Open questions**: Button placement on enclosure, momentary vs toggle, LED feedback on press, override duration (fixed timer? until AC calls? indefinite?), behavior when AC calls during active override.
- **Status**: DECIDED — implementation details TBD

### PDL-002: Keep both HW and SW interlock layers active in production (redundancy)
- **Date**: 2026-02-12
- **Decision**: Both the hardware circuit-level interlock and the software interlock remain active in production as redundant safety layers.
- **Context**: The hardware interlock prevents simultaneous AC and EV charger operation at the circuit level, independent of the microcontroller. The software interlock adds cloud override, "Charge Now" button, logging, and transition delay logic. The question was whether to rely on one layer or keep both.
- **Alternatives considered**: (1) HW-only in production, SW for development — rejected (loses cloud override and logging). (2) SW-only — rejected (single point of failure if firmware crashes).
- **Open questions**: None remaining. Transition delay is not needed (PDL-005 revised).
- **Status**: DECIDED

### PDL-003: Product name is "SideCharge"
- **Date**: 2026-02-11
- **Decision**: The product is named "SideCharge" (not "CircuitChomp" or other alternatives).
- **Context**: "CircuitChomp" was the working name from Eta Works. "SideCharge" references the Amazon Sidewalk connectivity and the EV charging use case.
- **Alternatives considered**: CircuitChomp — rejected (too playful for electrician audience, doesn't convey connectivity story).
- **Status**: DECIDED

### PDL-005: No transition delay needed — thermostat handles compressor protection
- **Date**: 2026-02-12 (revised)
- **Decision**: No transition delay mechanism in the SideCharge device. Modern thermostats have a built-in compressor protection timer (typically 5 minutes between cycles). Since SideCharge passes the thermostat's call signal through (or blocks it), the thermostat's existing protection remains in the loop.
- **Context**: Originally considered HW RC time constant on relay driver, SW timer, or both. On review, all three are unnecessary — the thermostat already prevents short-cycling, and SideCharge's interlock transitions are driven by thermostat calls (thermostat-timed), cloud commands (~15 min intervals), or user button presses (not rapid). Adding a delay mechanism introduces complexity and coordination risk for a problem already solved upstream.
- **Alternatives considered**: (1) HW RC + SW timer — rejected (oscillation risk). (2) HW RC only — rejected (unnecessary given thermostat protection). (3) SW timer only — rejected (unnecessary). (4) No delay — **selected**.
- **Status**: DECIDED

### PDL-004: Target audience includes electricians, homeowners, and tech people
- **Date**: 2026-02-11
- **Decision**: Three target audiences: (1) electricians/installers (primary — they buy and install), (2) homeowners (secondary — they benefit), (3) general tech people (for feedback and early adoption).
- **Status**: DECIDED

### PDL-006: Boot default is read-then-decide (not unconditional allow)
- **Date**: 2026-02-12
- **Decision**: On boot, the device reads the thermostat cool call GPIO before setting the charge enable state. If cool_call is HIGH (AC running), charge enable is set LOW (EV paused). If cool_call is LOW, charge enable is set HIGH (EV allowed).
- **Context**: The unconditional "allow on boot" default contradicts the HW interlock when AC is running at boot (e.g., after a power blip on a hot day). Read-then-decide ensures SW agrees with HW on every boot.
- **Alternatives considered**: (1) Unconditional allow — rejected (contradicts HW interlock when AC running). (2) Unconditional pause — rejected (blocks charging unnecessarily, requires cloud command to resume). (3) Read-then-decide — **selected**.
- **Edge cases**: Unconnected GPIOs (pull-downs → LOW → allow, correct for commissioning). Charge Now override lost on reboot (RAM-only, intentional). Cloud override lost on reboot (cloud re-sends on next scheduler cycle).
- **Implication**: `platform_gpio_init()` changes from `GPIO_OUTPUT_ACTIVE` to `GPIO_OUTPUT_INACTIVE`. `charge_control_init()` reads cool_call then sets GPIO.
- **Status**: DECIDED — implementation needed (PRD section 2.4.1)

### PDL-007: LED state matrix — priority-based, 1 LED prototype + 2 LED production
- **Date**: 2026-02-12
- **Decision**: LED patterns are assigned priorities (1=highest for error, 8=lowest for idle). Highest-priority active state determines LED pattern. Prototype: one green LED with 8 distinguishable blink patterns. Production: add one blue LED (2 total) — green for connectivity/health, blue for interlock/charging.
- **Context**: Device has no screen, no app, no UI in v1.0. LEDs are the entire UX. Installer needs to verify power, interlock response, and connectivity. Homeowner needs "everything is fine" signal.
- **Alternatives considered**: (1) Single LED for production — rejected (too many states overloaded). (2) Three LEDs (add red for errors) — rejected (BOM cost; rapid flash on green is sufficient for errors). (3) RGB LED — rejected (color-blind accessibility issue).
- **BOM impact**: One additional blue LED (0603 SMD), one resistor, one GPIO. Minimal.
- **Status**: DECIDED — implementation needed (PRD section 2.5)

### PDL-008: Regulatory risk acknowledged — UL certification not addressed in v1.0
- **Date**: 2026-02-12
- **Decision**: UL listing (or equivalent NRTL certification) is required for production sales through the electrician channel, but is not addressed in v1.0 scope. This is the biggest non-technical risk to the product.
- **Context**: The first electrician to pull a permit for a SideCharge installation will be asked "Is this device UL listed?" Without UL listing, no permit, no legal installation, no sales. The low-voltage design should simplify certification, but no NRTL engagement has occurred. A pre-submission meeting ($2K-$5K, 2-4 weeks) should happen before PCB design (TASK-019), as UL feedback may require design changes.
- **Action**: None in v1.0. Log the risk. Address when approaching production.
- **Status**: ACKNOWLEDGED — not addressed

### PDL-009: "Charge Now" override duration is 30 minutes
- **Date**: 2026-02-12
- **Decision**: Override lasts 30 minutes. User can press again for another 30 minutes.
- **Context**: 30 minutes provides ~3.6 kWh at 7.2 kW (15-20 miles of range) without leaving AC blocked for an extended period. Encourages conscious decision-making. See PRD 2.0.1.1 for the full options analysis (1 hr, 2 hr, 4 hr, until-AC-calls were considered).
- **Still open**: Thermal comfort auto-cancel threshold (should override auto-cancel if thermostat calls continuously for 30+ min?), button placement on enclosure.
- **Status**: DECIDED — thermal safeguard and placement still open

### PDL-010: 15-minute heartbeat + state-change uplink with device-side timestamps
- **Date**: 2026-02-12
- **Decision**: Device uplinks every 15 minutes AND on state change. Each uplink carries a 4-byte SideCharge epoch timestamp (seconds since 2026-01-01). Cloud ACKs with a watermark timestamp so device can trim its event buffer.
- **Context**: 15-minute heartbeat provides reliable offline detection (worst case 30 min). State-change uplinks capture transitions at high resolution during field validation. Device maintains a ring buffer (~50 entries) and trims on cloud ACK. Pattern is a simplified TCP cumulative ACK with sliding window.
- **Note**: On-state-change uplink may be removed in a future version — it is redundant with the heartbeat, could balloon during rapid transitions, and doesn't help detect offline devices.
- **Status**: DECIDED — implementation needed (PRD sections 3.2, 3.3)

### PDL-011: TIME_SYNC downlink gives device wall-clock time
- **Date**: 2026-02-12
- **Decision**: New downlink command (0x30) carries current SideCharge epoch time + ACK watermark. Sent on first uplink after boot and periodically (daily) to correct drift. Device computes time as sync_time + uptime offset. nRF52840 RTC drift is ~100 ppm (~8.6 sec/day), well within 5-minute accuracy target.
- **Status**: DECIDED — implementation needed (PRD section 3.3)

### PDL-013: Demand response uses delay windows, not pause/allow commands
- **Date**: 2026-02-12
- **Decision**: The cloud sends time-bounded delay windows `[start_time, end_time]` instead of real-time pause/allow commands. The device stores the window and manages transitions autonomously — when the window expires, charging resumes with no follow-up command needed.
- **Context**: The previous command-based approach has a critical flaw: if the "allow" command is lost over LoRa, the device stays paused indefinitely. Delay windows are self-resolving — the worst case for a lost downlink is that charging happens during a period it shouldn't (missed delay), not that charging is blocked forever (missed allow).
- **Alternatives considered**: (1) Pause/allow commands — rejected (lost "allow" = indefinite block). (2) Pause command with auto-resume timer — works but requires guessing timer duration; delay window is cleaner. (3) On-device schedule storage (full weekly TOU table) — over-engineered for v1.0; one active window at a time is sufficient.
- **Implication**: Charge control downlink (0x10) changes from 4 bytes (cmd + allow/pause + reserved) to 10 bytes (cmd + subcommand + start + end). Charge scheduler Lambda must compute window times instead of binary pause/allow decisions.
- **Status**: DECIDED — implementation needed (PRD sections 3.3, 4.4)

### PDL-009a: "Charge Now" override is 30 minutes for everything (revised)
- **Date**: 2026-02-12 (revised 2026-02-12)
- **Decision**: Charge Now overrides AC priority AND cancels any active delay window (TOU/MOER) for **30 minutes**. One timer, both overrides. After 30 minutes, everything returns to normal — AC priority restored, demand response honored. User can press again for another 30 minutes.
- **Context**: Previous design had two different durations (AC=30 min timer, demand response=session-based until car state changes). Emily simplified: 30 minutes period, then back to normal. Simple and predictable.
- **Early cancellation**: Unplugging the car or car reaching full cancels the override immediately.
- **Thermal safeguard**: The 30-minute hard limit IS the safeguard. No auto-cancel based on continuous cool call. The user made their choice; the timer protects them.
- **Status**: DECIDED — implementation needed (PRD sections 2.0.1.1, 4.4.5)

### PDL-012: Device registry with short device ID and installer-provided location
- **Date**: 2026-02-12
- **Decision**: DynamoDB device registry table (`sidecharge-device-registry`) tracks ownership, installation location, firmware version, and liveness. Device ID format is `SC-` + first 8 hex chars of SHA-256(Sidewalk UUID) — short enough for labels and phone support, unique enough for any fleet size. Location is installer-provided (address and optional lat/lon) since the RAK4631 has no GPS or location hardware.
- **Context**: Need to track which customer owns which device, where it's installed, and its current state. The Sidewalk UUID is a 36-character string — too long for labels, support calls, and UI. A derived short ID solves this while maintaining a 1:1 mapping to the Sidewalk identity.
- **Alternatives considered**: (1) Use Sidewalk UUID directly — rejected (too long for human use). (2) Sequential serial numbers — rejected (requires centralized counter, no cryptographic relationship to device identity). (3) Random short codes — rejected (collision risk without checking, no derivation from device identity). (4) GPS module on device — rejected (BOM cost, power, antenna size, unnecessary for fixed installation).
- **Status**: DECIDED — implementation needed (PRD section 4.6, TASK-036)

### PDL-014: Current clamp range is 48A for v1.0
- **Date**: 2026-02-12
- **Decision**: v1.0 target is 0-48A (covers a 60A circuit at 80% continuous). Higher ranges (80A for Ford Charge Station Pro) are a future iteration — different clamp and resistor divider, no firmware architecture change.
- **Status**: DECIDED — implementation needed (TASK-026)

### PDL-015: AC cloud override is out of v1.0 scope
- **Date**: 2026-02-12
- **Decision**: Cloud-initiated AC pause is architecturally possible but deferred. Requires safety guardrails (temperature limits, duration caps, homeowner consent) that are not designed.
- **Status**: DECIDED — deferred to future version

### PDL-016: BLE diagnostics is a hard no
- **Date**: 2026-02-12
- **Decision**: BLE stays disabled post-registration. No BLE diagnostics beacon, no BLE re-activation via button press. Production diagnostics are cloud-only.
- **Context**: Emily's explicit decision. BLE disabled post-registration is a security feature (PRD 6.3.2). Re-enabling it, even temporarily, reopens the local attack surface.
- **Status**: DECIDED

### PDL-017: Charge Now button press events are logged
- **Date**: 2026-02-12
- **Decision**: Button press events are reported in the next uplink. CHARGE_NOW flag (bit 3) in thermostat byte is already allocated.
- **Status**: DECIDED — implementation needed (PRD section 2.0.6)

---

## Open

### ~~PDL-OPEN-004: Production diagnostics story~~ → Partially resolved
- **Resolution**: BLE diagnostics is a hard no — BLE stays disabled post-registration. Production diagnostics are cloud-only: Tier 1 (CloudWatch alerting, with alarms initially disabled until first field install) for v1.0. Tier 2 (remote status query downlink 0x40) is a natural v1.1 feature. No debug header.
- **See**: TASK-029, PRD section 5.3.3

### ~~PDL-OPEN-005: Production heartbeat interval~~ → Resolved as PDL-010
- **Resolution**: 15 minutes. See PDL-010.

### ~~PDL-OPEN-006: Transition delay implementation~~ → Resolved as PDL-005
- **Resolution**: Hardware-only. See PDL-005.

### PDL-OPEN-007: Production commissioning process
- **Question**: LED-based (decided via PDL-007), but details of printed checklist card, device label, QR code TBD.
- **Partially resolved**: LED commissioning sequence defined in PRD 2.5.1. Electrician installation requirements defined in PRD 1.4.
- **Still open**: Printed card content, device label design, QR code destination URL.
- **See**: TASK-024, PRD section 1.4
