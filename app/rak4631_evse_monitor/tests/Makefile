# Host-side unit tests for EVSE app modules
#
# Usage: make test
#
# Compiles all app sources against a mock platform_api and runs on the host.
# This is the Grenning dual-target pattern — same source, different HAL.

CC = cc
CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c11 -g -O0 -DHOST_TEST

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
    $(APP_SRC)/time_sync.c \
    $(APP_SRC)/event_buffer.c

OUT = test_app

.PHONY: test clean

test: $(OUT)
	./$(OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

clean:
	rm -f $(OUT)
