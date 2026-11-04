#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Flash layout: erase blocks contain pages; pages hold bytes from the record log.
// Record header: [sequence | CRC | logical_id | length | marker (magic + commit)]
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
    FLASH_BUFFER_TOO_SMALL
};

#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR    0x08010000u
#endif

#ifndef FLASH_BLOCK_COUNT
#define FLASH_BLOCK_COUNT  1
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

#ifndef FLASH_USE_CRC32
// Set to 0 to use CRC-16 instead; CRC-32 is the default.
#define FLASH_USE_CRC32 1
#endif

#if FLASH_USE_CRC32 != 0 && FLASH_USE_CRC32 != 1
#error "FLASH_USE_CRC32 must be 0 or 1"
#endif

#if FLASH_BASE_ADDR % FLASH_BLOCK_SIZE != 0
#error "FLASH_BASE_ADDR must be aligned to FLASH_BLOCK_SIZE"
#endif

#if FLASH_ALIGNMENT == 0 || (FLASH_ALIGNMENT & (FLASH_ALIGNMENT - 1)) != 0
#error "FLASH_ALIGNMENT must be a nonzero power of two"
#endif

#define FLASH_LOG_END (FLASH_BASE_ADDR + (uint32_t)FLASH_BLOCK_COUNT * FLASH_BLOCK_SIZE)

#define FLASH_RECORD_MAGIC 0xBABE // 16-bit record marker
#define FLASH_RECORD_COMMIT 0xCAFE //
#define FLASH_RECORD_MARKER (((uint32_t)FLASH_RECORD_COMMIT << 16) | FLASH_RECORD_MAGIC)


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

#if FLASH_USE_CRC32
typedef uint32_t flash_crc;
#else
typedef uint16_t flash_crc;
#endif

// Header stored before each value in the append-only flash log.
//! Changing this layout or FLASH_USE_CRC32 requires erasing existing records.
typedef struct {
    uint32_t sequence;      // Increasing value used to find the newest record.
    flash_crc crc;          // Selected-width checksum for the payload.
    uint16_t logical_id;    // Logical ID of the value stored in this record.
    uint16_t length;        // Number of payload bytes following this header.
    //! NOTE: Keep last so preceding fields need one write; uint32_t stays 4-byte aligned here.
    uint32_t marker;        // Combined magic and commit marker, programmed last.
} flash_record;

#if FLASH_CACHE_LENGTH > 0
// Cache entry pointing to the latest record for one logical ID.
typedef struct {
    uint32_t address;    // Flash address of the record header.
    flash_crc crc;       // CRC of the cached payload.
    uint16_t logical_id; // ID used to find the cached record.
    uint16_t length;     // Cached payload length.
    uint8_t valid;       // Whether this slot is in use.
} flash_cache_entry;
#endif

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t next_sequence; // Sequence number to assign to the next record.
#if FLASH_CACHE_LENGTH > 0
    uint16_t cache_next; // Next empty slot, then next replacement slot.
    flash_cache_entry cache[FLASH_CACHE_LENGTH];
#endif
} flash_log;

// Hardware interface; implement these functions for the target flash device.
int HW_FLASH_READ(uint32_t address, void *dst, size_t length);
int HW_FLASH_WRITE(uint32_t address, const void *src, size_t length);
int HW_ERASE_BLOCK(uint32_t address);

// Check that a program operation stays within a physical page, when required.
static inline bool flash_fits_page(
    uint32_t address, uint32_t length, uint32_t page_size
) {
    if (page_size == 0 || length == 0) return true;
    return (address / page_size) == ((address + length - 1u) / page_size);
}

#if FLASH_USE_CRC32
#define FLASH_CRC_INITIAL 0xFFFFFFFFu
// Update CRC-32 (reflected polynomial 0xEDB88320) with a chunk of bytes.
static inline flash_crc flash_crc_update(
    flash_crc crc, const uint8_t *bytes, size_t length
) {
    static const uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
    };

    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
    }
    return crc;
}
#else
#define FLASH_CRC_INITIAL 0xFFFFu
// Update CRC-16/MODBUS (reflected polynomial 0xA001) with a byte chunk.
static inline flash_crc flash_crc_update(
    flash_crc crc, const uint8_t *bytes, size_t length
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
#endif

#if FLASH_CACHE_LENGTH > 0
// Cache a record, replacing the entry for its logical ID or the next old entry.
static inline void flash_cache_store(
    flash_log *log, const flash_record *record, uint32_t address
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
    log->cache[slot].length = record->length;
    log->cache[slot].valid = true;
}
#endif

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
    if (!log || FLASH_BLOCK_COUNT < 2)
        return FLASH_ERROR;

    log->next_address = FLASH_BASE_ADDR;
    log->next_sequence = 1;

#if FLASH_CACHE_LENGTH > 0
    log->cache_next = 0;
    for (uint16_t i = 0; i < FLASH_CACHE_LENGTH; ++i)
        log->cache[i].valid = false;
#endif

    flash_record record;

    while (log->next_address + sizeof(record) <= FLASH_LOG_END) {
        if (HW_FLASH_READ(log->next_address, &record, sizeof(record)))
            return FLASH_ERROR;
        if (flash_record_is_erased(&record)) break;

        if (record.marker != FLASH_RECORD_MARKER ||
            record.length > FLASH_LOG_END - log->next_address - sizeof(record)) {
            log->next_address = flash_get_next_block(log->next_address);
            continue;
        }

        // Verify flash payload in small chunks without buffering the whole record.
        uint8_t crc_chunk[32];
        flash_crc crc = FLASH_CRC_INITIAL;
        uint32_t crc_address = log->next_address + sizeof(record);
        uint16_t crc_remaining = record.length;

        while (crc_remaining) {
            const size_t chunk_size = crc_remaining < sizeof(crc_chunk)
                                    ? crc_remaining : sizeof(crc_chunk);
            if (HW_FLASH_READ(crc_address, crc_chunk, chunk_size))
                return FLASH_ERROR;
            crc = flash_crc_update(crc, crc_chunk, chunk_size);
            crc_address += chunk_size;
            crc_remaining -= (uint16_t)chunk_size;
        }
        crc = (flash_crc)~crc;

        if (crc != record.crc) {
            log->next_address = flash_get_next_block(log->next_address);
            continue;
        }

#if FLASH_CACHE_LENGTH > 0
        flash_cache_store(log, &record, log->next_address);
#endif

        // Signed subtraction compares wrapped sequence numbers safely.
        if ((int32_t)((record.sequence + 1u) - log->next_sequence) > 0)
            log->next_sequence = record.sequence + 1u;

        // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
        // With alignment 4 and size 13: ((13 + 3) / 4) * 4 = 16; (13 + 3) & ~3 = 16.
        const uint32_t record_size = (sizeof(record) + record.length + FLASH_ALIGNMENT - 1u) &
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u);
        if (record_size > FLASH_LOG_END - log->next_address)
            break;
        if (record_size > FLASH_BLOCK_SIZE -
            ((log->next_address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u))) {
            log->next_address = flash_get_next_block(log->next_address - 1u);
            continue;
        }
        log->next_address += record_size;
    }
    return FLASH_OK;
}

