#include <stdint.h>

#include "../../Mess-libs/flash/flash_log_ch5xx.h"

#include <stdio.h>

#define DEMO_LOGICAL_ID 1u

static flash_log log;

// Append, read back, and print a persistent counter stored under one logical ID.
static __HIGH_CODE void run_flash_demo(void) {
    flash_status status = flash_log_init(&log);
    if (status != FLASH_OK) {
        printf("flash_log_init failed: %u\n", status);
        return;
    }

    uint32_t value = 0;
    uint16_t length = 0;
    status = flash_log_read(&log, DEMO_LOGICAL_ID, FLASH_RECORD_MARKER_4,
                            &value, sizeof(value), &length);

    if (status == FLASH_OK && length != sizeof(value)) {
        printf("stored value has unexpected size\n");
        return;
    }
    if (status != FLASH_OK && status != FLASH_NOT_FOUND) {
        printf("flash_log_read failed: %u\n", status);
        return;
    }

    ++value;
    status = flash_log_write(&log, DEMO_LOGICAL_ID, &value, FLASH_RECORD_MARKER_4);
    if (status != FLASH_OK) {
        if (status == FLASH_FULL)
            printf("Flash log is full; next address: 0x%08lx\n",
                   (unsigned long)log.pools[FLASH_LOG_POOL_4].next_address);
        else
            printf("flash_log_write failed: %u\n", status);
        return;
    }

    uint32_t readback = 0;
    status = flash_log_read(&log, DEMO_LOGICAL_ID, FLASH_RECORD_MARKER_4,
                            &readback, sizeof(readback), &length);
    if (status == FLASH_OK)
        printf("Flash counter: %lu\n", (unsigned long)readback);
    else
        printf("readback failed: %u\n", status);
}

int main(void) {
    SystemInit();
    run_flash_demo();

    while (1) { }
}
