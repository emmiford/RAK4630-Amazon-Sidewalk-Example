# SideCharge Brand Guidelines

> "Level 2 charging. No panel upgrade."

---

## 1. Brand Overview

### Why "SideCharge"?

**SideCharge** fuses the product's two defining traits: **Side**(walk) -- the Amazon Sidewalk network that carries the signal -- and **Charge** -- the EV charging it enables on an existing circuit. It is short, memorable, action-oriented, and immediately communicates what the product does without drowning in acronyms (RAK, EVSE, LoRa, J1772). The name works as a noun ("Check your SideCharge dashboard") and as a verb concept ("SideCharge your home"). The name also carries a subtle double meaning: charging your EV *on the side* of an existing circuit -- which is literally what the interlock does.

If legal/trademark review requires alternatives, consider: **SideVolt**, **ChargeCast**, **VoltSide**, **WalkCharge**.

### Brand Purpose

SideCharge exists to eliminate the #1 barrier to Level 2 EV charger adoption: the panel upgrade. By interlocking an EV charger with an existing HVAC circuit, SideCharge enables same-day, code-compliant Level 2 installs in homes that would otherwise require $2,400+ in electrical upgrades and days of waiting. We believe every home with central air already has a Level 2 charger circuit -- it just doesn't know it yet.

### Brand Vision

A world where every home with central air can add Level 2 EV charging the same day, with no panel upgrade, no new breaker, and no infrastructure barrier -- connected and optimized over a free neighborhood network.

### Brand Mission

Deliver a code-compliant circuit interlock that shares an existing HVAC circuit with a Level 2 EV charger, connected over Amazon Sidewalk's free LoRa mesh network -- eliminating panel upgrades, WiFi dependencies, and monthly fees in a single device.

### Tagline Options (pick one, test with users)

| Tagline | Tone |
|---------|------|
| **"Level 2 charging. No panel upgrade."** | Direct, problem-solving (recommended primary) |
| "One circuit. Two loads. Zero upgrades." | Technical confidence, specificity |
| "Your AC circuit is also your EV circuit." | Clever, intriguing |
| "Smart charging. No WiFi required." | Direct, differentiating (connectivity angle) |
| "The invisible upgrade for every EV charger." | Intrigue, simplicity |

---

## 2. Competitive Positioning vs. ChargePoint

