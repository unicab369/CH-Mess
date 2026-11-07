#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Flash layout: erase blocks contain pages; pages hold bytes from the record log.
// Record header: [CRC | logical_id | marker (payload size + commit)]
// Before garbage collection, later flash addresses contain newer records for
// the same logical ID.
//
// [erase block - 4KB (configurable)]
//   [page: [header] [payload] [alignment padding] ...]
//   [page: [header] [payload] [alignment padding] ...]
//   [page: ...record bytes continue...]

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t flash_status;

enum {
    FLASH_OK = 0,
    FLASH_ERROR,
    FLASH_FULL,
    FLASH_RECORD_TOO_LARGE,
    FLASH_NOT_FOUND,
    FLASH_BUFFER_TOO_SMALL,
    FLASH_RECORD_INVALID_SIZE
};

#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR    0x08010000u
#endif

#ifndef FLASH_BLOCK_COUNT
#define FLASH_BLOCK_COUNT  2
#endif

#if FLASH_BLOCK_COUNT < 2
#error "FLASH_BLOCK_COUNT must be at least 2"
#endif

#ifndef FLASH_CACHE_LENGTH
// Set to 0 to disable the RAM cache.
#define FLASH_CACHE_LENGTH 0
#endif

#if FLASH_CACHE_LENGTH > 0 && \
    (FLASH_CACHE_LENGTH & (FLASH_CACHE_LENGTH - 1)) != 0
#error "FLASH_CACHE_LENGTH must be a power of two or zero"
#endif

#ifndef FLASH_BLOCK_SIZE
#define FLASH_BLOCK_SIZE   4096
#endif

#if FLASH_BLOCK_SIZE == 0 || (FLASH_BLOCK_SIZE & (FLASH_BLOCK_SIZE - 1)) != 0
#error "FLASH_BLOCK_SIZE must be a nonzero power of two"
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE    256
#endif

#ifndef FLASH_ALIGNMENT
#define FLASH_ALIGNMENT    4
#endif

#ifndef FLASH_ERASED_WORD
// Most flash reads as 0xFFFFFFFF after erase; override for scrambled CodeFlash.
#define FLASH_ERASED_WORD  0xFFFFFFFFu
#endif

#ifndef FLASH_BASE_ADDR_RUNTIME
#if FLASH_BASE_ADDR % FLASH_BLOCK_SIZE != 0
#error "FLASH_BASE_ADDR must be aligned to FLASH_BLOCK_SIZE"
#endif
#endif

#if FLASH_ALIGNMENT == 0 || (FLASH_ALIGNMENT & (FLASH_ALIGNMENT - 1)) != 0
#error "FLASH_ALIGNMENT must be a nonzero power of two"
#endif

#define FLASH_LOG_END (FLASH_BASE_ADDR + (uint32_t)FLASH_BLOCK_COUNT * FLASH_BLOCK_SIZE)

#define FLASH_RECORD_COMMIT 0xCAFEu
#define FLASH_RECORD_MARKER_4  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA04u)
#define FLASH_RECORD_MARKER_8  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA08u)
#define FLASH_RECORD_MARKER_16 (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA10u)


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

// Fixed payload sizes supported by the record marker.
typedef enum {
    FLASH_RECORD_SIZE_4 = 4,
    FLASH_RECORD_SIZE_8 = 8,
    FLASH_RECORD_SIZE_16 = 16
} flash_record_size;

// Header stored before each value in the append-only flash log.
//! Changing this layout or marker values requires erasing existing records.
// Size: 8 bytes.
typedef struct {
    uint16_t crc;        // CRC-16 checksum for the payload.
    uint16_t logical_id; // May repeat; later records replace earlier values.
    //! NOTE: Keep last so preceding fields need one write; uint32_t stays 4-byte aligned here.
    uint32_t marker;        // Combined magic and commit marker, programmed last.
} flash_record;

