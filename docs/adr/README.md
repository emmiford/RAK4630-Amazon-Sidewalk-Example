# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records for the RAK Sidewalk EVSE Monitor project.

ADRs record significant architectural decisions — the context, the decision, the consequences, and the alternatives considered. They are immutable once accepted (superseded by new ADRs, not edited).

## Format

Each ADR follows this template:

```
# ADR-NNN: Title

## Status
Accepted | Superseded by ADR-XXX | Deprecated

## Context
What is the issue that we're seeing that is motivating this decision?

## Decision
What is the change that we're proposing and/or doing?

## Consequences
What becomes easier or more difficult to do because of this change?

## Alternatives Considered
What other approaches were evaluated?
```

## Index

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| [001](001-version-mismatch-hard-stop.md) | API version mismatch is a hard stop | Accepted | 2026-02-11 |
| [002](002-time-sync-second-resolution.md) | Time sync uses second resolution in a 4-byte field | Accepted | 2026-02-16 |
| [003](003-charge-now-cancels-demand-response.md) | Charge Now cancels demand response window | Accepted | 2026-02-16 |
| [004](004-event-buffer-state-changes.md) | Event buffer records state changes, not poll cycles | Accepted | 2026-02-16 |
| [005](005-simulation-stays-in-evse-sensors.md) | Simulation mode stays in evse_sensors.c | Accepted | 2026-02-21 |
| [006](006-table-architecture.md) | DynamoDB table architecture — unified events, SC-ID PK, Mountain Time SK | Accepted | 2026-02-21 |
