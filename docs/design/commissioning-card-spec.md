# SideCharge Commissioning Checklist Card -- Design Specification

**Task**: TASK-041
**Version**: 1.0
**Date**: 2026-02-13
**Author**: Bobby (Brand Guardian)
**Source**: PRD sections 1.4, 2.5.2, 2.5.4; Brand Guidelines v2.0

---

## 1. Card Format and Physical Specs

### Dimensions
- **Flat size**: 8.5" x 11" (US Letter)
- **Folded size**: 5.5" x 4.25" (quarter-fold, fits in product box)
- **Fold method**: Single horizontal fold at 5.5" (top half / bottom half), then single vertical fold at 4.25" (left / right). The front panel after folding is the bottom-right quadrant of Side A.

### Paper Stock
- **Weight**: 100 lb cover (270 gsm) -- heavy enough to write on against a junction box, rigid enough to stay in the enclosure sleeve
- **Finish**: Matte coating on Side A (the checklist side -- matte accepts ballpoint pen ink without smearing). Uncoated on Side B if cost-prohibitive to coat both; otherwise matte both sides.
- **Color**: White stock

### Print
- **Colors**: 4/4 (full color both sides). The teal, amber, and charcoal brand colors require CMYK process.
- **Bleed**: 0.125" on all edges
- **Safe zone**: 0.25" margin from trim on all edges -- no critical content in the bleed area

### Packaging
- Card ships pre-folded in the product box, positioned on top of the device (first thing the installer sees when opening the box). A belly band or sticker reading "START HERE -- Commissioning Checklist" in Sidewalk Teal on white wraps the folded card.

---

## 2. Layout Overview

When unfolded flat, the 8.5" x 11" sheet has four quadrants:

```
+---------------------------+---------------------------+
|                           |                           |
|    SIDE A -- TOP LEFT     |   SIDE A -- TOP RIGHT     |
|    (Panel 2 when folded)  |   (Panel 3 when folded)   |
|                           |                           |
|    COMMISSIONING          |   COMMISSIONING           |
|    CHECKLIST              |   CHECKLIST               |
|    Steps C-01 to C-07     |   Steps C-08 to C-12      |
|                           |   + Installer Sign-Off    |
|                           |                           |
+---------------------------+---------------------------+   <-- Horizontal fold
|                           |                           |
|    SIDE A -- BOTTOM LEFT  |   SIDE A -- BOTTOM RIGHT  |
|    (Panel 4 / back cover  |   (Panel 1 / front cover  |
|     when folded)          |    when folded)            |
|                           |                           |
|    LED QUICK REFERENCE    |   FRONT COVER:            |
|    + Failure Mode Table   |   Logo, Tagline,          |
|                           |   "Commissioning           |
|                           |    Checklist"              |
|                           |                           |
+---------------------------+---------------------------+
```

**Side B** (reverse of sheet) has the wiring diagram. When the card is unfolded, Side B is a full 8.5" x 11" wiring reference that can be laid flat on a work surface.

```
+-------------------------------------------------------+
|                                                       |
|                   SIDE B (full sheet)                  |
|                                                       |
|              SIMPLIFIED WIRING DIAGRAM                |
|                                                       |
|   Left half: Connection diagram                       |
|   Right half: Current clamp detail + terminal map     |
|                                                       |
+-------------------------------------------------------+
```

---

## 3. Typography Specifications (Print Adaptations)

Print uses the same brand font families. For print production, specify:

