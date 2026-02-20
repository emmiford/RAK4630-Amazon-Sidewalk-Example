# SideCharge Product Lexicon

**Status**: Active
**Owner**: Pam (Product Manager)
**Company**: Eta Works, Inc.
**Last updated**: 2026-02-12

This is the single source of truth for terminology across all SideCharge documents, visuals, code, and communications. Every developer, marketer, and designer should use the canonical terms defined here. When in doubt, check this file first.

The guiding principle: clear, confident, warm. We never hide behind jargon, but we never dumb things down either. We respect our audience's intelligence while valuing their time.

---

## How to Read Each Entry

- **Canonical term**: The term we use. This is the one that goes in docs, UI, marketing, and code comments.
- **Definition**: What it means, in 1-2 sentences.
- **Context**: Where and when to use this term.
- **Do NOT use**: Alternatives we avoid, and why.

---

## 1. Hardware Components

### SideCharge (the device)

- **Definition**: The circuit interlock device made by Eta Works. It mounts near the EV charger and junction box, connecting to thermostat wiring and the J1772 pilot signal. It ensures the AC compressor and EV charger never draw power at the same time.
- **Context**: Use everywhere -- marketing, technical docs, installer guides, cloud dashboards.
- **Do NOT use**: "Charger monitor" (it is an interlock, not a monitor), "EVSE controller" (we do not control the EVSE; we coordinate access to a shared circuit), "IoT device" (say "connected device" or just "SideCharge" in consumer-facing copy).

### RAK4630 (the module)

- **Definition**: The wireless microcontroller module at the heart of SideCharge. It combines an nRF52840 processor with a Semtech SX1262 LoRa radio on a single stamp-sized board, manufactured by RAKwireless. This is the actual hardware component that will be soldered to the production PCB.
- **Context**: Hardware BOM, production PCB discussions, module specifications. Never in marketing or consumer-facing materials.
- **Do NOT use**: "RAK module" without the model number (ambiguous -- RAK makes many modules).

### RAK4631 (the dev board)

- **Definition**: The WisBlock Core breakout board that carries a RAK4630 module. Adds USB-C, antenna connector, and the WisBlock pin header for plugging into base boards like the RAK19007. This is the development platform -- all firmware and Zephyr board targets reference `rak4631` because that is what the code was developed on. Production will use the RAK4630 module directly on a custom PCB.
- **Context**: Firmware discussions, Zephyr build configuration, development setup. The Zephyr board target is `rak4631` and all code references use this name, even though the production hardware is the RAK4630 module.
- **Do NOT use**: "RAK4630" when referring to the Zephyr build target (the board target is `rak4631`), "development board" as a synonym for the module itself.

### nRF52840

- **Definition**: The main processor on the RAK4631 -- a Nordic Semiconductor ARM Cortex-M4F microcontroller running Zephyr RTOS. It handles all application logic, sensor reading, interlock decisions, and BLE/Sidewalk communication. Runs at 64MHz with 256KB RAM and 1MB flash.
- **Context**: Firmware docs, architecture discussions, flash memory layout references. The processor is relevant when discussing clock drift (32.768 kHz RTC crystal, ~100 ppm), ADC resolution (12-bit), or GPIO pin assignments.
- **Do NOT use**: "Nordic chip" (too vague), "the MCU" without first establishing what it is, "Bluetooth chip" (it has BLE, but that is not its primary role in SideCharge -- LoRa is).

### SX1262

- **Definition**: The Semtech LoRa radio transceiver chip on the RAK4631 module. It handles all 915 MHz wireless communication over the Amazon Sidewalk network. LoRa (Long Range) is a spread-spectrum radio modulation that trades data rate for range -- it can reach hundreds of meters through walls and buildings, which is why it works for a device installed in a garage or utility closet.
- **Context**: Technical docs, connectivity troubleshooting, radio configuration. The SX1262 is relevant when discussing TCXO patches (temperature-compensated crystal oscillator for frequency stability), link budgets, or Sidewalk LoRa registration.
- **Do NOT use**: "The radio" without context (ambiguous), "LoRa chip" (use the specific part number in technical docs), "WiFi radio" (SideCharge has no WiFi).

### TCXO (Temperature-Compensated Crystal Oscillator)

- **Definition**: A precision frequency reference for the SX1262 radio. The RAK4631's default crystal drifts with temperature, causing LoRa packet errors. The TCXO patch provides a stable frequency reference across operating temperatures.
- **Context**: Firmware docs, radio configuration, troubleshooting connectivity issues. The TCXO patch is already implemented.
- **Do NOT use**: "Crystal fix" (too vague), "clock patch" (could be confused with RTC patches).

### Current Clamp

- **Definition**: A non-invasive sensor that wraps around one of the 240V conductors on the EV charger leg of the circuit. It measures real-time charging current by sensing the magnetic field around the wire -- no electrical connection to the 240V circuit. The analog output (0-3.3V) feeds the nRF52840 ADC.
- **Context**: Technical docs, installation guides, hardware interface descriptions. The current clamp is the only point where SideCharge has any physical proximity to the 240V circuit, and it is read-only.
- **Do NOT use**: "CT" or "current transformer" without explanation (installer audience may not know the abbreviation), "amp clamp" (informal), "meter" (it is a sensor, not a meter).

