CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -std=gnu11 -O0
BUILD_DIR ?= /tmp/ble-gap-tests

OPENSSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags openssl)
OPENSSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl)

.PHONY: test central

test: central
	$(BUILD_DIR)/ble_gap_central_test

central:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) \
		-DGAP_CONNECTION_COUNT=2 \
		-DGAP_SECURITY_SUPPORT=2 -DGAP_PRIVACY_SUPPORT=1 \
		-DGAP_IDENTITY_COUNT=4 -DGAP_PERIPHERAL_SUPPORT=1 \
		ble_gap_central_test.c ../../micro-ecc/uECC.c \
		$(OPENSSL_LIBS) -o $(BUILD_DIR)/ble_gap_central_test
