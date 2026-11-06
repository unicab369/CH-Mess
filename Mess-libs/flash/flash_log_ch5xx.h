#ifndef FUN_FLASH_LOG_CH5XX_H
#define FUN_FLASH_LOG_CH5XX_H

// Set FLASH_LOG_SIZE, FLASH_BLOCK_SIZE, and any other options before including this port.
// The linker provides the log address from the selected chip's FLASH region.
#include "ch32fun.h"
#include "ch5xx_flash.h"
#include <stdint.h>

#ifndef FLASH_LOG_SIZE
#error "FLASH_LOG_SIZE must be configured before including the CH5xx flash-log port"
#endif

#ifndef FLASH_BLOCK_SIZE
#error "FLASH_BLOCK_SIZE must be configured before including the CH5xx flash-log port"
#endif

#if FLASH_LOG_SIZE % FLASH_BLOCK_SIZE != 0
#error "FLASH_LOG_SIZE must be a multiple of FLASH_BLOCK_SIZE"
#endif

#define FLASH_BLOCK_COUNT (FLASH_LOG_SIZE / FLASH_BLOCK_SIZE)

// The linker derives this symbol from the selected chip's FLASH memory region.
extern uint8_t __flash_log_start[];
#define FLASH_BASE_ADDR ((uint32_t)(uintptr_t)__flash_log_start)
#define FLASH_BASE_ADDR_RUNTIME

#ifndef FLASH_ERASED_WORD
// CodeFlash read pattern reported for CH5xx; override if the target differs:
// https://www.cnblogs.com/risc5-ble/p/18871865
#define FLASH_ERASED_WORD 0xF3F9BDA9u
#endif

#include "flash_log.h"

// Read any byte range using the CH5xx's memory-mapped 32-bit CodeFlash reads.
__HIGH_CODE int HW_FLASH_READ(uint32_t address, void *dest, size_t length) {
    // Keep the request inside the reserved log region and require a destination.
    if (address < FLASH_BASE_ADDR || address > FLASH_LOG_END ||
        length > FLASH_LOG_END - address || (length && !dest)
    ) return -1;

    // Copy whole words directly when the flash address, output, and length are
    // all word aligned. memcpy keeps writes to arbitrary output objects valid.
    // Multiples of 4 have both low bits clear; OR combines the three low-bit
    // checks, and & 3u is zero only when all three values meet that condition.
    if (((address | (uintptr_t)dest | length) & 3u) == 0) {
        uint8_t *aligned_dest = __builtin_assume_aligned(dest, 4);

        for (size_t i = 0; i < length; i += sizeof(uint32_t)) {
            const uint32_t word = *(const uint32_t *)(uintptr_t)(address + i);
            __builtin_memcpy(aligned_dest + i, &word, sizeof(word));
        }
        return 0;
    }

    // Round the flash address down to a word boundary. If the requested address
    // is between words, word_offset skips the unwanted leading bytes.
    uint8_t *destination = (uint8_t *)dest;
    uint32_t word_address = address & ~3u;
    size_t word_offset = address & 3u;

    while (length) {
        // Fetch one aligned word. Copy only the requested part of that word so
        // short or unaligned reads do not overrun the caller's byte buffer.
        const uint32_t word = *(const uint32_t *)(uintptr_t)word_address;
        // At most 4 - word_offset bytes remain in this word, and length may be
        // smaller. For offset 2 and length 3, copy 2 bytes now and 1 next time.
        size_t count = sizeof(word) - word_offset;
        if (count > length)
            count = length;

        // CH5xx is little-endian: byte n occupies bits 8*n of the word.
        for (size_t i = 0; i < count; ++i)
            destination[i] = (uint8_t)(word >> ((word_offset + i) * 8u));

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
        length > FLASH_LOG_END - address || (length && !src) || (address & 3u)
    ) return -1;

    if (!length) return 0;

    // The CH5xx writer reads full words from an aligned RAM buffer. The caller's
    // source may be unaligned or end partway through a word, so stage it here.
    const uint8_t *source = (const uint8_t *)src;
    uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

    while (length) {
        // Stage at most the bytes left in this CodeFlash page. FLASH_PAGE_SIZE
        // must match the target's programming page size.
        size_t page_remaining = FLASH_PAGE_SIZE - (address & (FLASH_PAGE_SIZE - 1u));
        size_t chunk = length < page_remaining ? length : page_remaining;

        // Round up to whole words and fill any unused final bytes with 0xFF.
        // For example, 5 source bytes become 8 staged bytes (two words).
        size_t program_length = (chunk + 3u) & ~3u;
        for (size_t i = 0; i < program_length; ++i)
            page[i] = i < chunk ? source[i] : 0xFFu;

        // Program the staged words while the flash controller is open. Staging
        // first also keeps the source readable if it resides in CodeFlash.
        ch5xx_flash_rom_open_erase_write();
        ch5xx_flash_rom_addr(0x02, address);

        for (size_t i = 0; i < program_length; i += sizeof(uint32_t)) {
            uint32_t word;
            __builtin_memcpy(&word, page + i, sizeof(word));
            R32_FLASH_DATA = word;

            // R32_FLASH_DATA holds four bytes. Each 0x15 command programs one
            // byte, so wait for the controller and issue the command four times.
            while ((int8_t)R8_FLASH_CTRL < 0) { }
            R8_FLASH_CTRL = 0x15;
            while ((int8_t)R8_FLASH_CTRL < 0) { }
            R8_FLASH_CTRL = 0x15;
            while ((int8_t)R8_FLASH_CTRL < 0) { }
            R8_FLASH_CTRL = 0x15;
            while ((int8_t)R8_FLASH_CTRL < 0) { }
            R8_FLASH_CTRL = 0x15;
        }
        if (!ch5xx_flash_rom_wait()) {
            ch5xx_flash_rom_close();
            return -1;
        }
        ch5xx_flash_rom_close();

        address += chunk;
        source += chunk;
        length -= chunk;
    }
    return 0;
}

// Erase one 4KB CodeFlash sector used by the log.
__HIGH_CODE uint32_t HW_ERASE_BLOCK(uint32_t address) {
    if (address < FLASH_BASE_ADDR || address >= FLASH_LOG_END ||
        ((address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u))
    ) return (uint32_t)-1;
    return ch5xx_flash_cmd_erase(address, FLASH_BLOCK_SIZE);
}

#endif // FUN_FLASH_LOG_CH5XX_H
