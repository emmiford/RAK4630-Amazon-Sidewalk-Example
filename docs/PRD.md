# SideCharge Product Requirements -- v1.6

**Status**: Active -- v1.6: inline pass-through wiring architecture (Y-in/Y-out, W-in/W-out, PILOT-in/PILOT-out replaces single-terminal tap design); heat pump support via W-in/W-out terminals and W-out relay (v1.0 hardware, v1.1 firmware); three relays (charge block, Y-out pass-through, W-out pass-through); 11 terminals; heat call flag (bit 0) defined in uplink; commissioning sequence updated for pass-through wiring; PCB spec updated for three relays and inline architecture. Previous: terminology aligned with lexicon (docs/lexicon.md); commissioning test sequence, self-test/fault detection, installation failure modes (sections 2.5.2-2.5.4); uplink fault flags (byte 7 bits 4-7); self-test 5-press trigger implemented with 500ms polling limitation documented (TASK-040 done, TASK-049 scoped); G terminal redefined as earth ground from compressor junction box (not fan wire), wiring terminal definitions added (2.0.3.1), EVSE connector selection guidance added (2.2.1), TASK-039/041 statuses updated to done (TASK-044)
**Hardware**: RAK4631 (nRF52840 + Semtech SX1262 LoRa)
**Connectivity**: Amazon Sidewalk (LoRa 915MHz) -- no WiFi, no cellular, no monthly fees
**Cloud**: AWS (Lambda, DynamoDB, S3, IoT Wireless, EventBridge)

### Status Convention

Requirements use these status labels to distinguish software vs. hardware implementation:

| Status | Meaning |
|--------|---------|
| `IMPLEMENTED (SW)` | Working in firmware/software on the dev prototype |
| `IMPLEMENTED (HW)` | Working in hardware on the dev prototype |
| `IMPLEMENTED (SW+HW)` | Working in both firmware and hardware |
| `IMPLEMENTED` | Platform/cloud — not a HW/SW distinction |
| `DESIGNED` | Design decided but not yet built or verified |
| `TBD` | Decision not yet made — requires testing or PM input |
| `NOT STARTED` | Not yet designed or implemented |
| `N/A` | Not applicable |

### Terminology

This document uses consistent terms (see `docs/lexicon.md` for the full reference):

- **AC** (not "HVAC") when referring to the air conditioning compressor and its circuit. Heat pump support is a v1.1 firmware goal, but the v1.0 PCB and wiring are designed to support it from day one (W-in/W-out terminals, heat call relay, heat call GPIO wired) so no board respin is needed.
- **EV charger** (not "EVSE") in user-facing context. "EVSE" appears only in J1772 technical sections.
- **Interlock** — the hardware + software system that prevents simultaneous operation of the compressor and EV charger. In v1.0, the interlock triggers on the cool call signal. In v1.1, it triggers on cool call OR heat call, blocking the compressor regardless of heating or cooling mode.
- **Cool call** — the thermostat signal requesting cooling (Y wire). Use "cool call" in spec/code context, "AC call" in user-facing context. Avoid "thermostat call" (ambiguous), "HVAC call", "AC demand" (as a signal name).
- **Heat call** — the thermostat signal requesting heating (W wire). Relevant for heat pump systems where the same compressor serves both heating and cooling. The production PCB wires the heat call GPIO (P0.04) and the W pass-through relay; the heat call GPIO is not connected on the WisBlock prototype (no available pin). Firmware reading and reporting is v1.1 scope.
- **Pause** / **allow** — the canonical verbs for stopping and permitting EV charging in user-facing and product context. Avoid "disable" (implies broken), "enable" (ambiguous with GPIO). The GPIO pin is named `charge_block` (HIGH = block, LOW = not blocking) — "block" is the correct technical term for the pin, while "pause" is the user-facing equivalent.
- **AC priority** — the default operating mode: when the thermostat calls for cooling, EV charging pauses.
- **Cloud override** — a cloud-initiated charge control command. Avoid "force-block", "remote override".
- **Charge Now** — physical button override. Overrides AC priority for 30 minutes and suspends any active delay window until the car's J1772 state changes (session-based). Avoid "force charge", "manual override", "user override".
- **TOU** (Time-of-Use) — utility rate structure where electricity costs more during peak demand periods (typically weekday afternoons/evenings) and less during off-peak hours. SideCharge delays EV charging during TOU peak windows to reduce cost.
- **Delay window** — a time range `[start, end]` during which the device should not charge. Sent by the cloud as a charge control downlink. The device manages transitions autonomously — no follow-up "allow" command needed.

### Low-Voltage Design Principle

SideCharge touches only low-voltage control wires: 24VAC thermostat signals and the J1772 pilot signal (+/-12V). The single exception is one current clamp on the 240V charging circuit (read-only, no electrical connection). We do not switch, interrupt, or connect to 240V mains. This is a deliberate design choice: by staying entirely in the low-voltage domain, we expect to avoid UL testing at 220V.

---

## 1. Product Overview

### 1.1 Why SideCharge Exists

SideCharge exists because the electrical infrastructure in most American homes was not built for electric vehicles.

A Level 2 EV charger draws 30-50A at 240V. Many homes don't have room in their electrical panel for a dedicated charger circuit -- this is most common with 100A panels, but a 200A panel that already serves a hot tub, workshop, or other large loads can be just as constrained. Installing a charger in these homes typically requires a panel upgrade -- $2,400+ in parts and labor, 3+ days of work, and often a utility service upgrade on top of that. This is the single largest barrier to residential EV adoption.

The insight: in a typical home with central air conditioning, the AC compressor draws 30-50A -- nearly identical to a Level 2 charger. But they don't need to run at the same time. On a peak summer day (95°F+) in Colorado, the AC compressor may operate 14-18 hours total, with duty cycles reaching 80-100% during afternoon peak hours (2-8 PM). But Colorado nights cool to the low 60s even in summer, so overnight AC demand drops to near zero -- and that's exactly when EVs charge. A daily commute (30 miles) replenishes in 1-2 hours on a 30A Level 2 charger, and even a full 0-100% charge completes in 6-10 hours -- well within a typical 10 PM - 6 AM overnight window when the AC compressor is mostly idle.

The interlock matters most during shoulder hours (6-9 PM, 7-9 AM) when both loads may contend. An interlock that prevents simultaneous operation lets the charger share the AC circuit safely and legally.