#if FLASH_CACHE_LENGTH > 0
// Cache entry pointing to the latest record for one logical ID.
typedef struct {
    uint32_t address;    // Flash address of the record header.
    uint16_t crc;        // CRC-16 checksum of the cached payload.
    uint16_t logical_id; // May repeat; the latest record is at the highest address.
    uint16_t length;     // Cached payload length.
    uint8_t valid;       // Whether this slot is in use.
} flash_cache_entry;
#endif

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
#if FLASH_CACHE_LENGTH > 0
    uint16_t cache_next; // Next empty slot, then next replacement slot.
    flash_cache_entry cache[FLASH_CACHE_LENGTH];
#endif
} flash_log;

// Hardware interface; implement these functions for the target flash device.
int HW_FLASH_READ(uint32_t address, void *dst, size_t length);
int HW_FLASH_WRITE(uint32_t address, const void *src, size_t length);
uint32_t HW_ERASE_BLOCK(uint32_t address);

// Check that a program operation stays within a physical page, when required.
static inline bool flash_fits_page(
    uint32_t address, uint32_t length, uint32_t page_size
) {
    if (page_size == 0 || length == 0) return true;
    return (address / page_size) == ((address + length - 1u) / page_size);
}

#define FLASH_CRC_INITIAL 0xFFFFu
// Update CRC-16/MODBUS (reflected polynomial 0xA001) with a byte chunk.
static inline uint16_t flash_crc_update(
    uint16_t crc, const uint8_t *bytes, size_t length
) {
    static const uint16_t table[16] = {
        0x0000u, 0xCC01u, 0xD801u, 0x1400u,
        0xF001u, 0x3C00u, 0x2800u, 0xE401u,
        0xA001u, 0x6C00u, 0x7800u, 0xB401u,
        0x5000u, 0x9C01u, 0x8801u, 0x4400u
    };

    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
    }
    return crc;
}

#if FLASH_CACHE_LENGTH > 0
// Cache a record, replacing the entry for its logical ID or the next old entry.
static inline void flash_cache_store(
    flash_log *log, const flash_record *record, uint32_t address, uint16_t length
) {
    uint16_t slot;
    for (slot = 0; slot < FLASH_CACHE_LENGTH; ++slot) {
        if (log->cache[slot].valid &&
            log->cache[slot].logical_id == record->logical_id
        ) break;
    }

    if (slot == FLASH_CACHE_LENGTH) {
        // Fill slots in order, then reuse them in round-robin order.
        slot = log->cache_next;
        log->cache_next = (slot + 1u) & (FLASH_CACHE_LENGTH - 1u);
    }

    log->cache[slot].logical_id = record->logical_id;
    log->cache[slot].address = address;
    log->cache[slot].crc = record->crc;
    log->cache[slot].length = length;
    log->cache[slot].valid = true;
}
#endif

static inline uint16_t flash_record_payload_size(uint32_t marker) {
    switch (marker) {
    case FLASH_RECORD_MARKER_4:  return FLASH_RECORD_SIZE_4;
    case FLASH_RECORD_MARKER_8:  return FLASH_RECORD_SIZE_8;
    case FLASH_RECORD_MARKER_16: return FLASH_RECORD_SIZE_16;
    default: return 0;
    }
}

static inline uint32_t flash_record_marker(flash_record_size size) {
    switch (size) {
    case FLASH_RECORD_SIZE_4:  return FLASH_RECORD_MARKER_4;
    case FLASH_RECORD_SIZE_8:  return FLASH_RECORD_MARKER_8;
    case FLASH_RECORD_SIZE_16: return FLASH_RECORD_MARKER_16;
    default: return 0;
    }
}

// Move an address to the start of the next erase block.
static inline uint32_t flash_get_next_block(uint32_t address) {
    // Equivalent to (address - FLASH_BASE_ADDR) % FLASH_BLOCK_SIZE.
    // For 4096-byte blocks:
    // 4100 % 4096 = 4; 4100 & 4095 = 4.
    const uint32_t offset = (address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u);
    return address + FLASH_BLOCK_SIZE - offset;
}

