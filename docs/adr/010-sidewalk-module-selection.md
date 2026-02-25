# ADR-010: Amazon Sidewalk Module Selection — RAK4630

## Status

Accepted (2026-02-25)

## Context

The project needs a hardware module for an Amazon Sidewalk LoRa endpoint that will be integrated onto a custom circuit board. Two hard requirements constrain the search:

1. **Pre-existing FCC approval.** In-house FCC certification is lengthy and expensive. The module must already carry an FCC grant so the final device can inherit certification without a full test cycle.
2. **Size < US quarter (~24 mm diameter).** The module must fit inside a compact enclosure alongside the EVSE relay board.

Additionally, Amazon's Sidewalk SDK targets specific chip vendors. The team is committed to a **Nordic processor + Semtech LoRa modem** pairing, which is the reference platform for Nordic's Sidewalk driver implementation (built on Zephyr RTOS).

A secondary concern surfaced during evaluation: the Sidewalk SDK appears to require a BLE subsystem even when the device operates LoRa-only. Any board that lacks BLE hardware — or cannot at least compile and link the BLE stack — may fail to build or register. This makes BLE capability a de facto requirement, even if the radio is never used over the air.

## Decision

Proceed with the **RAK4630** module (nRF52840 + SX1262).

It is the only evaluated option that satisfies both hard requirements (FCC-certified, smaller than a quarter) while aligning with the Nordic/Semtech component commitment and providing full BLE hardware support.

This decision also locks the project into **Zephyr RTOS**, since Nordic's Sidewalk drivers and the nRF Connect SDK (NCS) are built on Zephyr. Zephyr's build system and configuration model (Kconfig, devicetree) have proven fairly opaque compared to bare-metal or FreeRTOS approaches, but this is accepted as the cost of using the manufacturer's supported driver stack rather than writing custom drivers.

## Alternatives Considered

### Texas Instruments / Silicon Labs / STMicroelectronics development boards

All three vendors have released Amazon Sidewalk libraries, but only on **large development boards** (EVK-scale). No small, FCC-approved modules were found from any of them. Using a dev board in a production design would require in-house FCC approval — defeating the primary requirement.

**Rejected.** Fails the "small, FCC-approved module" requirement.

### Seeed Studio LoRa-E5 Mini (STM32WLE5 + Semtech modem)

Smaller and cheaper than the RAK4630. However, it **lacks Bluetooth hardware entirely**. The Sidewalk SDK's dependency on BLE — even when operating in LoRa-only mode — creates a significant integration risk: the ST Sidewalk libraries may not build or function correctly without BLE stack support. Investigating and working around this would consume unknown engineering time.

**Rejected.** BLE absence is an unacceptable risk for Sidewalk driver compatibility.

### Other RAK-compatible modules (same Nordic/Semtech components)

Cheaper alternatives exist (~$8 vs. RAK's ~$15), but it is unclear whether they carry an **FCC grant** that is inheritable when soldered onto a third-party PCB. Without confirmed FCC inheritance, the cost savings are illusory.

**Deferred.** Open to re-evaluation if RAK becomes unavailable or cost pressure increases, but only after confirming FCC inheritance for the specific alternative module.

## Consequences

**What becomes easier:**
- Nordic's pre-built Sidewalk drivers work out of the box — no custom radio or protocol driver development
- FCC certification is inherited from the module, removing a major regulatory milestone from the project timeline
- Full BLE hardware is available for Sidewalk registration and any future BLE features
- The module's form factor fits the enclosure without board redesign

**What becomes harder:**
- Unit cost is higher ($15 vs. $8 alternatives) — acceptable for initial volumes but a pressure point at scale
- Zephyr RTOS has a steep and opaque learning curve; its build system, Kconfig, and devicetree model are significantly more complex than bare-metal or FreeRTOS approaches
- Locked into Nordic/Zephyr ecosystem — switching processor vendors later would require a near-complete firmware rewrite

## Risks

### FCC inheritance on custom PCB

The decision assumes that soldering a pre-approved FCC module onto a custom carrier board inherits the module's FCC grant. If this assumption is wrong, the fallback is to place the Semtech modem and Nordic SoC **directly on the carrier board** as discrete components and pursue full FCC certification. This is a "first thousand units" problem — not a blocker for initial development and prototyping.

### Sidewalk BLE dependency

It is not fully confirmed whether the Sidewalk SDK *requires* BLE hardware or merely expects the BLE stack to compile and link. The RAK4630 sidesteps this question by having real BLE hardware (nRF52840's on-chip radio), but if a future module swap is considered, this dependency needs to be explicitly tested first.
