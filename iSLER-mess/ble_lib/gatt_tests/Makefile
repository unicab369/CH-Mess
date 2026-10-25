CC ?= cc
CFLAGS ?= -std=gnu11 -O0
BUILD_DIR ?= /tmp/ble-gatt-tests

CFLAGS += -ffunction-sections -fdata-sections

ifeq ($(shell uname -s),Darwin)
GC_SECTIONS := -Wl,-dead_strip
else
GC_SECTIONS := -Wl,--gc-sections
endif

.PHONY: test eatt

test: eatt

eatt:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) ble_gatt_eatt_test.c $(GC_SECTIONS) -o $(BUILD_DIR)/ble_gatt_eatt_test
	$(BUILD_DIR)/ble_gatt_eatt_test