// Check whether an unread record header is still fully erased.
static inline bool flash_record_is_erased(const flash_record *record) {
    const uint8_t *bytes = (const uint8_t *)record;

    for (size_t i = 0; i < sizeof(*record); ++i)
        if (bytes[i] != (uint8_t)(FLASH_ERASED_WORD >> ((i & 3u) * 8u)))
            return false;
    return true;
}

// Scan the reserved region to find the next unused record slot.
static inline flash_status flash_log_init(flash_log *log) {
    if (!log)
        return FLASH_ERROR;

    flash_record record;
    log->next_address = FLASH_BASE_ADDR;

#if FLASH_CACHE_LENGTH > 0
    log->cache_next = 0;
    for (uint16_t i = 0; i < FLASH_CACHE_LENGTH; ++i)
        log->cache[i].valid = false;
#endif

    while (log->next_address + sizeof(record) <= FLASH_LOG_END) {
        if (HW_FLASH_READ(log->next_address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t record_length = flash_record_payload_size(record.marker);
        if (!record_length) {
            if (flash_record_is_erased(&record))
                break;
            log->next_address = flash_get_next_block(log->next_address);
            continue;
        }
        if (record_length > FLASH_LOG_END - log->next_address - sizeof(record)) {
            log->next_address = flash_get_next_block(log->next_address);
            continue;
        }

        // Check whether this record fits before reading its payload for the CRC.
        const uint32_t record_size = (sizeof(record) + record_length + FLASH_ALIGNMENT - 1u) &
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u);
        if (record_size > FLASH_LOG_END - log->next_address)
            break;

        const uint32_t block_remaining = FLASH_BLOCK_SIZE -
                                ((log->next_address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u));
        if (record_size > block_remaining) {
            log->next_address += block_remaining;
            continue;
        }

        // Verify flash payload in small chunks without buffering the whole record.
        uint8_t crc_chunk[32];
        uint16_t crc = FLASH_CRC_INITIAL;
        uint32_t crc_address = log->next_address + sizeof(record);
        uint16_t crc_remaining = record_length;

        while (crc_remaining) {
            const size_t chunk_size = crc_remaining < sizeof(crc_chunk)
                                    ? crc_remaining : sizeof(crc_chunk);
            if (HW_FLASH_READ(crc_address, crc_chunk, chunk_size))
                return FLASH_ERROR;

            crc = flash_crc_update(crc, crc_chunk, chunk_size);
            crc_address += chunk_size;
            crc_remaining -= (uint16_t)chunk_size;
        }

        if ((uint16_t)~crc != record.crc) {
            log->next_address = flash_get_next_block(log->next_address);
            continue;
        }

#if FLASH_CACHE_LENGTH > 0
        flash_cache_store(log, &record, log->next_address, record_length);
#endif

        log->next_address += record_size;
    }

    return FLASH_OK;
}

// Append one value, committing the record only after its header and payload are written.
static inline flash_status flash_log_write(
    flash_log *log, uint16_t logical_id, const void *data, flash_record_size size
) {
    if (!log || !data)
        return FLASH_ERROR;
    const uint16_t length = (uint16_t)size;
    const uint32_t marker = flash_record_marker(size);
    if (!marker)
        return FLASH_RECORD_INVALID_SIZE;

    // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
    const uint32_t total = (sizeof(flash_record) + length + FLASH_ALIGNMENT - 1u) &
                            ~((uint32_t)FLASH_ALIGNMENT - 1u);
    if (total > FLASH_BLOCK_SIZE)
        return FLASH_RECORD_TOO_LARGE;

    const uint32_t block_offset = (log->next_address - FLASH_BASE_ADDR) &
                                  (FLASH_BLOCK_SIZE - 1u);
    if (block_offset + total > FLASH_BLOCK_SIZE)
        log->next_address += FLASH_BLOCK_SIZE - block_offset;

    if (log->next_address + total > FLASH_LOG_END)
        return FLASH_FULL; // GC not implemented.

    flash_record record = {
        .marker = marker,
        .logical_id = logical_id,
        .crc = (uint16_t)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data, length)
    };

    if (
        // Write the header fields before the final marker.
        HW_FLASH_WRITE(log->next_address, &record, offsetof(flash_record, marker)) ||
        // Write the payload before marking the record complete.
        (length && HW_FLASH_WRITE(log->next_address + sizeof(record), data, length)) ||
        // Write the combined marker last so startup can identify a complete record.
        HW_FLASH_WRITE(log->next_address + offsetof(flash_record, marker), &record.marker,
                       sizeof(record.marker))
    ) {
        log->next_address = flash_get_next_block(log->next_address);
        return FLASH_ERROR;
    }

    log->next_address += total;
#if FLASH_CACHE_LENGTH > 0
    flash_cache_store(log, &record, log->next_address - total, length);
#endif
    return FLASH_OK;
}

