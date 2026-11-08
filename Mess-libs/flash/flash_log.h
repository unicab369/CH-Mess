#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Each block starts with a 4-byte header [generation: 2 bytes | size-class commit: 2 bytes].
// The header fixes the payload size for every record in the block (4 or 8 bytes).
// Records start immediately after it. Each record has an 8-byte header
// [CRC: 2 bytes | logical_id: 2 bytes | marker: 4 bytes], then its payload and alignment padding.
// GC copies the newest value for each (logical ID, size class) into a shared spare block.
//
// [erase block - 4KB (configurable)]
//   [block header: generation (2 bytes) | size-class commit (2 bytes)]
//   [record header: CRC (2 bytes) | logical_id (2 bytes) | marker (4 bytes)] [payload] [padding]
//   [record header: CRC (2 bytes) | logical_id (2 bytes) | marker (4 bytes)] [payload] [padding] ...
//
// Capacity for the default 4 KB block and 4-byte alignment:
//
// | Payload | Block header | Record header | Record size | Records/block | Unused space |
// |---------|--------------|---------------|-------------|---------------|--------------|
// | 4 bytes | 4 bytes      | 8 bytes       | 12 bytes    | 341           | no tail      |
// | 8 bytes | 4 bytes      | 8 bytes       | 16 bytes    | 255           | 12-byte tail |

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
// Two active size classes need at least one additional block for shared GC scratch.

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
#define FLASH_RECORD_MAGIC  0xBA00u
// Pass one of these as the size selector to flash_log_read() and flash_log_write().
#define FLASH_RECORD_MARKER_4  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA04u)
#define FLASH_RECORD_MARKER_8  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA08u)

// These values commit the block and identify its fixed payload size.
#define FLASH_BLOCK_HEADER_COMMIT_4 0xBA84u
#define FLASH_BLOCK_HEADER_COMMIT_8 0xBA88u


// Header stored before each value in the append-only flash log.
//! Changing this layout or marker values requires erasing existing records.
// Size: 8 bytes. Records start on 4-byte boundaries and are padded to 4 bytes.
typedef struct {
    uint16_t crc;        // CRC-16 checksum for the payload.
    uint16_t logical_id; // May repeat; later records replace earlier values.
    //! NOTE: Keep last so preceding fields need one write; uint32_t stays 4-byte aligned here.
    uint32_t marker;        // Combined magic and commit marker, programmed last.
} flash_record;

// Block metadata for power-safe garbage collection.
// Write generation first and commit last; a block is usable only with a valid commit.
// Size: 4 bytes, stored at the start of an erase block.
typedef struct {
    uint16_t generation; // Increases when GC installs a copied block.
    uint16_t commit;     // Size-class commit value, written after records are copied.
} flash_block_header;

#define FLASH_RECORDS_OFFSET \
    ((sizeof(flash_block_header) + FLASH_ALIGNMENT - 1u) & ~((uint32_t)FLASH_ALIGNMENT - 1u))

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t record_end;    // End of committed records, even if a damaged tail fills the block.
    uint32_t active_block;  // Block selected by its committed generation header.
    uint16_t generation;
    uint16_t record_count;  // Number of committed records in the active block.
} log_pool_state;

// Indexes into the library's single global pool-state array.
enum {
    FLASH_LOG_POOL_4,
    FLASH_LOG_POOL_8,
    FLASH_LOG_POOL_COUNT
};

// The library manages one flash log globally; the target port defines this once.
extern log_pool_state flash_log_pools[FLASH_LOG_POOL_COUNT];
// Hardware interface; implement these functions for the target flash device.
int HW_FLASH_READ(uint32_t address, void *dst, size_t length);
int HW_FLASH_WRITE(uint32_t address, const void *src, size_t length);
uint32_t HW_ERASE_BLOCK(uint32_t address);

// Check that a program operation stays within a physical page, when required.
#define FLASH_CRC_INITIAL 0xFFFFu
// Calculate the finalized CRC-16/MODBUS (reflected polynomial 0xA001).
static inline uint16_t calculate_crc(const uint8_t *bytes, size_t length) {
    static const uint16_t table[16] = {
        0x0000u, 0xCC01u, 0xD801u, 0x1400u,
        0xF001u, 0x3C00u, 0x2800u, 0xE401u,
        0xA001u, 0x6C00u, 0x7800u, 0xB401u,
        0x5000u, 0x9C01u, 0x8801u, 0x4400u
    };

    uint16_t crc = FLASH_CRC_INITIAL;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
    }
    return (uint16_t)~crc;
}

static inline uint16_t get_record_len(uint32_t marker) {
    switch (marker) {
    case FLASH_RECORD_MARKER_4:  return 4;
    case FLASH_RECORD_MARKER_8:  return 8;
    default: return 0;
    }
}

