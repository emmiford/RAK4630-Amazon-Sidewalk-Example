# TASK-054: Replace per-module API setters with shared platform pointer

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Currently `app_entry.c` calls 10 separate `*_set_api()` functions to distribute the platform API pointer. Each module stores it in a file-scope static. This is 10 functions that do the same thing.

Replace with a single shared pointer:

```c
/* app_platform.h */
#include "platform_api.h"
extern const struct platform_api *platform;

/* app_platform.c */
const struct platform_api *platform = NULL;

/* app_entry.c: app_init() sets it once */
platform = api;
```

All modules `#include "app_platform.h"` and use `platform->` directly. Delete the 10 `*_set_api()` functions and their 10 static pointers.

Update mock: tests set `platform = mock_api()` once instead of calling 10 setters.

Reference: `docs/technical-design-rak-firmware.md`, Change 5.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] No `*_set_api()` functions remain in app sources
- [ ] No `static const struct platform_api *api` in any app module
- [ ] Single `const struct platform_api *platform` pointer in `app_platform.c`
- [ ] `app_init()` sets it once
- [ ] All 11 app modules use `platform->` for API calls
- [ ] App build succeeds (both standalone and host test)
- [ ] All 57 tests pass (mock updated to set shared pointer)

## Testing Requirements
- [ ] All 57 host-side tests pass
- [ ] App build succeeds
- [ ] Verify mock setup: `platform = mock_api()` in test setUp or mock_reset

## Deliverables
- New: `app_platform.h`, `app_platform.c` (5 lines total)
- Modified: all 11 app modules (remove setter + static, add include)
- Modified: `app_entry.c` (remove 10 setter calls, add 1 assignment)
- Modified: `mock_platform.c` / `test_app.c` (update init pattern)
- Modified: `app_evse/CMakeLists.txt`, `tests/Makefile` (add app_platform.c)