### What ChargePoint Does Well
- **Simplicity messaging**: "Electric mobility is the smart choice. We make it the easy one, too."
- **Trust signals**: 5,000+ brands, Fortune 500 adoption, 17+ years in market
- **Dual-tone palette**: Professional blue (#0F588A) grounded by energetic orange (#EF7622)
- **Accessible typography**: Gotham Narrow -- clean, rounded, approachable sans-serif
- **Platform positioning**: Hardware-agnostic, scale-oriented

### Where SideCharge Goes Further

| Dimension | ChargePoint | SideCharge (Our Advantage) |
|-----------|-------------|----------------------------|
| **Installation** | Requires dedicated circuit; may require panel upgrade | Circuit interlock: shares existing HVAC circuit, no new breaker, no panel upgrade |
| **Install time** | Days to weeks (if panel upgrade needed) | Same-day install |
| **Install cost** | $2,400+ if panel upgrade required | ~$1,000 equipment + install |
| **Connectivity** | Requires WiFi/cellular | Zero-infrastructure: Amazon Sidewalk LoRa mesh |
| **Target** | Commercial fleets, businesses | Home EV owners, electricians/installers, utilities |
| **Cost model** | Hardware + subscription | No monthly fees |
| **Update mechanism** | Standard OTA over broadband | Delta OTA over LoRa in seconds (not hours) |
| **Footprint** | Full charging station | 4KB app on existing hardware |
| **Energy scope** | EV charging only | Circuit interlock: EV + HVAC coordinated load management |
| **Grid impact** | Adds peak demand | Adds a charger without increasing peak demand |
| **Brand feel** | Corporate, enterprise-scale | Personal, neighborhood-scale, whimsical confidence |

### Brand Personality Shift

ChargePoint's personality is **corporate authority** -- "we are the biggest network." SideCharge's personality is **neighborhood ingenuity** -- "your home already has the circuit, you just need the interlock." We are the clever underdog that eliminates a $2,400 barrier with an elegant $1,000 device and zero infrastructure.

---

## 3. Color Palette

### Design Rationale

ChargePoint uses corporate blue + safety orange. We take a different path: **deep teal** (the intelligence of blue meets the sustainability of green) paired with **electric amber** (warmer, more inviting than ChargePoint's traffic-cone orange). A vivid **signal violet** accent adds the Whitney-approved whimsy -- unexpected in the EV space, immediately distinctive.

### Primary Colors

| Role | Name | Hex | RGB | Usage |
|------|------|-----|-----|-------|
| **Primary** | Sidewalk Teal | `#0A7E8C` | 10, 126, 140 | Headers, primary buttons, brand mark, nav bars |
| **Primary Dark** | Deep Current | `#064E56` | 6, 78, 86 | Text on light backgrounds, footer, dark UI |
| **Primary Light** | Mist Teal | `#B8E4E9` | 184, 228, 233 | Backgrounds, cards, hover states |

### Accent Colors

| Role | Name | Hex | RGB | Usage |
|------|------|-----|-----|-------|
| **Accent Warm** | Electric Amber | `#F5A623` | 245, 166, 35 | CTAs, highlights, charging-active states, badges |
| **Accent Pop** | Signal Violet | `#7B61FF` | 123, 97, 255 | Whimsy moments: icons, data viz, micro-animations, links |
| **Accent Success** | Grid Green | `#2ECC71` | 46, 204, 113 | Charging complete, healthy status, positive metrics |

### Neutral Colors

| Role | Name | Hex | RGB | Usage |
|------|------|-----|-----|-------|
| **Dark** | Charcoal | `#1A1A2E` | 26, 26, 46 | Body text, dark mode backgrounds |
| **Mid** | Slate | `#6B7280` | 107, 114, 128 | Secondary text, borders, disabled states |
| **Light** | Cloud | `#F4F5F7` | 244, 245, 247 | Page backgrounds, card backgrounds |
| **White** | Pure White | `#FFFFFF` | 255, 255, 255 | Content areas, contrast base |

### Color Usage Rules

1. **Sidewalk Teal** should represent 60% of branded surface area
2. **Electric Amber** is reserved for action and attention -- never use for large background fills
3. **Signal Violet** is the "surprise" -- use sparingly (5-10%) for delight moments
4. **Never place Amber text on Teal background** -- insufficient contrast ratio
5. All text must meet WCAG 2.1 AA contrast minimums (4.5:1 for body, 3:1 for large text)
6. Dark mode: swap Charcoal to backgrounds, use Mist Teal and Cloud for text

### Gradient (Hero Use Only)

```css
background: linear-gradient(135deg, #064E56 0%, #0A7E8C 50%, #7B61FF 100%);
```

Use for hero banners and feature section backgrounds. Never for buttons or small UI elements.

---

## 4. Typography

### Font Stack (Google Fonts -- free, web-optimized)

| Role | Font | Weight(s) | Fallback | Usage |
|------|------|-----------|----------|-------|
| **Headlines** | [Space Grotesk](https://fonts.google.com/specimen/Space+Grotesk) | 500 (Medium), 700 (Bold) | system-ui, sans-serif | H1-H3, hero text, feature titles |
| **Body** | [Inter](https://fonts.google.com/specimen/Inter) | 400 (Regular), 500 (Medium), 600 (Semi-bold) | system-ui, sans-serif | Paragraphs, UI labels, buttons |
| **Monospace/Data** | [JetBrains Mono](https://fonts.google.com/specimen/JetBrains+Mono) | 400, 500 | monospace | Firmware versions, technical specs, data readouts |

### Why These Fonts?

- **Space Grotesk** over Gotham Narrow: More geometric and modern, with a subtle tech personality that ChargePoint's Gotham lacks. The slightly squared terminals feel engineered without being cold. Free, unlike Gotham.
- **Inter** for body: The gold standard of web body text -- designed specifically for screens, excellent legibility at small sizes, massive language support.
- **JetBrains Mono** for data: Honors the product's embedded engineering DNA. When showing firmware versions (v0x05), voltage readings (242.1V), or chunk counts -- this font says "precision."

### Type Scale

| Element | Font | Size | Weight | Line Height | Letter Spacing |
|---------|------|------|--------|-------------|----------------|
| H1 (Hero) | Space Grotesk | 48px / 3rem | 700 | 1.1 | -0.02em |
| H2 (Section) | Space Grotesk | 36px / 2.25rem | 700 | 1.2 | -0.01em |
| H3 (Card title) | Space Grotesk | 24px / 1.5rem | 500 | 1.3 | 0 |
| Body Large | Inter | 18px / 1.125rem | 400 | 1.6 | 0 |
| Body | Inter | 16px / 1rem | 400 | 1.6 | 0 |
| Body Small | Inter | 14px / 0.875rem | 400 | 1.5 | 0.01em |
| Caption | Inter | 12px / 0.75rem | 500 | 1.4 | 0.02em |
| Data Readout | JetBrains Mono | 20px / 1.25rem | 500 | 1.4 | 0.05em |

### Mobile Adjustments

On viewports below 640px:
- H1 drops to 32px / 2rem
- H2 drops to 24px / 1.5rem
- Body remains 16px (never go below 16px on mobile)

---

## 5. Voice and Tone

### Brand Voice (Consistent Always)

SideCharge speaks with **confident clarity**. We are the electrician's friend who also speaks fluent grid economics -- we know the deep technical details but we lead with what matters to *you*. We never hide behind jargon, but we never dumb things down either. We respect our audience's intelligence while valuing their time.

### Voice Attributes

| Attribute | What It Means | Example |
|-----------|---------------|---------|
| **Clear** | No acronyms without explanation on first use. Short sentences. Active voice. | "SideCharge interlocks your EV charger with your AC circuit -- so you can install Level 2 charging without upgrading your panel." |
| **Confident** | We state capabilities directly. No hedging ("might," "up to," "helps you possibly"). | "Same-day install. No panel upgrade. Code compliant." Not: "May help reduce the need for panel upgrades in some cases." |
| **Warm** | Conversational but not slangy. We use "you/your" more than "our/we." | "Your home already has the circuit. SideCharge just lets your car use it too." |
| **Whimsical** | A light touch of surprise and delight. Not forced, never corny. The Whitney touch. | "4 kilobytes. That's smaller than this paragraph. And it coordinates your entire home's electrical appetite." |
| **Honest** | We name limitations. We don't oversell. Trust is our moat. | "Your AC and your car can't charge at the same time -- but your AC runs 2 hours a day and your car charges at night, so they were never going to fight over it anyway." |

### Tone Shifts by Context

| Context | Tone Adjustment | Example |
|---------|-----------------|---------|
| **Marketing / Hero** | Lead with the panel-upgrade problem, then the elegant solution | "Your panel says no. SideCharge says: you don't need to ask the panel." |
| **Installer / Trade** | Practical, code-aware, respects their expertise | "Circuit interlock per CEC 440.34. One device, one visit, one invoice. No permit delays." |
| **Product UI / Dashboard** | Precise, calm, functional | "HVAC: Idle. Charger: Active at 7.2 kW. Circuit: Within limits." |
| **Technical Docs** | Thorough, respectful of expertise | "The delta OTA system compares against the S3 baseline and transmits only changed chunks (15B payload, 19B with header)." |
| **Error States** | Reassuring, actionable | "HVAC call detected. Charging paused to protect circuit limits. Charging will resume when the AC cycle ends." |
| **Utility / Grid** | ROI-oriented, policy-aware | "One SideCharge adds a Level 2 charger without increasing peak demand or requiring distribution upgrades. At scale, that defers transformer investments." |
| **Social / Community** | Enthusiastic, peer-to-peer | "Installed a Level 2 charger in a house with a 100A panel today. No panel upgrade. No new breaker. Homeowner saved $2,400 and got same-day charging." |

### Words We Use

- **Interlock** (the technical term for what we do -- own it)
- **Circuit sharing** (consumer-friendly version of interlock)
- **Same-day install** (the outcome electricians and homeowners care about)
- **Code compliant** (CEC 440.34 -- this matters to inspectors and installers)
- **Smart** (not "intelligent" or "AI-powered" unless it actually is)
- **Connected** (not "IoT-enabled")
- **Neighborhood network** (for Amazon Sidewalk, when speaking to consumers)
- **Effortless** (not "seamless" -- that word has lost all meaning)
- **Compact** / **Tiny** (for the 4KB app -- this is a feature, celebrate it)

### Words We Avoid

- "Revolutionary" / "Disruptive" (we are evolutionary, and proud of it)
- "Leverage" / "Utilize" (say "use")
- "End-to-end" / "Holistic" (say what you actually mean)
- "Smart home" (we are smart *energy*, bigger than one category)
- "Just works" (Apple owns this; we say "works without WiFi")
- "Panel upgrade" used positively (it's always the problem we eliminate, never our solution)

---

## 6. Target Audiences

### Primary: Electricians and Installers

The people who hit the panel capacity wall every day. They show up to install a Level 2 charger, open the panel, and deliver the bad news: "You need a service upgrade." SideCharge turns that dead-end call into a same-day install and a satisfied customer.

**What they care about**: Code compliance, install speed, callback avoidance, margin per job, inspector approval.

**How we speak to them**: Peer-to-peer. Respect their expertise. Lead with CEC 440.34 and the install workflow, not the app or the cloud.

### Primary: Homeowners with Panel Constraints

Homes at or near electrical panel capacity (typically 100A service) who want Level 2 EV charging but face $2,400+ panel upgrade costs and multi-day timelines. They have central air. They have an existing 30-50A HVAC circuit. They have what they need -- they just don't know it.

**What they care about**: Cost savings, no hassle, same-day, will it work with my charger, is it safe.

**How we speak to them**: Warm, reassuring, benefit-first. Lead with "no panel upgrade," follow with "works with your existing AC circuit."

### Secondary: Utilities and Grid Operators

Utilities spending millions on panel upgrade incentives (SCE alone offers $4,200/home in disadvantaged communities) and distribution transformer upgrades. SideCharge adds EV chargers without increasing peak demand -- a far more cost-effective path to transportation electrification.

**What they care about**: Peak demand, transformer loading, program cost per charger deployed, demand response capability, equity/disadvantaged community access.

**How we speak to them**: ROI-first, data-driven. Lead with grid economics and program scalability.

### Secondary: EV Charger Manufacturers and Distributors

Companies selling Level 2 chargers who lose sales when customers discover they need a panel upgrade. SideCharge is a sell-through accessory that converts "no" into "yes."

**What they care about**: Attach rate, compatibility, channel simplicity.

---

## 7. Messaging Pillars

### Pillar 1: No Panel Upgrade Required.

**Core message**: SideCharge is a code-compliant circuit interlock that enables same-day Level 2 EV charger installation by sharing an existing HVAC circuit. No new breaker. No panel upgrade. No permit delays. If you have central air, you have a Level 2 charger circuit.

**How it works**: The device interlocks with the circuit running to the AC compressor and ties into the home thermostat. If the thermostat calls for cooling, car charging pauses. If the car is charging, the AC compressor signal is held. Both loads share one circuit -- they just never run simultaneously. This is the same principle as a generator interlock, applied to everyday load management.

**The numbers**:
- Panel upgrades cost **$2,400+** and add 3+ days to install timelines
- **14-26%** of California homes have applicable 100A panels + central air (17-33% in disadvantaged communities)
- HVAC compressors run **~2 hrs/day** in summer, mostly daytime. EVs charge **~2 hrs/day**, mostly nighttime. Natural circuit sharing.
- A typical HVAC for a 1,500-2,400 sq ft home matches Level 2 charging power: **30-50A**
- California Electric Code **440.34** allows HVAC circuits to share with other loads if interlocked
- SCE currently offers **$4,200 grants** per home for panel upgrades in disadvantaged communities. SideCharge achieves the same outcome at a fraction of the cost.
- Target price: **~$1,000** equipment + install

**Code compliance**: Per CEC 440.34, HVAC compressor circuits may be shared with other loads when a listed interlock device prevents simultaneous operation. SideCharge is that interlock.

**Proof points**:
- Works with most Level 2 chargers (any J1772/NACS charger with a standard plug)
- Uses low-voltage control wires -- no high-voltage modifications to existing wiring
- Cloud override capability: can force-block either load via microcontroller outputs
- Installs as part of the standard EV charger installation process

**Headline options**:
- "Level 2 charging. No panel upgrade."
- "Your AC circuit is also your EV circuit."
- "One circuit. Two loads. Zero upgrades."
- "Same-day charger install, even on a 100A panel."

---

### Pillar 2: No WiFi Required.

**Core message**: SideCharge connects over Amazon Sidewalk's free LoRa mesh network. No router configuration. No cellular plan. No monthly fees. If your neighbor has an Echo, you probably already have coverage. The device works in detached garages, carports, and driveways where WiFi doesn't reach.

**Proof points**:
- Amazon Sidewalk covers 90%+ of the US population
- LoRa range: 500-800m per hop, penetrates walls and garages
- Zero recurring connectivity cost
- Works in detached garages, carports, and driveways where WiFi doesn't reach
- No app pairing, no password entry, no network configuration

**Headline options**:
- "Connected by your neighborhood. Controlled by you."
- "Your garage has better coverage than you think."

---

### Pillar 3: Smart Energy Coordination.

**Core message**: The interlock is just the beginning. Once SideCharge is connected to the cloud, it gets smarter over time -- coordinating charging with time-of-use rates, grid carbon intensity, utility demand response signals, and HVAC scheduling. The same device that eliminates your panel upgrade becomes the energy brain of your home.

**Proof points**:
- Time-of-use rate awareness: charge when electricity is cheapest
- Demand response capability for utility programs
- Grid carbon optimization: charge when the grid is cleanest
- HVAC + EV coordinated scheduling (beyond simple interlocking)
- Cloud override for both loads -- demand response with one device, two controllable loads
- Adds a charger without increasing peak demand -- net-zero impact on the grid
- Can defer distribution transformer upgrades at the utility scale

**Headline options**:
- "The interlock that gets smarter over time."
- "Your charger and your thermostat, on the same team."
- "One device. Two loads. Infinite grid value."

---

### Pillar 4: Tiny, Updatable, Future-Proof.

**Core message**: SideCharge runs on a 4KB application -- smaller than a thumbnail image. Delta OTA firmware updates arrive over LoRa in seconds, not hours. The device you install today keeps improving without a truck roll, a WiFi network, or a service call.

**Proof points**:
- 4KB app binary (split-image architecture)
- Delta mode compares against baseline, sends only changed chunks
- Full update verified end-to-end in ~15 seconds over LoRa
- Reliable split-image architecture: platform and app update independently
- OTA staging partition with validation before apply -- never bricks

**Headline options**:
- "4 kilobytes of calm."
- "Firmware updates at the speed of 'did that just happen?'"

---

## 8. Logo Direction (Brief for Designer)

### Concept

The SideCharge logo should combine three visual ideas:
1. A **circuit interlock or toggle** (the core product function -- two loads sharing one circuit, never simultaneously)
2. A **charging bolt or plug silhouette** (EV charging)
3. A **radio wave or mesh arc** (Sidewalk/LoRa connectivity)

The primary visual concept is the **interlock**: two paths diverging from a single source, with a toggle or gate between them. Think of a railroad switch, a circuit breaker toggle, or two arrows branching from one line with a control point at the fork. This represents the core product truth: one circuit, two loads, intelligently switched.

The wordmark "SideCharge" uses **Space Grotesk Bold**, with the "S" or "C" potentially incorporating the toggle/switch motif or mesh-wave as a ligature or accent.

### Mark (Icon)

A standalone icon for app icons, favicons, and small contexts. Primary concept: a stylized **split path** -- a single line that forks into two branches (one representing HVAC, one representing EV), with a small node or switch at the junction. Signal arcs can emanate from the node to represent connectivity. Think: "a smart switch broadcasting." Color: Sidewalk Teal mark on white, or white mark on Sidewalk Teal.

Alternative concept: the original "bolt broadcasting" (charging bolt with signal arcs) still works as a simplified version, but the interlock/fork visual is more distinctive and tells the product story.

### Logo Color Variants

| Variant | Usage |
|---------|-------|
| Full color (Teal + Amber) | Primary, light backgrounds |
| White (reversed) | Dark backgrounds, hero sections |
| Charcoal (monochrome) | Print, formal documents |
| Teal-only | Simplified contexts, embroidery |

### Clear Space

Minimum clear space around logo: equal to the height of the "S" in the wordmark on all sides.

---

## 9. Visual Style Notes for Vanessa (Visual Storyteller)

### The Interlock Visual (Key Illustration)

This is the most important visual in the SideCharge brand. It must communicate the core product concept clearly and elegantly:

**The concept**: One circuit, two loads (HVAC + EV), never running simultaneously.

**Recommended visual treatments**:

1. **The Toggle/Split Diagram**: A single power line enters from the left. At a central node (the SideCharge device), it splits into two paths -- one going up to an HVAC compressor icon, one going down to an EV/plug icon. At any given moment, one path is lit (Electric Amber, active) and the other is dimmed (Slate, waiting). Animate the toggle: HVAC active during a sun icon, EV active during a moon icon. This is the hero diagram for the website, the installer sell sheet, and the pitch deck.

2. **The 24-Hour Timeline**: A circular clock or horizontal timeline showing a full day. HVAC usage blocks (amber) cluster in afternoon hours. EV charging blocks (teal) cluster in overnight hours. The visual makes it obvious: these loads naturally don't compete. A small SideCharge icon sits at the boundary, managing the handoff.

3. **The Before/After Panel**: Side-by-side illustration. Left ("Before"): an electrical panel with a red X and a price tag showing "$2,400+ panel upgrade." Right ("After"): the same panel, unchanged, but now with a small SideCharge device on the HVAC circuit and an EV charger plugged in. Green checkmark. "$1,000 total." Same-day.

4. **The Interlock Logic**: A simple, clean state diagram showing the four states:
   - HVAC off + EV off = both available
   - HVAC on + EV off = HVAC has priority (thermostat calling)
   - HVAC off + EV on = EV charging
   - HVAC on + EV on = never happens (SideCharge prevents this)

   Use teal for available states, amber for active loads, and a gentle violet glow on the "never happens" state with the SideCharge logo as the guardian.

### Photography / Illustration Direction

- **Setting**: Residential, not commercial. Driveways, garages, suburban streets at golden hour. Real homes, not corporate parking structures. Electrical panels (clean, well-labeled) are now part of the visual vocabulary -- show the installer's perspective.
- **Mood**: Relief and confidence. The homeowner who was told they needed a panel upgrade, now watching their car charge the same day. The electrician closing the panel with a satisfied nod.
- **People**: Three contexts:
  - **Homeowners**: Relaxed -- checking a phone notification from SideCharge while cooking dinner, not standing next to their car watching it charge.
  - **Electricians**: Professional, competent -- installing the SideCharge device at a panel, handing keys back to the homeowner, showing the app on a tablet.
  - **The handoff moment**: Electrician and homeowner together, car plugged in, both looking at the dashboard showing "Charging: 7.2 kW." The money shot.
- **Technical diagrams**: When showing the LoRa mesh or architecture, use the Signal Violet accent for data flow lines over a Charcoal background. Make the invisible network visible and beautiful. When showing the interlock circuit, use the Toggle/Split Diagram style above.

### Iconography Style

- Rounded line icons, 2px stroke weight, Sidewalk Teal default color
- Filled variants in Electric Amber for active/highlighted states
- Consistent 24px grid, rounded end caps
- Key icons needed: **interlock/toggle switch** (new, primary), charging bolt, signal waves, thermometer/HVAC, home, electrical panel, clock, shield (code compliance), refresh (OTA), dollar sign (savings)

### Data Visualization

- Primary chart color: Sidewalk Teal
- Secondary: Signal Violet
- Highlight/alert: Electric Amber
- Background grid: Cloud (#F4F5F7)
- Use JetBrains Mono for axis labels and data callouts
- Animate data transitions with ease-out curves (300ms)
- **Interlock timeline chart**: Show HVAC and EV load on the same time axis, with SideCharge ensuring they never overlap. This is a key dashboard visualization.

### Animation / Motion

- Micro-animations: 200-300ms, ease-out
- **Interlock toggle**: The signature animation. One load fades out (ease-out, 300ms) as the other fades in (ease-in, 300ms) with a brief 100ms overlap where neither is fully active -- representing the safe switchover. Use this in hero sections and product demos.
- Charging pulse: a gentle glow animation on the bolt icon when actively charging (Amber pulse)
- Signal ripple: concentric arcs emanating outward when showing connectivity (Teal, fading to Mist Teal)
- Page transitions: subtle slide-up with fade (400ms)

---

## 10. Application Examples

### Mobile Dashboard (Primary Interface)

```
+------------------------------------------+
|  [Teal Nav Bar]                          |
|  SideCharge              [Signal Icon]   |
+------------------------------------------+
|                                          |
|  [Card - Cloud background]               |
|  +--------------------------------------+|
|  |  CIRCUIT STATUS          [Interlock] ||
|  |                                      ||
|  |  HVAC: Idle              [Teal dim]  ||
|  |  Charger: Active         [Amber]     ||
|  |  ================================    ||
|  |  7.2 kW    32A    242V              ||
|  |  [JetBrains Mono, Data Readout]     ||
|  |                                      ||
|  |  Est. complete: 6:42 AM              ||
|  +--------------------------------------+|
|                                          |
|  [Card - Cloud background]               |
|  +--------------------------------------+|
|  |  TODAY'S SHARING          [Timeline] ||
|  |  HVAC: 1.8 hrs  |  EV: 2.1 hrs      ||
|  |  [visual timeline bar]               ||
|  |  No conflicts. Circuit shared clean. ||
|  +--------------------------------------+|
|                                          |
|  [Card - Cloud background]               |
|  +--------------------------------------+|
|  |  SAVINGS                  [Dollar]   ||
|  |  Panel upgrade avoided: $2,400+      ||
|  |  This month energy cost: $48.20      ||
|  +--------------------------------------+|
|                                          |
|  [Card - Cloud background]               |
|  +--------------------------------------+|
|  |  NETWORK STATUS        [Teal icon]   ||
|  |  Sidewalk LoRa: Connected            ||
|  |  Last seen: 12s ago                  ||
|  |  Signal: Strong (3/4 arcs)           ||
|  +--------------------------------------+|
|                                          |
+------------------------------------------+
|  [Bottom Nav: Home | Energy | Settings]  |
+------------------------------------------+
```

### Notification Styles

> **SideCharge**: Your Tesla finished charging at 6:38 AM. 41.2 kWh added. Battery full.

> **SideCharge**: AC turned on. Charging paused at 28.4 kWh. Will resume when the cooling cycle ends.

> **SideCharge**: Cooling cycle complete. Charging resumed at 7.2 kW.

Simple. Warm. Useful. No exclamation marks. The interlock is invisible and reliable.

---

## 11. The Numbers (Reference for All Communications)

| Metric | Value | Source / Context |
|--------|-------|------------------|
| Panel upgrade cost avoided | **$2,400+** | Average cost to upgrade 100A to 200A service |
| Panel upgrade timeline | **3+ days** | Permit + scheduling + inspection |
| SideCharge install cost (target) | **~$1,000** | Equipment + installation |
| Addressable CA homes | **14-26%** | Homes with 100A panels + central air |
| Disadvantaged community % | **17-33%** | Higher % of applicable homes in underserved areas |
| HVAC daily runtime (summer) | **~2 hrs/day** | Mostly daytime (noon-6 PM) |
| EV daily charge time | **~2 hrs/day** | Mostly overnight (10 PM-6 AM) |
| Typical HVAC circuit | **30-50A** | Matches Level 2 charging power for 1,500-2,400 sq ft homes |
| Code basis | **CEC 440.34** | Allows HVAC circuit sharing with interlock |
| SCE panel upgrade incentive | **$4,200/home** | Current grant in disadvantaged communities |
| Connectivity cost | **$0/month** | Amazon Sidewalk LoRa -- free, no subscription |
| App binary size | **4 KB** | Split-image architecture |
| Delta OTA update time | **~15 seconds** | Over LoRa, verified end-to-end |
| Sidewalk US coverage | **90%+** | Amazon Sidewalk network population coverage |

---

## 12. Brand Do's and Don'ts

### Do

- Lead with the interlock -- the panel upgrade is the problem, the circuit interlock is the answer
- Follow the interlock with "and it connects over radio, no WiFi"
- Use real numbers ($2,400, 4KB, 15 seconds, 14-26%) -- specificity builds trust
- Show the product in residential contexts, including electrical panels
- Speak differently to different audiences: homeowners hear "no panel upgrade," electricians hear "CEC 440.34 compliant, same-day install," utilities hear "adds chargers without peak demand increase"
- Celebrate the engineering elegance (tiny footprint, delta OTA, circuit sharing)
- Use Signal Violet as a "wink" -- the unexpected detail that makes people smile
- Maintain generous white space -- let the content breathe
- Show the interlock toggle animation whenever demonstrating the product

### Don't

- Don't describe SideCharge as a "charger monitor" -- it is a circuit interlock that enables Level 2 charging
- Don't compare directly to ChargePoint in marketing (different market, different scale)
- Don't use "IoT" in consumer-facing copy (say "connected" or "smart")
- Don't make the LoRa network the hero -- it's the invisible enabler; the *interlock* is the hero, and the *charging access* is the benefit
- Don't use gradients on buttons or interactive elements
- Don't use more than 2 brand colors in a single card/component
- Don't stack more than 3 messaging pillars on a single page/screen
- Don't bury the interlock story below connectivity or monitoring features -- it always comes first

---

## Appendix A: CSS Custom Properties

```css
:root {
  /* Primary */
  --sc-teal: #0A7E8C;
  --sc-teal-dark: #064E56;
  --sc-teal-light: #B8E4E9;

  /* Accent */
  --sc-amber: #F5A623;
  --sc-violet: #7B61FF;
  --sc-green: #2ECC71;

  /* Neutral */
  --sc-charcoal: #1A1A2E;
  --sc-slate: #6B7280;
  --sc-cloud: #F4F5F7;
  --sc-white: #FFFFFF;

  /* Typography */
  --font-display: 'Space Grotesk', system-ui, sans-serif;
  --font-body: 'Inter', system-ui, sans-serif;
  --font-mono: 'JetBrains Mono', monospace;

  /* Spacing */
  --space-xs: 4px;
  --space-sm: 8px;
  --space-md: 16px;
  --space-lg: 24px;
  --space-xl: 32px;
  --space-2xl: 48px;
  --space-3xl: 64px;

  /* Radius */
  --radius-sm: 6px;
  --radius-md: 12px;
  --radius-lg: 20px;
  --radius-full: 9999px;

  /* Shadows */
  --shadow-card: 0 1px 3px rgba(26, 26, 46, 0.08), 0 4px 12px rgba(26, 26, 46, 0.04);
  --shadow-elevated: 0 4px 12px rgba(26, 26, 46, 0.12), 0 12px 32px rgba(26, 26, 46, 0.08);
}
```

## Appendix B: Google Fonts Embed

```html
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600&family=JetBrains+Mono:wght@400;500&family=Space+Grotesk:wght@500;700&display=swap" rel="stylesheet">
```

---

*Brand guidelines v2.0 -- Updated 2026-02-12*
*Major revision: Reframed around circuit interlock as core value proposition*
*Prepared for handoff to Vanessa (Visual Storyteller)*
*Research baseline: ChargePoint.com brand analysis + founder product brief*
*Company: Eta Works -- CEO Eric Smith*
