# TASK-009: Set up GitHub Actions for host-side unit tests

**Status**: MERGED DONE (2026-02-11, Oliver + Eero)
**Branch**: `feature/testing-pyramid`

## Summary
CI at `.github/workflows/ci.yml` runs both test suites: CMake/ctest (Unity tests) and Grenning Makefile tests. Also includes cppcheck static analysis and Python pytest. All 4 CI jobs passing.

## Deliverables
- `.github/workflows/ci.yml`
