# EXP-006: Raw 8-Byte Payload vs sid_demo Protocol

**Status**: Concluded
**Verdict**: GO
**Type**: Architecture change
**Date**: pre-2026-02
**Owner**: Oliver

---

## Problem Statement

The inherited sid_demo protocol used a complex 3-state state machine with capability discovery, adding ~290 lines of code and protocol overhead that blocked reliable sensor data flow.

## Hypothesis

Replacing the sid_demo protocol with a direct 8-byte raw payload format will improve reliability and reduce code complexity without losing functionality.

**Success Metrics**:
- Primary: code reduction, elimination of protocol-related data flow blockages

## Method

**Variants**:
- Control: sid_demo protocol with SMF state machine, capability discovery, LED/button response handlers (~420 lines)
- Variant: Raw 8-byte format — magic(1) + version(1) + J1772(1) + voltage(2) + current(2) + thermo(1) (~130 lines)

## Results

**Decision**: GO — merged to main (commit `550560f`)
**Primary Metric Impact**: 69% code reduction (420 → 130 lines). Eliminated protocol-related TX blockages.
**Payload Format**: `[0xE5, 0x01, state, volt_lo, volt_hi, curr_lo, curr_hi, thermo_flags]`
**Trade-off**: Lost backward compatibility with sid_demo decoders. Lambda updated to handle new format.

## Key Insights

- The sid_demo protocol was designed for a general-purpose demo, not for a single-purpose sensor. Stripping it was the right call.
- Magic byte (0xE5) + version byte (0x01) gives forward compatibility for payload evolution without protocol overhead.
- 8 bytes fits easily in the 19B MTU with room for future fields.