### Junction Box

- **Definition**: The electrical enclosure where the existing AC compressor circuit splits to serve both the compressor and the new EV charger. The electrician installs this at a convenient point on the existing circuit run. The SideCharge device mounts at or near the junction box.
- **Context**: Installation guides, wiring diagrams, electrician-facing materials.
- **Do NOT use**: "J-box" in formal docs (acceptable in conversation with electricians), "splitter" (not an electrical term).

---

## 2. Thermostat Wiring

SideCharge intercepts low-voltage thermostat wiring (24VAC) to detect when the AC compressor is being called and to block that call when the EV charger has priority. Here are the standard thermostat wire letter conventions:

### R Wire (24VAC Power)

- **Definition**: The power wire from the HVAC transformer. Carries 24VAC that powers the thermostat and control circuits. In SideCharge, the R wire is the production power source -- the device draws power from the AC system's 24VAC transformer, eliminating the need for a separate outlet.
- **Context**: Installation guides, wiring diagrams, power supply discussions.
- **Do NOT use**: "Red wire" in isolation (color can vary; the letter designation is what matters).

### Y Wire (Cool Call)

- **Definition**: The thermostat signal wire that calls for cooling. When the thermostat closes the Y circuit, it energizes the compressor contactor and the AC compressor starts. This is the primary interlock trigger in SideCharge v1.0 -- a rising edge on the Y wire (cool call) pauses EV charging.
- **Context**: Interlock logic, installation verification, commissioning sequence. In firmware, this is GPIO P0.05 (active high, pull-down).
- **Do NOT use**: "Yellow wire" in isolation, "AC signal" (too vague -- specify "cool call" or "Y wire").

### W Wire (Heat Call)

- **Definition**: The thermostat signal wire that calls for heating. The W wire energizes the furnace or heat strips. In SideCharge v1.0, the heat call GPIO is wired and monitored (reported in the uplink payload), but it does not trigger the interlock. Heat pump support is a future goal.
- **Context**: Thermostat monitoring, future heat pump discussions. In firmware, this is GPIO P0.04 (active high, pull-down).
- **Do NOT use**: "White wire" in isolation, "heat signal" (use "heat call" or "W wire").

### C Wire (Common / Return Ground)

- **Definition**: The common return wire for the 24VAC thermostat circuit. Completes the circuit back to the transformer. Required for any thermostat or device (like SideCharge) that needs continuous 24VAC power rather than stealing it from the call wires.
- **Context**: Installation guides, power supply discussions. If a home has no C wire, it may need to be added -- this is a common issue with older thermostat installations.
- **Do NOT use**: "Ground wire" (the C wire is a circuit common, not an earth ground -- confusing these is dangerous), "neutral" (24VAC control circuits do not use the term "neutral" the way 120V/240V circuits do).

### G Terminal (Earth Ground)

- **Definition**: The G terminal on the SideCharge device accepts an earth ground conductor from the AC compressor's outdoor junction box ground screw. This provides a ground reference for signal integrity and fault detection. **This is NOT the thermostat G wire.** On a standard thermostat, the G wire controls the indoor blower fan — SideCharge repurposes this terminal label for a completely different function. The fan wire from the thermostat cable bundle is not connected to SideCharge; it remains connected to the air handler (or is capped).
- **Context**: Installation guides, wiring diagrams, commissioning checklist (C-03), terminal reference (PRD 2.0.3.1). Critical to get right — an installer accustomed to HVAC wiring may instinctively connect the thermostat's G (fan) wire to this terminal.
- **Do NOT use**: "Fan wire" (SideCharge does not use or control the fan), "G wire" without specifying the source (ambiguous — could mean thermostat fan or earth ground), "ground wire" alone in formal docs (specify "earth ground from compressor junction box").

### Cool Call / Heat Call

- **Definition**: The thermostat request signals. A "cool call" means the thermostat is asking the AC compressor to run. A "heat call" means it is asking the furnace/heat strips to run. These are binary signals: active (24VAC present on the wire) or inactive (no voltage).
- **Context**: Interlock logic, uplink payload, status displays, installer diagnostics.
- **Do NOT use**: "HVAC call" (ambiguous -- specify which call), "compressor request" (too technical for consumer context; fine in firmware docs).

---

## 3. J1772 Terms

J1772 (also known as SAE J1772) is the standard for AC EV charging in North America. SideCharge monitors and manipulates J1772 signals to coordinate charging.

### Pilot Signal

- **Definition**: The J1772 control signal -- a 1 kHz square wave at +/-12V on the Cp (control pilot) pin. The pilot signal tells the EV and the charger about each other's state: whether a car is connected, whether it is ready to charge, and how much current the charger can provide. SideCharge reads this signal to detect car presence and charging state, and spoofs it to pause charging when the interlock activates.
- **Context**: Technical docs, interlock mechanism descriptions, hardware interface tables. The pilot signal is low-voltage (+/-12V), well within our design principle of avoiding mains voltage.
- **Do NOT use**: "Control signal" without specifying J1772 (too generic), "CP signal" without defining it first.

