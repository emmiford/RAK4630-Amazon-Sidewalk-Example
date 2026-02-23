# PRD §2.0.3a Pin Name Cross-Reference — DRAFT

**Status**: COMMITTED — inserted into PRD on branch `task/109-prd-pin-mapping` (commit f6d3215).
**Location**: Inserted after the "AIN1 serves double duty..." paragraph (line 229), before §2.0.3.1.
**Author**: Pam (2026-02-22)

---

#### 2.0.3a Pin Name Cross-Reference (Definitive Mapping)

The nRF52840 chip, the RAK4631 module, the WisBlock connector standard, and each baseboard use **different names for the same physical pins**. This table is the single source of truth. All other documents should reference this table, not invent pin names.

> **Critical naming hazard**: The nRF52840 SAADC names (AIN0-AIN7) do NOT match the WisBlock connector labels (AIN0, AIN1). For example, WisBlock "AIN1" is nRF52840 AIN**7** (P0.31), not AIN1 (P0.03). This mismatch caused a firmware bug that went undetected for 18 days (see experiment log EXP-009b). **Always use the WisBlock silkscreen label as the primary identifier in user-facing docs and the nRF52840 pin name (P0.xx) in firmware.**

**Analog pins**

| WisBlock Label | Connector Pin | nRF52840 GPIO | nRF52840 SAADC | RAK19007 (J11) | RAK19001 (J10/J15) | Firmware `zephyr,input-positive` | SideCharge Signal |
|---------------|:------------:|--------------|:-------------:|:--------------:|:------------------:|--------------------------------|-------------------|
| AIN0 | 21 (even) | P0.05 | AIN3 | — (not on J11) | AIN0 | `NRF_SAADC_AIN3` | (unused) |
| **AIN1** | **22 (even)** | **P0.31** | **AIN7** | **A1 (J11 pin 1)** | **AIN1** | **`NRF_SAADC_AIN7`** | **J1772 pilot voltage** |

**Digital IO pins**

| WisBlock Label | Connector Pin | nRF52840 GPIO | RAK19007 (J11) | RAK19001 (J10/J15) | SideCharge Signal | Direction |
|---------------|:------------:|--------------|:--------------:|:------------------:|-------------------|-----------|
| **IO1** | **29 (odd)** | **P0.17** | **IO1 (J11 pin 2)** | **IO1** | **charge_block** | Output |
| **IO2** | **30 (even)** | **P1.02** | **IO2 (J11 pin 3)** | **IO2** | **cool_call** | Input |
| IO3 | 31 (odd) | P0.21 | — | IO3 | (available) | — |
| IO4 | 32 (even) | P0.04 | — | IO4 | (AIN2 fallback for pilot if P0.31 dead) | ADC or GPIO |
| IO5 | 37 (odd) | P0.09 | — | IO5 | (available; shares NFC pin, needs `CONFIG_NFCT_PINS_AS_GPIOS=y`) | — |
| IO6 | 38 (even) | P0.10 | — | IO6 | (available; shares NFC pin, needs `CONFIG_NFCT_PINS_AS_GPIOS=y`) | — |
| IO7 | 24 (even) | NC | — | IO7 | Not connected on RAK4631 | — |

**Power and system pins (on 40-pin connector, not directly user-accessible)**

| WisBlock Label | Connector Pin(s) | Voltage | Notes |
|---------------|:----------------:|---------|-------|
| VBAT | 1, 2 | 3.7-4.2V | Battery supply |
| GND | 3, 4, 39, 40 | 0V | Ground (4 pins) |
| 3V3 | 5, 6 | 3.3V | Regulated supply |
| USB+/USB- | 7, 8 | — | USB data lines |
| VBUS | 9 | 5V | USB power, only when USB connected |
| VDD | 17, 18 | ~3.0V | Sensor board power from MCU DCDC |

**Connector geometry note**: Odd-numbered pins and even-numbered pins sit on opposite physical rows of the Hirose DF40C 40-pin board-to-board connector. EXP-010 demonstrated that incomplete module seating can leave one entire row (even pins) disconnected while the module appears physically attached. The RAK19007 J11 header uses a standard 2.54mm through-hole header and does not have this failure mode.

**RAK19007 J11 header pinout** (only 3 signal pins + power):

```
J11 Pin  WisBlock   nRF52840    Signal
───────  ─────────  ──────────  ──────────
1        AIN1       P0.31       Pilot ADC
2        IO1        P0.17       charge_block
3        IO2        P1.02       cool_call
4        VDD        —           3.0V sensor power
5        GND        —           Ground
6        3V3        —           3.3V supply
```

---

**TODO**: Verify RAK19001 J10/J15 silkscreen labels against the physical board. The labels above assume they match the WisBlock standard names (IO1-IO6, AIN0, AIN1) but this should be confirmed.
