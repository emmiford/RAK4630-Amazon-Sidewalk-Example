# TASK-010: Set up GitHub Actions for Lambda tests + linting

**Status**: MERGED DONE (2026-02-11, Oliver + Eero)
**Branch**: `feature/testing-pyramid`

## Summary
CI runs pytest and ruff linting. Config in `pyproject.toml` (line-length 100, E/W/F/I rules). Auto-fixed 30 violations (unused f-prefixes, unused imports).

## Deliverables
- Updated `.github/workflows/ci.yml`
- `pyproject.toml` (ruff config)
- `aws/requirements-test.txt`
