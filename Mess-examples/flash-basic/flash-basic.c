#include "ch32fun.h"
#include "ch5xx_flash.h"

#define FLASH_BASE_ADDR    (2u * FLASH_BLOCK_SIZE)
#define FLASH_BLOCK_COUNT  2
#define FLASH_CACHE_LENGTH 4

// Erased CodeFlash read pattern reported in the CH5xx FAQ; it may vary by chip:
// https://www.cnblogs.com/risc5-ble/p/18871865
#define FLASH_ERASED_WORD  0xF3F9BDA9u

#include "../../Mess-libs/flash/flash_log.h"

#include <stdio.h>

#define DEMO_LOGICAL_ID 1u

static flash_log log;

// Read any byte range using the CH5xx's memory-mapped 32-bit CodeFlash reads.
__HIGH_CODE int HW_FLASH_READ(uint32_t address, void *dst, size_t length) {
    // Keep the request inside the reserved log region and require a destination.
    if (address < FLASH_BASE_ADDR || address > FLASH_LOG_END ||
        length > FLASH_LOG_END - address || (length && !dst))
        return -1;

    // Round the flash address down to a word boundary. If the requested address
    // is between words, word_offset skips the unwanted leading bytes.
    uint8_t *destination = (uint8_t *)dst;
    uint32_t word_address = address & ~3u;
    size_t word_offset = address & 3u;

    while (length) {
        // Fetch one aligned word. Copy only the requested part of that word so
        // short or unaligned reads do not overrun the caller's byte buffer.
        const uint32_t word = *(const uint32_t *)(uintptr_t)word_address;
        // At most 4 - word_offset bytes remain in this word, and length may be
        // smaller. For offset 2 and length 3, copy 2 bytes now and 1 next time.
        size_t count = sizeof(word) - word_offset;
        if (count > length) count = length;
        for (size_t i = 0; i < count; ++i)
            destination[i] = ((const uint8_t *)&word)[word_offset + i];
        // Advance both buffers by the bytes copied. Later words start at byte 0.
        destination += count;
        word_address += sizeof(word);
        word_offset = 0;
        length -= count;
    }
    return 0;
}

// Program arbitrary byte data through the CH5xx's 32-bit flash writer.
__HIGH_CODE int HW_FLASH_WRITE(uint32_t address, const void *src, size_t length) {
    // Require an aligned start and keep writes inside the reserved log region.
    if (address < FLASH_BASE_ADDR || address > FLASH_LOG_END ||
        length > FLASH_LOG_END - address || (length && !src) || (address & 3u))
        return -1;
    if (!length) return 0;

    // The CH5xx writer reads full words from an aligned RAM buffer. The caller's
    // source may be unaligned or end partway through a word, so stage it here.
    const uint8_t *source = (const uint8_t *)src;
    uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

    while (length) {
        // Stage at most the bytes left in this flash page; the outer loop handles
        // longer requests one page at a time.
        size_t page_remaining = FLASH_PAGE_SIZE - (address & (FLASH_PAGE_SIZE - 1u));
        size_t chunk = length < page_remaining ? length : page_remaining;
        // Round up to whole words and fill any unused final bytes with 0xFF.
        // For example, 5 source bytes become 8 staged bytes (two words).
        size_t program_length = (chunk + 3u) & ~3u;
        for (size_t i = 0; i < program_length; ++i)
            page[i] = i < chunk ? source[i] : 0xFFu;

        // The loop above only copies into RAM; this call actually programs flash.
        if (ch5xx_flash_cmd_write(address, page, program_length))
            return -1;

        address += chunk;
        source += chunk;
        length -= chunk;
    }
    return 0;
}

// Erase one 4KB CodeFlash sector used by the log.
__HIGH_CODE int HW_ERASE_BLOCK(uint32_t address) {
    if (address < FLASH_BASE_ADDR || address >= FLASH_LOG_END ||
        (address - FLASH_BASE_ADDR) % FLASH_BLOCK_SIZE
    ) return -1;
    return ch5xx_flash_cmd_erase(address, FLASH_BLOCK_SIZE) ? -1 : 0;
}

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
        if (!flash_record_is_erased(&erased_check)) {
            printf("CodeFlash erase pattern mismatch at 0x%08lx; first bytes:",
                   (unsigned long)FLASH_BASE_ADDR);
            const uint8_t *bytes = (const uint8_t *)&erased_check;
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
    status = flash_log_write(&log, DEMO_LOGICAL_ID, &value, sizeof(value));
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