static inline uint16_t block_commit_for_size(uint32_t size_code) {
    switch (size_code) {
    case FLASH_RECORD_MARKER_4: return FLASH_BLOCK_HEADER_COMMIT_4;
    case FLASH_RECORD_MARKER_8: return FLASH_BLOCK_HEADER_COMMIT_8;
    default: return 0;
    }
}

static inline uint32_t block_size_for_commit(uint16_t commit) {
    if (commit == FLASH_BLOCK_HEADER_COMMIT_4) return FLASH_RECORD_MARKER_4;
    if (commit == FLASH_BLOCK_HEADER_COMMIT_8) return FLASH_RECORD_MARKER_8;
    return 0;
}

static inline flash_status block_header_write(
    uint32_t address, uint16_t generation, uint32_t size_code
) {
    const uint16_t commit = block_commit_for_size(size_code);
    if (!commit)
        return FLASH_RECORD_INVALID_SIZE;
    // Program an erased commit field first, then program the complete 4-byte
    // word. Both writes stay 4-byte aligned; the second write commits the block.
    const flash_block_header pending = {
        .generation = generation,
        .commit = 0xFFFFu
    };
    const flash_block_header committed = {
        .generation = generation,
        .commit = commit
    };
    return (HW_FLASH_WRITE(address, &pending, sizeof(pending)) ||
            HW_FLASH_WRITE(address, &committed, sizeof(committed)))
            ? FLASH_ERROR : FLASH_OK;
}

static inline log_pool_state *log_pool_for_size(uint32_t size_code) {
    if (size_code == FLASH_RECORD_MARKER_4) return &flash_log_pools[FLASH_LOG_POOL_4];
    if (size_code == FLASH_RECORD_MARKER_8) return &flash_log_pools[FLASH_LOG_POOL_8];
    return NULL;
}

static inline uint32_t log_free_blocks(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
        const uint32_t block = FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE;
        if (block != flash_log_pools[FLASH_LOG_POOL_4].active_block &&
            block != flash_log_pools[FLASH_LOG_POOL_8].active_block
        ) ++count;
    }
    return count;
}

static inline uint32_t get_record_size(uint16_t payload_length);

// Rebuild one class's append cursor by scanning its committed record headers.
static inline flash_status flash_log_pool_scan(log_pool_state *pool, uint32_t size_code) {
    const uint16_t expected_length = get_record_len(size_code);
    const uint32_t block_end = pool->active_block + FLASH_BLOCK_SIZE;
    pool->next_address = pool->active_block + FLASH_RECORDS_OFFSET;
    pool->record_end = pool->next_address;
    pool->record_count = 0;
    flash_record record;

    while (pool->next_address + sizeof(record) <= block_end) {
        if (HW_FLASH_READ(pool->next_address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t length = get_record_len(record.marker);
        if (!length) {
            const uint8_t *bytes = (const uint8_t *)&record;
            for (size_t i = 0; i < sizeof(record); ++i) {
                if (bytes[i] != (uint8_t)(FLASH_ERASED_WORD >> ((i & 3u) * 8u))) {
                    pool->next_address = block_end;
                    return FLASH_OK;
                }
            }
            return FLASH_OK;
        }

        if (length != expected_length ||
            length > block_end - pool->next_address - sizeof(record)
        ) {
            pool->next_address = block_end;
            return FLASH_OK;
        }

        const uint32_t record_size = get_record_size(length);
        if (record_size > block_end - pool->next_address) {
            pool->next_address = block_end;
            return FLASH_OK;
        }

        uint8_t payload[8];
        if (HW_FLASH_READ(pool->next_address + sizeof(record), payload, length) ||
            calculate_crc(payload, length) != record.crc
        ) {
            pool->next_address = block_end;
            return FLASH_OK;
        }

        pool->next_address += record_size;
        pool->record_end = pool->next_address;
        ++pool->record_count;
    }
    return FLASH_OK;
}

// Find the newest committed block in each size class. Blank storage is formatted lazily.
static inline flash_status flash_log_init(void) {
    flash_log_pools[FLASH_LOG_POOL_4] = (log_pool_state){0};
    flash_log_pools[FLASH_LOG_POOL_8] = (log_pool_state){0};
    flash_block_header header;
    bool found = false;

    for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
        const uint32_t block = FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE;
        if (HW_FLASH_READ(block, &header, sizeof(header)))
            return FLASH_ERROR;

        const uint32_t size_code = block_size_for_commit(header.commit);
        log_pool_state *pool = log_pool_for_size(size_code);
        if (!pool)
            continue;

        found = true;
        if (!pool->active_block || (int16_t)(header.generation - pool->generation) > 0) {
            pool->active_block = block;
            pool->generation = header.generation;
        }
    }

    if (!found) {
        // No valid block exists; erase stale or unformatted contents before first use.
        for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i)
            if (HW_ERASE_BLOCK(FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE))
                return FLASH_ERROR;
    }

    if ((flash_log_pools[FLASH_LOG_POOL_4].active_block &&
         flash_log_pool_scan(&flash_log_pools[FLASH_LOG_POOL_4], FLASH_RECORD_MARKER_4) != FLASH_OK) ||
        (flash_log_pools[FLASH_LOG_POOL_8].active_block &&
         flash_log_pool_scan(&flash_log_pools[FLASH_LOG_POOL_8], FLASH_RECORD_MARKER_8) != FLASH_OK)
    ) return FLASH_ERROR;

    return FLASH_OK;
}

