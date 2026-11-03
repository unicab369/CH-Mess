#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Flash layout: erase blocks contain pages; pages hold bytes from the record log.
// Record header: [sequence | crc32 | logical_id | magic | length]
//
// [erase block - 4KB (configurable)]
//   [page: [header] [payload] [alignment padding] ...]
//   [page: [header] [payload] [alignment padding] ...]
//   [page: ...record bytes continue...]

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR    0x08010000u
#endif

#ifndef FLASH_BLOCK_COUNT
#define FLASH_BLOCK_COUNT  8
#endif

#ifndef FLASH_BLOCK_SIZE
#define FLASH_BLOCK_SIZE   4096
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE    256
#endif

#ifndef FLASH_ALIGNMENT
#define FLASH_ALIGNMENT    4
#endif

#define FLASH_CRC32_POLYNOMIAL 0xEDB88320u

#define FLASH_RECORD_MAGIC 0xFACE // 16-bit record marke


// Tracks erase usage and the next write position for one flash block.
typedef struct {
    // Range: 0–65,535 erases; rebase all counts before incrementing at max.
    uint16_t erase_count;  // Number of times this block has been erased.
    uint16_t write_offset; // Offset of the next writable byte in the block.
    bool     erased;       // True if the block is erased and ready to use.
} flash_block;

// | Record | Key_id | Sequence | Meaning
// |--------|--------|----------|--------------------------
// | 1      | 1      | 1        | set X to 40
// | 2      | 2      | 2        | set Y to 8
// | 3      | 1      | 3        | update X 60

// Header stored before each value in the append-only flash log.
typedef struct {
    uint32_t sequence;      // Increasing value used to find the newest record.
    uint32_t crc32;         // Checksum used to detect incomplete or corrupt data.
    uint16_t logical_id;    // Logical ID of the value stored in this record.
    uint16_t magic;         // Constant used to identify a valid record.
    uint16_t length;        // Number of payload bytes following this header.
} flash_record;

// Holds the log configuration and current append position in RAM.
typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t next_sequence; // Sequence number to assign to the next record.
} flash_log;

// Hardware interface; implement these functions for the target flash device.
int HW_READ(uint32_t address, void *dst, size_t length);
int HW_PROGRAM(uint32_t address, const void *src, size_t length);
int HW_ERASE_BLOCK(uint32_t address);

// Round a byte count up to the hardware's required program alignment.
static inline uint32_t flash_align_up(uint32_t size, uint32_t alignment) {
    if (alignment == 0) return size;
    return ((size + alignment - 1u) / alignment) * alignment;
}

// Check that a program operation stays within a physical page, when required.
static inline bool flash_fits_page(
    uint32_t address, uint32_t length, uint32_t page_size
) {
    if (page_size == 0 || length == 0) return true;
    return (address / page_size) == ((address + length - 1u) / page_size);
}

// Update an in-progress CRC with a chunk of bytes.
static inline uint32_t flash_crc32_update(
    uint32_t crc, const uint8_t *bytes, size_t length
) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (FLASH_CRC32_POLYNOMIAL & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

// Calculate a record payload CRC in small RAM chunks.
static inline int flash_crc32_at(uint32_t address, uint16_t length, uint32_t *result) {
    uint8_t chunk[32];
    uint32_t crc = 0xFFFFFFFFu;

    if (!result) return -1;
    while (length) {
        const size_t count = length < sizeof(chunk) ? length : sizeof(chunk);
        if (HW_READ(address, chunk, count)) return -1;

        crc = flash_crc32_update(crc, chunk, count);
        address += count;
        length -= (uint16_t)count;
    }
    *result = ~crc;
    return 0;
}

// Scan the reserved region to find the next unused record slot.
static inline int flash_log_init(flash_log *log) {
    if (!log || FLASH_BLOCK_COUNT < 2)
        return -1;

    log->next_address = FLASH_BASE_ADDR;
    log->next_sequence = 1;
    const uint32_t end = FLASH_BASE_ADDR + (uint32_t)FLASH_BLOCK_COUNT * FLASH_BLOCK_SIZE;
    flash_record record;

    while (log->next_address + sizeof(record) <= end) {
        if (HW_READ(log->next_address, &record, sizeof(record))) return -1;
        if (record.magic != FLASH_RECORD_MAGIC) break;
        if (record.length > end - log->next_address - sizeof(record)) break;

        uint32_t crc;
        if (flash_crc32_at(log->next_address + sizeof(record), record.length, &crc))
            return -1;
        if (crc != record.crc32) break;

        // Signed subtraction compares wrapped sequence numbers safely.
        if ((int32_t)((record.sequence + 1u) - log->next_sequence) > 0)
            log->next_sequence = record.sequence + 1u;
        const uint32_t record_size = flash_align_up(sizeof(record) + record.length, FLASH_ALIGNMENT);
        if (record_size > end - log->next_address) break;
        log->next_address += record_size;
    }
    return 0;
}

// Append one value. The record header and payload are programmed together.
static inline int flash_log_write(
    flash_log *log, uint16_t logical_id, const void *data, uint16_t length
) {
    if (!log || (!data && length)) return -1;
    const uint32_t total = flash_align_up(sizeof(flash_record) + length, FLASH_ALIGNMENT);
    const uint32_t end = FLASH_BASE_ADDR + (uint32_t)FLASH_BLOCK_COUNT * FLASH_BLOCK_SIZE;
    if (log->next_address + total > end) return -2; // Full: GC not implemented.

    flash_record record = { .magic = FLASH_RECORD_MAGIC,
                            .sequence = log->next_sequence,
                            .logical_id = logical_id,
                            .length = length,
                            .crc32 = ~flash_crc32_update(0xFFFFFFFFu, (const uint8_t *)data, length) };

    if (HW_PROGRAM(log->next_address, &record, sizeof(record))) return -1;
    if (length && HW_PROGRAM(log->next_address + sizeof(record), data, length))
        return -1;

    log->next_address += total;
    ++log->next_sequence;
    return 0;
}

// Read the newest valid payload for a logical ID.
// Returns 0 on success, -1 on flash/CRC error, -2 if missing, -3 if buffer is small.
static inline int flash_log_read(
    const flash_log *log, uint16_t logical_id, void *data,
    uint16_t capacity, uint16_t *length
) {
    if (!log || !length || (!data && capacity)) return -1;

    uint32_t address = FLASH_BASE_ADDR;
    uint32_t new_sequence = 0;
    uint32_t new_address = 0;
    uint16_t new_length = 0;
    bool found = false;
    flash_record record;

    while (address + sizeof(record) <= log->next_address) {
        if (HW_READ(address, &record, sizeof(record))) return -1;
        if (record.magic != FLASH_RECORD_MAGIC) break;
        if (record.length > log->next_address - address - sizeof(record)) return -1;

        if (record.logical_id == logical_id &&
            (!found || (int32_t)(record.sequence - new_sequence) > 0)) {
            new_sequence = record.sequence;
            new_address = address;
            new_length = record.length;
            found = true;
        }

        address += flash_align_up(sizeof(record) + record.length, FLASH_ALIGNMENT);
    }

    if (!found) return -2;
    if (new_length > capacity) return -3;
    if (new_length &&
        HW_READ(new_address + sizeof(record), data, new_length))
        return -1;

    if (HW_READ(new_address, &record, sizeof(record))) return -1;
    if (~flash_crc32_update(0xFFFFFFFFu, (const uint8_t *)data, new_length) !=
        record.crc32)
        return -1;

    *length = new_length;
    return 0;
}

#endif // FUN_FLASH_LOG_H
