# Makefile -- Build and run host-based unit tests for ECU_diagnostic
#
# Run from the repository root:
#   make -f tests/Makefile test
#
# Or from inside tests/:
#   make test

REPO     := $(if $(MAKELEVEL),$(dir $(lastword $(MAKEFILE_LIST)))..,$(CURDIR)/..)
TESTDIR  := $(if $(MAKELEVEL),$(dir $(lastword $(MAKEFILE_LIST))),$(CURDIR))
BUILDDIR := $(TESTDIR)/build

CC     := gcc
CFLAGS := -Wall -Wextra -std=c11 -g \
           -I $(TESTDIR)/stubs \
           -I $(REPO)/Core/Inc \
           -DHOST_TEST_BUILD

# Stub object files shared by all test binaries
STUBS := $(TESTDIR)/stubs/hal_stubs.c \
         $(TESTDIR)/stubs/freertos_stubs.c

# Production source files
SRC_CAN   := $(REPO)/Core/Src/can_diagnostic.c
SRC_UART  := $(REPO)/Core/Src/uart_bridge.c
SRC_RTOS  := $(REPO)/Core/Src/rtos_monitor.c

# Test binaries
BIN_CAN   := $(BUILDDIR)/test_can_diagnostic
BIN_UART  := $(BUILDDIR)/test_uart_bridge
BIN_RTOS  := $(BUILDDIR)/test_rtos_monitor

.PHONY: all test clean

all: $(BIN_CAN) $(BIN_UART) $(BIN_RTOS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BIN_CAN): $(STUBS) $(SRC_CAN) $(TESTDIR)/test_can_diagnostic.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_UART): $(STUBS) $(SRC_CAN) $(SRC_UART) $(SRC_RTOS) $(TESTDIR)/test_uart_bridge.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_RTOS): $(STUBS) $(SRC_RTOS) $(TESTDIR)/test_rtos_monitor.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

test: all
	@echo ""
	$(BIN_CAN)
	@echo ""
	$(BIN_UART)
	@echo ""
	$(BIN_RTOS)
	@echo ""
	@echo "All test suites completed."

clean:
	rm -rf $(BUILDDIR)