static inline uint32_t get_record_size(uint16_t payload_length) {
    return (sizeof(flash_record) + payload_length + FLASH_ALIGNMENT - 1u) &
           ~((uint32_t)FLASH_ALIGNMENT - 1u);
}

static inline flash_status store_record_at(
    uint32_t address, uint16_t logical_id, const uint8_t *data,
    uint16_t length, uint32_t marker
) {
    flash_record record = {
        .marker = marker,
        .logical_id = logical_id,
        .crc = calculate_crc(data, length)
    };
    if (HW_FLASH_WRITE(address, &record, offsetof(flash_record, marker)) ||
        HW_FLASH_WRITE(address + sizeof(record), data, length) ||
        HW_FLASH_WRITE(address + offsetof(flash_record, marker), &record.marker,
                       sizeof(record.marker))
    ) return FLASH_ERROR;
    return FLASH_OK;
}

// Copy current values of one size class into an erased shared spare block.
static inline flash_status flash_log_gc(log_pool_state *pool, uint32_t size_code) {
    const uint32_t old_block = pool->active_block;
    uint32_t new_block = 0;

    for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
        const uint32_t candidate = FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE;
        if (candidate != flash_log_pools[FLASH_LOG_POOL_4].active_block &&
            candidate != flash_log_pools[FLASH_LOG_POOL_8].active_block
        ) {
            new_block = candidate;
            break;
        }
    }
    if (!new_block)
        return FLASH_FULL;
    if (HW_ERASE_BLOCK(new_block))
        return FLASH_ERROR;

    uint32_t write_address = new_block + FLASH_RECORDS_OFFSET;
    const uint32_t new_end = new_block + FLASH_BLOCK_SIZE;
    uint32_t address = old_block + FLASH_RECORDS_OFFSET;
    const uint32_t old_end = pool->next_address;
    const uint16_t old_count = pool->record_count;
    const uint16_t payload_len = get_record_len(size_code);
    const uint32_t record_size = get_record_size(payload_len);
    uint16_t copied_count = 0;
    flash_record record;
    uint8_t payload[8];

    // For each record, scan later records to determine whether its ID was updated.
    for (uint16_t i = 0; i < old_count && address + sizeof(record) <= old_end; ++i) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;
        if (get_record_len(record.marker) != payload_len ||
            payload_len > old_end - address - sizeof(record)
        ) break;

        bool replaced = false;
        uint32_t later_address = address + record_size;

        for (uint16_t j = (uint16_t)(i + 1u);
             j < old_count && later_address + sizeof(record) <= old_end; ++j
        ) {
            flash_record later;

            if (HW_FLASH_READ(later_address, &later, sizeof(later)))
                return FLASH_ERROR;
            if (get_record_len(later.marker) != payload_len ||
                payload_len > old_end - later_address - sizeof(later))
                break;
            if (later.logical_id == record.logical_id) {
                replaced = true;
                break;
            }
            later_address += record_size;
        }

        if (!replaced) {
            if (write_address + record_size > new_end)
                return FLASH_FULL;
            if (HW_FLASH_READ(address + sizeof(record), payload, payload_len) ||
                calculate_crc(payload, payload_len) != record.crc ||
                store_record_at(write_address, record.logical_id, payload,
                                payload_len, size_code) != FLASH_OK
            ) return FLASH_ERROR;

            write_address += record_size;
            ++copied_count;
        }
        address += record_size;
    }

    // Commit the size class and generation after all copied records are complete.
    const uint16_t new_generation = (uint16_t)(pool->generation + 1u);
    if (block_header_write(new_block, new_generation, size_code) != FLASH_OK)
        return FLASH_ERROR;

    pool->active_block = new_block;
    pool->generation = new_generation;
    pool->next_address = write_address;
    pool->record_end = write_address;
    pool->record_count = copied_count;
    // Keep the old block until the destination header is committed; it becomes spare after erase.
    return HW_ERASE_BLOCK(old_block) ? FLASH_ERROR : FLASH_OK;
}