**The numbers**:
- A significant share of homes with central A/C have panels at or near capacity -- primarily 100A panels, but also loaded 200A panels. The exact addressable market percentage depends on regional panel age, housing stock, and existing load profiles, and has not been rigorously quantified.
- The problem is more acute in disadvantaged communities, where panel upgrade costs are a harder barrier to EV adoption.
- Panel upgrade: $2,400+ and 3+ days. SideCharge target: ~$1,000 equipment and installation.
- SCE offers $4,200 grants for 100A panel upgrades -- SideCharge achieves the same outcome at a fraction of the cost.
- [NEC 220.60](https://up.codes/s/noncoincident-loads) (noncoincident loads) and [NEC 220.70 / Article 750](https://blog.se.com/homes/2025/06/09/charging-ahead-managing-ev-load-demand-without-breaking-the-bank/) (energy management systems) explicitly allow interlocked and managed loads on shared circuits.

**Why Level 2 matters**: At 30A/240V (~7.2 kW), a daily commute replenishes in 1-2 hours. At 15A/120V (~1.4 kW, Level 1), the same commute takes 5-10 hours -- and for larger vehicles like the F-150 Lightning, a full charge is physically impossible overnight. A 20A/240V circuit could theoretically work, but the modest speed increase over Level 1 doesn't justify the cost of the interlock installation. SideCharge targets 30-50A Level 2 circuits where the charging speed makes the product worthwhile.

**Grid value**: Every SideCharge installation adds a Level 2 charger without increasing peak demand on the distribution grid. No service upgrade, no transformer upgrade, no utility capital expenditure. Combined with the demand response layer (TOU + MOER), the device also provides load flexibility -- the utility can shift both AC and EV loads in response to grid conditions.

**Company**: Eta Works, Inc.

### 1.2 What SideCharge Does

SideCharge is a circuit interlock that enables same-day, code-compliant installation of a Level 2 EV charger in homes without enough room in their electrical panel for a dedicated charger circuit. It ensures the AC compressor and EV charger never draw power simultaneously -- mutual exclusion enforced in hardware and software -- so a 30-50A charger can share the existing AC circuit without a panel upgrade. This applies to any panel that's at capacity, whether it's a 100A panel or a 200A panel that's already loaded up with a hot tub and other large loads.

The interlock is the product. Everything else is built on top of it.

Connected via Amazon Sidewalk's free LoRa mesh network, SideCharge reads J1772 pilot state, charging current, and thermostat call signals (cool call in v1.0, cool call + heat call in v1.1), reporting them to the cloud in a 12-byte payload that fits within Sidewalk's 19-byte LoRa MTU. The cloud layer adds smart coordination: time-of-use pricing, real-time grid carbon intensity, and cloud override -- sending delay windows and commands back down over the same radio link.

The entire application is a compact 4KB binary running on a split-image architecture. The platform handles Sidewalk, BLE, and OTA. The app handles everything EV charger and AC related. They update independently -- and the app updates over the air in minutes, not hours, thanks to delta OTA.

### 1.3 Target Deployment

Single-site residential installation in a home with central air conditioning and a panel that doesn't have room for a dedicated EV charger circuit. The SideCharge device sits at the junction of two systems:

- **Thermostat side**: Sits inline on the thermostat call wires between the thermostat and the compressor contactor. The cool call (Y) and heat call (W) wires are broken at the device -- signals enter on Y-in/W-in and pass through to the compressor on Y-out/W-out. The device blocks either signal when EV charging has priority. The device is powered from the 24VAC transformer in the AC system (R and C terminals) -- no separate power supply needed.
- **EV charger side**: Sits inline on the J1772 pilot wire between the EVSE and the vehicle connector. The pilot signal enters on PILOT-in and passes through to the vehicle on PILOT-out. To pause charging, the device inserts ~900 ohm resistance on the PILOT-out side, making the charger see J1772 State B (see 2.0.1 for the asymmetric interlock mechanism). A current clamp on the charging circuit measures real-time current delivery.

All connections are low-voltage control wires (24VAC thermostat, +/-12V J1772 pilot). The only contact with the 240V circuit is a read-only current clamp. See section 2.0.4 for isolation details.

Physically, this is a small device that mounts just below the EV charger, wrapping around the input leg of the charger's circuit. This location provides access to the 240V conductors for the current clamp, the J1772 pilot signal from the charger, and a convenient run back to the thermostat wiring.

### 1.4 Who Uses It

**Homeowner**: Passive -- benefits from Level 2 charging capability and demand response without interaction. No user-facing app or dashboard in v1.0. The charger just works, and the AC always has priority. When the interlock activates (AC calls for cooling while the car is charging), the homeowner notices that the car stops charging and the AC turns on -- no action required. When the AC cycle ends, charging resumes automatically. The only active interaction is the "Charge Now" button (see 2.0.1.1).

**Electrician / Installer**: This is the primary buyer. The electrician installs the Level 2 EV charger and the SideCharge interlock as a single job. The EV charger installation itself follows standard practice per NEC Article 625, **except** that the charger is wired as a branch circuit modification on the existing AC compressor circuit rather than on a new dedicated circuit from the panel.

The installation requires:

- **Branch circuit modification**: The existing 240V branch circuit serving the AC compressor is extended to also serve the EV charger. Both loads share the same circuit breaker (typically 40A or 50A, 2-pole). A junction box is installed at a convenient point on the existing circuit run where the conductors split to serve both loads.
- **Conductors**: Two ungrounded conductors (L1/L2) and an equipment grounding conductor (EGC). No neutral is required for a 240V-only circuit. Wire gauge per NEC 240.4(D): #8 AWG copper minimum for 40A circuits, #6 AWG copper for 50A circuits. All conductors from the panel to the junction box, and from the junction box to each load, must be sized for the circuit breaker rating.
- **Conduit**: EMT (electrical metallic tubing) for main runs, FMC (flexible metal conduit) for the final connection to each appliance. Conduit sizing per NEC Chapter 9 for the conductor fill.
- **SideCharge device**: Mounts at or near the junction box. Low-voltage thermostat wires (18-22 AWG, typically thermostat cable) pass through the device inline -- the Y (cool call) and W (heat call) wires are broken at the device, entering on Y-in/W-in and exiting on Y-out/W-out to the compressor contactor. The J1772 pilot signal also passes through inline (PILOT-in from the EVSE, PILOT-out to the vehicle connector). One current clamp wraps around one of the ungrounded conductors on the EV charger leg.
- **Interlock**: The SideCharge device ensures the AC compressor and EV charger never operate simultaneously, satisfying NEC 220.60 (noncoincident loads) for load calculation purposes.

The electrician verifies correct operation using the LED-based commissioning sequence (see section 2.5.1). A printed commissioning checklist card is included in the box. No phone app or cloud access is required during installation.

Wiring diagrams, terminal specifications, and a step-by-step installation guide will ship with the product. These are **not yet created** -- they are a deliverable of the PCB design task (TASK-019).

**Operator/Developer**: Uses `ota_deploy.py` for firmware updates and AWS console/CLI for cloud monitoring. Note: USB serial is available during development but will not be accessible after the device leaves the factory -- all post-deployment diagnostics happen via the cloud. This is who the rest of this document is written for.

**Utilities (stakeholder)**: Benefit from charger deployments that add no peak demand. Potential demand response participants via the cloud layer. SCE, PG&E, and similar IOUs are the policy and incentive partners -- they do not interact with the device directly, but their rate structures and grid programs shape the cloud logic.

---

## 2. Device Requirements

### 2.0 Circuit Interlock

This is the core function of SideCharge -- the reason the product exists. The interlock guarantees that the AC compressor and EV charger never draw power at the same time, enabling a Level 2 charger installation on a panel that would otherwise require an expensive upgrade.

#### 2.0.1 Interlock Logic

| Requirement | Status |
|-------------|--------|
| Mutual exclusion: compressor and EV charger loads never operate simultaneously | IMPLEMENTED (SW+HW) |
| AC priority: if thermostat calls for cooling OR heating, EV charging pauses | IMPLEMENTED (SW+HW) (cool call v1.0; heat call v1.1 firmware, v1.0 hardware) |
| EV lockout: if current is flowing to EV charger, thermostat signal to compressor is blocked (both Y-out and W-out interrupted) | IMPLEMENTED (SW+HW) |
| "Charge Now" button: user presses physical momentary button to override AC priority and charge for a limited duration | DESIGNED (see 2.0.1.1 for duration options) |
| Compressor short-cycle protection: thermostat's built-in protection timer (no additional delay needed) | N/A — handled by thermostat |
| Default state on boot: read thermostat, then decide (see 2.4.1) | IMPLEMENTED (SW+HW) (TASK-065) |

**Asymmetric interlock mechanism**: SideCharge does not simply cut power to either load. The mechanism is asymmetric and protocol-aware:

- **To pause EV charging**: Two mechanisms, used for different triggers:
  - *Local interlock* (compressor needs priority): The device inserts ~900 ohm resistance on the PILOT-out line (between the device and the vehicle connector), making the charger see J1772 State B (connected, not ready). The charger stops supplying power gracefully. The PILOT-in line continues to carry the charger's signal to the device for monitoring.
  - *Cloud/utility override*: Set the J1772 PWM duty cycle to 0%, telling the car that no current is available. **This approach is not finalized** -- it needs testing with multiple car makes to verify behavior. See Oliver EXP-001.
- **To pause compressor** (when EV is charging): The device blocks the 24VAC thermostat call signals by opening the Y-out and W-out pass-through relays. Both cool call and heat call are interrupted -- the compressor cannot run in either heating or cooling mode while the EV charger is active. This is a v1.0 hardware requirement: even though firmware reading of the heat call GPIO is v1.1 scope, the W-out relay must be wired and controlled by the hardware interlock from day one so that heat pump installations are safe without a board respin.

The interlock is implemented in both hardware and software -- a deliberate redundancy choice. The hardware circuit enforces mutual exclusion independently of the microcontroller, so basic interlock function works even if the firmware crashes. The software layer adds the smarts: cloud override, Charge Now button, logging, and boot-time thermostat reading. Both layers stay active in production.

AC always wins by default -- if the thermostat calls, charging pauses. The "Charge Now" button (physical momentary, see PDL-001) lets the user override AC priority for a limited duration. See section 2.0.1.1 for duration options.

**Compressor short-cycle protection**: Modern thermostats (Nest, Ecobee, Honeywell, etc.) include a built-in compressor protection timer -- typically 5 minutes between cycles. SideCharge sits downstream of the thermostat: the thermostat's call signal enters on Y-in (or W-in for heat), and SideCharge passes it through to Y-out (or W-out) or blocks it. The thermostat's built-in protection timer is upstream of the break, so it remains in the loop regardless. No additional transition delay mechanism is needed in the SideCharge device.

#### 2.0.1.1 "Charge Now" Override: Duration Options

The "Charge Now" button is a physical momentary push button (not toggle) on the device enclosure. When pressed, it overrides two things:

1. **AC priority override**: EV charging takes priority over AC for **30 minutes**. The AC compressor call signal is blocked. When the 30-minute timer expires, any pending AC call is immediately honored. The user can press again for another 30 minutes.

2. **Demand response cancellation**: Any active delay window (TOU peak, MOER curtailment — see section 4.4) is **cancelled entirely**, not just paused. The user is opting out of demand response for the remainder of this peak window. After the 30-minute AC override expires, normal load-sharing resumes (AC has priority, EVSE yields when AC is on) — but the scheduler's delay window is not reinstated. See section 4.4.5 for the cloud-side protocol.

Both overrides start from the same button press but have different lifetimes:
- **AC priority override**: 30 minutes, then AC priority restored
- **Demand response cancellation**: remainder of the current peak window (e.g., until 9 PM for TOU)

**Early cancellation**: Unplugging the car cancels both overrides immediately. The car reaching full cancels both overrides. This ensures AC isn't unduly blocked after the car is done charging.

**Duration options considered** (30 minutes selected — see PDL-009):

| Duration | Pros | Cons |
|----------|------|------|
| **30 minutes** (current recommendation) | Short enough to limit thermal discomfort. Simple mental model ("half hour of charging"). Encourages conscious decision-making -- user must actively re-press. | May be too short for a meaningful charge session on some vehicles. User must re-press for longer charges, which could be annoying. |
| **1 hour** | Enough for ~7 kWh / 25-30 miles at 30A. Reasonable balance between charging utility and AC downtime. | On a 100°F day, one hour without AC could be uncomfortable. |
| **2 hours** | Enough for a substantial charge (~14 kWh / 50+ miles at 30A). Matches a typical AC compressor off-cycle in moderate weather. | Risks real thermal discomfort in extreme heat. Home may heat up significantly. |
| **4 hours** | Covers nearly a full charge for most EVs at 30A. Minimizes button presses. | Unacceptable thermal risk on hot days. If a vulnerable person is home (elderly, infant), 4 hours without AC in summer heat is a safety concern. |
| **Until AC calls** | Charges as long as AC doesn't need to run. Automatically yields when cooling is needed. Dynamic and weather-aware. | Unpredictable duration -- user doesn't know how long they'll get. On a cool day, could mean hours; on a hot day, could mean minutes. Hard to communicate expectations. |
| **Indefinite** (rejected) | Maximum charging flexibility. | Effectively disables the interlock. Defeats the purpose of the product. Rejected. |

**Additional considerations:**
- ~~**Thermal comfort safeguard**~~: RESOLVED — the 30-minute hard limit is the safeguard. No auto-cancel based on continuous cool call. The override is 30 minutes, period.
- **LED feedback**: The amber/blue LED shows override status (see section 2.5). When override expires or is cancelled, the LED returns to the normal interlock pattern.
- **Button interaction**: Single press = activate override (TASK-048). Long press (3 seconds) = cancel override early. Long press (10 seconds) = activate BLE diagnostics beacon (see production diagnostics). 5 presses within 5 seconds = trigger on-demand self-test (TASK-040, implemented). All button detection uses 500ms GPIO polling in v1.0; TASK-049 would add interrupt-driven detection for faster response.
- **Cloud reporting**: Override activation and cancellation are reported in the next uplink.
- **Power loss**: Override state is RAM-only. Power cycle = override lost, safe default restored. This is intentional.

#### 2.0.2 Cloud Override

The cloud can force-pause EV charging via the charge control downlink (0x10). Each pause command includes a configurable timeout — if the cloud goes silent (lost connectivity, Lambda failure), charging automatically resumes after the timeout expires, so a cloud outage never permanently blocks charging.

Cloud-initiated AC pause is architecturally possible but **out of scope for v1.0**. Pausing a compressor remotely on a hot day has comfort and safety implications that require careful design (temperature limits, duration caps, homeowner consent). This is a future feature.

| Requirement | Status |
|-------------|--------|
| Pause EV charging via cloud downlink | IMPLEMENTED (SW) |
| Pause AC via cloud downlink | Out of v1.0 scope |
| Override timeout: auto-release after configurable duration | IMPLEMENTED (SW, EV charger only) |

#### 2.0.6 Logging and Reporting

| Requirement | Status |
|-------------|--------|
| Interlock state reported in uplink payload | IMPLEMENTED (SW) |
| Cloud override status reported in uplink payload | IMPLEMENTED (SW, EV charger only) |
| Interlock transition events logged to cloud (AC→EV, EV→AC, override) | IMPLEMENTED (SW) (TASK-069) |
| "Charge Now" button press events reported in next uplink | IMPLEMENTED (SW) (TASK-048b — FLAG_CHARGE_NOW in uplink bit 3) |

Logging is how the cloud knows what the interlock is doing. The current uplink payload includes interlock state, charge_control flags (including FLAG_CHARGE_NOW), and fault flags. Interlock transition events with reason codes are logged via the event buffer (TASK-069). This data supports demand response reporting, debugging, and demonstrating code compliance to inspectors.

#### 2.0.3 Hardware Interfaces

The device has 4 inputs (2 analog, 2 digital), 2 outputs, and 1 power input. The prototype is USB-powered; production will use 24VAC from the thermostat transformer.

**Inputs**

| Pin | Signal | Description | Status |
|-----|--------|-------------|--------|
| AIN7 | J1772 Cp voltage | Pilot voltage level — classifies car presence/state (A-F) | IMPLEMENTED (HW+SW) |
| AIN7 | J1772 Cp PWM | Pilot duty cycle — encodes max allowed current per J1772 | NOT STARTED |
| — | Current clamp | Not available on WisBlock prototype (no second analog pin). Production PCB will use dedicated ADC channel | NOT AVAILABLE (WisBlock) |
| P1.02 (IO2) | Cool call | Thermostat cooling request from Y-in (active high, pull-down) | IMPLEMENTED (HW+SW) |
| P0.04 | Heat call | Not connected on WisBlock prototype. Production PCB will use dedicated GPIO | DESIGNED (production PCB) |

**Outputs**

| Pin | Signal | Description | Status |
|-----|--------|-------------|--------|
| P0.17 (IO1) | Charge block | Controls EV charging via PILOT-out — inserts ~900 ohm resistance on the PILOT-out line when HIGH (blocking), making charger see State B (see 2.0.1 asymmetric mechanism) | IMPLEMENTED (HW) |
| TBD | Y-out relay | Pass-through relay on cool call: when open, blocks Y-in signal from reaching compressor contactor via Y-out | IMPLEMENTED (HW) |
| TBD | W-out relay | Pass-through relay on heat call: when open, blocks W-in signal from reaching compressor contactor via W-out. Controlled by hardware interlock in v1.0; firmware control in v1.1 | DESIGNED (HW v1.0) |

**Power**

| Source | Description | Status |
|--------|-------------|--------|
| USB-C | RAK4631 USB port (development only) | IMPLEMENTED |
| 24VAC | AC system transformer — production power source | NOT STARTED |

AIN7 serves double duty: the same pin reads both the Cp voltage level (implemented) and the Cp PWM duty cycle (not yet implemented). The voltage level tells us the car's state; the duty cycle tells us the maximum current the charger is offering. These are two different measurements of the same physical signal.

#### 2.0.3.1 Wiring Terminal Definitions (Installer-Facing)

These are the terminal labels on the SideCharge device as the electrician sees them during installation. The device sits inline on the thermostat call wires and the J1772 pilot wire -- signals pass through the device rather than being tapped. This pass-through architecture is what enables the interlock: the device can block a signal by opening the relay between the "in" and "out" terminals.

**Terminal count: 11** -- two power (R, C), four thermostat pass-through (Y-in, Y-out, W-in, W-out), one earth ground (G), two pilot pass-through (PILOT-in, PILOT-out), two current clamp (CT+, CT-).

| Terminal | Wire Source | Signal | Description |
|----------|------------|--------|-------------|
| **R** | Thermostat (air handler) | 24VAC hot | Production power source -- always connected to the HVAC transformer |
| **C** | Thermostat (air handler) | 24VAC common | Power return -- completes the 24VAC circuit back to the transformer |
| **Y-in** | Thermostat (air handler) | Cool call input | Signal from thermostat -- goes HIGH (24VAC present) when the thermostat calls for cooling. The device reads this signal (GPIO P1.02) and decides whether to pass it through to Y-out |
| **Y-out** | To compressor contactor | Cool call output | Signal to compressor -- when the Y-out relay is closed, the cool call passes through to energize the compressor contactor. When the relay is open (EV charging has priority), the cool call is blocked and the compressor does not start. Primary interlock trigger in v1.0 |
| **W-in** | Thermostat (air handler) | Heat call input | Signal from thermostat -- goes HIGH (24VAC present) when the thermostat calls for heating. Relevant for heat pump systems where the same compressor serves both heating and cooling. Not connected on WisBlock prototype; production PCB will read this signal (GPIO P0.04) in v1.1 firmware. The hardware interlock blocks W-out in v1.0 regardless |
| **W-out** | To compressor contactor | Heat call output | Signal to compressor -- when the W-out relay is closed, the heat call passes through. When the relay is open (EV charging has priority), the heat call is blocked. **This relay is wired and controlled by the hardware interlock in v1.0** even though firmware reading of W-in is v1.1 scope -- this ensures heat pump installations are safe from day one without a board respin |
| **G** | AC compressor junction box | Earth ground | Reference ground from the equipment grounding conductor (EGC) at the compressor's junction box ground screw. **This is NOT the thermostat fan wire.** The G terminal on a standard thermostat controls the indoor fan -- SideCharge repurposes this terminal for earth ground because the device needs a ground reference and does not control the indoor fan. The fan wire from the thermostat bundle is not connected to SideCharge; it either remains connected directly to the air handler (normal operation) or is capped if not needed |
| **PILOT-in** | EV charger (EVSE) | J1772 Cp input | Pilot signal from the charger -- 1 kHz +/-12V square wave from the charger's control pilot pin. The device reads this signal (ADC AIN7) to detect car presence and charging state. The signal passes through to PILOT-out when charging is allowed |
| **PILOT-out** | To vehicle connector | J1772 Cp output | Pilot signal to the vehicle -- in normal operation, the EVSE's pilot signal passes through unmodified. When the interlock pauses charging, the device inserts ~900 ohm resistance on this line, making the charger see State B (connected, not ready). This is the inline spoof mechanism described in 2.0.1 |
| **CT+/CT-** | Current clamp leads | Analog current | Two-wire analog input from the current clamp wrapped around one 240V conductor on the EV charger leg. Voltage proportional to charging current (0-3.3V = 0-48A target range) |

**Why G = earth ground, not fan**: A standard thermostat cable bundles R, Y, C, G, and W wires in a single sheath. In a typical HVAC installation, the G wire controls the indoor blower fan. SideCharge does not need or use the fan signal -- the indoor fan continues to operate normally under thermostat control, independent of SideCharge. Instead, the G terminal on the SideCharge device accepts an earth ground conductor from the AC compressor's outdoor junction box. The installer routes this ground wire from the compressor's ground screw to the SideCharge G terminal. This ground reference is used for signal integrity and fault detection, not for safety grounding of the device enclosure (which is handled by the branch circuit EGC per NEC 250).

**Thermostat cable wiring**: The standard 5-conductor thermostat cable (R, Y, C, G, W) carries four of the signals SideCharge needs. R and C connect to the device's power terminals. The Y wire is cut and the two ends connect to Y-in (thermostat side) and Y-out (compressor side). The W wire is cut similarly and connects to W-in and W-out. The G wire in the thermostat cable is not used by SideCharge -- the installer caps it or leaves it connected directly through to the air handler. The SideCharge G terminal receives a separate earth ground conductor from the compressor junction box, not the thermostat cable's G wire.

**Pilot wire wiring**: The J1772 pilot wire between the EVSE and the vehicle connector is cut. The EVSE side connects to PILOT-in; the vehicle connector side connects to PILOT-out. This replaces the previous "tap" approach with a true inline pass-through, giving the device full control over the signal reaching the vehicle.

**Installation note**: The earth ground wire from the compressor junction box is a separate conductor — it does not travel in the thermostat cable bundle. It is typically a short run of 18-22 AWG green or bare copper wire from the nearest ground screw on the compressor's junction box to the SideCharge device.

#### 2.0.4 Isolation and Safety

As stated in the design principles above: SideCharge only touches low-voltage control wires (24VAC thermostat, +/-12V J1772 pilot). The only contact with 240V is a read-only current clamp. We do not switch, interrupt, or connect to mains power.

| Requirement | Status |
|-------------|--------|
| Microcontroller isolated from 24VAC thermostat circuit | IMPLEMENTED (HW) |
| Microcontroller isolated from J1772 pilot circuit (+/-12V) | IMPLEMENTED (HW) |
| Earth ground reference from AC compressor junction box ground screw (G terminal — see 2.0.3.1) | IMPLEMENTED (HW) |
| 24VAC AC power galvanically isolated from 240VAC charging circuit | IMPLEMENTED (HW) |
| Fail-safe: if microcontroller loses power, basic interlock still functions | IMPLEMENTED (HW) |

Isolation means there is no direct electrical connection between the microcontroller and the circuits it monitors or controls. Signals cross the isolation barrier via optocouplers (an LED shines on a photodetector across a gap -- light carries the signal, not electricity) or magnetic couplers (transformer-based). This protects the low-voltage microcontroller from surges or faults on the thermostat or J1772 circuits.

The isolation is implemented in the current prototype hardware. The fail-safe behavior is intentionally asymmetric: if the microcontroller loses power, the hardware interlock continues to prevent simultaneous AC and EV charger operation (this is the basic safety guarantee). However, cloud override commands and the "Charge Now" button override will not function without the microcontroller -- only the fundamental mutual exclusion is preserved.

Because we stay entirely in the low-voltage domain (no mains switching), we expect this design to fall outside the scope of UL testing at 220V. This is a key part of the product strategy -- keeping the device simple and certifiable.

#### 2.0.5 Code Compliance

| Requirement | Status |
|-------------|--------|
| NEC 220.60 (Noncoincident Loads): interlocked loads calculated using only the larger | DESIGNED (not formally verified) |
| NEC 220.70 / Article 750 (Energy Management Systems): automatic load management for EV | DESIGNED (not formally verified) |
| NEC 440.34 exception: reduced circuit sizing for interlocked AC compressor circuits | DESIGNED (not formally verified) |
| Colorado electrical code compliance | DESIGNED (not formally verified) |
| Compatible with heat pump systems (reversing valve, defrost cycle) | DESIGNED (HW wiring v1.0 -- W-in/W-out terminals and relay; firmware v1.1) |
| Compatible with Level 2 J1772 chargers | NOT STARTED (tested with simulated signals only) |
| Interlock documentation sufficient for inspector sign-off | NOT STARTED |
| UL listing (or equivalent safety certification) | NOT STARTED (future -- required for production) |

Three NEC sections are directly relevant to the SideCharge interlock:

- **[NEC 220.60 (Noncoincident Loads)](https://up.codes/s/noncoincident-loads)** — When two loads are interlocked so they cannot operate simultaneously, the load calculation uses only the larger of the two, not the sum. This is the fundamental code basis for sharing a circuit between AC compressor and EV charger.
- **[NEC 220.70](https://blog.se.com/homes/2025/06/09/charging-ahead-managing-ev-load-demand-without-breaking-the-bank/) / [Article 750](https://qmerit.com/blog/top-five-ways-the-2023-nec-will-impact-ev-charging-station-installation-contractors/) (Energy Management Systems)** — Added in the 2023 NEC cycle, this section specifically addresses automated load management systems for EV charging. It provides a code path for devices that dynamically manage EV charging load based on other circuit demands.
- **[NEC 440.34 exception](https://www.ecmag.com/magazine/articles/article-detail/more-on-2023-s-chapter-2-updates-accepting-(nec)-change-part-7)** — Allows motor-driven air conditioning equipment to share circuits with other loads when interlocked to prevent simultaneous operation, with reduced conductor and overcurrent protection sizing.

We are targeting NEC and Colorado code compliance. However, no formal code compliance review has been conducted, no AHJ (Authority Having Jurisdiction) has evaluated the design, and no inspector has signed off on an installation. UL listing will eventually be required for production deployment -- but because the device operates entirely in the low-voltage domain, the certification path should be simpler than a device that switches mains power. The v1.0 PCB and wiring support both standard AC-only and heat pump installations (W-in/W-out terminals and relay are present). Firmware heat call reading is v1.1 scope, but the hardware interlock blocks the compressor via both Y-out and W-out from day one.

### 2.1 J1772 Pilot State Monitoring

| Requirement | Status |
|-------------|--------|
| Read J1772 pilot voltage via ADC (AIN7, 12-bit, 0-3.6V range) | IMPLEMENTED |
| Classify into states A through F using voltage thresholds | IMPLEMENTED |
| State A: >2600 mV (not connected) | IMPLEMENTED |
| State B: 1850-2600 mV (connected, not ready) | IMPLEMENTED |
| State C: 1100-1850 mV (charging) | IMPLEMENTED |
| State D: 350-1100 mV (charging with ventilation required) | IMPLEMENTED |
| State E: <350 mV (error -- short circuit) | IMPLEMENTED |
| State F: no pilot signal (EVSE error) | IMPLEMENTED |
| Poll interval: 500ms | IMPLEMENTED |
| Transmit on state change only (not every poll) | IMPLEMENTED |
| Include raw pilot voltage (mV) in uplink alongside state enum | IMPLEMENTED (see ADR-004 rationale) |
| Simulation mode for testing (states A-D, 10s duration) | IMPLEMENTED |

**Notes**: Voltage thresholds are hardcoded from datasheet values -- no field calibration against physical J1772 hardware has been done yet. See Oliver REC-002 for recommended validation. The thresholds work, but they have only been tested against simulated signals.

### 2.2 Current Clamp Monitoring

| Requirement | Status |
|-------------|--------|
| Read current clamp voltage via ADC (AIN0, 12-bit) | NOT AVAILABLE (WisBlock) — no second analog pin on RAK19007 J11 header. Production PCB will use dedicated ADC channel |
| Linear scaling: 0-3.3V = 0-30A (0-30,000 mA) | IMPLEMENTED (needs rescaling) |
| Scale to 48A range (v1.0 target) | NOT STARTED |
| Transmit on change detection | IMPLEMENTED |

The current scaling factor (0-30A) is too low for production. The v1.0 target is **48A** (a 60A circuit at 80% continuous). This covers all standard residential Level 2 chargers. Higher ranges (80A for Ford Charge Station Pro and similar high-power units) are a future iteration — they require a different clamp and resistor divider but no firmware architecture change. This is a clamp selection and resistor divider change -- the ADC and firmware are straightforward to update. No field calibration procedure exists yet; the scaling factor is a straight line from 0V to 3.3V, and real clamps have nonlinearities at the extremes.

#### 2.2.1 EVSE Connector Selection Guidance

SideCharge is compatible with any Level 2 J1772-compliant EV charger (EVSE). However, the shared-circuit installation creates constraints that narrow the field. This section guides the electrician on charger selection for a SideCharge installation.

**Hard requirements:**

| Requirement | Reason |
|-------------|--------|
| J1772 (SAE J1772) or NACS with J1772-compatible pilot signaling | SideCharge monitors the J1772 pilot signal to detect car presence and charging state. Chargers that do not implement standard J1772 pilot signaling are incompatible. Most NACS (Tesla-style) wall connectors still use J1772-compatible pilot circuitry — verify with the charger manufacturer. |
| Hardwired connection (no NEMA 14-50 plug) | The branch circuit modification (section 1.4) terminates at a junction box, not a receptacle. The EVSE must be hardwired to the junction box via conduit and wire. Plug-in chargers (NEMA 14-50) are designed for dedicated circuits with a receptacle — they are not appropriate for a shared-circuit installation. |
| Maximum rated current ≤ circuit breaker rating × 80% | NEC 625.41 requires continuous loads (EV charging qualifies) to not exceed 80% of the circuit breaker rating. A 40A breaker → 32A max charger. A 50A breaker → 40A max charger. The charger's rated current must be at or below this threshold. |
| Accessible pilot wire for inline pass-through | The installer must be able to access and cut the J1772 pilot signal wire between the EVSE and the vehicle connector. The pilot wire is broken and routed through the SideCharge device (PILOT-in from EVSE, PILOT-out to vehicle connector). Chargers with sealed, potted, or tamper-proof enclosures that prevent pilot wire access are incompatible without modification. |

**Recommendations:**

- **Preferred: 32A or 40A hardwired J1772 wall unit** — e.g., Grizzl-E, ChargePoint Home Flex (hardwired mode), Emporia, Wallbox Pulsar Plus. These are widely available, UL-listed, and designed for hardwired installation. The ChargePoint Home Flex is particularly good because its amperage is adjustable via the app, allowing the installer to set it to match the circuit.
- **Tesla Wall Connector (Gen 3 or Universal)**: Compatible if the pilot signal follows J1772 conventions. The Gen 3 NACS connector still uses standard pilot signaling when a non-Tesla vehicle uses the J1772 adapter. The Universal Wall Connector supports both NACS and J1772 natively. Verify pilot access.
- **Avoid: Portable/travel chargers** (NEMA 14-50 plug-in): These are designed for dedicated receptacle circuits. Even if hard-wired, their thermal design may assume the plug provides strain relief that a hardwired connection does not.
- **Avoid: Smart chargers with proprietary load management**: Some smart EVSEs (e.g., Span integration, Emporia utility program mode) have their own current management that may conflict with SideCharge's interlock. Disable any built-in load management features — SideCharge is the load manager.

**Pilot wire access**: The installer needs to cut the J1772 pilot wire and route both ends to the SideCharge device (PILOT-in from the EVSE, PILOT-out to the vehicle connector). The cut point is typically accessible at the charger's internal terminal block, the cable between the charger and the connector, or at a junction point. The exact location depends on the specific EVSE model. A wiring diagram for each supported model is a future deliverable (not in v1.0 scope). For the initial deployment, the electrician must verify pilot wire access during charger selection.

### 2.3 Thermostat Input Monitoring

| Requirement | Status |
|-------------|--------|
| Read cool call signal from Y-in (GPIO P1.02, active high, pull-down) | IMPLEMENTED |
| Read heat call signal from W-in (GPIO P0.04, active high, pull-down) | NOT AVAILABLE (WisBlock) — not connected on WisBlock prototype. Production PCB will use dedicated GPIO; firmware reading v1.1 |
| Pack cool call as flag in uplink payload (bit 1 of flags byte) | IMPLEMENTED |
| Pack heat call as flag in uplink payload (bit 0 of flags byte) | NOT STARTED (v1.1 firmware -- bit 0 reserved in v1.0, always 0) |
| Transmit on change detection (cool call v1.0; cool call or heat call v1.1) | IMPLEMENTED (cool call) |

**v1.0 behavior**: The cool call signal (Y-in, GPIO P1.02) is the thermostat input read by firmware in v1.0 -- a rising edge pauses EV charging if active. The heat call GPIO (P0.04) is not connected on the WisBlock prototype (no available pin); the production PCB will wire it to the W-in terminal. Bit 0 of the uplink flags byte is reserved for the heat call flag and is always 0 in v1.0.

**v1.1 behavior (firmware)**: Firmware reads both cool call (P1.02) and heat call (P0.04, production PCB only — not available on WisBlock prototype). Either signal pauses EV charging. Bit 0 of the flags byte reports the heat call state. This supports heat pump systems where the same compressor serves both heating and cooling -- the interlock blocks the compressor regardless of operating mode.

**Hardware interlock (v1.0)**: The pass-through architecture means both Y-out and W-out are controlled by relays that the hardware interlock can open independently of firmware. When EV current is flowing, the hardware interlock opens both relays, blocking both cool call and heat call signals from reaching the compressor contactor. This is a v1.0 hardware requirement -- it ensures heat pump installations are safe from day one, even before v1.1 firmware reads the heat call GPIO. The software layer adds redundancy, cloud override, Charge Now, and logging on top of the hardware interlock. Both layers stay active in production -- see section 2.0.1.

### 2.4 Charge Control Output

| Requirement | Status |
|-------------|--------|
| GPIO output for charge_block relay (P0.17, active high) | IMPLEMENTED |
| Allow/pause via cloud downlink command (0x10) | IMPLEMENTED |
| Auto-resume timer (configurable duration in minutes) | IMPLEMENTED |
| Default state on boot: read thermostat, then decide | IMPLEMENTED (SW) (TASK-065) |

The auto-resume timer is a safety net: if the cloud goes silent, charging resumes after the configured timeout.

Shell commands for manual charge control (`app evse allow`, `app evse pause`) are available during development -- see section 2.6 (CLI Commands).

#### 2.4.1 Boot and Power Recovery

**Decision: Read thermostat state before setting charge_block.** On every boot (cold start, power loss recovery, watchdog reset, `sys_reboot`), the device reads the thermostat GPIOs before deciding whether to allow EV charging. This replaces the previous unconditional "allow on boot" default.

**Boot sequence (app layer, after platform passes control to `app_init()`):**

| Step | Action |
|------|--------|
| 1 | Platform configures charge_block GPIO as `GPIO_OUTPUT_INACTIVE` (LOW = not blocking, hardware safety gate decides) |
| 2 | `app_init()` called. API pointers distributed to all app modules |
| 3 | `charge_control_init()` reads cool call GPIO (P1.02). Heat call GPIO (P0.04) is not available on WisBlock prototype; production PCB will read it here |
| 4 | **If cool_call HIGH or heat_call HIGH** (compressor running): set charge_block HIGH (EV blocked). Log: "Boot: compressor active, EV blocked" |
| 4 | **If both LOW** (compressor idle): leave charge_block LOW (not blocking, EV allowed). Log: "Boot: compressor idle, EV allowed" |
| 5 | Sensors initialized, LED set to commissioning mode (1Hz flash) |
| 6 | First poll cycle (t=500ms): read all sensors, send first uplink with boot state |

**v1.0 vs v1.1 note**: In v1.0 firmware, only the cool call GPIO (P1.02) is read at boot. Heat call GPIO (P0.04) is not connected on the WisBlock prototype; heat call reading at boot is v1.1 scope and requires the production PCB. However, the hardware interlock independently blocks both Y-out and W-out when charging is active, so the safety guarantee holds even before v1.1 firmware.

**Rationale**: The hardware interlock prevents double-load at the circuit level regardless of what the software does. But the software should not contradict the hardware. If the HW interlock is blocking charging because the compressor is running (cool call or heat call), the SW leaving charge_block LOW fights the HW -- even though HW wins (it is downstream), the momentary conflict is unnecessary. Setting charge_block HIGH agrees with the hardware state.

**Edge cases:**

| Scenario | Behavior | Safe? |
|----------|----------|-------|
| **Unconnected thermostat GPIOs** (commissioning) | Pull-downs read LOW → allow charging | Yes -- correct for bench testing |
| **Power cycle during compressor call** | Reads cool_call or heat_call HIGH → pause EV. Agrees with HW interlock. | Yes |
| **Power cycle during EV charging** | Reads cool_call LOW → allow EV. J1772 handshake restarts automatically. | Yes |
| **Power cycle during Charge Now override** | Override lost (RAM-only). Reads thermostat → safe default. | Yes -- user presses button again if needed |
| **Power cycle during cloud override** | Cloud override lost (RAM-only). Cloud re-sends on next scheduler cycle. | Yes -- brief gap is acceptable |
| **Watchdog reset** | Identical to power cycle. All RAM state reinitialized. | Yes |
| **Platform-only mode (no app)** | Platform sets charge_block GPIO inactive (LOW = not blocking). No read-then-decide. HW interlock still protects. | Acceptable for dev/recovery |

**Implementation**: All items addressed in TASK-065 (AC-priority software interlock + charge_block rename, merged). Terminology propagated in TASK-068.

### 2.5 LED Indicators

The device has no screen, no mobile app, and no user-facing dashboard in v1.0. LEDs are the entire visual interface. Two audiences rely on them: the electrician during commissioning (who needs to verify power, interlock response, and connectivity before leaving the job site) and the homeowner post-install (who needs a single-glance "everything is fine" signal).

Design principle: **slower blink = calmer = healthier**. Fast blinking means something needs attention.

#### 2.5.1 Prototype LED Matrix (Single Green LED)

The RAK4631 has one user-controllable green LED (LED_ID_0). With a single LED, information is encoded entirely through blink patterns. The blink engine runs in the app layer via the platform `led_set()` API, driven by the 500ms poll timer. When multiple states are active simultaneously (e.g., OTA during charging, or Sidewalk disconnect during AC priority), the LED shows the pattern of the **highest-priority** active state.

| Priority | State | Pattern | Description |
|----------|-------|---------|-------------|
| 1 (highest) | **Error / fault** | 5Hz rapid flash (100ms on, 100ms off) | Hardware fault, sensor read failure, or persistent Sidewalk failure after 10 min. Installer: investigate. Homeowner: call installer. |
| 2 | **OTA in progress** | Double-blink (100ms on, 100ms off, 100ms on, 700ms off) | Firmware update active. Do not power off. Returns to normal when OTA completes. |
| 3 | **Commissioning mode** (first 5 min after boot, or until first successful Sidewalk uplink) | 1Hz even flash (500ms on, 500ms off) | Device is starting up and establishing connectivity. Installer: wait for pattern to change. |
| 4 | **Sidewalk disconnected** (after commissioning window) | Slow triple-blink (100ms on, 100ms off ×3, then 1500ms off) | Connectivity lost. Interlock still works locally but cannot receive cloud commands. |
| 5 | **Charge Now override active** | 0.5Hz slow blink (1000ms on, 1000ms off) | User pressed Charge Now. EV charging has priority. AC call pending. |
| 6 | **AC has priority / EV paused** | Heartbeat pulse (200ms on, 1800ms off) | Normal operation -- AC is calling, EV charging paused. The interlock is doing its job. |
| 7 | **EV actively charging** | Solid ON | Current flowing to EV charger. Everything working. |
| 8 (lowest) | **Idle / no load** | Off with 50ms blip every 10 seconds | No AC call, no EV charging, Sidewalk connected. The periodic blip confirms the device is alive -- a completely dark LED looks dead. |

**"Charge Now" button feedback**: On press, the LED flashes 3 rapid blinks (50ms on/off) as acknowledgment before transitioning to the override pattern (priority 5). If the override is rejected (device is in error state or OTA), the LED stays on its current higher-priority pattern -- no acknowledgment flash.

**Error state entry criteria** (any of these triggers priority 1 rapid flash):
- ADC read returns negative error code 3 consecutive times
- GPIO read returns negative error code 3 consecutive times
- Sidewalk not ready 10+ minutes after boot (past commissioning window)
- OTA apply failure (CRC mismatch after staging)
- Charge_block GPIO set fails

Error states clear automatically on the next successful sensor poll cycle. A persistent error that never clears keeps the LED on rapid flash indefinitely -- correct behavior (installer needs to investigate).

**Commissioning sequence** (what the installer observes after wiring and powering on):
1. LED begins 1Hz flash (commissioning mode)
2. After Sidewalk connects (typically 30-90s), LED transitions to idle blip
3. Installer triggers AC call via thermostat -- LED transitions to heartbeat pulse, confirming interlock detected the call
4. Installer removes AC call -- LED returns to idle blip
5. Installer plugs in EV -- LED goes solid ON when current flows
6. Installer triggers AC call while EV is charging -- LED transitions to heartbeat pulse, confirming interlock paused EV

If the LED stays on 1Hz flash for more than 5 minutes, Sidewalk connectivity has not been established. Installer should verify gateway proximity.

#### 2.5.1.1 Production LED Matrix (2 LEDs)

The production PCB (TASK-019) adds one LED, for a total of two:

- **LED 1 (green)**: Connectivity and system health
- **LED 2 (blue)**: Interlock and charging state

Two LEDs eliminate ambiguity. Green answers "is the device healthy and connected?" Blue answers "what is the interlock doing?"

| Condition | Green (Health) | Blue (Interlock) |
|-----------|----------------|------------------|
| Normal, idle, connected | Solid | Off |
| EV charging | Solid | Solid |
| AC priority, EV paused | Solid | Heartbeat pulse (200ms on, 1800ms off) |
| Charge Now override active | Solid | 0.5Hz slow blink (1000ms on, 1000ms off) |
| Cloud override (charging paused) | Solid | Double-blink (100ms on, 100ms off, 100ms on, 1700ms off) |
| Sidewalk disconnected | Slow flash (200ms on, 1800ms off) | (per interlock state) |
| Commissioning (first 5 min) | 1Hz flash | Off |
| OTA in progress | Double-blink | Off |
| Error / fault | 5Hz rapid flash | 5Hz rapid flash |

**Combined reading for the installer**: Green solid + Blue off = healthy idle. Green solid + Blue solid = EV charging. Green solid + Blue heartbeat = AC has priority. Green 1Hz = commissioning. Green rapid = error.

**"Charge Now" button feedback (production)**: Both LEDs flash 3 times (50ms on/off) as acknowledgment. Then blue transitions to Charge Now pattern.

**BOM impact**: One additional blue LED (0603/0805 SMD), one current-limiting resistor, one GPIO pin. The `app_leds` module already supports LED_ID_0 through LED_ID_3 -- no architecture change needed.

**Implementation**: LED control via `app_leds` module is implemented. The blink priority state machine with all 8 priority levels and patterns is implemented in `led_engine.c` (TASK-067). Production dual-LED requires TASK-019 PCB.

#### 2.5.2 Commissioning Test Sequence

The commissioning sequence is the installer's final verification before leaving the job site. It is the only defense against the most dangerous class of installation errors -- 240V branch circuit wiring mistakes that the device cannot detect. No sensor on the SideCharge board touches the 240V circuit, so a reversed L1/L2 connection, an undersized conductor, or a missing equipment ground are invisible to firmware. The commissioning checklist catches these through visual inspection steps that happen before any power is applied.

The full sequence takes 10-15 minutes. Steps C-01 through C-05 are visual/manual inspections performed before powering on. Steps C-06 through C-12 (plus C-08b for heat pump installations) are powered-on functional tests verified by LED response and (optionally) cloud confirmation. The electrician needs no phone app, no cloud login, and no special tools beyond standard electrical testing equipment.

| ID | Test | Method | Pass Criteria | Priority |
|----|------|--------|---------------|----------|
| C-01 | Branch circuit wiring | Visual inspection: conductor gauge, breaker rating, junction box, EGC continuity | Conductors match breaker rating per NEC 240.4(D); EGC continuous from panel to both loads | P0 |
| C-02 | Current clamp placement | Visual inspection | Clamp on correct conductor (EV charger leg, not AC leg), correct orientation (arrow toward load) | P0 |
| C-03 | Thermostat and ground wiring | Visual inspection | R (24VAC) and C (common) on power terminals; Y wire cut and connected Y-in (thermostat side) / Y-out (compressor side); W wire cut and connected W-in / W-out; G terminal connected to earth ground from AC compressor junction box ground screw (see 2.0.3.1 terminal definitions); no shorts between conductors. For AC-only installations without a W wire, cap W-in and W-out terminals | P0 |
| C-04 | J1772 pilot pass-through | Visual inspection | Pilot wire cut and connected PILOT-in (EVSE side) / PILOT-out (vehicle connector side). Verify correct polarity -- PILOT-in from charger, PILOT-out toward vehicle | P0 |
| C-05 | Physical mounting | Visual inspection | Device secured, conduit entries sealed, no strain on low-voltage wires | P1 |
| C-06 | Power-on | Apply power, observe LED | Green LED begins 1Hz flash (commissioning mode) within 5 seconds | P0 |
| C-07 | Sidewalk connectivity | Wait for LED transition | Green LED transitions from 1Hz flash to solid (typically 30-90s). If still flashing after 5 min, verify gateway proximity | P0 |
| C-08 | Cool call detection | Set thermostat to call for cooling | Blue LED transitions to heartbeat pulse. Confirms device sees cool call signal on Y-in. Verify Y-out passes signal to compressor contactor (compressor starts) | P0 |
| C-08b | Heat call detection (heat pump installations only) | Set thermostat to call for heating | Blue LED transitions to heartbeat pulse (v1.1 firmware; v1.0 may not show LED change but hardware interlock still blocks EV). Verify W-out passes signal to compressor contactor | P1 |
| C-09 | Thermostat release | Cancel cooling/heating call | Blue LED returns to off (idle). Confirms device sees signal drop | P0 |
| C-10 | EV charge detection | Plug in EV (or simulate State C via `app evse c`) | Blue LED goes solid. Current reading >0 in uplink (if cloud-verified) | P0 |
| C-11 | Interlock test | Trigger AC call while EV is charging | Blue LED transitions to heartbeat pulse. EV charger pauses (pilot drops to State B). This is the critical safety test -- confirms mutual exclusion works end to end | P0 |
| C-12 | Charge resume | Cancel AC call while EV is still connected | Blue LED returns to solid. EV charger resumes (pilot returns to State C) | P1 |

**Commissioning checklist card**: A printed card ships in every box. One side has the commissioning checklist with pass/fail checkboxes. The other side has a wiring diagram showing current clamp orientation, thermostat pass-through terminal mapping (Y-in/Y-out, W-in/W-out), pilot pass-through wiring (PILOT-in/PILOT-out), and earth ground connection. The installer completes the card during commissioning and leaves it at the installation site (attached to the device enclosure or the junction box cover). This card is the installation record -- if an inspector or future electrician needs to verify the installation, the card documents what was checked and by whom.

Card fields: installer name, date, device ID (printed on label), all 12 test results (pass/fail), installer signature. The card design is a deliverable of TASK-041.

| Requirement | Status |
|-------------|--------|
| Commissioning test sequence defined (12 steps + C-08b for heat pump) | DESIGNED |
| Commissioning checklist card design | DESIGNED (TASK-041 done — card spec, SVG sources, and PDF complete; see `docs/design/commissioning-card-spec.md`) |
| Commissioning card included in product packaging | NOT STARTED (requires print production) |
| Estimated commissioning time: 10-15 minutes | DESIGNED |

#### 2.5.3 Self-Test and Fault Detection

The device cannot detect 240V wiring errors, but it can catch a significant class of installation and operational faults through automated self-tests. These tests run at three points: on every boot, continuously during operation, and on demand (via shell command or physical button). The goal is to surface problems that would otherwise go unnoticed until a safety-critical moment -- for example, a current clamp that was installed backwards would report zero current while the EVSE is actively charging, silently defeating the current-based safety checks.

**Boot self-test (runs automatically on every power-on)**

On every boot, after the platform passes control to `app_init()`, the following checks run before normal operation begins. Any failure sets the corresponding fault flag in the uplink thermostat byte (bits 4-7) and triggers the error LED pattern. The boot self-test adds <100ms to startup.

| Check | What It Detects | Failure Action |
|-------|-----------------|----------------|
| ADC channels readable | Dead or disconnected ADC (AIN7). Current clamp ADC (AIN0) not available on WisBlock prototype | Set SENSOR_FAULT flag, error LED |
| GPIO pins readable | Dead thermostat input GPIO (P1.02 cool call). Heat call GPIO (P0.04) not connected on WisBlock prototype | Set SENSOR_FAULT flag, error LED |
| Charge_block toggle-and-verify | Charge_block GPIO not controlling the relay — if toggling the output does not change the readback state, the relay is stuck or disconnected | Set INTERLOCK_FAULT flag, error LED |
| Sidewalk init check | MFG keys missing or session keys absent (already implemented, see 3.1.1) | Degraded mode (no connectivity) |

**Continuous monitoring (runs during normal operation)**

These checks run on every 500ms sensor poll cycle. They detect faults that develop after installation -- a clamp that shifts, a loose thermostat wire, a relay that welds shut.

| Check | What It Detects | Failure Action |
|-------|-----------------|----------------|
| Current vs. J1772 cross-check | If J1772 state is C (charging) but current reads <500mA for >10s, the clamp is likely disconnected, installed on the wrong conductor, or installed backwards. If J1772 is A/B but current reads >500mA for >10s, the clamp is on the wrong circuit. | Set CLAMP_MISMATCH flag in uplink |
| Charge_block effectiveness | After setting charge_block HIGH (block), if current does not drop below 500mA within 30s, the relay or J1772 spoof circuit is not working — the software interlock is defeated | Set INTERLOCK_FAULT flag, error LED |
| Pilot voltage range | If pilot voltage is outside all valid J1772 ranges (not A, B, C, D, E, or F) for >5s, the pilot signal connection may be damaged or noisy | Set SENSOR_FAULT flag in uplink |
| Thermostat chatter | If cool_call or heat_call toggles >10 times in 60s, a thermostat wire may be loose or the signal is noisy | Set SENSOR_FAULT flag in uplink (informational, no error LED) |

**Charge_block effectiveness is the most important automated safety check.** If the relay does not actually pause the EVSE when commanded, the entire software interlock is defeated — the device thinks it has paused charging, but current continues to flow. The cross-check (set charge_block HIGH, verify current drops) catches stuck relays, failed J1772 spoof circuits, and wiring errors that bypass the charge control output.

**On-demand self-test**

During development, `sid selftest` triggers a full self-test cycle (boot checks + one pass of continuous checks) and prints results to the shell. In production (no USB), the same test is triggered by pressing the Charge Now button 5 times within 5 seconds. Results are reported via LED blink codes: green rapid-blinks the count of passed tests, then pauses, then red/both rapid-blinks the count of failed tests (0 blinks = all passed). Results are also sent as a special uplink with the SELFTEST_FAIL flag if any test fails.

**FAULT_SELFTEST lifecycle**: The boot self-test fault flag (0x80) is latched on failure and included in every subsequent uplink. Unlike the continuous fault flags (which self-heal on the next clean poll cycle), FAULT_SELFTEST persists because the boot checks are not re-evaluated during normal operation. The flag clears in two ways:
1. **Device reboot** (power cycle or OTA apply) — all fault flags are RAM-only; on reboot, `selftest_boot()` re-evaluates from scratch. A transient boot glitch self-heals on the next clean boot.
2. **Button-triggered re-test** — if the installer presses the button 5 times and all 5 checks pass, FAULT_SELFTEST is cleared (TASK-066). This lets the installer verify a fix in the field without power-cycling the device.

There is no remote reboot or remote fault-clear command. The continuous monitors (FAULT_SENSOR, FAULT_CLAMP, FAULT_INTERLOCK) cover the same failure modes at runtime, so a latched FAULT_SELFTEST with clean continuous flags means the boot failure was transient.

**Button detection limitation (v1.0)**: The app layer has no GPIO interrupt path — button state is read by polling `gpio_get()` inside `app_on_timer()`, which fires every 500ms. Each press must be held long enough to span at least one polling tick to be detected. With 500ms resolution, five deliberate press-and-release cycles require ~5 seconds, so the original 3-second window was impractical. The 5-second window accommodates this. Quick taps shorter than 500ms may be missed entirely. This is adequate for production use (installers press deliberately), but TASK-049 would add a platform-side GPIO interrupt callback (`on_button_press`) that delivers sub-millisecond press timestamps, enabling the tighter 3-second window and natural quick presses. TASK-049 requires an `APP_CALLBACK_VERSION` bump (v3 → v4) per ADR-001.

**What can vs. cannot be automated**

| Category | Auto-Detectable? | Method |
|----------|-------------------|--------|
| Branch circuit wiring (240V) | No -- device has no 240V sensing | Manual inspection only (C-01) |
| Current clamp errors | Yes (most) | Current vs. J1772 cross-check |
| J1772 pilot problems | Yes | ADC range validation |
| Thermostat wiring | Partially -- detects signal presence/noise, not correct wiring | Requires manual trigger to verify (C-08, C-09) |
| Charge_block / relay | Yes | Toggle-and-verify on boot + effectiveness check during operation |
| Physical mounting | No | Visual inspection only (C-05) |
| Sidewalk connectivity | Yes | Existing Sidewalk init checks (3.1.1) |

| Requirement | Status | Priority |
|-------------|--------|----------|
| Boot self-test (ADC, GPIO, charge_block toggle-and-verify) | IMPLEMENTED (SW) (TASK-039) | P0 |
| Current vs. J1772 cross-check (continuous) | IMPLEMENTED (SW) (TASK-039) | P0 |
| Charge_block effectiveness check (continuous) | IMPLEMENTED (SW) (TASK-039) | P0 |
| Pilot voltage range validation (continuous) | IMPLEMENTED (SW) (TASK-039) | P1 |
| Thermostat chatter detection (continuous) | IMPLEMENTED (SW) (TASK-039) | P2 |
| `sid selftest` shell command | IMPLEMENTED (SW) (TASK-039) | P0 |
| Production 5-press button trigger for self-test (5s window, polling-based — see limitation note above) | IMPLEMENTED (SW) (TASK-040) | P1 |
| Platform GPIO interrupt callback for tighter 3s button detection | NOT STARTED (TASK-049b) | P3 |
| Self-test results in uplink fault flags (byte 7, bits 4-7) | IMPLEMENTED (SW) (TASK-039) | P0 |
| Button re-test clears FAULT_SELFTEST on all-pass | IMPLEMENTED (SW) (TASK-066) | P1 |
| LED blink priority state machine (§2.5.1 patterns) | IMPLEMENTED (SW) (TASK-067) | P1 |

#### 2.5.4 Installation Failure Modes

This section catalogs the installation failure modes that commissioning and self-test are designed to catch. It is a condensed reference -- not every possible failure, but the categories that matter most, organized by what the device can and cannot detect.

**Key insight: the device cannot detect any 240V wiring error.** The current clamp is the only connection to the 240V circuit, and it measures current magnitude only -- not voltage, not polarity, not conductor sizing. A reversed L1/L2, a missing equipment ground, or an undersized conductor are all invisible to firmware. The commissioning checklist (section 2.5.2) is the only defense against this class of failure.

| Category | Example Failures | Severity | Auto-Detectable? | Commissioning Test | Notes |
|----------|-----------------|----------|-------------------|-------------------|-------|
| **Branch circuit** | Wrong breaker size, undersized conductors, missing EGC, loose connections in junction box | CRITICAL | No -- device has no 240V sensing | C-01 (visual inspection) | Most dangerous failures. Breaker is last line of defense. |
| **Current clamp** | Wrong conductor (AC leg instead of EV leg), reversed orientation, loose or fallen off | HIGH | Yes -- J1772 cross-check detects current/state mismatch | C-10, C-11 | Backwards clamp reads zero or negative; wrong conductor reads AC current as EV current. Cross-check catches both. |
| **J1772 pilot** | Disconnected PILOT-in or PILOT-out wire, swapped PILOT-in/PILOT-out, damaged signal | HIGH | Yes -- ADC reads out-of-range voltage | C-04, C-10, C-11 | Device sees State A (disconnected) or State E/F (error) when it should see State C. Swapped PILOT-in/PILOT-out would mean the spoof resistance is inserted on the wrong side. |
| **Thermostat** | Reversed R/Y wires, swapped Y-in/Y-out or W-in/W-out, loose connection, wrong terminals | MEDIUM | Partially -- detects signal presence and chatter, not correct wiring | C-03 (visual: R/C on power, Y-in/Y-out pass-through, W-in/W-out pass-through, G on earth ground), C-08, C-08b, C-09 (manual trigger required) | Swapped in/out on Y or W would mean the device reads the compressor side instead of the thermostat side -- the interlock still prevents double-load but the firmware sees the wrong signal direction. G terminal connected to wrong source (e.g., thermostat fan wire instead of earth ground) would not cause a safety failure but defeats the ground reference function. Manual test is required to verify correct behavior. |
| **Charge_block / relay** | Relay wired backwards, stuck relay, disconnected relay output, failed J1772 spoof circuit | CRITICAL | Yes — toggle-and-verify on boot, effectiveness check during operation | C-11 (interlock test) | If charge_block doesn't work, software interlock is defeated. Hardware interlock is the backstop. |
| **Physical / power** | Device not secured, loose conduit, no power (LED dark) | LOW-MEDIUM | Partial -- power loss detected (device goes offline), mounting is visual only | C-05 (visual), C-06 (power-on) | Cloud detects offline device via missing heartbeats (section 5.3.2). |

**Implementation**: Boot self-test, continuous monitoring (current/J1772 cross-check, charge_block effectiveness), and `sid selftest` shell command all implemented (TASK-039). Production 5-press button trigger implemented (TASK-040). Commissioning checklist card designed (TASK-041). On-device verification pending (TASK-048).

### 2.6 CLI Commands (Development and Testing Only)

The shell is the primary diagnostic interface during development. Every sensor reading, every Sidewalk state, every OTA phase is queryable from the serial console via USB CDC ACM at 115200 baud. **USB serial will not be available after the device leaves the factory** -- production devices will be sealed with no USB access. All post-deployment diagnostics must happen via the cloud (AWS monitoring, DynamoDB event history). Whether any form of local diagnostics survives into production (e.g., via BLE or a debug header) is TBD.

All commands below are development/testing tools. They are not part of the product's user interface.

#### Sidewalk Commands

| Command | Description | Status |
|---------|-------------|--------|
| `sid status` | Show Sidewalk connection state, active link type (BLE/LoRa), and init status | IMPLEMENTED |
| `sid mfg` | Show MFG store version and provisioned device ID | IMPLEMENTED |
| `sid link <mask>` | Switch active Sidewalk link type (1=BLE, 2=LoRa, 3=auto) | IMPLEMENTED |

#### OTA Commands

| Command | Description | Status |
|---------|-------------|--------|
| `sid ota status` | Show OTA state machine phase (IDLE, RECEIVING, VALIDATING, APPLYING, COMPLETE) | IMPLEMENTED |
| `sid ota abort` | Cancel an in-progress OTA session | IMPLEMENTED |
| `sid ota report` | Send an OTA status uplink (for debugging cloud-side session state) | IMPLEMENTED |
| `sid ota delta_test` | Run a local delta validation test (CRC check against primary) | IMPLEMENTED |

#### EVSE / Charge Control Commands

| Command | Description | Status |
|---------|-------------|--------|
| `app evse status` | Show J1772 pilot state, pilot voltage (mV), charging current (mA), and charge control state (allowed/paused) | IMPLEMENTED |
| `app evse a` | Simulate J1772 State A (not connected) for 10 seconds | IMPLEMENTED |
| `app evse b` | Simulate J1772 State B (connected, not ready) for 10 seconds | IMPLEMENTED |
| `app evse c` | Simulate J1772 State C (charging) for 10 seconds | IMPLEMENTED |
| `app evse d` | Simulate J1772 State D (charging with ventilation) for 10 seconds | IMPLEMENTED |
| `app evse allow` | Allow EV charging (charge_block LOW = not blocking) | IMPLEMENTED |
| `app evse pause` | Pause EV charging (charge_block HIGH = blocking) | IMPLEMENTED |

#### Thermostat Commands

| Command | Description | Status |
|---------|-------------|--------|
| `app hvac status` | Show thermostat cool call and heat call flags (active/inactive). Heat call reporting is v1.1 firmware | IMPLEMENTED (cool call); DESIGNED (heat call, v1.1) |

#### Uplink Commands

| Command | Description | Status |
|---------|-------------|--------|
| `app sid send` | Manually trigger an uplink with the current sensor payload | IMPLEMENTED |

---

## 3. Connectivity Requirements

### 3.1 Sidewalk LoRa Link

| Requirement | Status |
|-------------|--------|
| Amazon Sidewalk LoRa 915MHz via SX1262 | IMPLEMENTED |
| BLE registration (first boot only -- pairs with nearby Sidewalk gateway) | IMPLEMENTED |
| LoRa data link on subsequent boots | IMPLEMENTED |
| TCXO patch for RAK4631 radio stability | IMPLEMENTED |

The SX1262 is a Semtech LoRa radio transceiver -- the chip on the RAK4631 that handles all 915MHz wireless communication. LoRa (Long Range) is a spread-spectrum modulation that trades data rate for range: it can reach hundreds of meters through walls and buildings, which is why it works for a device buried in a garage or utility closet.

BLE (Bluetooth Low Energy) is used only once: on the very first boot, the device uses BLE to register with a nearby Amazon Sidewalk gateway (typically an Echo or Ring device). This one-time registration establishes the device's identity and negotiates session keys. After that, the device never uses BLE again -- all data goes over LoRa.

The TCXO (Temperature-Compensated Crystal Oscillator) patch was necessary because the RAK4631's default crystal drifts enough with temperature to cause LoRa packet errors. The TCXO provides a more stable frequency reference for the SX1262 radio, ensuring reliable communication across operating temperatures.

Link mask switching (`sid link <mask>`) for testing is available via the CLI -- see section 2.6.

#### 3.1.1 Startup and Power Recovery

On every boot (including power loss recovery), the platform performs these checks before starting normal operation:

| Check | Action on Failure | Status |
|-------|-------------------|--------|
| MFG key health check (detect missing/empty credentials at 0xFF000) | Log error, continue with degraded operation (no Sidewalk) | IMPLEMENTED |
| Sidewalk session key presence (settings partition) | Trigger BLE registration if no keys found | IMPLEMENTED |
| OTA recovery metadata check (0xCFF00) | Resume interrupted flash copy if metadata present | IMPLEMENTED |
| App callback table magic check (0x90000) | Skip app initialization if no valid app found | IMPLEMENTED |

The MFG key health check is particularly important: if the MFG partition is empty or corrupted (e.g., after a chip erase without re-provisioning), the device cannot authenticate with Amazon Sidewalk. The boot log will show the error, but the device will not crash -- it continues running without connectivity.

### 3.2 Uplink (Device to Cloud)

The device uplinks on two triggers: a **15-minute heartbeat** (periodic, whether or not anything changed) and **on state change** (any sensor transition). Both carry the same payload format with a device-side timestamp.

The 5-second minimum TX rate limiter prevents the device from flooding the LoRa link when multiple sensors change in rapid succession (for example, a J1772 state change and current change within milliseconds). The limiter coalesces rapid changes into a single uplink after the quiet period.

**Note**: The on-state-change uplink may be removed in a future version. It is redundant with the 15-minute heartbeat for steady-state monitoring, could generate excessive uplinks during rapid state transitions, and does not help detect when the device has gone offline (only periodic heartbeats can do that). It is included in v1.0 to capture high-resolution transition data during field validation.

| Requirement | Status |
|-------------|--------|
| 12-byte payload: J1772 state + pilot voltage + current + flags + 4-byte timestamp | IMPLEMENTED v0x08 (TASK-035/060) |
| Transmit on sensor state change | IMPLEMENTED |
| 15-minute heartbeat (transmit even if no change) | IMPLEMENTED (TASK-070) |
| 5-second minimum TX rate limiter | IMPLEMENTED |
| Fits within 19-byte Sidewalk LoRa MTU | IMPLEMENTED (12 bytes = 7 bytes spare) |

#### 3.2.1 Payload Format (Bit-Level)

```
Byte 0:    0xE5                    Magic byte (EVSE_MAGIC)
Byte 1:    Version                 Payload format version (0x08)
Byte 2:    J1772 state             Enum: 0=A (disconnected), 1=B (connected/not ready),
                                   2=C (charging), 3=D (charging+ventilation),
                                   4=E (error/short), 5=F (no pilot)
Byte 3:    Pilot voltage, low      J1772 pilot voltage in millivolts, little-endian uint16
Byte 4:    Pilot voltage, high     (e.g., 0xBA 0x08 = 2234 mV ≈ State B)
Byte 5:    Current, low            Charging current in milliamps, little-endian uint16
Byte 6:    Current, high           (e.g., 0xD0 0x07 = 2000 mA = 2.0A)
Byte 7:    Flags                   Bitfield:
             Bit 0 (0x01): HEAT            Heat call active from W-in (P0.04, production PCB only — not connected on WisBlock prototype). Always 0 in v1.0; reported in v1.1 firmware
             Bit 1 (0x02): COOL            Cool call active (P1.02)
             Bit 2 (0x04): CHARGE_ALLOWED  Charge control state (1=allowed, 0=paused)
             Bit 3 (0x08): CHARGE_NOW      Charge Now override active
             Bit 4 (0x10): SENSOR_FAULT    ADC/GPIO read failure or pilot out-of-range (see 2.5.3)
             Bit 5 (0x20): CLAMP_MISMATCH  Current vs. J1772 state disagreement (see 2.5.3)
             Bit 6 (0x40): INTERLOCK_FAULT Charge_block ineffective or relay stuck (see 2.5.3)
             Bit 7 (0x80): SELFTEST_FAIL   On-demand self-test detected a failure (see 2.5.3)
Bytes 8-11: Timestamp              SideCharge epoch: seconds since 2026-01-01 00:00:00 UTC,
                                   little-endian uint32. 1-second granularity. Device computes
                                   from last time sync + local uptime offset.
```

**Total: 12 bytes. Fits within 19-byte Sidewalk LoRa MTU with 7 bytes to spare.**

AC supply voltage is assumed to be 240V for all power calculations. The device does not
measure line voltage. The J1772 pilot signal voltage (ADC AIN7) is included in the uplink
alongside the classified state enum (byte 2). The raw millivolt reading enables cloud-side
detection of marginal pilot connections — readings near a threshold boundary (e.g., 2590 mV
near the 2600 mV A/B boundary) indicate a flaky or degraded connection that the enum alone
would not reveal. See ADR-004 for the rationale.

The timestamp uses a SideCharge-specific epoch (2026-01-01) rather than Unix epoch to avoid the 2038 overflow issue and to keep the values smaller and more debuggable. 4 bytes at 1-second granularity gives ~136 years of range. The device computes the current time as `sync_time + (current_uptime - sync_uptime)`, where `sync_time` is set by the TIME_SYNC downlink (see section 3.3). Clock drift on the nRF52840's 32.768 kHz RTC crystal is ~100 ppm, which is ~8.6 seconds per day — well within the 5-minute accuracy target.

#### 3.2.2 Device-Side Event Buffer

The device maintains a **ring buffer of 50 state-change snapshots** in RAM (50 × 12 bytes = 600 bytes from the app's 8KB budget). A snapshot is written only when device state actually changes — J1772 pilot state, charge control (pause/allow), thermostat flags, or current on/off transitions. Steady-state polls (every 500ms) do not write to the buffer.

Under normal operation an EVSE sees ~5-10 state transitions per day (vehicle plug/unplug, charging start/stop, TOU pause/resume), so 50 entries comfortably covers **multiple days** of history. In pathological cases (rapid state bouncing from a wiring fault), the buffer fills faster — but the most recent transitions are the diagnostically valuable ones.

On each uplink, the device sends the **most recent** state. The cloud ACKs received data by piggybacking a watermark timestamp on the next downlink (see TIME_SYNC command, section 3.3). The device drops all buffer entries at or before the ACK'd timestamp.

If no ACK arrives, the ring buffer wraps and overwrites the oldest entries. This is acceptable — current state is more valuable than stale data.

This pattern is a simplified version of **TCP's cumulative ACK with a sliding window**: the device keeps data until the cloud confirms receipt, then trims. No retransmission, no sequencing, no gap detection. See ADR-002 for the design rationale.

### 3.3 Downlink (Cloud to Device)

Downlinks are precious over LoRa -- 19 bytes max, and the Sidewalk network determines delivery timing (the device cannot request a downlink; it receives them when the network has one queued). All downlink commands fit well within the 19-byte MTU.

#### Time Sync (0x30)

| Byte | Field | Values |
|------|-------|--------|
| 0 | Command type | `0x30` |
| 1-4 | Current time | SideCharge epoch (seconds since 2026-01-01 00:00:00 UTC), little-endian uint32 |
| 5-8 | ACK watermark | Timestamp of most recent uplink the cloud has received, little-endian uint32. `0x00000000` = no ACK (first sync). |

The cloud sends TIME_SYNC on two occasions: (1) in response to the device's first uplink after boot (the decode Lambda detects a time gap or version=0x07 with timestamp=0 and triggers a sync), and (2) periodically (e.g., daily) to correct clock drift. The device stores the sync time and its own `k_uptime_get()` at the moment of receipt, then computes current time as `sync_time + (uptime_now - uptime_at_sync)`.

The ACK watermark tells the device "I have all your data through this timestamp" so the device can trim its event buffer (see section 3.2.2). This piggybacks the ACK on the sync command to avoid burning a separate downlink.

| Requirement | Status |
|-------------|--------|
| TIME_SYNC downlink command (0x30) | IMPLEMENTED (TASK-033) |
| Device-side time tracking (sync + uptime offset) | IMPLEMENTED (TASK-033) |
| Cloud-side auto-sync on first uplink after boot | IMPLEMENTED (decode Lambda) |
| Periodic drift correction (daily) | IMPLEMENTED (decode Lambda) |

#### Charge Control (0x10)

| Byte | Field | Values |
|------|-------|--------|
| 0 | Command type | `0x10` |
| 1 | Subcommand | `0x01` = set delay window, `0x02` = clear delay (allow now) |
| 2-5 | Window start | SideCharge epoch (seconds since 2026-01-01), little-endian uint32. When charging should pause. |
| 6-9 | Window end | SideCharge epoch, little-endian uint32. When charging may resume. |

**Set delay window** (`0x01`): The device stores the window. If the current time falls within `[start, end]`, charging pauses. When `now > end`, charging resumes autonomously — no follow-up command needed. Each new window replaces the previous one (one active window at a time). If `start` is in the past and `end` is in the future, the delay takes effect immediately.

**Clear delay** (`0x02`): Clears any active delay window. Bytes 2-9 are ignored. Charging resumes immediately (subject to AC priority / hardware interlock). Used when the cloud determines a previously sent window is no longer needed (e.g., MOER dropped below threshold mid-window).

The previous pause/allow format (`0x00`/`0x01` in byte 1 with no timestamps) is deprecated. The delay window approach is more resilient — see section 4.4.1 for rationale. Total downlink size is 10 bytes, well within the 19-byte LoRa MTU.

**Charge Now interaction**: When the user presses Charge Now, the active delay window is suspended until the car's J1772 state changes (see section 4.4.5). The window itself is not deleted — if the car state change happens before the window ends, the remaining window time is honored for any subsequent charging session.

#### OTA Commands (0x20)

OTA firmware updates use command type `0x20` with subcommands. For full details on the OTA pipeline, see sections 4.2 (cloud-side) and 4.3 (device-side).

| Subcommand | Direction | Format | Description |
|------------|-----------|--------|-------------|
| START (`0x01`) | Cloud → Device | `[0x20, 0x01, version, total_size(2B), total_chunks(2B), crc32(4B), flags, delta_count(2B), delta_list(nB)]` | Begin OTA session with target version, image size, CRC, and (in delta mode) list of changed chunk indices |
| CHUNK (`0x02`) | Cloud → Device | `[0x20, 0x02, chunk_idx(2B), data(15B)]` | Deliver one 15-byte chunk of firmware data |
| ABORT (`0x03`) | Cloud → Device | `[0x20, 0x03]` | Cancel the current OTA session |
| ACK (`0x81`) | Device → Cloud | `[0x20, 0x81, status, next_chunk(2B)]` | Acknowledge a chunk or report error; includes next expected chunk index |
| COMPLETE (`0x82`) | Device → Cloud | `[0x20, 0x82, status, crc32(4B)]` | Report successful validation; includes verified CRC |
| STATUS (`0x83`) | Device → Cloud | `[0x20, 0x83, phase, chunks_received(2B)]` | Report current OTA state machine phase and progress |

#### Unknown Commands

| Requirement | Status |
|-------------|--------|
| Unknown command type logging (graceful reject) | IMPLEMENTED |

Unknown command types are logged and discarded -- the device does not crash on unrecognized downlinks.

---

## 4. Cloud Requirements

### 4.1 Payload Decode

The decode Lambda is the entry point for all device-to-cloud data. Every uplink from the device arrives as a raw byte payload via Amazon Sidewalk's IoT Wireless destination. The Lambda decodes these bytes into structured fields (J1772 state, voltage, current, thermostat flags), stores them in DynamoDB as timestamped events, and — for OTA-related messages — routes ACKs to the OTA sender Lambda to continue the firmware update flow.

| Requirement | Status |
|-------------|--------|
| Lambda decodes raw 12-byte EVSE payload (v0x07/v0x08) | IMPLEMENTED |
| Decoded events stored in DynamoDB (device_id + timestamp) | IMPLEMENTED |
| TTL expiration on DynamoDB records | IMPLEMENTED |
| Backward compatibility: payload version field (byte 1) enables format evolution | IMPLEMENTED (v0x07 and v0x08 both decoded) |

The payload format is identified by byte 0 (`0xE5` for EVSE telemetry) and versioned by byte 1. If the format changes, the decode Lambda must continue to handle older versions -- devices in the field may not all update at the same time. The version byte gives a clean path to evolve the format without breaking existing deployments.

### 4.2 OTA Firmware Update Pipeline (Cloud Side)

The OTA pipeline lets us push firmware updates to the device over LoRa without physical access. Here's how it works: a developer uploads a new app binary to S3, which triggers the OTA sender Lambda. The Lambda compares the new binary against a stored baseline (the firmware currently running on the device), identifies which 15-byte chunks have changed, and sends only those chunks as Sidewalk downlinks. The device acknowledges each chunk — and when the decode Lambda (section 4.1) sees an OTA ACK in the uplink stream, it invokes the OTA sender to send the next chunk. An EventBridge timer retries if a chunk goes unacknowledged for 30 seconds, and the whole session aborts after 5 failed retries.

For a typical one-function code change, this means 2-3 chunks over LoRa — about 5 minutes end-to-end including the LoRa uplink/downlink round-trip delays between each chunk. Full-image mode (all ~276 chunks, ~69 minutes) exists as a fallback when there's no baseline to diff against.

| Requirement | Status |
|-------------|--------|
| S3 upload triggers OTA sender Lambda | IMPLEMENTED |
| OTA ACK detection in decode Lambda triggers OTA sender | IMPLEMENTED |
| Chunk delivery: 15-byte data + 4-byte header = 19-byte MTU | IMPLEMENTED |
| Delta mode: compare against S3 baseline, send only changed chunks | IMPLEMENTED |
| Full mode: send all chunks (fallback) | IMPLEMENTED |
| Session state tracked in DynamoDB (sentinel key timestamp=-1) | IMPLEMENTED |
| EventBridge retry timer (1-minute interval) | IMPLEMENTED |
| Stale session detection (30s threshold) | IMPLEMENTED |
| Max 5 retries per chunk before abort | IMPLEMENTED |
| NO_SESSION restart: if device reboots mid-OTA, resend OTA_START (up to 3 restarts) | IMPLEMENTED |
| CloudWatch alarms for Lambda errors and missing invocations | IMPLEMENTED |
| Deploy CLI: `ota_deploy.py baseline/deploy/preview/status/abort` | IMPLEMENTED |

### 4.3 Device-Side OTA

On the device, incoming OTA chunks are written to a staging area in flash — a separate region from the running application. The device tracks which chunks it has received (via a bitfield), and only after all chunks arrive does it validate the complete image with a CRC32 checksum. If the CRC passes, the device sends a COMPLETE uplink to the cloud, waits 15 seconds for it to transmit over LoRa, then copies the new firmware from staging to the primary app partition and reboots. The entire process is designed so that the device cannot be bricked: if power is lost during the flash copy, recovery metadata survives and the next boot resumes the copy from where it left off.

In delta mode, the device receives only the chunks that differ from what's already in the primary partition. It reconstructs the full image by merging the new chunks (in staging) with the unchanged chunks (still in primary), then validates the merged result with CRC32 before applying.

| Requirement | Status |
|-------------|--------|
| OTA state machine: IDLE -> RECEIVING -> VALIDATING -> APPLYING -> reboot | IMPLEMENTED |
| CRC32 validation on complete image | IMPLEMENTED |
| Flash staging area at 0xD0000 (148KB) | IMPLEMENTED |
| Delta mode: bitfield tracking of received chunks | IMPLEMENTED |
| Delta mode: merged CRC validation (staging + primary) | IMPLEMENTED |
| Deferred apply (15s delay for COMPLETE uplink to transmit) | IMPLEMENTED |
| Recovery metadata at 0xCFF00 (survives power loss during apply) | IMPLEMENTED |
| Boot recovery: resume interrupted flash copy | IMPLEMENTED |
| Duplicate/stale START rejection (CRC match = already applied) | IMPLEMENTED |
| Pre-apply hook: stop app callbacks before flash copy | IMPLEMENTED |

### 4.4 Demand Response and Charge Scheduling

#### 4.4.1 Architecture: Delay Windows, Not Commands

The current implementation sends "pause" and "allow" commands in real time. This has a critical flaw: **if the "allow" command is lost over LoRa (which is unreliable), the device stays paused indefinitely.** A cloud outage during a pause could block charging for hours.

The revised architecture sends **delay windows** instead of commands. The cloud tells the device "don't charge between time X and time Y." The device stores the window locally and manages transitions autonomously. When the window expires, charging resumes — no cloud message needed. If connectivity is lost, the device still knows when to resume.

**How it works:**

1. The scheduler Lambda evaluates TOU schedule and WattTime MOER grid signal
2. If charging should be delayed, it sends a charge control downlink with a delay window: `[start_time, end_time]` in SideCharge epoch
3. The device stores the window in RAM (one active window at a time; new downlinks replace the previous window)
4. If `start_time ≤ now ≤ end_time`, charging is paused
5. When `now > end_time`, charging resumes automatically — no "allow" command needed
6. If the cloud sends a window with `start_time > now`, charging is allowed until the window begins

This is more resilient than the command-based approach because the device never depends on receiving a second message to resume charging. The worst case for a lost downlink is that a delay window doesn't get applied — charging happens during a period it shouldn't. The worst case for a lost command in the old architecture was charging blocked indefinitely.

#### 4.4.2 Time-of-Use (TOU) Scheduling

**TOU** (Time-of-Use) is a utility rate structure where electricity costs more during peak demand periods and less during off-peak hours. Xcel Colorado's residential TOU peak window is weekdays 5-9 PM Mountain Time. Charging during peak costs roughly 2-3x more than off-peak.

The scheduler Lambda knows the device's utility and rate schedule (from the meter number in the device registry, section 4.6). For Xcel Colorado's TOU plan, it sends a delay window each day for the 5-9 PM peak window. For devices on flat-rate plans, no TOU delay is sent.

#### 4.4.3 WattTime MOER Grid Signal

WattTime's Marginal Operating Emissions Rate (MOER) measures the carbon intensity of the next marginal unit of electricity on the grid. When MOER is high, the grid is serving new demand with dirty peaker plants. When MOER is low, renewables or efficient baseload are on the margin.

The scheduler checks MOER for the device's grid region (PSCO for Colorado). If MOER exceeds the configured threshold (default 70%), the scheduler sends a delay window for the current period. MOER-based windows are shorter (typically 15-60 minutes) and more dynamic than TOU windows.

The 70% threshold has not been validated against real PSCO data. A threshold too low over-curtails charging; too high provides little grid benefit. See Oliver REC-004 / TASK-012 for planned calibration.

#### 4.4.4 Requirements

| Requirement | Status |
|-------------|--------|
| EventBridge-triggered scheduler Lambda (configurable rate) | IMPLEMENTED |
| Xcel Colorado TOU peak detection (weekdays 5-9 PM MT) | IMPLEMENTED |
| WattTime MOER grid signal for PSCO region | IMPLEMENTED |
| Delay window downlink format (start_time, end_time) | IMPLEMENTED (TASK-063) |
| Device-side window storage and autonomous resume | IMPLEMENTED (TASK-063) |
| Per-device utility/rate schedule lookup (from device registry meter_number) | NOT STARTED |
| State deduplication: skip downlink if same window already sent | IMPLEMENTED (needs update for window format) |
| DynamoDB state tracking (sentinel key timestamp=0) | IMPLEMENTED |
| Audit log: every window sent logged with timestamps | IMPLEMENTED |
| Send charge control downlink via IoT Wireless | IMPLEMENTED |
| MOER threshold configurable (env var, default 70%) | IMPLEMENTED |

#### 4.4.5 Interaction with "Charge Now" Override

When the user presses "Charge Now" during an active delay window:

1. The AC override activates for 30 minutes (section 2.0.1.1)
2. The active delay window is **cancelled entirely** — the device deletes the stored window
3. The device sets `FLAG_CHARGE_NOW` (bit 3) in subsequent uplinks for the duration of the 30-minute AC override
4. Unplugging the car or car reaching full cancels the AC override early (and clears `FLAG_CHARGE_NOW`)

**After the 30-minute AC override expires:**
- AC priority is restored (AC can pause EVSE when compressor calls)
- Normal load-sharing resumes — EVSE and AC share the breaker per the standard interlock rules
- The scheduler's delay window is **not reinstated** — the user has opted out of demand response for this peak window

**Cloud-side protocol:**
1. The decode Lambda sees `FLAG_CHARGE_NOW=1` in an uplink
2. It writes `charge_now_override_until` to the scheduler sentinel (`timestamp=0`), set to the end of the current peak window (e.g., 9 PM for TOU peak)
3. The scheduler Lambda checks this field before sending a pause — if `now < charge_now_override_until`, it skips the downlink
4. After the peak window ends, the field expires naturally and the scheduler resumes normal control

**Example timeline (TOU peak 5-9 PM):**
| Time | Event | EVSE | AC | Scheduler |
|------|-------|------|----|-----------|
| 5:00 PM | Peak starts, scheduler sends delay window | Paused | Normal | Sent pause |
| 6:00 PM | User presses Charge Now | Charging | Suppressed | — |
| 6:30 PM | 30-min AC override expires | Load-sharing | Priority restored | Suppressed (opt-out until 9 PM) |
| 7:00 PM | AC calls | Yields to AC | Running | Suppressed |
| 7:15 PM | AC satisfied | Resumes | Off | Suppressed |
| 9:00 PM | Peak ends | Normal | Normal | Evaluates: off-peak → allow |

### 4.5 Utility Identification and Multi-Utility Support

#### 4.5.1 Problem

The charge scheduler (section 4.4) hardcodes Xcel Colorado: `WATTTIME_REGION = "PSCO"`, `is_tou_peak()` checks weekdays 5-9 PM Mountain Time. This works for the single-site v1.0 deployment but blocks multi-customer or multi-region use. Any second customer on a different utility — or even a different Xcel rate plan — gets the wrong schedule.

#### 4.5.2 Lookup Pipeline

Utility identification requires a two-step lookup, not one. The original assumption (meter number → utility → TOU) is wrong because US electric meter numbers are utility-specific with no standard format — you cannot determine the utility from the meter number alone.

The correct pipeline is:

```
install_address → utility → TOU schedule
                          → WattTime region
meter_number   → rate plan (within the utility)
```

**Step 1: Address → Utility.** The install address (collected during commissioning, stored in the device registry) identifies which utility serves the property. In v1.0 this is manually configured. In v1.1+ it could be automated via the OpenEI USURDB API (free, from NREL — 3,700+ US utilities, lookup by address or zip code, requires free API key from developer.nrel.gov).

**Step 2: Utility → TOU Schedule + WattTime Region.** Each utility maps to a TOU schedule (peak window, timezone, weekend rules) and a WattTime balancing authority region. This is a static configuration table — utilities change rate structures at most once or twice per year.

**Step 3: Meter Number → Rate Plan (optional).** Within a single utility, the meter number disambiguates the customer's rate plan. Xcel Colorado has multiple residential TOU plans (R, RE-TOU, S-EV) with different peak windows and pricing. For v1.0, we assume all Xcel customers are on the standard TOU plan. For v1.1+, the meter number (or customer self-selection) resolves the specific plan.

#### 4.5.3 TOU Schedule Data Model

Each TOU schedule is a JSON object describing the peak window:

```json
{
  "schedule_id": "xcel-co-tou-residential",
  "utility_name": "Xcel Energy",
  "utility_region": "Colorado",
  "timezone": "America/Denver",
  "watttime_region": "PSCO",
  "peak_windows": [
    {
      "name": "weekday_evening",
      "days": [0, 1, 2, 3, 4],
      "start_hour": 17,
      "end_hour": 21,
      "seasonal": false
    }
  ],
  "notes": "Weekdays 5-9 PM MT year-round. Weekends and holidays are off-peak."
}
```

The `peak_windows` array supports multiple windows (e.g., summer vs. winter) and seasonal variation. The `days` array uses Python weekday convention (0=Monday, 6=Sunday).

Reference schedules for the top 5 US residential EV utility markets:

| Utility | Region | TOU Peak | Timezone | WattTime BA | Notes |
|---------|--------|----------|----------|-------------|-------|
| Xcel Energy | Colorado (PSCO) | Weekdays 5-9 PM | America/Denver | PSCO | v1.0 target. Year-round, no seasonal variation. |
| SCE | Southern California | Weekdays 4-9 PM | America/Los_Angeles | CAISO | Some plans 5-8 PM. Summer/winter rates differ. |
| PG&E | Northern California | Weekdays 4-9 PM | America/Los_Angeles | CAISO | EV-specific plans with super off-peak midnight-3 PM. |
| SDG&E | San Diego | Weekdays 4-9 PM | America/Los_Angeles | CAISO | Similar to SCE/PG&E. |
| Con Edison | New York | Weekdays 2-6 PM (summer only, Jun-Sep) | America/New_York | NYISO | Seasonal — no TOU peak in winter. |

**Key observation**: Peak windows are remarkably consistent — 4-9 PM is the de facto standard for California utilities, Xcel is 5-9 PM, and Con Ed is the outlier at 2-6 PM summer-only. A static table covering these 5 utilities handles the vast majority of US residential EV charging deployments.

#### 4.5.4 Configuration Storage

**v1.0**: No per-device config. Hardcoded Xcel Colorado in the Lambda. One utility, one schedule.

**v1.1**: TOU schedule table in DynamoDB (`sidecharge-tou-schedules`), keyed by `schedule_id`. Device registry (TASK-036) gains a `schedule_id` field that maps each device to its TOU schedule. The charge scheduler Lambda reads the device's `schedule_id` from the registry and loads the corresponding schedule. Devices without a `schedule_id` fall back to Xcel Colorado (backward-compatible default).

**v1.1+ (optional)**: OpenEI API integration — on commissioning, automatically look up the utility from the install address and assign the default TOU schedule for that utility. Installer confirms or overrides.

#### 4.5.5 Charge Scheduler Refactor Path

The scheduler refactor for multi-utility support is tracked in TASK-037. The core decision logic (pause if TOU peak OR MOER high) doesn't change — only where the parameters come from (per-device schedule from device registry instead of hardcoded Xcel Colorado).

#### 4.5.6 Requirements

| Requirement | Status |
|-------------|--------|
| Meter number collected during commissioning | NOT STARTED |
| Meter number stored in device registry (`meter_number` field) | NOT STARTED (registry deployed, field not yet populated — TASK-037) |
| TOU schedule data model defined | DESIGNED (section 4.5.3) |
| TOU schedule table in DynamoDB | NOT STARTED |
| Charge scheduler reads per-device schedule from registry | NOT STARTED |
| Fallback to Xcel Colorado when no schedule configured | NOT STARTED (current behavior is the fallback) |
| Address → utility lookup (OpenEI API) | NOT STARTED (v1.1+) |
| Meter number → rate plan disambiguation | NOT STARTED (v1.1+) |

**Open question** (resolved): The meter number alone doesn't identify the utility — the install address does. The meter number identifies the specific rate plan within a utility. Both are captured during commissioning, but they serve different purposes in the lookup pipeline.

### 4.6 Infrastructure as Code

| Requirement | Status |
|-------------|--------|
| All AWS resources defined in Terraform | IMPLEMENTED |
| IAM roles with least-privilege policies | IMPLEMENTED |
| CloudWatch log groups with 14-day retention | IMPLEMENTED |
| S3 bucket for firmware binaries | IMPLEMENTED |
| DynamoDB with PAY_PER_REQUEST billing | IMPLEMENTED |

**Notes**: Every AWS resource is Terraform-managed. No clicking around in the console, no `aws lambda update-function-code` shortcuts. The infrastructure is the code.

### 4.6 Device Registry

Every SideCharge device needs a persistent identity in the cloud — who owns it, where it's installed, and what firmware it's running. This table is the source of truth for all device management, customer support, and fleet operations.

**Device ID**: Each device gets a short, human-readable ID derived from the Sidewalk wireless device UUID. Format: `SC-` followed by the first 8 uppercase hex characters of the SHA-256 hash of the UUID (e.g., `SC-C014EA63`). This gives ~4 billion unique values — more than enough for any realistic fleet — while being short enough to print on a device label, read over the phone to support, or type into a search box. The full Sidewalk UUID is stored in the registry for cross-referencing with AWS IoT Wireless.

**Location**: The RAK4631 has no GPS or location hardware. Device location is provided by the installer during commissioning — either a street address or lat/lon coordinates. This is the only practical approach for a fixed-installation device with no cellular or WiFi radio for network-based geolocation. The installation address also serves as the service address for the customer record.

**Two-step provisioning**: The registry record is created in two stages:

1. **Auto-provision (first uplink)**: The decode Lambda creates the record automatically when a device sends its first uplink. This sets the device_id, sidewalk_id, status, last_seen, app_version, and created_at. The device is immediately `active` and functional — no manual step is required for the device to start reporting telemetry and participating in demand response.

2. **Installation enrichment (commissioning)**: During physical installation, the electrician provides customer and site information — owner name/email, install address, meter number. This data enables utility identification (section 4.5), customer support lookup, and fleet management. Until this step is completed, the device works but lacks the context needed for per-utility TOU scheduling.

**DynamoDB Table**: `sidecharge-device-registry`

| Field | Type | Set by | Description |
|-------|------|--------|-------------|
| `device_id` (PK) | String | Auto-provision | `SC-XXXXXXXX` — derived from SHA-256 of Sidewalk UUID |
| `sidewalk_id` | String | Auto-provision | Full Sidewalk wireless_device_id UUID |
| `status` | String | Auto-provision | `active` (on first uplink) → `inactive` / `returned` |
| `app_version` | Number | Auto-update | Last known app firmware version (from OTA diagnostics) |
| `last_seen` | String (ISO 8601) | Auto-update | Timestamp of most recent uplink |
| `created_at` | String (ISO 8601) | Auto-provision | Record creation time |
| `owner_name` | String | Installation | Customer name |
| `owner_email` | String | Installation | Customer contact email (GSI key — omitted until set) |
| `meter_number` | String | Installation | Utility electric meter number (identifies utility + rate schedule) |
| `install_address` | String | Installation | Street address of installation |
| `install_lat` | Number | Installation | Latitude (optional, for future map view) |
| `install_lon` | Number | Installation | Longitude (optional, for future map view) |
| `install_date` | String (ISO 8601) | Installation | When the electrician installed the device |
| `installer_name` | String | Installation | Who performed the installation |

**GSIs**: `owner_email-index` (sparse — devices without an owner are excluded, which is correct since unowned devices shouldn't appear in "my devices" queries) and `status-index` (for fleet health queries).

| Requirement | Status |
|-------------|--------|
| DynamoDB device registry table (Terraform-managed) | IMPLEMENTED (TASK-036, TASK-049) |
| Device ID generation: `SC-` + first 8 hex chars of SHA-256(sidewalk_uuid) | IMPLEMENTED |
| Auto-provision on first uplink (device_id, sidewalk_id, status, last_seen) | IMPLEMENTED (TASK-049) |
| Decode Lambda updates `last_seen` and `app_version` on every uplink | IMPLEMENTED (TASK-049) |
| GSI on `owner_email` for "my devices" lookup (sparse) | IMPLEMENTED (TASK-036) |
| GSI on `status` for fleet health queries | IMPLEMENTED (TASK-036) |
| Device ID printed on device label for installer/support reference | NOT STARTED |
| Installation enrichment tool (add owner, address, meter number) | NOT STARTED |
| Installer provides location during commissioning | NOT STARTED |

The decode Lambda (section 4.1) calls `get_or_create_device()` and `update_last_seen()` on every uplink (best-effort, never blocks event processing). On the first uplink from an unknown device, the record is created and the Lambda logs "Auto-provisioned device SC-XXXXXXXX". Subsequent uplinks update `last_seen` and optionally `app_version` (when the uplink is a diagnostics response). Installation fields (owner, address, meter number) are added separately during commissioning — these fields are omitted from the auto-provisioned record because DynamoDB rejects empty strings as GSI key attributes.

---

## 5. Operational Requirements

### 5.1 Firmware Update

SideCharge uses a split-image firmware architecture with two independent binaries sharing the same flash chip:

- **Platform** (576KB at 0x00000): The generic runtime — Zephyr RTOS, Amazon Sidewalk BLE and LoRa stacks, OTA engine, shell, and hardware drivers. This is the "operating system" of the device. It knows nothing about EV charging, thermostats, or interlock logic. The platform is large, stable, and rarely changes. It can only be updated via a physical USB programmer (pyOCD) — which means it can be flashed during development and at the factory, but **never after the device is deployed**.

- **App** (4KB at 0x90000): All the EVSE and interlock domain logic — sensor reading, change detection, payload formatting, charge control, shell commands. This is tiny, changes frequently, and is **OTA-updatable over LoRa**. A typical delta update takes 2-3 chunks and completes in ~5 minutes (dominated by LoRa round-trip delays between chunks).

The key implication: once a device leaves the factory, the only firmware that can be changed is the app. The platform is frozen. This means platform bugs require a physical recall, while app bugs can be patched remotely in minutes.

During development and factory provisioning, all firmware is flashed via USB (pyOCD). After deployment, everything happens over the air via Amazon Sidewalk LoRa.

| Requirement | Status |
|-------------|--------|
| App-only OTA over Sidewalk LoRa (no physical access needed) | IMPLEMENTED |
| Platform update requires physical programmer (pyOCD) | IMPLEMENTED |
| Delta OTA for fast incremental updates (~5 min for 2-3 chunks) | IMPLEMENTED |
| Full OTA fallback (~69 minutes) | IMPLEMENTED |
| Recovery from power loss during apply | IMPLEMENTED |
| OTA recovery runbook / operator documentation | IMPLEMENTED (TASK-008) |

**Notes**: The OTA recovery runbook is documented at `docs/ota-recovery.md` (TASK-008).

### 5.2 Device Provisioning

| Requirement | Status |
|-------------|--------|
| Sidewalk credentials in MFG partition (0xFF000) | IMPLEMENTED |
| `credentials.example/` template directory | IMPLEMENTED |
| Flash script for all partitions (`flash.sh all`) | IMPLEMENTED |
| Provisioning documentation (step-by-step workflow) | IMPLEMENTED (TASK-011) |

**Notes**: Provisioning workflow is documented at `docs/provisioning.md` (TASK-011).

### 5.3 Observability

Observability requirements split into two distinct contexts: development/testing (USB available) and production (cloud-only, no physical access).

#### 5.3.1 Development and Testing (USB Serial Available)

During development, the USB serial console provides full real-time visibility into device state. See section 2.6 for the complete CLI command reference.

| Requirement | Status |
|-------------|--------|
| Shell diagnostics over USB serial (see section 2.6 CLI Commands) | IMPLEMENTED |
| Sidewalk init status tracking (7 states, shell-queryable) | IMPLEMENTED |
| MFG key health check at boot | IMPLEMENTED |
| All sensor values queryable via shell | IMPLEMENTED |
| OTA state machine queryable via shell | IMPLEMENTED |

#### 5.3.2 Production (Cloud-Only, No Physical Access)

Once deployed, the device has no USB port, no serial console, and no local interface. All observability comes through the cloud — what the device reports in its 12-byte uplink payload, and what the Lambdas log. This is a significantly narrower view than development, and it needs to be more robust.

| Requirement | Status |
|-------------|--------|
| CloudWatch Lambda logs (14-day retention) | IMPLEMENTED |
| CloudWatch alarms for OTA sender errors and stalls | IMPLEMENTED |
| Cloud-side OTA status monitoring (`ota_deploy.py status`) | IMPLEMENTED |
| DynamoDB event history (queryable per device) | IMPLEMENTED |
| Device heartbeat (detects liveness via periodic uplinks) | IMPLEMENTED (15-min production via TASK-070, 60s development) |
| Dashboard or alerting for device offline detection | IMPLEMENTED (TASK-029) |
| Remote device state query (request status uplink on demand) | IMPLEMENTED (TASK-029) |
| OTA failure alerting (notify operator when OTA stalls or aborts) | IMPLEMENTED (TASK-029) |
| Interlock state change logging (cloud-side, from uplink payload) | IMPLEMENTED (TASK-069) |

Production observability is implemented across Tier 1 (CloudWatch alarms for device offline, OTA failures, daily health digest Lambda) and Tier 2 (remote status query via 0x40 downlink / 0xE6 response). See TASK-029.

#### 5.3.3 Production Observability Options

Three tiers of increasing capability. Each tier builds on the previous one.

**Tier 1 — Cloud Alerting (minimum viable, cloud-only changes)**

No device firmware changes. Uses existing uplink data and AWS infrastructure.

| Capability | How | Effort |
|------------|-----|--------|
| **Device offline detection** | CloudWatch metric filter on DynamoDB writes per device_id. Alarm when no write for 2× heartbeat interval. SNS notification to operator. | S — Terraform + CloudWatch config |
| **OTA failure alerting** | CloudWatch alarm on OTA sender Lambda errors or stalled sessions (no ACK for >5 min). SNS notification. | S — already partially implemented |
| **Interlock state change logging** | Decode Lambda already stores thermostat flags. Add a CloudWatch metric filter for cool_call transitions. Dashboard widget showing interlock activations per day. | S — Lambda + CloudWatch config |
| **Daily health digest** | Scheduled Lambda that queries DynamoDB for all devices, checks last-seen timestamp, firmware version, error counts. Sends summary email. | M — new Lambda |

**Tier 2 — Remote Status Query (requires device firmware change)**

Adds a new downlink command `0x40` that triggers an on-demand uplink with extended diagnostics. Triggering is available both manually (operator sends `0x40` via `aws iot` CLI or `sidewalk_utils.send_sidewalk_msg()`) and automatically (health digest Lambda sends `0x40` to unhealthy devices, TASK-073 implemented).

| Capability | How | Effort |
|------------|-----|--------|
| **Remote status request** | New downlink command `0x40` (1 byte, no arguments). Device responds immediately with an extended diagnostics uplink (magic `0xE6`). Manual trigger only for v1.0. | M — firmware + decode Lambda + CLI |
| **Extended diagnostics payload** | Magic `0xE6`, 14 bytes total. Sent only on request, not on every heartbeat. Fits within 19-byte MTU. | M — firmware + decode Lambda |

Extended diagnostics wire format (`0xE6`, 14 bytes):

| Byte(s) | Field | Type | Description |
|---------|-------|------|-------------|
| 0 | magic | uint8 | `0xE6` |
| 1 | diag_version | uint8 | `0x01` |
| 2–3 | app_version | uint16_le | `APP_CALLBACK_VERSION` |
| 4–7 | uptime_seconds | uint32_le | from `uptime_ms()/1000` |
| 8–9 | boot_count | uint16_le | 0 until persistent storage added (TASK-072+) |
| 10 | last_error_code | uint8 | Highest-priority active fault flag, or 0 |
| 11 | state_flags | uint8 | Live device state snapshot (see below) |
| 12 | event_buffer_pending | uint8 | Unsent events in ring buffer |
| 13 | reserved | uint8 | `0x00` |

`state_flags` bit definitions:

| Bit | Meaning |
|-----|---------|
| 0 | Sidewalk connected |
| 1 | Charge allowed (relay state) |
| 2 | Charge Now active |
| 3 | Interlock active (AC demand blocking charge) |
| 4 | Selftest passed |
| 5 | OTA in progress |
| 6 | Time synced (has valid epoch) |
| 7 | Reserved |

**Status**: Tier 1 (CloudWatch alarms, daily health digest) and Tier 2 (remote status query 0x40/0xE6) are implemented (TASK-029). Automated diagnostic queries (health digest sends 0x40 to unhealthy devices) are also implemented (TASK-073). BLE diagnostics are out of scope — BLE stays disabled post-registration (see 6.3.2).

### 5.4 Testing

#### Testing Architecture

SideCharge uses a three-layer testing pyramid: host-side C unit tests for device firmware, pytest for cloud Lambdas, and serial integration tests for on-device verification.

**Grenning Dual-Target Pattern (C unit tests)**: The key architectural insight is that the app layer talks to the platform exclusively through a `struct platform_api` of function pointers (see `include/platform_api.h`). On the device, these point to real Zephyr/Sidewalk implementations. On the host (macOS/Linux), they point to mock implementations (`mock_platform_api.c`) that simulate ADC readings, GPIO states, flash memory (RAM-backed), and Sidewalk send calls. The same app source files compile and run on both targets with zero `#ifdef`s — only the platform implementation changes.

This means every app-layer function (sensor reading, charge control decisions, payload encoding, shell command dispatch, OTA state machine) can be tested on the developer's laptop in milliseconds, without hardware.

**Mock infrastructure**: Mocks are hand-written and checked into git (`tests/mocks/`). They include a full set of Zephyr header stubs (`kernel.h`, `device.h`, `logging/log.h`, `drivers/flash.h`, `sys/reboot.h`, `sys/crc.h`) and platform-level mocks with controllable state globals (`mock_adc_values[]`, `mock_gpio_values[]`, `mock_uptime_ms`, `mock_last_send_buf[]`). Tests manipulate these globals directly to set up scenarios and verify behavior.

**Python Lambda tests**: Each Lambda has its own test file using pytest and `unittest.mock`. The `conftest.py` patches `boto3` and `sidewalk_utils` at module import time (before the Lambda code loads) to prevent any real AWS calls. Tests verify payload decoding, OTA state machine transitions, retry logic, and charge scheduling decisions.

#### Test Coverage

| Layer | Framework | Tests | What's Covered |
|-------|-----------|-------|----------------|
| C unit tests (app layer) | Unity + CMake | 90 | J1772 state machine, voltage thresholds, charge control, thermostat inputs, payload encoding, downlink parsing, shell commands |
| C unit tests (platform) | Unity + CMake | ~33 | OTA state machine, chunk receive, delta bitmap, CRC validation, MFG key health, flash recovery |
| Python Lambda tests | pytest | 94 | Decode Lambda (18), charge scheduler (19), OTA sender (27), OTA deploy CLI (16), Lambda chain integration (14) |
| Serial integration | pytest + serial | 7 | Live device shell commands, state simulation, charge control |
| CI/CD | GitHub Actions | -- | Lint (cppcheck), C tests (CMake + Grenning), Python tests (pytest + ruff) |

#### Test Runner Commands

```
# C unit tests (CMake — recommended):
cmake -S rak-sid/tests -B tests/build && cmake --build tests/build && ctest --test-dir tests/build --output-on-failure

# C unit tests (Grenning legacy Makefile):
make -C rak-sid/app/rak4631_evse_monitor/tests/ clean test

# Python Lambda tests:
python3 -m pytest rak-sid/aws/tests/ -v

# Serial integration (requires device on /dev/tty.usbmodem101):
pytest rak-sid/tests/integration/test_shell.py --serial-port /dev/tty.usbmodem101 -v
```

#### Gaps

| Gap | Status |
|-----|--------|
| Charge scheduler Lambda tests | IMPLEMENTED (19 tests) |
| Decode Lambda tests | IMPLEMENTED (18 tests) |
| OTA recovery path tests (safety-critical) | NOT STARTED |
| CI/CD pipeline running on push/PR | IMPLEMENTED (`.github/workflows/ci.yml`) |
| E2E test plan (device -> cloud -> device round-trip) | NOT STARTED |
| Field reliability testing across RF conditions | NOT STARTED |
| Interlock logic integration tests (HW+SW combined) | NOT STARTED |

The code that exists is well-tested. The code that matters most for safety -- OTA recovery and interlock hardware integration -- is not. The CI pipeline runs all lint, C, and Python tests on push to main and feature branches, but there is no automated way to test the device-cloud round-trip or the physical interlock behavior.

---

## 6. Non-Functional Requirements

### 6.1 Power

| Requirement | Status |
|-------------|--------|
| USB-powered (not battery) | IMPLEMENTED |
| 24VAC-powered from AC system transformer (production target) | NOT STARTED |
| No specific power budget target | N/A |

SideCharge v1.0 is USB-powered via the RAK4631 USB-C port. The production design will draw power from the AC system's 24VAC transformer -- the same source that powers the thermostat -- eliminating the need for a separate power supply or outlet in the installation location. A small AC-DC converter (24VAC to 3.3VDC) is the only additional component required. Battery optimization is not a concern for a hardwired device.

### 6.2 Reliability

| Requirement | Status |
|-------------|--------|
| OTA recovery from power loss during apply | IMPLEMENTED |
| Lost ACK recovery via cloud retry timer | IMPLEMENTED |
| Stale session detection and abort | IMPLEMENTED |
| Heartbeat for liveness detection (15-min) | IMPLEMENTED (TASK-070) |
| Quantified reliability targets (uptime %, delivery rate) | NOT STARTED |
| Field-tested under real LoRa conditions | NOT STARTED |

The system has multiple recovery mechanisms -- OTA power-loss recovery, cloud-side retries, stale session cleanup, and a 15-minute heartbeat (TASK-070). What it does not have yet is quantified reliability targets or field data from real LoRa conditions. The device has been tested on a desk 20 feet from a gateway, not in a detached garage 200 meters away through two walls.

### 6.3 Security

#### 6.3.1 What's Implemented

| Requirement | Status |
|-------------|--------|
| Sidewalk protocol encryption (built-in) | IMPLEMENTED |
| MFG credentials in dedicated flash partition | IMPLEMENTED |
| HUK for PSA crypto key derivation | IMPLEMENTED |
| MFG key health check (detect empty/missing keys) | IMPLEMENTED |
| OTA image CRC32 validation | IMPLEMENTED |
| OTA image cryptographic signing (ED25519) | IMPLEMENTED (TASK-031/045) |
| Credential rotation procedure | NOT STARTED |
| Rate limiting on cloud-to-device commands | NOT STARTED |
| Fleet-wide command throttling | NOT STARTED |

Sidewalk provides transport-layer encryption, and PSA crypto handles on-device key derivation from the hardware unique key. OTA images are validated by CRC32 and ED25519 cryptographic signature (TASK-031/045). The device-side signature verification is in `ota_signing.c` and the cloud-side signing is in `ota_signing.py`.

#### 6.3.2 Threat Model: What Could Go Wrong

SideCharge controls two high-power loads (30-50A each). A compromised device or cloud doesn't just leak data -- it switches physical loads on and off. The failure modes are more serious than a typical IoT device:

**Double-load on a single breaker**: If the interlock fails (both loads enabled simultaneously), the circuit draws 60-100A through a 30-50A breaker. The breaker trips. This is the designed safety net -- the breaker is the last line of defense. But frequent breaker trips damage the breaker itself, and a breaker that fails to trip is a fire hazard. The hardware interlock (section 2.0.1) prevents this even if the software is compromised, because mutual exclusion is enforced in the circuit itself. **A software-only interlock is not acceptable for production.**

**Fleet-wide coordinated load switching**: This is the utility-scale risk. If an attacker compromises the cloud layer (Lambda, IoT Wireless, or the Sidewalk network itself), they could send simultaneous delay windows or overrides to every SideCharge device in a service area. Thousands of AC compressors or EV chargers turning on or off at the same instant creates a massive demand spike or drop on the distribution grid. This is not hypothetical -- grid operators specifically model "cold load pickup" scenarios where thousands of AC units restart simultaneously after a power outage. A compromised fleet could trigger this artificially.

This is an **open design question** (PDL-OPEN-005). The core challenge: any mitigation that lives in the cloud layer (staggered delays, rate limiting in Lambda, CloudWatch anomaly detection) can be bypassed by an attacker who has compromised that same cloud layer. A complete solution must include protections that survive cloud compromise — meaning enforcement on the device itself, or cryptographic controls whose keys are not accessible from the cloud. Possible directions to explore:

- **Device-side rate limiting**: The device enforces a minimum interval between charge control state changes, regardless of command source. This survives cloud compromise since it runs in firmware.
- **Downlink command authentication**: Cloud commands carry a cryptographic signature the device verifies before acting. The signing key must be held separately from the cloud infrastructure (e.g., HSM, separate AWS account, or offline key). Without the signing key, a compromised cloud cannot forge valid commands.
- **Behavioral limits**: The device refuses to change charge state more than N times per hour, providing a hard ceiling on damage even if individual commands are authenticated.
- **Out-of-band monitoring**: Anomaly detection that lives in a separate trust domain from the main cloud infrastructure (different AWS account, third-party service) so it cannot be silenced by the same compromise.

The right design likely combines several of these. The key constraint is that **any layer that can be compromised by the attacker must not be the sole line of defense**. See TASK-030 for the design investigation.

**Malicious firmware via OTA**: ED25519 image signing (TASK-031/045) mitigates this risk. The device verifies the cryptographic signature of every OTA image before applying it. A compromised S3 bucket or Lambda cannot push unsigned or incorrectly signed firmware. The signing keys are provisioned in the MFG store and the private key is held offline.

**Physical access**: After factory sealing, the device has no USB port and no debug headers. Physical tampering requires opening the enclosure. This is adequate for residential deployment but should be documented in the installation guide (tamper-evident sealing, mounting location guidance).

**BLE disabled post-registration**: The nRF52840 supports BLE, and Sidewalk uses it during initial device registration (key exchange with a nearby gateway). After registration completes, BLE is disabled in firmware — the device communicates exclusively over LoRa. This eliminates BLE as a local attack surface. An attacker with physical proximity cannot pair with, interrogate, or send commands to the device over BLE. If BLE diagnostics are added in a future version (see PDL-OPEN-004), they should require a physical button press to activate and auto-disable after a timeout.

#### 6.3.3 Security Gaps Summary

| Risk | Severity | Mitigation | Status |
|------|----------|------------|--------|
| Malicious OTA firmware | High | ED25519 image signing | IMPLEMENTED (TASK-031/045) |
| Fleet-wide coordinated command | High | Open design question (PDL-OPEN-005) — must survive cloud compromise | NOT STARTED |
| Cloud compromise → load switching | High | Command authentication (HMAC-SHA256 signed downlinks) | IMPLEMENTED (TASK-032) |
| Double-load (interlock bypass) | Critical | Hardware interlock (circuit-level mutual exclusion) | IMPLEMENTED (HW) |
| Credential theft from flash | Medium | HUK encryption + PSA key derivation | IMPLEMENTED |
| Man-in-the-middle | Low | Sidewalk transport encryption | IMPLEMENTED |
| Physical tampering | Low | Sealed enclosure, no debug ports post-factory | DESIGNED |
| BLE local attack | Low | BLE disabled after initial registration; LoRa-only in production | IMPLEMENTED |

### 6.4 Privacy, Account, and Location

SideCharge collects and stores data that falls into three categories with different sensitivity levels:

**Behavioral data** — When the AC compressor runs, when the EV charges, how long each session lasts. Over time this reveals occupancy patterns (home vs. away), daily routines, and vehicle usage. This data lives in DynamoDB as timestamped telemetry events.

**Personal data** — Owner name, email, installation address, and installer identity (section 4.6 device registry). Standard PII that requires baseline protections.

**Utility account data** — To apply the correct TOU schedule and grid signal, SideCharge needs to know which utility serves the device and which rate schedule applies. Today this is hardcoded (Xcel Colorado, PSCO region — section 4.4). For multi-utility support, we need a per-device utility identifier.

#### 6.4.1 Utility Identification

The electric meter number is the most precise identifier for a utility service point. It maps directly to a utility, a rate schedule, and a grid region — all of which the charge scheduler needs. The install address alone is ambiguous (multi-meter properties, utility territory boundaries that don't follow street addresses, different rate classes at the same address). The meter number resolves all of these.

The meter number would be collected during commissioning (printed on the meter, the electrician is standing right next to it) and stored in the device registry alongside the install address. The charge scheduler Lambda would look up the device's meter number → utility → TOU schedule instead of using a hardcoded region.

| Requirement | Status |
|-------------|--------|
| Meter number collected during commissioning | NOT STARTED |
| Meter number → utility/rate schedule lookup | NOT STARTED |
| Charge scheduler uses per-device utility config (not hardcoded) | NOT STARTED |
| Meter number stored in device registry (`meter_number` field) | NOT STARTED |

**Open question**: The meter number alone doesn't tell us the rate schedule — the same utility may have multiple residential TOU plans (e.g., Xcel's R, RE-TOU, S-EV). We may need the customer to confirm their plan, or query the utility's API if available. For v1.0 with a single utility, this is not urgent.

#### 6.4.2 Data Privacy Requirements

| Requirement | Status |
|-------------|--------|
| DynamoDB encryption at rest (AWS default) | IMPLEMENTED |
| Sidewalk transport encryption (in transit) | IMPLEMENTED |
| No PII stored on the device itself | IMPLEMENTED (device has no concept of owner/address) |
| PII access limited to authenticated operators (IAM) | IMPLEMENTED (AWS IAM) |
| No PII in CloudWatch logs (no logging of owner name, email, address) | NOT VERIFIED |
| Data retention policy (how long telemetry is kept) | NOT STARTED |
| Customer data deletion on device return/decommission | NOT STARTED |
| Privacy policy document for customers | NOT STARTED |
| CCPA/state privacy law compliance review | NOT STARTED |

The device itself stores zero PII — it knows nothing about its owner, location, or meter number. All personal data lives in the cloud (DynamoDB device registry). This is a deliberate architectural choice: a stolen or discarded device reveals nothing about the customer.

The behavioral data (AC/EV run patterns) is the harder privacy problem. Even without a name attached, timestamped load-switching data at a known address is identifying. A data retention policy should limit how long raw telemetry is kept — aggregated statistics (daily kWh, monthly AC hours) can persist, but per-minute state transitions should expire.

### 6.4 Warranty and Liability

#### 6.4.1 The Risk

SideCharge's core function requires physically sitting inline on two control circuits that belong to other manufacturers' equipment:

1. **J1772 pilot wire (EVSE ↔ vehicle)**: SideCharge breaks the pilot wire and passes the signal through inline (PILOT-in from the EVSE, PILOT-out to the vehicle connector). It actively manipulates the outgoing signal -- inserting ~900 ohm resistance on PILOT-out to force the charger into State B (pause charging), and potentially driving PWM to 0% for cloud override. This is not passive monitoring. It is active modification of the signaling protocol between the charger and the car.

2. **Thermostat call wires (thermostat ↔ compressor contactor)**: SideCharge breaks both the cool call (Y) and heat call (W) wires, passing them through inline (Y-in/Y-out, W-in/W-out). When the interlock needs to block the compressor, it opens the pass-through relays on both circuits.

Both are modifications to control circuits that belong to other manufacturers' equipment. The EVSE manufacturer, the vehicle manufacturer, and the HVAC manufacturer could each argue that a third-party device inserted into their control wiring constitutes unauthorized modification that voids the warranty.

#### 6.4.2 Risk Assessment by Circuit

**EVSE warranty — HIGH risk.** Most residential EVSE manufacturers (ChargePoint, JuiceBox, Wallbox, Emporia, Tesla Wall Connector, etc.) include warranty terms that exclude damage caused by "improper installation" or "unauthorized modification." Inserting a third-party device into the pilot wire is not an installation method contemplated by any EVSE manufacturer's installation manual. If the EVSE develops a fault — even one unrelated to SideCharge — the manufacturer could point to the pilot wire modification and deny the claim.

There is also a mechanical wear concern: SideCharge cycles the EVSE's internal relay (which disconnects AC power when it sees State B) more frequently than normal use. A car that normally charges uninterrupted overnight might now see 5-10 State B → State C transitions per day as the interlock and demand response toggle charging. Accelerated relay wear is a plausible SideCharge-caused defect, not just a warranty technicality.

**Vehicle warranty — MEDIUM risk.** SideCharge's pilot manipulation sends valid J1772 signals — the vehicle's onboard charger sees standard State B (pause) and State C (charge) transitions that it was designed to handle. The car doesn't know a third-party device is involved. However, if a charging-related defect occurs (onboard charger failure, battery management issue), a vehicle manufacturer could investigate the charging history and discover non-standard pilot signaling patterns. The risk is lower because the signals are protocol-compliant, but it is not zero.

**HVAC warranty -- LOW risk.** Thermostat replacement and wiring modification is standard homeowner/contractor practice. HVAC manufacturers generally don't void warranties for thermostat-side changes. SideCharge sits inline on the thermostat call wires (Y and W), passing signals through or blocking them entirely. The thermostat's built-in short-cycle protection timer is upstream of the SideCharge break, so it remains in the loop. For heat pump installations, blocking both Y-out and W-out prevents the compressor from running in either mode -- the reversing valve state is irrelevant because the compressor itself is de-energized.

#### 6.4.3 Legal Protection: Magnuson-Moss Warranty Act

The federal Magnuson-Moss Warranty Act (15 U.S.C. §§ 2301-2312) is SideCharge's primary legal defense:

- A manufacturer **cannot void a warranty** simply because a third-party product was installed
- The manufacturer **must prove** that the third-party product **caused** the specific defect being claimed
- "Tie-in sales provisions" (requiring OEM-only accessories) are generally prohibited for consumer products

This is the same law that protects aftermarket car parts, third-party phone accessories, and non-OEM components across industries. SideCharge has a strong legal position under MMWA for defects unrelated to the pilot wire or thermostat modifications.

**Where MMWA doesn't help**: If SideCharge's modifications actually cause the defect -- accelerated relay wear from frequent State B cycling, signal integrity issues from the inline pilot pass-through, or a wiring error during installation -- the manufacturer has a legitimate basis for denial. MMWA protects against blanket voiding, not against genuine causation.

#### 6.4.4 Mitigation Strategies

| # | Strategy | Effort | Risk Reduction | Timeline |
|---|----------|--------|----------------|----------|
| 1 | **J1772 protocol compliance** — All pilot manipulations use valid J1772 signaling (standard State A-F voltage levels, standard PWM). The EVSE and vehicle never see out-of-spec signals. | Done | Medium | v1.0 (current) |
| 2 | **Professional installation only** — Licensed electrician installs with commissioning checklist. Documented wiring, verified connections, installer sign-off. Reduces risk of installation-caused defects. | Low | Medium | v1.0 |
| 3 | **Customer disclosure** — Installation documentation explicitly states SideCharge modifies the EVSE pilot circuit and thermostat call wire, explains MMWA protections, recommends checking EVSE/vehicle warranty terms before installation. | Low | Low (legal CYA) | v1.0 |
| 4 | **Product liability insurance** — General commercial liability + product liability coverage for claims arising from SideCharge installations. | Medium ($2-5K/yr) | Medium (financial protection) | Pre-customer deployment |
| 5 | **Reversible connector design** -- The v1.0 inline pass-through architecture (PILOT-in/PILOT-out) requires cutting the pilot wire. A v1.1 improvement would use standard connectors (e.g., quick-disconnect or adapter plugs) so the EVSE's original wiring is unmodified and removal restores the original circuit instantly. Similarly, thermostat Y/W pass-through could use lever-cage connectors for tool-free reversibility. | Medium | High | v1.1 |
| 6 | **Relay cycle logging** — Track how many State B ↔ State C transitions SideCharge causes per day/week. If an EVSE relay fails, we have data showing whether SideCharge's cycling was within the relay's rated lifetime. | Low | Medium (evidentiary) | v1.1 |
| 7 | **EVSE manufacturer partnerships** — Approach 1-2 EVSE manufacturers (Emporia and OpenEVSE are most accessible) for explicit compatibility acknowledgment or co-testing. | High | Very High | v1.1+ |
| 8 | **OCPP software integration** — For OCPP-capable EVSEs, control charging via the EVSE's own management API instead of hardware pilot manipulation. Eliminates the pilot wire modification entirely for compatible chargers. | High | Very High | v2.0 |

#### 6.4.5 Recommended Phased Approach

**v1.0 (current — early adopters, installer-owned properties)**:
Accept the warranty risk. Mitigate with strategies 1-3: protocol compliance (done), professional installation (planned), customer disclosure (add to commissioning docs). The first installations are on properties where the installer understands and accepts the tradeoff.

**Pre-customer deployment (before any non-installer customer)**:
Add strategy 4: product liability insurance. Non-negotiable before any customer installation. Budget $2-5K/year for a small commercial general liability + product liability policy. An insurance broker with IoT/hardware product experience can quote this.

**v1.1**:
Add strategies 5-6: reversible connector and relay cycle logging. The reversible connector is the single highest-impact mitigation — it changes the installation from "modified your EVSE wiring" to "plugged in an adapter." This fundamentally shifts the warranty argument. Relay cycle logging provides evidentiary defense if an EVSE relay fails.

**v1.1+ (growth phase)**:
Add strategy 7: EVSE manufacturer partnerships. Start with Emporia (small, US-based, receptive to integrations) or OpenEVSE (open-source hardware, community-driven). A single manufacturer's explicit compatibility acknowledgment validates the product category.

**v2.0 (scale)**:
Add strategy 8: OCPP integration. This is the long-term answer for compatible EVSEs. OCPP adoption is growing rapidly — ChargePoint, JuiceBox, Wallbox, and OpenEVSE all support it. Software-only control eliminates the pilot wire modification entirely. Hardware pilot control remains as the universal fallback for non-OCPP chargers.

#### 6.4.6 Open Questions

1. **EVSE relay wear**: Does SideCharge's cycling pattern (5-10 transitions/day from demand response + interlock) exceed the EVSE's internal relay rated lifetime? Most contactors are rated for 100K+ operations, which would be 27+ years at 10/day. But cheap relays in consumer EVSEs may have lower ratings. **Need to check spec sheets for target EVSE models.**

2. **Product positioning**: Should SideCharge be marketed as an "EVSE accessory" or an "energy management system"? The framing affects how EVSE manufacturers perceive the relationship. An "accessory" implies subordinate to the EVSE; an "energy management system" implies independent and authorized by code (NEC Article 750).

3. **Insurance scope**: Does product liability insurance cover warranty claims that a customer brings against us (i.e., "SideCharge told me it was safe and my EVSE warranty was voided"), or only claims for physical damage? Need to confirm scope with an insurance broker.

4. **State lemon laws and implied warranty**: Some states extend implied warranty protections beyond the manufacturer's written warranty. If SideCharge causes a defect within the implied warranty period, state law may provide the customer additional remedies. This varies by state and needs legal review.

---

## 7. Scope Boundaries

### 7.1 In Scope for v1.0

- **Circuit interlock**: mutual exclusion of AC and EV charger loads, AC priority, cloud override (EV charger)
- Single-device, single-site deployment
- J1772 monitoring, current sensing, thermostat inputs
- Charge control via Sidewalk downlink
- Demand response (TOU + WattTime MOER)
- App-only OTA over LoRa with delta mode
- CLI diagnostics over USB (development/testing only -- see section 2.6)
- AWS cloud infrastructure (Terraform-managed)
- Device registry (ownership, location, firmware version, liveness tracking)
- Host-side unit tests

### 7.2 Out of Scope for v1.0

- Multi-device fleet management
- User-facing mobile app or web dashboard
- Battery-powered operation
- BLE-only mode (LoRa is the primary link)
- On-device data logging (the device is stateless; the cloud stores everything)
- OCPP (Open Charge Point Protocol) integration
- Solar/battery storage integration
- Multi-tariff or multi-utility TOU schedules (Xcel Colorado only for v1.0 — data model designed in 4.5, implementation is v1.1)

### 7.3 Where SideCharge Goes Next

These are the natural extensions once v1.0 is proven in the field:

- **PCB production**: Move from protoboard to a manufactured PCB with proper isolation, 24VAC power supply, three relay outputs (charge block, Y-out pass-through, W-out pass-through), inline pilot pass-through circuit, and current transducer interface
- **Multi-unit manufacturing**: BOM, assembly, and test procedures for producing devices beyond one-off prototypes
- **UL/safety certification**: Required for any product that switches AC control signals and interfaces with EV charger equipment
- Fleet provisioning and management tooling for multi-device deployments
- Real-time monitoring dashboard (DynamoDB -> API Gateway -> frontend)
- Battery-powered variant with sleep modes and a longer heartbeat interval
- Additional utility TOU schedules beyond Xcel Colorado
- OCPP gateway for commercial EVSE integration

### 7.4 TASK-019: PCB Design Scope

**Status**: Scoped (2026-02-12)
**Owner**: Emily (hardware), Pam (product requirements)
**Estimated complexity**: XL -- this is the single largest dependency for moving from prototype to production

TASK-019 transitions SideCharge from a protoboard wired to a RAK4631 dev kit into a manufactured PCB that can be enclosed, installed by an electrician, and eventually certified. Everything downstream -- UL submission, multi-unit manufacturing, field deployment -- is blocked until this board exists.

#### 7.4.1 What the PCB Must Do

Every requirement below is traced to a PRD section. The PCB must implement all hardware functions currently on the protoboard, plus the production-only additions that the protoboard cannot support.

**Power supply** (PRD 6.1, 2.0.3)
- 24VAC to 3.3VDC power supply, drawing from the AC system's thermostat transformer. This replaces USB power (development only). The 24VAC source is already present at the installation location -- no separate outlet or adapter needed.
- The converter must be galvanically isolated from the 240VAC charging circuit (PRD 2.0.4). The 24VAC thermostat circuit shares a common ground with the AC compressor's 240V circuit in some installations, so the isolation barrier must be between the 24VAC input and the 3.3V MCU domain.
- Estimated MCU + radio power budget: ~50mA continuous at 3.3V during LoRa TX, ~15mA idle. The 24VAC transformer in a typical AC system can supply 1-2A at 24VAC (40-48VA), so SideCharge's draw (~0.17W) is negligible.

**Interlock circuit** (PRD 2.0.1, 2.0.3, 2.0.4)
- Hardware mutual exclusion: the circuit must prevent simultaneous operation of the compressor and EV charger independently of the microcontroller. If the MCU loses power or crashes, the hardware interlock must continue to enforce mutual exclusion. This is the fundamental safety guarantee (PRD 6.3.2).
- Three relay outputs:
  - **Charge block relay** (PRD 2.4): Controls the J1772 pilot spoof circuit on the PILOT-out line. When the MCU drives charge_block HIGH, the relay engages and inserts ~900 ohm resistance on PILOT-out, making the charger see J1772 State B (connected, not ready) and stop supplying power. When charge_block is LOW (or MCU loses power), the hardware safety gate controls the relay independently. GPIO P0.17 (IO1) on WisBlock prototype, active high = blocking. Production PCB pin TBD.
  - **Y-out pass-through relay** (PRD 2.0.3, 2.3): Sits inline between Y-in and Y-out. When closed, the cool call signal passes through to the compressor contactor. When open, the cool call is blocked. This is the primary AC interlock relay in v1.0.
  - **W-out pass-through relay** (PRD 2.0.3, 2.3): Sits inline between W-in and W-out. When closed, the heat call signal passes through to the compressor contactor. When open, the heat call is blocked. **This relay is a v1.0 hardware requirement** -- it must be wired and controlled by the hardware interlock even though firmware reading of W-in is v1.1 scope. This ensures heat pump installations are safe from day one without a board respin. Pin TBD.
- The hardware interlock logic must be implemented so that the charge block relay and both thermostat pass-through relays cannot simultaneously be in the "active load" state. When EV current is flowing, both Y-out and W-out must be open (compressor blocked in both heating and cooling modes). When either thermostat call is active, the charge block relay must engage (EV paused). This can be achieved through relay wiring topology (series/parallel constraints) or through a simple logic gate -- the key requirement is that it works without firmware.
- Fail-safe default: on power loss, charge_block GPIO floats LOW (not blocking -- hardware safety gate controls EV relay), Y-out and W-out relays de-energize and close (thermostat signals pass through to compressor). Both loads off simultaneously is safe. Both loads on simultaneously must be physically impossible.

**Analog inputs** (PRD 2.0.3, 2.1, 2.2)
- **J1772 Cp voltage** (AIN7 on WisBlock prototype; production PCB pin TBD): Reads pilot voltage level to classify car presence/state (A-F). The pilot signal is +/-12V, 1kHz square wave. The ADC input circuit must condition this to 0-3.3V range for the nRF52840's 12-bit SAR ADC. Isolation from the J1772 pilot circuit is required (PRD 2.0.4).
- **J1772 Cp PWM duty cycle** (AIN7, same pin): The same physical signal also encodes maximum allowed current as a duty cycle percentage per J1772 spec. The ADC samples both voltage and timing -- no separate hardware needed, but the conditioning circuit must preserve the square wave shape well enough for duty cycle measurement. (TASK-022, NOT STARTED in firmware.)
- **Current clamp** (AIN0 on production PCB; not available on WisBlock prototype — no second analog pin on RAK19007 J11 header): Analog voltage proportional to EV charger current. The v1.0 range is 0-48A (PDL-014), covering a 60A circuit at 80% continuous. The clamp selection and resistor divider must produce 0-3.3V across this range. Higher ranges (80A for Ford Charge Station Pro) are a future resistor divider change -- the PCB layout should accommodate this without a board respin.

**Digital inputs** (PRD 2.3)
- **Cool call** (P1.02/IO2 on WisBlock prototype; production PCB pin TBD): Thermostat cooling request from Y-in. Active high with pull-down. This is the primary interlock trigger in v1.0. The input circuit must condition the 24VAC thermostat signal to a 3.3V logic level with isolation (PRD 2.0.4).
- **Heat call** (P0.04 on production PCB; not connected on WisBlock prototype — no available pin): Thermostat heating request from W-in. Active high with pull-down. Same conditioning circuit as cool call. Wired and connected to the hardware interlock on production PCB; firmware GPIO reading is v1.1. For heat pump installations, this signal indicates the compressor is running in heating mode.

**Charge Now button** (PRD 2.0.1.1, PDL-001)
- Physical momentary push button on the device enclosure. Single press = activate 30-minute override. Long press (3s) = cancel override. Long press (10s) = activate BLE diagnostics beacon (future). Requires one GPIO with debounce (hardware RC or firmware).
- Button must be accessible on the sealed enclosure without opening it.

**LED indicators** (PRD 2.5.1.1, PDL-007)
- **Two LEDs**: Green (connectivity/health) + Blue (interlock/charging state). Both 0603 or 0805 SMD with current-limiting resistors. Two GPIO pins. The `app_leds` module already supports LED_ID_0 through LED_ID_3.
- LEDs must be visible through the enclosure (light pipe, translucent window, or enclosure-mounted).

**Radio** (PRD 3.1)
- nRF52840 + SX1262 LoRa, as on the RAK4631. The PCB can either:
  - (a) Use the RAK4631 as a module (drop-in, pre-certified for FCC/IC), or
  - (b) Place the nRF52840 and SX1262 as discrete components (smaller, cheaper at volume, but requires FCC intentional radiator certification).
- LoRa antenna: 915MHz. If the device is in a metal enclosure or mounted against a metal surface, an external antenna or antenna placement strategy is needed. The RAK4631 uses an IPEX connector for an external antenna.
- TCXO for SX1262 frequency stability is required (PRD 3.1).

**Programming and debug** (PRD 5.1, 5.2)
- SWD header (or pads) for factory programming via pyOCD. Used for MFG credential flashing and platform firmware. Not accessible after enclosure sealing.
- USB-C port for factory testing and initial development. Not accessible after enclosure sealing (PRD 5.3.2, 6.3.2).

**Isolation** (PRD 2.0.4)
- MCU isolated from 24VAC thermostat circuit (optocouplers or magnetic couplers).
- MCU isolated from J1772 pilot circuit (+/-12V).
- 24VAC power galvanically isolated from 240VAC charging circuit.
- Earth ground reference from AC compressor junction box ground screw (G terminal — see 2.0.3.1).
- All isolation barriers are already implemented on the protoboard. The PCB must replicate them with proper creepage and clearance distances for the voltage classes involved.

**Self-test support** (PRD 2.5.3)
- Charge_block toggle-and-verify: the charge_block GPIO must have a readback path so firmware can confirm the relay actually toggled. This is essential for the boot self-test and continuous effectiveness monitoring.
- Current clamp cross-check: the ADC path for current must be reliable enough to detect 500mA thresholds for the J1772 cross-check.

**Enclosure** (PRD 1.3, 1.4, 6.3.2)
- The device mounts near the EV charger, wrapping around the input leg of the charger's circuit (PRD 1.3). Access to 240V conductors (for current clamp), J1772 pilot, and thermostat wiring.
- Sealed after factory -- no USB, no debug headers accessible post-deployment.
- Tamper-evident sealing (PRD 6.3.2).
- Button accessible through enclosure.
- LEDs visible through enclosure.
- Conduit or cable gland entries for: thermostat wires (18-22 AWG, 4 wires in + 2 wires out for Y/W pass-through, plus R, C, G), J1772 pilot wires (PILOT-in and PILOT-out), current clamp cable.
- Mounting provisions (screw holes, DIN rail clip, or similar).
- IP rating TBD -- indoor installation (garage or utility closet), but may be exposed to dust, temperature swings, and occasional condensation.

#### 7.4.2 Inputs from Other Tasks

These decisions and deliverables feed into PCB design. Some are already resolved; others must be resolved before layout begins.

| Input | Source | Status | Impact on PCB |
|-------|--------|--------|---------------|
| Current clamp range: 48A | PDL-014 | DECIDED | Sets clamp selection and resistor divider values. PCB should allow resistor swap for 80A future without respin. |
| UL pre-submission feedback | PDL-008 | DEFERRED (acknowledged, not addressed) | UL reviewer may require specific creepage distances, isolation ratings, or component deratings that affect layout. Proceeding without this feedback risks a respin after UL engagement. This is the biggest risk to the PCB design. |
| Charge Now button type | PDL-001 | DECIDED (momentary) | Enclosure design must accommodate button. GPIO + debounce circuit needed on PCB. |
| LED count and colors | PDL-007 | DECIDED (green + blue) | Two LEDs, two GPIOs, two resistors. Enclosure must have light pipes or windows. |
| Transition delay | PDL-005 | DECIDED (not needed) | No RC time constant on relay driver. Simplifies relay drive circuit. |
| Boot default | PDL-006 | DECIDED (read-then-decide) | Charge_block GPIO must initialize LOW (safe default). Relay must be normally-open (de-energized = EV paused). |
| Dual interlock layers | PDL-002 | DECIDED (HW + SW redundancy) | Hardware interlock circuit is a hard requirement on the PCB, not just a relay driven by GPIO. |
| J1772 Cp duty cycle measurement | TASK-022 | NOT STARTED (firmware) | The analog conditioning circuit for AIN7 (WisBlock) / production ADC pin must preserve PWM shape. No separate hardware, but affects component selection (bandwidth). |
| Car-side interlock mechanism (PWM 0%) | TASK-023 / EXP-001 | NOT VALIDATED | If PWM 0% works across car makes, the PILOT-out circuit needs a PWM output path in addition to the resistance spoof. May affect charge_block circuit design. |
| Self-test toggle-and-verify | TASK-039 | IMPLEMENTED (firmware, TASK-039) | Relay readback path needed on PCB (GPIO or analog feedback from relay coil/contact). |
| BLE diagnostics (future) | PDL-016 | DECIDED (hard no for v1.0) | No BLE antenna optimization needed for post-registration use. BLE antenna only needs to work for initial registration (one-time, close range). |

#### 7.4.3 Outputs / Deliverables

"Done" for TASK-019 means all of the following exist and have been verified:

1. **Schematic** -- complete circuit schematic in KiCad (or equivalent), reviewed for correctness against PRD requirements. All component values specified. All isolation barriers annotated with voltage ratings.

2. **PCB layout** -- routed board with:
   - Correct creepage/clearance for isolation barriers
   - Antenna placement and ground plane strategy for 915MHz LoRa
   - Test points for factory programming (SWD), power rails, analog signals, and relay outputs
   - Mounting holes matching enclosure design
   - Silkscreen with component references, orientation marks, board revision, and Eta Works logo

3. **Bill of Materials (BOM)** -- complete parts list with manufacturer part numbers, quantities, unit costs, and sourcing status (in stock / lead time). Includes:
   - nRF52840 (or RAK4631 module -- decision point)
   - SX1262 + TCXO (if discrete)
   - 24VAC-to-3.3VDC converter (isolated)
   - Relays (x3: charge block, Y-out pass-through, W-out pass-through) with driver circuits
   - Optocouplers for signal isolation
   - Current clamp interface (connector + resistor divider)
   - LEDs (green, blue), resistors, capacitors, connectors
   - Momentary push button
   - SWD header / pads
   - Antenna (IPEX or PCB trace, depending on module vs. discrete decision)

4. **Gerber files** -- manufacturing-ready output files for PCB fabrication.

5. **Assembly drawing** -- component placement guide for hand assembly or pick-and-place.

6. **Wiring diagram** -- installer-facing diagram showing:
   - Current clamp connection (which conductor, orientation)
   - Terminal mapping: R (24VAC), C (common), Y-in / Y-out (cool call pass-through), W-in / W-out (heat call pass-through), G (earth ground from compressor junction box), PILOT-in / PILOT-out (J1772 Cp pass-through), CT+/CT- (current clamp) -- see section 2.0.3.1
   - Pilot wire cut point and PILOT-in / PILOT-out connections
   - Thermostat Y/W wire cut points and in/out connections
   - Earth ground connection
   - Note for AC-only installations (no W wire): cap W-in and W-out terminals
   - This diagram ships in the box and is printed on the commissioning checklist card (TASK-041).

7. **Enclosure specification** -- dimensions, material, IP rating, mounting method, button and LED cutouts, cable entry points. May be a separate deliverable if enclosure is sourced (not custom-designed).

8. **Assembled and tested prototype** -- at least one assembled board, powered from 24VAC, with:
   - All analog inputs reading correctly (Cp voltage, current clamp)
   - Both relay outputs toggling and verified
   - Hardware interlock confirmed (both loads cannot be on simultaneously)
   - Sidewalk LoRa connection established
   - LED and button functional
   - Firmware running (platform + app, same binary as protoboard with pin mapping updates)

#### 7.4.4 Key Design Decisions

These choices must be made during PCB design. They are not yet decided.

| Decision | Options | Considerations |
|----------|---------|----------------|
| **Module vs. discrete radio** | (a) RAK4631 module: pre-certified FCC/IC, larger footprint, higher unit cost (~$20). (b) nRF52840 + SX1262 discrete: smaller, cheaper at volume (~$8-10), requires FCC intentional radiator certification ($10K-$30K). | For initial production run (<100 units), module is almost certainly the right call. Discrete makes sense at 1K+ units if FCC cert cost is amortized. |
| **Relay type** | Mechanical relay (cheap, proven, audible click, limited cycle life ~100K). Solid-state relay / MOSFET switch (silent, unlimited cycles, more complex drive circuit, potential leakage current). Latching relay (holds state without power, lower power draw, more complex drive). | Three relays total: charge block (pilot spoof), Y-out pass-through, W-out pass-through. All switch low-voltage signals (24VAC thermostat, +/-12V J1772 pilot) -- not mains power. Mechanical relays are likely fine for the signal levels involved. Cycle life of 100K at ~20 cycles/day = 13+ years. The Y-out and W-out relays could potentially be a single dual-pole relay if the hardware interlock always blocks both simultaneously, but separate relays provide more flexibility for future independent control. |
| **Connector types** | Screw terminals (field-wirable, installer-friendly, larger footprint). Spring-cage / lever terminals (tool-free, faster installation, more expensive). Pin headers (compact, not field-wirable, development only). | Electricians expect screw terminals. Spring-cage terminals are gaining acceptance and faster for low-voltage wiring. |
| **Form factor** | Single board with all components. Two-board stack (radio module on top, power/interlock on bottom). Circular, rectangular, or shaped to fit a specific enclosure. | Driven by enclosure choice. Standard rectangular PCB is cheapest to fabricate. Two-board stack can reduce footprint. |
| **Enclosure type** | Off-the-shelf plastic junction box (cheap, available, but may not accommodate button/LED). Custom-molded enclosure (ideal UX, expensive tooling $5K-$15K, 6-8 week lead time). 3D-printed (prototype only, not production-grade). DIN-rail mount (industrial aesthetic, standard mounting, good for panel-adjacent installations). | First production run should use an off-the-shelf enclosure with minimal modification (drill for button, light pipe for LEDs). Custom enclosure is a future investment. |
| **Current clamp connector** | 3.5mm audio jack (common for clamp-on CTs). Screw terminal pair. Dedicated CT connector (e.g., Molex or JST). | Must match the selected clamp. Screw terminals are universal but larger. A dedicated connector prevents mis-wiring. |
| **Antenna strategy** | IPEX connector + external whip (flexible placement, proven with RAK4631). PCB trace antenna (cheaper, no connector, but performance depends on ground plane and enclosure). Enclosure-mounted external antenna. | If using RAK4631 module, IPEX is already provided. If going discrete, PCB trace antenna saves cost but requires careful RF layout. Metal enclosures require external antenna. |
| **24VAC input protection** | Fuse + TVS diode (standard). PTC resettable fuse (self-recovering). Varistor (surge absorption). | 24VAC thermostat circuits can see surges when the contactor coil switches. Input protection is essential. |

#### 7.4.5 Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| **UL feedback requires PCB respin** | Medium | High (weeks of delay, wasted fab cost) | Schedule a UL pre-submission meeting ($2K-$5K, 2-4 weeks) before committing to final layout. Get their feedback on isolation strategy, creepage distances, and component selection. Even informal guidance reduces respin risk substantially. |
| **LoRa antenna performance in enclosure** | Medium | Medium (range degradation, failed commissioning) | Test antenna performance with the actual enclosure material and mounting configuration before committing to production enclosure. Metal enclosures are particularly risky -- plastic or fiberglass preferred. |
| **Hardware interlock logic error** | Low | Critical (safety -- simultaneous load operation) | The hardware interlock must be verified independently of firmware. Design review by a second engineer. Physical testing: disconnect MCU power, verify both loads cannot be on simultaneously. |
| **24VAC transformer loading** | Low | Medium (brownout or thermostat malfunction) | SideCharge draws ~0.17W from the 24VAC transformer. Most transformers supply 40-48VA. Risk is only with undersized or heavily loaded transformers (multiple smart thermostats, humidifiers, etc.). Measure actual draw and document minimum transformer VA requirement. |
| **J1772 pilot signal integrity** | Medium | Medium (state misclassification) | The Cp voltage thresholds have only been tested with simulated signals. The analog conditioning circuit (voltage divider, filter, protection) must preserve signal fidelity for both voltage level and PWM duty cycle measurement. Validate with a real EVSE + car before production. |
| **Supply chain for key components** | Low-Medium | High (delay) | nRF52840 and SX1262 have had supply constraints. RAK4631 modules depend on RAKwireless inventory. Identify second sources or hold safety stock for initial production. |
| **Thermal management** | Low | Low-Medium | The 24VAC-to-3.3V converter and relay coils generate heat. In an enclosed space in a garage (ambient up to 50C/122F in direct sun), verify component deratings. Unlikely to be a problem at SideCharge's low power levels, but should be checked. |
| **Current clamp accuracy at extremes** | Medium | Low (incorrect current readings, not a safety issue) | CT clamps have nonlinearities near zero and at saturation. The 48A range means the clamp may not accurately read below 1-2A or above 45A. This affects reporting, not safety (the interlock does not depend on current readings for mutual exclusion). |

#### 7.4.6 Dependencies

**What blocks TASK-019 (inputs)**:

| Blocking Task/Decision | Status | Notes |
|------------------------|--------|-------|
| PDL-014 (current clamp 48A) | DECIDED | Clamp selection can proceed |
| PDL-001 (Charge Now button) | DECIDED | Button type known (momentary) |
| PDL-007 (LED matrix) | DECIDED | LED count and colors known (green + blue) |
| PDL-002 (dual interlock layers) | DECIDED | HW interlock is a hard requirement |
| PDL-005 (no transition delay) | DECIDED | Simplifies relay drive |
| PDL-006 (boot default) | DECIDED | Relay polarity known (normally-open) |
| TASK-023 / EXP-001 (PWM 0% validation) | NOT VALIDATED | Soft blocker — if PWM 0% is the production mechanism, charge_block circuit design changes. Can proceed with resistance spoof and add PWM path later. |
| UL pre-submission (PDL-008) | DEFERRED | Strongest recommendation: do this before final layout. Not a hard blocker, but proceeding without it is the single highest risk. |

**What TASK-019 blocks (outputs)**:

| Blocked Task | Impact |
|--------------|--------|
| TASK-041 (commissioning checklist card) | Wiring diagram depends on PCB terminal layout and connector types |
| TASK-040 (production self-test trigger) | 5-press button trigger requires the physical button on the PCB |
| Production dual-LED patterns (PRD 2.5.1.1) | Requires the second (blue) LED on the PCB |
| UL/safety certification (Future) | Cannot submit for UL review without a production board |
| Multi-unit manufacturing (Future) | Cannot produce units without manufacturing files |
| Field deployment (Future) | Cannot install in customer homes without a production device |
| 24VAC power (PRD 6.1) | Protoboard is USB-powered; production power requires PCB |
| Enclosure design | Cannot finalize enclosure without knowing PCB dimensions and mounting |

#### 7.4.7 Estimated Complexity

**Size: XL**

This is not a single task -- it is a project with multiple phases:

| Phase | Effort | Duration |
|-------|--------|----------|
| Component selection and schematic capture | 2-3 weeks | Includes relay selection, isolation component selection, 24VAC converter design, antenna strategy |
| Schematic review | 1 week | Second-engineer review, PRD cross-check |
| PCB layout | 2-3 weeks | Placement, routing, ground plane, antenna, creepage verification |
| Layout review | 1 week | DRC, antenna simulation or reference design check |
| Fabrication | 1-2 weeks | Prototype PCB order (expedited) |
| Assembly | 1 week | Hand-assemble first boards, populate BOM |
| Bring-up and test | 1-2 weeks | Power supply, relay toggling, analog inputs, LoRa link, firmware port |
| Enclosure fit and integration | 1 week | Mount board, verify button/LED/antenna, cable routing |
| **Total estimated** | **10-14 weeks** | **~3 months from start to tested prototype** |

The UL pre-submission meeting (if done) adds 2-4 weeks to the front end but potentially saves a full respin cycle (4-6 weeks) on the back end.

---

## 8. Known Gaps and Limitations

The following limitations are acknowledged but not tracked as backlog tasks (either out of v1.0 scope or pending external dependencies):

- **No UL or safety certification** — cannot deploy to customer homes without third-party safety certification. No timeline, budget, or NRTL engagement yet. Biggest non-technical risk to the product.
- **NEC code compliance not formally verified** — NEC 220.60, 220.70, 440.34 compliance is designed but no AHJ review or inspector sign-off.
- **J1772 thresholds not hardware-calibrated** — possible state misclassification with real chargers (Oliver REC-002).
- **EVSE rear-entry wiring not supported** — bottom-entry only in v1.0.
- **Car-side interlock mechanism (PWM 0%) not validated** — needs multi-make testing (Oliver EXP-001).
- **Heat pump compatibility** — GPIO wired (P0.04) on production PCB but not connected on WisBlock prototype; not read in v1.0 firmware. Future goal.

All active gaps with implementation work are tracked in `ai/memory-bank/tasks/INDEX.md`.