// Read the latest committed payload for one logical ID.
// Returns a FLASH_* value and writes the payload size to length.
// Fast read for callers that guarantee valid pointers and arguments.
static inline flash_status flash_log_read_fast(
    flash_log *log, uint16_t logical_id, void *data,
    uint16_t capacity, uint16_t *length
) {
#if FLASH_CACHE_LENGTH > 0
    // Use a cached record location before scanning flash headers.
    for (uint16_t i = 0; i < FLASH_CACHE_LENGTH; ++i) {
        flash_cache_entry *entry = &log->cache[i];

        if (!entry->valid || entry->logical_id != logical_id)
            continue;
        if (entry->length > capacity)
            return FLASH_BUFFER_TOO_SMALL;

        if ((entry->length && HW_FLASH_READ(entry->address + sizeof(flash_record),
                                            data, entry->length)) ||
            (uint16_t)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data,
                                        entry->length) != entry->crc
        ) return FLASH_ERROR;

        *length = entry->length;
        return FLASH_OK;
    }
#endif

    // Scan records and remember the newest matching logical ID.
    uint32_t address = FLASH_BASE_ADDR;
    uint32_t new_address = 0;
    uint16_t new_crc = 0;
    uint16_t new_length = 0;
    bool found = false;
    flash_record record;

    // Scan in address order; the last matching record is the newest.
    while (address + sizeof(record) <= log->next_address) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t record_length = flash_record_payload_size(record.marker);
        if (!record_length) {
            // An erased header means there are no more records in this block.
            if (flash_record_is_erased(&record))
                break;
            // Skip to the next block if this record's header is invalid or incomplete.
            address = flash_get_next_block(address);
            continue;
        }
        if (record_length > log->next_address - address - sizeof(record)) {
            address = flash_get_next_block(address);
            continue;
        }

        // Later addresses represent later writes for this logical ID.
        if (record.logical_id == logical_id) {
            new_crc = record.crc;
            new_length = record_length;
            new_address = address;
            found = true;
        }

        // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
        const uint32_t record_size = (sizeof(record) + record_length + FLASH_ALIGNMENT - 1u) &
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u);
        // Move on if this record would cross the current block boundary.
        const uint32_t block_remaining = FLASH_BLOCK_SIZE -
                                        ((address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u));
        address += record_size > block_remaining ? block_remaining : record_size;
    }

    if (!found) return FLASH_NOT_FOUND;
    if (new_length > capacity)
        return FLASH_BUFFER_TOO_SMALL;

    // Read the selected payload and verify its CRC before returning it.
    if ((new_length && HW_FLASH_READ(new_address + sizeof(record), data, new_length)) ||
        (uint16_t)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data, new_length) != new_crc
    ) return FLASH_ERROR;

#if FLASH_CACHE_LENGTH > 0
    // Cache only the selected record after its payload passes the CRC check.
    flash_record newest = {
        .logical_id = logical_id,
        .crc = new_crc
    };
    flash_cache_store(log, &newest, new_address, new_length);
#endif

    *length = new_length;
    return FLASH_OK;
}

// Read a value after checking that required pointers are valid.
static inline flash_status flash_log_read(
    flash_log *log, uint16_t logical_id, void *data,
    uint16_t capacity, uint16_t *length
) {
    if (!log || !length || (!data && capacity)) return FLASH_ERROR;
    return flash_log_read_fast(log, logical_id, data, capacity, length);
}

#endif // FUN_FLASH_LOG_H
