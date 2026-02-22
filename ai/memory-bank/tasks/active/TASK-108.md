# TASK-108: Dashboard event table missing timestamps

**Status**: not started
**Priority**: P2
**Owner**: Utz
**Branch**: —
**Size**: XS (0.5 points)

## Description
The dashboard frontend event table shows blank timestamps. The JavaScript references `ev.timestamp || ev.time` (line 812 of `index.html`) but the API returns `ev.timestamp_mt`. Simple field name mismatch.

Also: the CORS preflight fix (OPTIONS bypasses auth) was deployed this session — verify it works from the served dashboard.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Event table shows timestamps in `YYYY-MM-DD HH:MM:SS` format
- [ ] Fix `ev.timestamp || ev.time` → `ev.timestamp_mt` in `aws/dashboard/index.html:812`
- [ ] Dashboard loads data when served via `python3 -m http.server`

## Testing Requirements
- [ ] Visual verification: events show timestamps in the table

## Deliverables
- Fix in `aws/dashboard/index.html`