### Cp Pin (Control Pilot)

- **Definition**: The physical pin on the J1772 connector that carries the pilot signal. SideCharge sits inline on the pilot wire (PILOT-in from EVSE, PILOT-out to vehicle) and reads the pilot voltage (via ADC on AIN1). When charge_block is driven HIGH, the pilot spoof relay energizes and inserts a resistive load; when LOW (or MCU power loss), the relay is de-energized and pilot passes straight through (normally-closed design).
- **Context**: Hardware interface docs, wiring diagrams, ADC configuration.
- **Do NOT use**: "Pilot pin" (not the standard name), "PP pin" (that is the proximity pilot -- a different pin).

### J1772 States A through F

- **Definition**: The six possible states of the pilot signal, determined by voltage level:
  - **State A** (>2600 mV): Not connected. No vehicle plugged in.
  - **State B** (1850-2600 mV): Connected, not ready. Vehicle is plugged in but not requesting charge.
  - **State C** (1100-1850 mV): Charging. Current is flowing to the vehicle.
  - **State D** (350-1100 mV): Charging with ventilation required. Rare; used by older lead-acid EVs.
  - **State E** (<350 mV): Error. Short circuit on the pilot line.
  - **State F**: No pilot signal detected. EVSE fault.
- **Context**: Firmware state machine, uplink payload, diagnostics, status displays. In consumer-facing UI, translate these: "Not connected," "Plugged in," "Charging," etc. Never show "State C" to a homeowner.
- **Do NOT use**: The letter-number codes in consumer-facing UI. Always translate to plain language for homeowners. Use the letter codes freely in technical docs and firmware.

### PWM Duty Cycle

- **Definition**: The percentage of time the pilot signal's square wave is in the high (+12V) state. The duty cycle encodes the maximum current the charger is offering to the vehicle. For example, a 50% duty cycle means 30A available. SideCharge reads the pilot voltage level (implemented) but does not yet decode the duty cycle (not started).
- **Context**: Technical docs, J1772 protocol discussions, future feature planning.
- **Do NOT use**: "Pulse width" without context (ambiguous), "signal strength" (it is not about strength, it is about timing).

### EVSE (Electric Vehicle Supply Equipment)

- **Definition**: The technical/industry term for an EV charger -- the box on the wall or pedestal that supplies AC power to the vehicle through a J1772 (or NACS) connector. The EVSE handles the pilot signal handshake, ground fault protection, and power delivery.
- **Context**: J1772 technical sections, code compliance discussions, electrician-facing materials. EVSE is the correct term when discussing the charging equipment itself in a technical context.
- **Do NOT use**: "EVSE" in consumer-facing or marketing copy. Say "EV charger" or "charger" instead. Homeowners do not know what EVSE means, and explaining it wastes their time.

### EV Charger