| Role | Font | Weight | Size (print) | Color |
|------|------|--------|--------------|-------|
| Card title ("Commissioning Checklist") | Space Grotesk | 700 (Bold) | 18pt | Deep Current (#064E56) |
| Section headers ("Pre-Power Checks", "Powered Tests") | Space Grotesk | 700 (Bold) | 12pt | Sidewalk Teal (#0A7E8C) |
| Step ID (C-01, C-02...) | JetBrains Mono | 500 (Medium) | 11pt | Sidewalk Teal (#0A7E8C) |
| Step description text | Inter | 400 (Regular) | 9pt | Charcoal (#1A1A2E) |
| Pass criteria text | Inter | 400 (Regular) | 8pt | Slate (#6B7280) |
| Checkbox labels ("PASS" / "FAIL") | Inter | 600 (Semi-bold) | 8pt | Charcoal (#1A1A2E) |
| Installer sign-off labels | Inter | 500 (Medium) | 9pt | Charcoal (#1A1A2E) |
| LED reference table | Inter | 400 (Regular) | 8pt | Charcoal (#1A1A2E) |
| Footer / regulatory text | Inter | 400 (Regular) | 7pt | Slate (#6B7280) |
| Warning callouts | Inter | 600 (Semi-bold) | 9pt | Charcoal (#1A1A2E) on Electric Amber (#F5A623) background |

---

## 4. Color Usage (Print)

| Element | Color | Hex | CMYK Equivalent |
|---------|-------|-----|-----------------|
| Headers, step IDs, rules | Sidewalk Teal | #0A7E8C | C:83 M:20 Y:28 K:5 |
| Body text, checkbox outlines | Charcoal | #1A1A2E | C:75 M:70 Y:50 K:65 |
| Secondary text, pass criteria | Slate | #6B7280 | C:45 M:35 Y:25 K:15 |
| Warning bars, critical callouts | Electric Amber | #F5A623 | C:0 M:36 Y:88 K:0 |
| Success indicators, pass marks | Grid Green | #2ECC71 | C:65 M:0 Y:65 K:0 |
| Card background | White | #FFFFFF | -- |
| Front cover background | Deep Current | #064E56 | C:90 M:35 Y:40 K:40 |
| Section background tint | Mist Teal 20% opacity | #B8E4E9 @ 20% | Light tint |

---

## 5. Side A Content -- Full Text

### Panel 1: Front Cover (Bottom-Right Quadrant, 4.25" x 5.5")

**Background**: Deep Current (#064E56), full bleed to edges of this quadrant.

**Content** (all text white on dark background):

```
[SideCharge logo -- white reversed variant, centered, 1.5" wide]

------

COMMISSIONING
CHECKLIST

------

Level 2 charging. No panel upgrade.

------

[Small text, bottom of panel:]
This card is your installation record.
Complete all 12 steps. Leave this card at the job site.
```

The logo uses the white (reversed) variant per brand guidelines section 8. The tagline "Level 2 charging. No panel upgrade." is the recommended primary tagline.

A thin horizontal rule (0.5pt, white at 40% opacity) separates the title block from the tagline and the instruction.

---

### Panel 2: Checklist Steps C-01 through C-07 (Top-Left Quadrant, 4.25" x 5.5")

**Background**: White. A 4pt Sidewalk Teal rule runs across the top edge.

**Section header**: "PRE-POWER INSPECTION" in Space Grotesk Bold 12pt, Sidewalk Teal, with a warning icon (triangle with exclamation mark) in Electric Amber beside it.

**Warning bar** (full width, Electric Amber background, 8pt padding):
> CIRCUIT BREAKER MUST BE OFF FOR STEPS C-01 THROUGH C-05.
> This device cannot detect 240V wiring errors. These visual checks are the only defense against branch circuit faults.

**Step format** (repeated for each step):

```
+-------------------------------------------------------+
| [PASS] [FAIL]                                         |
|  [ ]    [ ]     C-01  BRANCH CIRCUIT WIRING           |
|                                                       |
|  Inspect: conductor gauge, breaker rating,            |
|  junction box connections, EGC continuity.             |
|                                                       |
|  PASS: Conductors match breaker rating per            |
|  NEC 240.4(D). EGC continuous from panel to           |
|  both loads.                                          |
+-------------------------------------------------------+
```

Each step is a bordered card (0.5pt Slate border, 4pt padding, 6pt bottom margin). The step ID (C-01) is in JetBrains Mono, Sidewalk Teal. The step title is in Inter Semi-bold, Charcoal. The description and pass criteria are in Inter Regular, Slate. PASS and FAIL checkboxes are 4mm squares with 0.5pt Charcoal borders.

**Steps on this panel:**

---

**C-01 -- BRANCH CIRCUIT WIRING** [P0]

Inspect: Conductor gauge, breaker rating, junction box connections, EGC continuity.

PASS: Conductors match breaker rating per NEC 240.4(D). EGC continuous from panel to both loads.

---

**C-02 -- CURRENT CLAMP PLACEMENT** [P0]

Inspect: Clamp on correct conductor (EV charger leg, not AC leg). Arrow on clamp points toward load.

PASS: Clamp on EV charger conductor, correct orientation (arrow toward load). See wiring diagram on reverse.

---

**C-03 -- THERMOSTAT WIRING** [P0]

Inspect: Y, C, and G wires on correct SideCharge terminals. No shorts between thermostat conductors.

PASS: Y (cool call), C (common/24VAC), G (fan) on correct terminals per wiring diagram on reverse.

---

**C-04 -- J1772 PILOT TAP** [P0]

Inspect: Pilot signal wire connected to correct terminal on EVSE side.

PASS: Pilot wire connected to SideCharge pilot input terminal. See wiring diagram on reverse.

---

**C-05 -- PHYSICAL MOUNTING** [P1]

Inspect: Device secured to wall or junction box. Conduit entries sealed. No strain on low-voltage wires.

PASS: Device firmly mounted. All conduit entries sealed. No wire strain.

---

**Section header**: "POWERED TESTS" in Space Grotesk Bold 12pt, Sidewalk Teal.

**Instruction bar** (full width, Mist Teal 20% background):
> Turn ON circuit breaker. Observe LEDs. Steps C-06 through C-12 verify device operation.

---

**C-06 -- POWER ON** [P0]

Action: Apply power (close breaker). Observe device.

PASS: Green LED begins 1Hz flash (commissioning mode) within 5 seconds.

---

**C-07 -- SIDEWALK CONNECTIVITY** [P0]

Action: Wait for LED transition. Allow up to 5 minutes.

PASS: Green LED transitions from 1Hz flash to solid (typically 30--90 seconds). If still flashing after 5 minutes, verify Sidewalk gateway is within range.

---

### Panel 3: Checklist Steps C-08 through C-12 + Installer Sign-Off (Top-Right Quadrant, 4.25" x 5.5")

**Background**: White. 4pt Sidewalk Teal rule across top edge (continues from Panel 2).

**Steps (continued):**

---

**C-08 -- THERMOSTAT DETECTION** [P0]

Action: Set thermostat to call for cooling.

PASS: Blue LED transitions to heartbeat pulse (200ms on, 1800ms off). Confirms device sees cool call signal.

---

**C-09 -- THERMOSTAT RELEASE** [P0]

Action: Cancel cooling call at thermostat.

PASS: Blue LED returns to off (idle). Confirms device sees signal drop.

---

**C-10 -- EV CHARGE DETECTION** [P0]

Action: Plug in EV, or have an assistant start a charge session. (If no EV available, verify via shell command `app evse c` during development.)

PASS: Blue LED goes solid. If cloud-verified: current reading >0 in uplink.

---

**C-11 -- INTERLOCK TEST** [P0]

Action: Trigger AC call (set thermostat to cool) WHILE the EV is charging.

PASS: Blue LED transitions to heartbeat pulse. EV charger pauses (pilot drops to State B).

**Warning bar** (Electric Amber background):
> THIS IS THE CRITICAL SAFETY TEST. It confirms mutual exclusion works end to end. If the EV charger does NOT pause, STOP. Do not leave the job site. Investigate charge enable wiring and J1772 pilot connection.

---

**C-12 -- CHARGE RESUME** [P1]

Action: Cancel AC call (turn off cooling at thermostat) while EV is still connected.

PASS: Blue LED returns to solid. EV charger resumes (pilot returns to State C).

---

**Separator**: 1pt Sidewalk Teal rule, full width, 12pt vertical space above and below.

---

**INSTALLER SIGN-OFF**

This section has a light Mist Teal 20% background to visually distinguish it from the checklist.

```
INSTALLATION RECORD

Installer Name: ___________________________________________

Company: __________________________________________________

License Number: ____________________________________________

Date: _____________________________________________________

Device ID (from label): ___________________________________

Steps Passed: ____ / 12       Steps Failed: ____ / 12

All P0 steps passed?    [ ] YES    [ ] NO

Installer Signature: ______________________________________

Notes: ____________________________________________________

___________________________________________________________
```

All fields use 0.5pt Charcoal underlines on white, with Inter Medium 9pt labels. The "All P0 steps passed?" line is emphasized with Inter Semi-bold. The YES/NO checkboxes are 5mm squares.

**Footer** (7pt, Slate, bottom of panel):
> This card is the installation record. Attach to device enclosure or junction box cover. Retain for inspection.

---

### Panel 4: LED Quick Reference + Failure Modes (Bottom-Left Quadrant, 4.25" x 5.5")

**Background**: White. 4pt Sidewalk Teal rule across top edge.

**Section header**: "LED QUICK REFERENCE" in Space Grotesk Bold 12pt, Sidewalk Teal.

**Subhead**: "Green LED = Health and Connectivity. Blue LED = Interlock and Charging." in Inter Medium 9pt, Slate.

**LED state table** (bordered, alternating Mist Teal 10% / white row backgrounds):

| What You See | Green LED | Blue LED | Meaning |
|---|---|---|---|
| **Healthy, idle** | Solid | Off | Connected, no load active |
| **EV charging** | Solid | Solid | EV drawing current, interlock healthy |
| **AC has priority** | Solid | Heartbeat pulse | AC cooling call active, EV paused |
| **Commissioning** | 1Hz flash | Off | Starting up, connecting to Sidewalk |
| **Sidewalk disconnected** | Slow flash | (per load state) | No network -- interlock still works locally |
| **OTA update** | Double-blink | Off | Firmware updating -- do not power off |

Table uses Inter Regular 8pt for cell content, Inter Semi-bold 8pt for the "What You See" column. Cell padding 3pt.

---

**Separator**: 0.5pt Slate rule, 8pt vertical space.

**Section header**: "TROUBLESHOOTING -- LED ERROR PATTERNS" in Space Grotesk Bold 12pt, Electric Amber.

**Four error scenarios**, each in a bordered card with Electric Amber left-edge accent (3pt solid amber bar on the left side of each card):

---

**1. BOTH LEDs RAPID FLASH (5Hz)**
**Meaning**: Hardware fault -- sensor read failure, relay stuck, or persistent connectivity failure after 10 minutes.
**What to do**: Power-cycle the device (open and close breaker). If rapid flash returns within 60 seconds, check: (a) thermostat wire connections, (b) current clamp connection, (c) J1772 pilot wire. If the fault persists after re-checking all connections, contact SideCharge support.

---

**2. GREEN LED STAYS 1Hz FLASH FOR >5 MINUTES**
**Meaning**: Device cannot connect to Amazon Sidewalk network.
**What to do**: Verify a Sidewalk gateway (Amazon Echo, Ring camera, or eero router) is within 500 meters with line of sight. Move the gateway closer if possible. The interlock still works without connectivity, but the device cannot report status or receive cloud commands until connected.

---

**3. BLUE LED DOES NOT RESPOND TO THERMOSTAT CALL (C-08 fails)**
**Meaning**: Device is not detecting the thermostat signal.
**What to do**: Verify Y (cool call) and C (common/24VAC) wires are on the correct SideCharge terminals. Check for 24VAC at the C terminal with a multimeter. If voltage is present but the LED does not respond, the thermostat may not be sending a call signal -- verify at the thermostat that cooling is actually requested.

---

**4. EV CHARGER DOES NOT PAUSE DURING INTERLOCK TEST (C-11 fails)**
**Meaning**: The charge enable circuit is not controlling the EVSE. The software interlock may be defeated.
**What to do**: DO NOT leave the job site. Verify the J1772 pilot tap wire is connected to the correct terminal on both the EVSE and the SideCharge device. Verify the charge enable relay wiring. If the EV charger does not respond to the SideCharge interlock, the installation is not safe for operation. Disconnect the EV charger circuit until the issue is resolved.

---

**Footer** (7pt, Slate, bottom of panel):
> SideCharge by Eta Works, Inc. | support@sidecharge.com | sidecharge.com/install

---

## 6. Side B Content -- Full Wiring Diagram (Full 8.5" x 11")

Side B is a single full-sheet wiring reference. When the installer unfolds the card, they see the complete connection diagram laid flat.

### Layout

```
+-------------------------------------------------------+
|  SIDECHARGE WIRING REFERENCE              [Logo, sm]  |
|                                                       |
|  +-------------------------+  +---------------------+ |
|  |                         |  |                     | |
|  |   MAIN CONNECTION       |  |  CURRENT CLAMP     | |
|  |   DIAGRAM               |  |  DETAIL             | |
|  |   (left 60%)            |  |  (right 40%)        | |
|  |                         |  |                     | |
|  |                         |  |                     | |
|  |                         |  |                     | |
|  +-------------------------+  +---------------------+ |
|                                                       |
|  +---------------------------------------------------+|
|  |  TERMINAL MAP (bottom strip, full width)           ||
|  +---------------------------------------------------+|
|                                                       |
+-------------------------------------------------------+
```

### Main Connection Diagram (Left 60%, approximately 5" x 7")

**What to illustrate** (description for graphic designer / illustrator):

The diagram shows the SideCharge device at center, with four connection groups radiating outward. Use Sidewalk Teal for signal/control wires, Electric Amber for the 240V circuit path, and Slate for ground/neutral. All wires are labeled with both their function and their terminal designation.

**Elements to show:**

1. **24VAC Power Source (top-left)**
   - Thermostat transformer, shown as a simple box labeled "24VAC XFMR"
   - Two wires from transformer to SideCharge: R (24VAC hot) and C (24VAC common)
   - Wire color: Sidewalk Teal
   - Label: "18-22 AWG thermostat cable"

2. **Thermostat Wires (top-center)**
   - Three wires from thermostat to SideCharge terminals:
     - Y (cool call signal) -- Sidewalk Teal
     - C (common) -- Sidewalk Teal
     - G (fan) -- Sidewalk Teal, dashed line (optional connection)
   - Thermostat shown as simple box labeled "THERMOSTAT"
   - Arrow indicating signal direction: thermostat to SideCharge

3. **J1772 Pilot Connection (right)**
   - Single signal wire from EVSE pilot circuit to SideCharge pilot input terminal
   - Wire color: Signal Violet (to visually distinguish from thermostat wires)
   - EVSE shown as a box labeled "LEVEL 2 EVSE (EV CHARGER)"
   - Label: "J1772 Pilot Signal -- tap from EVSE pilot circuit"
   - Note: "Low voltage signal wire only. Do not connect to 240V."

4. **Current Clamp (bottom)**
   - The 240V circuit path is shown as two thick lines (L1, L2) running from "PANEL / JUNCTION BOX" to the EVSE
   - The current clamp is illustrated wrapping around ONE conductor on the EV charger leg
   - A directional arrow on the clamp body points toward the load (EVSE)
   - Callout box with Electric Amber border: "CLAMP ON EV CHARGER LEG ONLY. Arrow toward load."
   - A second callout shows the WRONG placement (on the AC leg) with a red X through it
   - The AC compressor branch is shown splitting from the junction box in the opposite direction, labeled "TO AC COMPRESSOR" -- the clamp is NOT on this leg

5. **SideCharge Device (center)**
   - Shown as a rounded rectangle with terminal labels clearly marked
   - Terminal strip along one edge with labeled positions for each connection
   - The device label area is shown with "Device ID: ___________" and a callout: "Record this ID on the checklist (front)"

### Current Clamp Detail (Right 40%, approximately 3.5" x 4")

A close-up illustration of the current clamp installation:

1. **Correct installation**: Clamp around a single conductor (not both L1 and L2 together). Arrow on clamp body points toward the EVSE. The conductor is the EV charger leg, not the AC compressor leg. Label: "CORRECT" with Grid Green checkmark.

2. **Three common mistakes** (each with a red X):
   - Clamp on AC leg instead of EV leg: "WRONG -- clamp must be on EV charger conductor"
   - Clamp around both conductors: "WRONG -- clamp around both wires reads zero"
   - Arrow pointing away from load: "WRONG -- arrow must point toward EVSE (load)"

Use simple line art style. Conductors shown as thick parallel lines. The clamp shown as a hinged ring with a clear directional arrow.

### Terminal Map (Bottom Strip, full width, approximately 8.5" x 2")

A horizontal strip showing the SideCharge terminal block in a 1:1 or 2:1 scale illustration, with each terminal labeled:

| Terminal | Wire | Source | Notes |
|----------|------|--------|-------|
| C | 24VAC Common | Transformer | Power supply -- always connected |
| R | 24VAC Hot | Transformer | Power supply -- always connected |
| Y | Cool Call | Thermostat | Signal input -- goes HIGH when thermostat calls for cooling |
| G | Fan | Thermostat | Optional -- fan control |
| PILOT | J1772 Pilot | EVSE | Signal input -- 1kHz square wave from charger |
| CT+ / CT- | Current Clamp | Clamp leads | Polarity matters -- match clamp lead colors to terminal markings |

**Note below terminal map** (Inter Regular 8pt, Slate):
> All SideCharge connections are low-voltage control signals (24VAC max). The current clamp is the only contact with the 240V circuit and is read-only. SideCharge does not switch, interrupt, or carry 240V power.

**Warning bar at bottom of Side B** (full width, Electric Amber background):
> CAUTION: All 240V branch circuit work must be performed by a licensed electrician with the circuit breaker OFF and locked out. Verify conductor sizing per NEC 240.4(D): #8 AWG Cu min for 40A, #6 AWG Cu for 50A circuits.

---

## 7. Brand Application Notes

1. **Logo placement**: White reversed variant on the Deep Current front cover panel (Panel 1). Small Charcoal monochrome variant in the top-right corner of Side B. No logo on the interior checklist panels -- the teal rule and typography carry the brand.

2. **Color restraint**: The checklist interior (Panels 2 and 3) uses only Charcoal, Slate, Sidewalk Teal, and the Electric Amber warning bars. No violet, no green, no gradient. The card must read as a professional trade document, not a marketing piece.

3. **Electric Amber for warnings only**: On this card, amber marks safety-critical callouts. It is never decorative. Every amber element means "pay attention here."

4. **JetBrains Mono for identifiers**: Step IDs (C-01 through C-12), device ID field, and any technical values (voltage, current, wire gauge) use JetBrains Mono to signal precision and distinguish technical content from prose.

5. **No whimsy on this card**: This is a safety document. The Whitney-approved personality has no place here. The only brand expression is the front cover tagline and the consistent use of the type and color system. The tone is Installer/Trade: practical, code-aware, respects their expertise.

---

## 8. Print Production Requirements

| Specification | Value |
|---------------|-------|
| Final trim size | 8.5" x 11" (flat), 4.25" x 5.5" (folded) |
| Fold type | Quarter-fold (1 horizontal + 1 vertical) |
| Stock | 100 lb cover (270 gsm), white, uncoated or matte coated |
| Print method | Offset lithography (for runs >500) or digital (for initial runs <500) |
| Color | 4/4 CMYK process, both sides |
| Coating | Matte aqueous coating, Side A (checklist side) -- must accept ballpoint pen |
| Bleed | 0.125" all edges |
| Scoring | Score both fold lines before folding to prevent cracking on heavy stock |
| Quantity (initial run) | Match product box quantity -- 1 card per box |
| Proofing | Hard proof required before production run -- verify Sidewalk Teal and Electric Amber match brand swatches |

### Pen Compatibility Note

The checklist is designed to be filled in with a ballpoint pen on-site. The matte finish is critical -- gloss or UV coating will cause ink beading and smearing. If matte aqueous coating is used, verify pen compatibility on a press proof before committing to the full run.

---

## 9. Design File Deliverables

The graphic designer should produce:

1. **Print-ready PDF** (PDF/X-4): Both sides, with bleed, crop marks, and color bars
2. **Working file**: Adobe InDesign (.indd) or Affinity Publisher (.afpub) with packaged fonts and linked images
3. **Wiring diagram source**: Adobe Illustrator (.ai) or SVG -- vector art, editable, so terminal labels and wire colors can be updated when the PCB terminal layout is finalized (TASK-019)
4. **Current clamp detail source**: Separate vector file, reusable in installation guide and on the website

### Dependency on TASK-019 (PCB Design)

The terminal map on Side B depends on the final PCB terminal layout from TASK-019. The current terminal names (C, R, Y, G, PILOT, CT+, CT-) are based on the PRD section 1.4 requirements. If the PCB design changes terminal positions or labels, the wiring diagram must be updated before the card goes to print. **Do not send to print until TASK-019 terminal layout is confirmed.**

---

## 10. Content Review Checklist

Before sending to print, verify:

- [ ] All 12 commissioning steps match PRD section 2.5.2 exactly (step IDs, methods, pass criteria)
- [ ] Warning language on C-11 (interlock test) is present and prominent
- [ ] "Circuit breaker OFF" warning appears before steps C-01 through C-05
- [ ] Current clamp orientation detail shows correct and incorrect placements
- [ ] Thermostat wire designations (Y, C, G) match final PCB silk screen (TASK-019)
- [ ] Device ID field is present in installer sign-off section
- [ ] LED patterns match PRD section 2.5.1.1 (production dual-LED matrix)
- [ ] Failure mode quick reference covers all 4 scenarios from PRD 2.5.4
- [ ] All brand colors match approved CMYK values on hard proof
- [ ] Matte coating confirmed pen-compatible on press proof
- [ ] Footer includes Eta Works company name and support contact
- [ ] No use of "IoT," "seamless," "revolutionary," or other brand-prohibited words
- [ ] Tone is Installer/Trade throughout -- no consumer marketing language

---

*Specification prepared by Bobby (Brand Guardian) for TASK-041.*
*References: PRD v2026-02-13 sections 1.4, 2.5.1.1, 2.5.2, 2.5.4; Brand Guidelines v2.0.*