// Append one value, committing the record only after its header and payload are written.
static inline flash_status flash_log_write(
    flash_log *log, uint16_t logical_id, const void *data, uint16_t length
) {
    if (!log || (!data && length))
        return FLASH_ERROR;
    // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
    const uint32_t total = (sizeof(flash_record) + length + FLASH_ALIGNMENT - 1u) &
                            ~((uint32_t)FLASH_ALIGNMENT - 1u);
    if (total > FLASH_BLOCK_SIZE)
        return FLASH_RECORD_TOO_LARGE;

    if (((log->next_address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u)) + total >
        FLASH_BLOCK_SIZE
    ) {
        log->next_address = flash_get_next_block(log->next_address - 1u);
    }

    if (log->next_address + total > FLASH_LOG_END)
        return FLASH_FULL; // GC not implemented.

    flash_record record = {
        .marker = FLASH_RECORD_MARKER,
        .sequence = log->next_sequence,
        .logical_id = logical_id,
        .length = length,
        .crc = (flash_crc)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data, length)
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
    ++log->next_sequence;
#if FLASH_CACHE_LENGTH > 0
    flash_cache_store(log, &record, log->next_address - total);
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
            (flash_crc)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data,
                                        entry->length) != entry->crc
        ) return FLASH_ERROR;

        *length = entry->length;
        return FLASH_OK;
    }
#endif

    // Scan records and remember the newest matching logical ID.
    uint32_t address = FLASH_BASE_ADDR;
    uint32_t new_sequence = 0;
    uint32_t new_address = 0;
    flash_crc new_crc = 0;
    uint16_t new_length = 0;
    bool found = false;
    flash_record record;

    while (address + sizeof(record) <= log->next_address) {
        // Read this header to check whether it matches the requested logical ID.
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;
        // An erased header means there are no more records in this block.
        if (flash_record_is_erased(&record))
            break;

        if (record.marker != FLASH_RECORD_MARKER ||
            record.length > log->next_address - address - sizeof(record)
        ) {
            // Skip to the next block if this record's header is invalid or incomplete.
            address = flash_get_next_block(address);
            continue;
        }

        // Save this record if it is the first match or newer than the saved one.
        if (record.logical_id == logical_id &&
            (!found || (int32_t)(record.sequence - new_sequence) > 0)
        ) {
            new_sequence = record.sequence;
            new_crc = record.crc;
            new_length = record.length;
            new_address = address;
            found = true;
        }

        // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
        const uint32_t record_size = (sizeof(record) + record.length + FLASH_ALIGNMENT - 1u) &
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u);
        // Move on if this record would cross the current block boundary.
        if (record_size > FLASH_BLOCK_SIZE - ((address - FLASH_BASE_ADDR) & (FLASH_BLOCK_SIZE - 1u)))
            address = flash_get_next_block(address);
        else
            address += record_size;
    }

    if (!found) return FLASH_NOT_FOUND;
    if (new_length > capacity)
        return FLASH_BUFFER_TOO_SMALL;

    // Read the selected payload and verify its CRC before returning it.
    if ((new_length && HW_FLASH_READ(new_address + sizeof(record), data, new_length)) ||
        (flash_crc)~flash_crc_update(FLASH_CRC_INITIAL, (const uint8_t *)data, new_length) != new_crc
    ) return FLASH_ERROR;

#if FLASH_CACHE_LENGTH > 0
    // Cache only the selected record after its payload passes the CRC check.
    flash_record newest = {
        .logical_id = logical_id,
        .crc = new_crc,
        .length = new_length
    };
    flash_cache_store(log, &newest, new_address);
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