- **Definition**: The consumer-friendly term for the EVSE. The box on the wall that you plug your car into.
- **Context**: All consumer-facing materials, marketing, homeowner communications, dashboard UI, installer sell sheets (when talking about the outcome, not the protocol).
- **Do NOT use**: "Charging station" (implies commercial/public infrastructure, not residential), "charge point" (that is a competitor's name), "charger" alone when it could be confused with a phone charger or other charger (add "EV" for clarity in mixed contexts).

---

## 4. Interlock Terms

The interlock is the product. Everything else is built on top of it.

### Circuit Interlock

- **Definition**: The firmware-driven system that prevents the AC compressor and EV charger from drawing power simultaneously on a shared circuit. The MCU reads thermostat inputs and drives the charge_block GPIO to control all relays. Both the pilot spoof relay and the AC block relay are normally-closed (pass-through when de-energized). On MCU power loss, all relays de-energize and the device becomes transparent — this is safe by design because the EVSE retains its own safety systems (GFCI, overcurrent protection, J1772 state machine).
- **Context**: Everywhere -- this is the core concept of SideCharge. Lead with it in marketing, technical docs, installer materials, and code compliance discussions.
- **Do NOT use**: "Load management" alone (too generic -- load management could mean anything from dimming lights to curtailing an entire building), "smart switch" (misleading -- we do not switch mains power), "energy management system" (this is the NEC 220.70 / Article 750 term, which is broader than what SideCharge does in v1.0; use it only when citing code compliance).

### Mutual Exclusion

- **Definition**: The guarantee that the AC compressor and EV charger never draw power at the same time during normal operation. This is the fundamental safety and code compliance property of SideCharge. Enforced by firmware (the MCU reads thermostat inputs and drives the charge_block GPIO). On MCU power loss, the device becomes transparent (NC relays pass through) — both loads could theoretically run simultaneously, but the circuit breaker provides overcurrent protection and the EVSE retains its own safety systems.
- **Context**: Technical docs, safety discussions, code compliance. This is the formal term for what the interlock achieves.
- **Do NOT use**: "Mutex" in non-firmware contexts (that is a software concurrency primitive, not an electrical concept), "lockout" alone (ambiguous without specifying which load is locked out).

### AC Priority

- **Definition**: The default operating mode: if the thermostat calls for cooling, EV charging pauses. The AC compressor always wins. This is the safe default because blocking AC on a hot day has comfort and safety implications, while pausing a car charge is merely inconvenient.
- **Context**: Interlock logic, consumer-facing explanations, installer guides, status displays.
- **Do NOT use**: "HVAC priority" in consumer copy (homeowners think "air conditioning," not "HVAC"), "thermal priority" (not a standard term).

### Charge Now Override

- **Definition**: A physical momentary push button on the SideCharge device. When pressed, EV charging takes priority over AC for a limited duration (currently recommended: 30 minutes). The AC compressor call signal is blocked for the override period. When it expires, any pending AC call is immediately honored.
- **Context**: Product features, homeowner-facing explanations, LED feedback descriptions, button interaction specs.
- **Do NOT use**: "Override mode" alone (too vague -- override of what?), "force charge" (sounds aggressive), "bypass" (implies defeating a safety system, which is not accurate -- only the load priority changes; the interlock logic continues running).

### Circuit Sharing

- **Definition**: The consumer-friendly way to describe what the interlock does: two loads (AC and EV charger) sharing one circuit, never running simultaneously. Use this when "interlock" feels too technical.
- **Context**: Marketing, homeowner-facing materials, taglines, social media.
- **Do NOT use**: "Load sharing" (implies they share simultaneous power, which is the opposite of what we do), "power splitting" (same problem).

---

## 5. Connectivity

### Amazon Sidewalk

- **Definition**: Amazon's free, community-shared wireless network. Sidewalk uses LoRa (long range, low power) and BLE radios built into Echo and Ring devices to create a mesh network that covers 90%+ of the US population. SideCharge communicates over Sidewalk's LoRa link -- no WiFi, no cellular, no monthly fees.
- **Context**: Connectivity discussions, product positioning, consumer explanations. In technical docs, "Sidewalk" refers to the full protocol stack (registration, session keys, transport encryption, message routing).
- **Do NOT use**: "The Sidewalk network" in consumer copy without briefly explaining what it is. Say "Amazon Sidewalk's free neighborhood network" or "a free radio network powered by nearby Echo and Ring devices." Never assume the audience knows what Sidewalk is.

### Neighborhood Network

- **Definition**: The consumer-friendly term for Amazon Sidewalk. Emphasizes that coverage comes from nearby devices (your neighbor's Echo, the Ring doorbell down the street), not from a cellular tower or your home router.
- **Context**: Marketing, consumer-facing copy, homeowner explanations. Use when "Amazon Sidewalk" is too technical or too brand-specific.
- **Do NOT use**: "Mesh network" in consumer copy (too technical), "IoT network" (we avoid "IoT" in consumer materials -- say "connected" instead).

### LoRa (Long Range)

- **Definition**: A spread-spectrum radio modulation technology that trades data rate for range. LoRa signals can travel 500-800 meters per hop and penetrate walls, garages, and other structures. SideCharge uses LoRa at 915 MHz (the US ISM band) via the SX1262 radio chip and the Amazon Sidewalk protocol.
- **Context**: Technical docs, connectivity architecture, range discussions. In consumer materials, translate: "radio waves that travel through walls" or "long-range radio."
- **Do NOT use**: "LoRaWAN" (SideCharge uses Amazon Sidewalk, not LoRaWAN -- these are different protocols on the same radio modulation), "WiFi" (completely different technology), "Bluetooth" (BLE is used only once, during initial registration).

### Gateway

- **Definition**: An Amazon Echo, Ring, or other Sidewalk-enabled device in the vicinity of the SideCharge installation. The gateway bridges the LoRa radio link to Amazon's cloud via the homeowner's (or neighbor's) internet connection. SideCharge does not require the gateway to be in the same home -- Sidewalk's neighborhood coverage means a gateway one or two houses away works fine.
- **Context**: Connectivity discussions, installation prerequisites, range troubleshooting.
- **Do NOT use**: "Router" (a gateway is not a WiFi router), "hub" (too generic), "bridge" (technically accurate but uncommon in the Sidewalk context).

### Uplink

- **Definition**: A message sent from the SideCharge device to the cloud. Uplinks carry sensor data (J1772 state, pilot voltage, charging current, thermostat flags) in a compact payload that fits within Sidewalk's 19-byte LoRa MTU. The device uplinks on a 15-minute heartbeat and on state changes.
- **Context**: Cloud architecture, payload format, connectivity monitoring.
- **Do NOT use**: "Upload" (implies bulk data transfer), "report" without context (too vague), "telemetry" in consumer copy (use "status update" or "check-in").

### Downlink

- **Definition**: A message sent from the cloud to the SideCharge device. Downlinks carry commands: charge control (pause/allow), OTA firmware chunks, and time sync. Downlinks are precious over LoRa -- 19 bytes max, and the Sidewalk network determines delivery timing.
- **Context**: Cloud architecture, command descriptions, OTA pipeline.
- **Do NOT use**: "Download" (implies bulk data), "push notification" (that is a phone/app concept).

### MTU (Maximum Transmission Unit)

- **Definition**: The maximum payload size for a single LoRa message over Amazon Sidewalk: 19 bytes. Every uplink and downlink must fit within this limit. The SideCharge uplink payload is 12 bytes (8 bytes of sensor data + 4 bytes of timestamp), leaving 7 bytes of headroom.
- **Context**: Payload design, protocol constraints, OTA chunk sizing (15 bytes of data + 4 bytes of header = 19 bytes).
- **Do NOT use**: "Bandwidth" (MTU is about message size, not data rate), "packet size" without specifying the Sidewalk context.

---

## 6. Cloud and OTA Terms

### Split-Image Architecture

- **Definition**: SideCharge firmware is divided into two independent binaries that share the same flash chip:
  - **Platform** (576KB): The generic runtime -- Zephyr RTOS, Sidewalk stacks, OTA engine, hardware drivers. Large, stable, rarely changes. Can only be updated via physical USB programmer (pyOCD). Frozen after the device leaves the factory.
  - **App** (4KB): All the EVSE and interlock domain logic -- sensor reading, charge control, payload formatting. Tiny, changes frequently, OTA-updatable over LoRa.
- **Context**: Architecture discussions, OTA pipeline, firmware versioning, update strategy.
- **Do NOT use**: "Dual firmware" (suggests two complete firmwares), "bootloader + app" (the platform is far more than a bootloader -- it runs the entire Sidewalk and BLE stack).

### Platform

- **Definition**: The lower half of the split-image architecture. Contains Zephyr RTOS, Amazon Sidewalk BLE and LoRa stacks, the OTA engine, the shell, and hardware drivers. The platform knows nothing about EV charging, thermostats, or interlock logic. It provides a function-pointer API (`struct platform_api`) that the app calls.
- **Context**: Firmware architecture, OTA discussions, flash memory layout (starts at 0x00000).
- **Do NOT use**: "OS" alone (technically Zephyr is the OS; the platform is Zephyr + Sidewalk + OTA + drivers), "firmware" without qualification (specify "platform firmware" or "app firmware").

### App

- **Definition**: The upper half of the split-image architecture. A compact 4KB binary containing all EVSE and interlock domain logic: sensor reading, change detection, payload formatting, charge control, and shell commands. This is the only part of the firmware that can be updated over the air after deployment.
- **Context**: OTA updates, feature development, firmware versioning. The 4KB size is a feature worth celebrating -- it means fast delta OTA updates.
- **Do NOT use**: "Application" spelled out in technical contexts (the codebase uses "app" consistently), "mobile app" (SideCharge has no mobile app in v1.0 -- use "app binary" or "app firmware" if there is any ambiguity).

### Delta OTA

- **Definition**: The incremental firmware update mode. The cloud compares the new app binary against the stored baseline (the firmware currently running on the device), identifies which 15-byte chunks have changed, and sends only those chunks over LoRa. A typical one-function code change means 2-3 chunks, completing in about 5 minutes. This is the normal update path.
- **Context**: OTA pipeline, update speed claims, marketing ("updates in seconds, not hours").
- **Do NOT use**: "Partial OTA" (not a standard term), "incremental update" (delta is more precise), "patch" (implies a diff format; our delta is chunk-level, not byte-level).

### Full OTA

- **Definition**: The fallback firmware update mode. Sends all ~276 chunks of the app binary, regardless of what changed. Takes approximately 69 minutes over LoRa. Used when there is no baseline to diff against (first deployment, baseline mismatch, or recovery scenarios).
- **Context**: OTA fallback discussions, recovery procedures, worst-case update time.
- **Do NOT use**: "Complete OTA" (redundant), "fresh install" (the mechanism is the same -- just more chunks).

### Baseline

- **Definition**: The reference firmware binary stored in S3 that represents what is currently running on the device. The OTA pipeline diffs the new binary against the baseline to compute the delta. After a successful OTA, the new binary becomes the new baseline. If the baseline gets out of sync with the device, delta OTA fails gracefully and falls back to full mode.
- **Context**: OTA pipeline, `ota_deploy.py` commands (`baseline`, `deploy`, `preview`), troubleshooting update failures.
- **Do NOT use**: "Reference image" (not used in the codebase), "golden image" (implies a factory-original image, which is a different concept).

### Staging Area

- **Definition**: A separate region of flash memory (at 0xD0000, 148KB) where incoming OTA chunks are written before being applied. The device validates the complete image in staging (CRC32 check) before copying it to the primary app partition. This design ensures the device cannot be bricked by a partial or corrupt update.
- **Context**: OTA architecture, flash memory layout, recovery discussions.
- **Do NOT use**: "Download area" (no downloading involved -- chunks arrive over radio), "buffer" (too generic; the staging area is a persistent flash partition, not RAM).

### Recovery Metadata

- **Definition**: A small data structure at flash address 0xCFF00 that tracks whether a flash copy from staging to primary was in progress when power was lost. On boot, if recovery metadata is present, the device resumes the copy from where it left off. This is the mechanism that prevents bricking.
- **Context**: OTA safety, power-loss recovery, boot sequence.
- **Do NOT use**: "Recovery flag" (it is more than a flag -- it includes progress information), "boot metadata" (could be confused with other boot-time data).

---

## 7. Electrical Terms

These terms matter for electrician-facing materials, code compliance, and installation documentation.

### Branch Circuit

- **Definition**: The wiring that runs from a circuit breaker in the electrical panel to the loads it serves. In a SideCharge installation, the existing branch circuit serving the AC compressor is extended (via a junction box) to also serve the EV charger. Both loads share the same circuit breaker (typically 40A or 50A, 2-pole).
- **Context**: Installation guides, code compliance, electrician-facing materials.
- **Do NOT use**: "Power line" (too vague), "home run" (electrician slang for a direct panel-to-device circuit -- not what we have after the modification).

### EGC (Equipment Grounding Conductor)

- **Definition**: The grounding wire in the branch circuit that provides a fault-current path back to the panel. Required by NEC for safety. In a SideCharge installation: two ungrounded conductors (L1/L2) and one EGC. No neutral is required for a 240V-only circuit.
- **Context**: Installation guides, wiring specifications, code compliance.
- **Do NOT use**: "Ground wire" alone in formal docs (specify EGC), "earth wire" (UK terminology), "neutral" (the EGC and neutral serve different functions -- confusing them is a code violation).

### NEC (National Electrical Code)

- **Definition**: The model electrical code published by NFPA, adopted (with state-specific amendments) across the US. SideCharge's code compliance rests on three NEC sections:
  - **NEC 220.60 (Noncoincident Loads)**: When two loads are interlocked so they cannot operate simultaneously, the load calculation uses only the larger of the two, not the sum. This is the fundamental code basis for sharing a circuit.
  - **NEC 220.70 / Article 750 (Energy Management Systems)**: Added in the 2023 NEC cycle. Specifically addresses automated load management systems for EV charging.
  - **NEC 440.34 exception**: Allows motor-driven AC equipment to share circuits with other loads when interlocked to prevent simultaneous operation.
- **Context**: Code compliance, electrician-facing materials, permit discussions, inspector conversations. Lead with NEC section numbers when talking to electricians and inspectors -- they speak this language.
- **Do NOT use**: "Building code" (NEC is specifically the electrical code), "CEC" without clarification (CEC can mean California Electrical Code or Canadian Electrical Code, depending on context -- always specify).

### AHJ (Authority Having Jurisdiction)

- **Definition**: The local entity (usually a city or county building department) that enforces electrical codes and approves installations. An AHJ inspector must sign off on any electrical work requiring a permit. No AHJ has yet evaluated or approved a SideCharge installation.
- **Context**: Code compliance discussions, regulatory planning, installer training.
- **Do NOT use**: "Inspector" alone when referring to the institutional authority (the inspector is a person; the AHJ is the agency).

### Contactor

- **Definition**: An electrically controlled switch that turns the AC compressor on and off. The thermostat's 24VAC cool call signal energizes the contactor coil, which closes the contactor and connects the compressor to power. SideCharge intercepts this 24VAC signal -- it does not touch the contactor itself or the high-voltage wiring to the compressor.
- **Context**: Interlock mechanism explanations, thermostat wiring discussions, "how it works" content for electricians.
- **Do NOT use**: "Relay" (a contactor is a heavy-duty relay designed for motor loads -- the terms are technically different, and electricians know the difference), "switch" (too vague).

---

## 8. AC and HVAC Terms

### AC (Air Conditioning)

- **Definition**: The air conditioning compressor and its circuit. In SideCharge context, "AC" always means the air conditioning system, specifically the compressor that draws 30-50A at 240V.
- **Context**: Everywhere. Use "AC" consistently, not "HVAC," when referring to the compressor and its circuit in the interlock context.
- **Do NOT use**: "HVAC" when you mean the AC compressor specifically. HVAC includes heating, ventilation, and air conditioning -- SideCharge v1.0 interlocks only with the cooling compressor. "HVAC" is acceptable when referring to the overall system (e.g., "the HVAC system's 24VAC transformer"), but not when describing the interlock load. "Air conditioner" is fine in consumer copy.

### Compressor

- **Definition**: The motor-driven unit in the AC system that circulates refrigerant. It draws 30-50A at 240V -- nearly identical to a Level 2 EV charger. The compressor is the AC load that SideCharge interlocks with.
- **Context**: Technical discussions about the load profile, interlock logic, NEC 440 references (which specifically cover motor-driven AC equipment).
- **Do NOT use**: "Condenser" (the condenser is a different component -- the coil that rejects heat outdoors), "AC unit" in technical docs (too vague -- specify compressor).

### Heat Pump

- **Definition**: A system that uses the same compressor for both heating and cooling by reversing refrigerant flow. Heat pump support is a future goal for SideCharge, not current scope. When it is supported, the interlock will need to monitor both heat and cool calls.
- **Context**: Future roadmap discussions, scope boundary definitions.
- **Do NOT use**: "Supported" or "compatible" in current marketing -- heat pump support is explicitly out of scope for v1.0.

---

## 9. Demand Response and Grid Terms

### TOU (Time-of-Use)

- **Definition**: An electricity rate structure where the price per kWh varies by time of day. Peak hours (typically late afternoon and evening) cost more; off-peak hours (overnight) cost less. SideCharge's charge scheduler pauses EV charging during peak windows and allows it during off-peak, saving the homeowner money.
- **Context**: Charge scheduling logic, consumer savings messaging, utility discussions. Current implementation: Xcel Colorado weekdays 5-9 PM MT.
- **Do NOT use**: "Peak pricing" alone (TOU is the standard term), "dynamic pricing" (that is a different, more granular rate structure).

### MOER (Marginal Operating Emissions Rate)

- **Definition**: A real-time signal from WattTime that indicates how clean or dirty the electricity grid is right now. When MOER is high, the marginal generator is a fossil fuel plant; when low, it is wind or solar. SideCharge pauses charging when MOER exceeds a configurable threshold (default: 70%).
- **Context**: Grid carbon optimization, demand response logic, environmental messaging.
- **Do NOT use**: "Carbon intensity" without citing the source (MOER is specifically WattTime's metric), "grid cleanliness" is acceptable in consumer copy as a plain-language translation.

### Demand Response

- **Definition**: The coordination of electricity consumption in response to grid conditions -- price signals, carbon intensity, or direct utility commands. SideCharge enables demand response for two loads (AC and EV charger) simultaneously, which is unusual and valuable.
- **Context**: Utility-facing materials, grid value proposition, cloud architecture.
- **Do NOT use**: "Load shedding" (implies forced curtailment; SideCharge is voluntary and automated), "DR" without first defining it (not all audiences know the abbreviation).

---

## 10. Code and Firmware Terms

### Zephyr RTOS

- **Definition**: The open-source real-time operating system that runs on the nRF52840. Zephyr provides the kernel, drivers, shell, and build system. SideCharge uses Zephyr's GPIO, ADC, flash, and logging subsystems. The platform firmware is built on the nRF Connect SDK, which layers Nordic-specific libraries on top of Zephyr.
- **Context**: Firmware architecture, developer onboarding, build system discussions.
- **Do NOT use**: "The OS" without specifying Zephyr (could be confused with a host OS), "FreeRTOS" (different RTOS; SideCharge uses Zephyr).

### Platform API (`struct platform_api`)

- **Definition**: The interface between the platform and app firmware layers. A C struct of function pointers that the platform populates with real Zephyr/Sidewalk implementations on the device, or mock implementations during host-side testing (the Grenning Dual-Target pattern). The app never calls Zephyr or Sidewalk functions directly -- it calls through this API.
- **Context**: Architecture docs, testing discussions, app/platform boundary.
- **Do NOT use**: "HAL" (Hardware Abstraction Layer -- the platform API is higher-level than a HAL), "SDK" (the nRF Connect SDK is different from the platform API).

### Grenning Dual-Target Pattern

- **Definition**: The testing architecture that allows the same app source code to compile and run on both the device (with real hardware) and the developer's laptop (with mocks). Named after James Grenning. The key mechanism is the `struct platform_api` function pointers -- swap the implementation, same app code.
- **Context**: Testing docs, developer onboarding, CI/CD pipeline discussions.
- **Do NOT use**: "Mocking framework" (it is a pattern, not a framework), "simulation" (the mocks are not simulating hardware -- they are replacing the platform API with controllable test doubles).

---

## 11. Audience-Specific Terminology Rules

Different audiences need different language for the same concepts. Here is the mapping:

### For Homeowners (Marketing, Consumer UI, Notifications)

| Concept | Say this | Not this |
|---------|----------|----------|
| The product | SideCharge | Circuit interlock device |
| What it does | Lets your EV charger share your AC circuit | Mutual exclusion of noncoincident loads |
| The charger | EV charger | EVSE |
| The AC system | Air conditioning, AC | HVAC compressor contactor |
| Connectivity | Connected over a free neighborhood network | Amazon Sidewalk LoRa 915MHz |
| Charging paused | Charging paused -- your AC needed the circuit | State transition: interlock engaged, cool call active |
| Charging resumed | Charging resumed | Charge block GPIO set LOW (not blocking) |
| Savings | No panel upgrade needed (saves $2,400+) | NEC 220.60 noncoincident load calculation |
| Override button | Charge Now button | Charge Now override with 30-minute AC call block |
| Update | Your SideCharge just got smarter | Delta OTA app binary update via Sidewalk LoRa |

**Tone**: Warm, reassuring, benefit-first. Short sentences. Use "you" and "your." Lead with what matters to them: cost savings, same-day install, it just works.

### For Electricians and Installers (Sell Sheets, Install Guides, Commissioning)

| Concept | Say this | Not this |
|---------|----------|----------|
| The product | SideCharge circuit interlock | The IoT device |
| Code basis | NEC 220.60 (noncoincident loads), NEC 440.34 exception | We're code compliant |
| The circuit | Branch circuit modification, shared 240V circuit | Power splitting |
| Wiring | Two ungrounded conductors + EGC, #8 AWG min for 40A, #6 for 50A | Wires |
| The mechanism | Interlocked loads -- AC and EV charger never operate simultaneously | Smart energy management |
| Install scope | Same-day install, one junction box, thermostat tap, current clamp | Quick and easy |
| Commissioning | LED verification sequence (see commissioning card) | Plug and play |

**Tone**: Peer-to-peer. Respect their expertise. Lead with NEC section numbers and install workflow, not the cloud or the app. They care about code compliance, install speed, callback avoidance, and inspector approval.

### For Technical Docs (PRD, Architecture, Firmware)

| Concept | Say this | Not this |
|---------|----------|----------|
| The charger | EVSE (acceptable), EV charger (also acceptable) | Charger alone (ambiguous) |
| Radio | SX1262 LoRa transceiver, Sidewalk LoRa link | The radio |
| Processor | nRF52840 | The chip, the MCU |
| States | J1772 State A/B/C/D/E/F with voltage ranges | Connected / charging (too vague) |
| Update | Delta OTA via Sidewalk, 15-byte chunks, CRC32 validation | Firmware update |
| Flash layout | Platform at 0x00000 (576KB), App at 0x90000 (4KB), Staging at 0xD0000 (148KB) | The memory |

**Tone**: Precise and thorough. Use concrete numbers, part numbers, memory addresses, and protocol names. The audience is technical -- never dumb things down, but still write clearly.

### For Utilities and Grid Operators (Pitch Decks, Program Proposals)

| Concept | Say this | Not this |
|---------|----------|----------|
| The product | SideCharge -- a circuit interlock that adds Level 2 EV charging without increasing peak demand | A smart home gadget |
| Grid value | Zero net demand increase per installation, demand response for two loads (AC + EV) | Saves money |
| Scale | Defers transformer upgrades, reduces panel upgrade incentive spend | Good for the environment |
| Connectivity | Zero-infrastructure: Amazon Sidewalk, no WiFi provisioning, no cellular cost | Connects to the internet |
| Equity | Targets homes with 100A panels -- disproportionately in disadvantaged communities | Affordable |
| Cost comparison | ~$1,000 vs. $2,400+ panel upgrade, vs. $4,200 SCE incentive per home | Cheaper |

**Tone**: ROI-first, data-driven. Lead with grid economics and program scalability. Use real numbers. Never lead with the technology -- lead with the outcome.

---

## 12. Terms We Always Avoid (Across All Audiences)

| Avoid | Why | Say instead |
|-------|-----|-------------|
| Revolutionary / Disruptive | We are evolutionary, and proud of it | A better way, an elegant solution |
| Leverage / Utilize | Corporate jargon | Use |
| End-to-end / Holistic | Say what you actually mean | Specify what is included |
| Smart home | We are smart energy, bigger than one category | Connected, smart energy coordination |
| Just works | Apple owns this phrase | Works without WiFi, works automatically |
| IoT / IoT device | Consumer-hostile acronym | Connected device, smart device |
| Seamless | That word has lost all meaning | Effortless, straightforward |
| Panel upgrade (used positively) | It is always the problem we eliminate, never a solution | No panel upgrade needed |
| HVAC (when you mean AC) | SideCharge v1.0 interlocks with the cooling compressor only | AC, air conditioning |
| Charger monitor | SideCharge is an interlock, not a monitor | Circuit interlock |
| Load sharing | Implies simultaneous sharing of power | Circuit sharing, interlocked loads |

---

## 13. Quick Reference: The SideCharge Story in One Sentence Per Audience

- **Homeowner**: SideCharge lets you install a Level 2 EV charger without upgrading your electrical panel -- your charger and your AC share the same circuit, and they take turns automatically.
- **Electrician**: SideCharge is a code-compliant circuit interlock (NEC 220.60, 440.34) that enables same-day Level 2 charger installation on an existing AC compressor circuit -- one device, one visit, one invoice.
- **Utility**: Every SideCharge installation adds a Level 2 EV charger without increasing peak demand or requiring distribution upgrades, with built-in demand response for both AC and EV loads.
- **Developer**: SideCharge is a 4KB app running on a split-image architecture (nRF52840 + SX1262), connected via Amazon Sidewalk LoRa, with delta OTA updates that complete in minutes over a 19-byte MTU.

---

*Lexicon v1.0 -- Created 2026-02-12 by Pam (Product Manager), Eta Works*
*Source documents: PRD v1.4, Brand Guidelines v2.0, Brand Whimsy & Personality Recommendations*