// Append one value to its fixed-size class; size is part of the logical key.
static inline flash_status flash_log_write(
    uint16_t logical_id, const void *data, uint32_t size_code
) {
    if (!data) return FLASH_ERROR;

    log_pool_state *pool = log_pool_for_size(size_code);
    const uint16_t length = get_record_len(size_code);
    if (!pool || !length)
        return FLASH_RECORD_INVALID_SIZE;

    // The first class can use one block and leave one spare. A second class needs
    // a block of its own while preserving a shared spare, so it needs two free blocks.
    if (!pool->active_block) {
        log_pool_state *other_pool = (pool == &flash_log_pools[FLASH_LOG_POOL_4])
                            ? &flash_log_pools[FLASH_LOG_POOL_8] : &flash_log_pools[FLASH_LOG_POOL_4];
        const uint32_t needed_free = other_pool->active_block ? 2u : 1u;
        if (log_free_blocks() < needed_free)
            return FLASH_FULL;

        for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
            const uint32_t block = FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE;
            if (block == flash_log_pools[FLASH_LOG_POOL_4].active_block ||
                block == flash_log_pools[FLASH_LOG_POOL_8].active_block
            ) continue;

            if (HW_ERASE_BLOCK(block))
                return FLASH_ERROR;

            pool->active_block = block;
            pool->generation = 1;
            if (block_header_write(block, pool->generation, size_code) != FLASH_OK) {
                pool->active_block = 0;
                return FLASH_ERROR;
            }
            pool->next_address = block + FLASH_RECORDS_OFFSET;
            pool->record_end = pool->next_address;
            pool->record_count = 0;
            break;
        }
    }

    const uint32_t record_size = get_record_size(length);
    if (record_size > FLASH_BLOCK_SIZE - FLASH_RECORDS_OFFSET)
        return FLASH_RECORD_TOO_LARGE;

    if (pool->next_address + record_size > pool->active_block + FLASH_BLOCK_SIZE) {
        const flash_status status = flash_log_gc(pool, size_code);

        if (status != FLASH_OK)
            return status;
        if (pool->next_address + record_size > pool->active_block + FLASH_BLOCK_SIZE)
            return FLASH_FULL;
    }

    if (store_record_at(pool->next_address, logical_id, data, length, size_code) != FLASH_OK) {
        pool->next_address = pool->active_block + FLASH_BLOCK_SIZE;
        return FLASH_ERROR;
    }
    pool->next_address += record_size;
    pool->record_end = pool->next_address;
    ++pool->record_count;
    return FLASH_OK;
}

// Read the newest value for a logical ID from the requested size class.
static inline flash_status flash_log_read_fast(
    uint16_t logical_id, uint32_t size_code, void *data,
    uint16_t capacity, uint16_t *length
) {
    log_pool_state *pool = log_pool_for_size(size_code);
    const uint16_t requested_length = get_record_len(size_code);

    if (!pool || !requested_length)
        return FLASH_RECORD_INVALID_SIZE;
    if (!pool->active_block)
        return FLASH_NOT_FOUND;

    uint32_t address = pool->active_block + FLASH_RECORDS_OFFSET;
    uint32_t newest_address = 0;
    uint16_t newest_crc = 0;
    bool found = false;
    flash_record record;

    // Later records with the same ID replace earlier values in this size class.
    // record_end excludes any damaged or incomplete tail that must be skipped by future writes.
    while (address + sizeof(record) <= pool->record_end) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;
        if (get_record_len(record.marker) != requested_length ||
            requested_length > pool->record_end - address - sizeof(record)
        ) break;

        if (record.logical_id == logical_id) {
            newest_address = address;
            newest_crc = record.crc;
            found = true;
        }
        address += get_record_size(requested_length);
    }

    if (!found)
        return FLASH_NOT_FOUND;
    if (requested_length > capacity)
        return FLASH_BUFFER_TOO_SMALL;

    if (HW_FLASH_READ(newest_address + sizeof(record), data, requested_length) ||
        calculate_crc((const uint8_t *)data, requested_length) != newest_crc
    ) return FLASH_ERROR;

    *length = requested_length;
    return FLASH_OK;
}

// Read a value after checking that required pointers are valid.
static inline flash_status flash_log_read(
    uint16_t logical_id, uint32_t size_code, void *data,
    uint16_t capacity, uint16_t *length
) {
    if (!length || (!data && capacity)) return FLASH_ERROR;
    return flash_log_read_fast(logical_id, size_code, data, capacity, length);
}

#endif // FUN_FLASH_LOG_H
