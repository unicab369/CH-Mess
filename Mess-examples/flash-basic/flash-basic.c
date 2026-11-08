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
    status = flash_log_read(&log, DEMO_LOGICAL_ID, &value, sizeof(value), &length);

    // Initialize the reserved log sectors if they contain no usable records.
    if (status == FLASH_NOT_FOUND && log.next_address == FLASH_LOG_END) {
        printf("No usable log found; erasing reserved CodeFlash sectors\n");
        for (uint32_t address = FLASH_BASE_ADDR; address < FLASH_LOG_END;
             address += FLASH_BLOCK_SIZE) {
            if (HW_ERASE_BLOCK(address)) {
                printf("CodeFlash erase failed at 0x%08lx\n", (unsigned long)address);
                return;
            }
        }

        // Confirm the erase before rescanning; a successful command alone does
        // not prove the reserved CodeFlash bytes are blank.
        flash_record erased_check;
        if (HW_FLASH_READ(FLASH_BASE_ADDR, &erased_check, sizeof(erased_check))) {
            printf("CodeFlash erase readback failed\n");
            return;
        }
        const uint8_t *bytes = (const uint8_t *)&erased_check;
        bool erased = true;
        for (size_t i = 0; i < sizeof(erased_check); ++i) {
            if (bytes[i] != (uint8_t)(FLASH_ERASED_WORD >> ((i & 0b11u) * 8u))) {
                erased = false;
                break;
            }
        }
        if (!erased) {
            printf("CodeFlash erase pattern mismatch at 0x%08lx; first bytes:",
                   (unsigned long)FLASH_BASE_ADDR);
            for (size_t i = 0; i < sizeof(erased_check); ++i)
                printf(" %02x", bytes[i]);
            printf("\n");
            return;
        }

        status = flash_log_init(&log);
        if (status != FLASH_OK) {
            printf("flash_log_init failed after erase: %u\n", status);
            return;
        }
        status = FLASH_NOT_FOUND;
    }

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
                   (unsigned long)log.next_address);
        else
            printf("flash_log_write failed: %u\n", status);
        return;
    }

    uint32_t readback = 0;
    status = flash_log_read(&log, DEMO_LOGICAL_ID, &readback, sizeof(readback), &length);
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
