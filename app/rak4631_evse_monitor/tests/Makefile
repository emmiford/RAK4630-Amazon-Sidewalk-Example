# Host-side unit tests for EVSE app modules
#
# Usage: make test
#
# Compiles all app sources against a mock platform_api and runs on the host.
# This is the Grenning dual-target pattern — same source, different HAL.

CC = cc
CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c11 -g -O0 -DHOST_TEST -DHEARTBEAT_INTERVAL_MS=60000

# Include paths
INC = -I../include -I.

# App source directory
APP_SRC = ../src/app_evse

# Sources to compile (app modules + mock + test)
SRCS = \
    test_app.c \
    mock_platform.c \
    $(APP_SRC)/app_entry.c \
    $(APP_SRC)/evse_sensors.c \
    $(APP_SRC)/charge_control.c \
    $(APP_SRC)/thermostat_inputs.c \
    $(APP_SRC)/evse_payload.c \
    $(APP_SRC)/app_tx.c \
    $(APP_SRC)/app_rx.c \
    $(APP_SRC)/selftest.c \
    $(APP_SRC)/selftest_trigger.c \
    $(APP_SRC)/charge_now.c \
    $(APP_SRC)/time_sync.c \
    $(APP_SRC)/event_buffer.c \
    $(APP_SRC)/event_filter.c \
    $(APP_SRC)/delay_window.c \
    $(APP_SRC)/diag_request.c \
    $(APP_SRC)/led_engine.c \
    $(APP_SRC)/cmd_auth.c

OUT = test_app

# Boot path tests — platform-side code (app.c) with mock Zephyr/Sidewalk
BOOT_INC = -Imock_include -I../include -I.
BOOT_SRCS = \
    test_boot_path.c \
    mock_boot.c \
    ../src/app.c

BOOT_OUT = test_boot_path

.PHONY: test clean

test: $(OUT) $(BOOT_OUT)
	./$(OUT)
	./$(BOOT_OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BOOT_OUT): $(BOOT_SRCS)
	$(CC) $(CFLAGS) -Wno-unused-function $(BOOT_INC) -o $@ $^

clean:
	rm -f $(OUT) $(BOOT_OUT)
