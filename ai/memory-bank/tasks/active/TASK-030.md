# TASK-030: Fleet command safety — design investigation for coordinated load-switching mitigation

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Size**: M (3 points)

## Description
PRD 6.3.2 identifies fleet-wide coordinated load switching as a high-severity threat: a compromised cloud could send simultaneous commands to all devices, creating dangerous demand spikes on the grid.

The original design (staggered Lambda delays + device-side rate limiting + CloudWatch anomaly detection) was rejected because **cloud-side mitigations don't survive a cloud compromise** — the attacker can simply bypass or disable them. Any effective design must include protections that hold even when the cloud is fully compromised.

This task is a **design investigation**, not an implementation task. The deliverable is a recommended approach (likely an ADR) that addresses the threat model honestly.

## Open Questions (PDL-OPEN-005)
1. What protections must live on the device to survive cloud compromise?
2. Should downlink commands carry cryptographic signatures (separate from OTA signing)? If so, where does the signing key live?
3. What device-side behavioral limits are appropriate (max state changes per hour, cooldown periods)?
4. Can out-of-band monitoring (separate AWS account, third-party) detect a compromised cloud sending malicious commands?
5. How does this interact with TASK-032 (cloud command authentication)?

## Dependencies
**Blocked by**: none
**Blocks**: implementation tasks TBD after design is accepted
**Related**: TASK-032 (cloud command authentication) — may be absorbed into or informed by this design

## Acceptance Criteria
- [ ] Threat model analysis: document which mitigations survive which compromise scenarios
- [ ] Recommended architecture (ADR draft) with rationale
- [ ] Identify implementation tasks that follow from the chosen design
- [ ] Review with stakeholders before committing to implementation

## Deliverables
- `docs/adr/ADR-NNN-fleet-command-safety.md` (draft for review)
- Updated PRD 6.3.2 referencing the accepted design (after ADR is accepted)
