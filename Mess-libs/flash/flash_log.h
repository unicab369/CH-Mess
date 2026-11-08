#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Each block starts with a 4-byte header [generation: 2 bytes | size-class commit: 2 bytes].
// A packed tag table follows: one 4-bit logical-ID tag for each record slot.
// In each 32-bit word, the newest record's tag is in bits 3-0; the previous
// record's tag is in bits 7-4. The record area follows the table.
// Each record has an 8-byte header [CRC: 2 bytes | logical_id: 2 bytes | marker: 4 bytes],
// then its payload and alignment padding.
// GC copies the newest value for each (logical ID, size class) into a shared spare block.
//
// [erase_block - 4KB (configurable)]
//   [block_header: generation 2B | size-class commit 2B]
//   [tag_table: 8 x 4-bit logical-ID tags per 32-bit word, newest tag in low bits]
//   [alignment padding]
//   4B_class: [record_header 8B [payload 4B] ... (12B stride)
//   8B_class: [record_header 8B [payload 8B] ... (16B stride)
//
// Capacity for the default 4KB block and 4B alignment, including tag table:
// | Payload | Block header | Tag table | Record header | Record size | Rec/block | Unused space |
// |---------|--------------|-----------|---------------|-------------|-----------|--------------|
// | 4 bytes | 4 bytes      | 164 bytes | 8 bytes       | 12 bytes    | 327       | 4 bytes      |
// | 8 bytes | 4 bytes      | 124 bytes | 8 bytes       | 16 bytes    | 248       | none         |
//
// In each group of records 0-7, record 7 (newest) is stored in bits 3-0,
// record 6 in bits 7-4, continuing through record 0 (oldest) in bits 31-28.
// Each tag is the logical ID's low 4 bits: tag = logical_id & 0x0F.
// Example records, oldest to newest:
//   index 0: ID 0x1207 -> tag 7     index 4: ID 0x1243 -> tag 3
//   index 1: ID 0x1216 -> tag 6     index 5: ID 0x1252 -> tag 2
//   index 2: ID 0x1225 -> tag 5     index 6: ID 0x1261 -> tag 1
//   index 3: ID 0x1234 -> tag 4     index 7: ID 0x1270 -> tag 0

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

// These version-3 values commit the block, identify its payload size, and
// distinguish newest-first tag packing from the previous block format.
#define FLASH_BLOCK_HEADER_COMMIT_4 0xBB94u
#define FLASH_BLOCK_HEADER_COMMIT_8 0xBB98u


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

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t record_end;    // End of committed records, even if a damaged tail fills the block.
    uint32_t active_block;  // Block selected by its committed generation header.
    uint32_t records_start; // After this class's tag table and alignment padding.
    uint32_t pending_tags;  // Tags for the incomplete group of up to eight records.
    uint16_t generation;
    uint16_t record_count;  // Number of committed records in the active block.
    bool tag_map_valid;     // False means reads must scan full record headers.
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

static inline uint32_t get_record_size(uint16_t payload_length) {
    return (sizeof(flash_record) + payload_length + FLASH_ALIGNMENT - 1u) &
           ~((uint32_t)FLASH_ALIGNMENT - 1u);
}

// Reserve one 32-bit tag word for each group of eight record slots.
static inline uint16_t max_records_for_length(uint16_t payload_length) {
    const uint32_t record_size = get_record_size(payload_length);
    uint16_t count = 0;

    while (count < UINT16_MAX) {
        const uint32_t next_count = (uint32_t)count + 1u;
        const uint32_t records_offset = (sizeof(flash_block_header) +
                                    ((next_count + 7u) / 8u) * sizeof(uint32_t) +
                                    FLASH_ALIGNMENT - 1u) & ~((uint32_t)FLASH_ALIGNMENT - 1u);
        if (records_offset + next_count * record_size > FLASH_BLOCK_SIZE)
            break;
        ++count;
    }
    return count;
}

static inline uint32_t records_start_for_length(uint32_t block, uint16_t payload_length) {
    const uint32_t tag_words = ((uint32_t)max_records_for_length(payload_length) + 7u) / 8u;
    return block + ((sizeof(flash_block_header) + tag_words * sizeof(uint32_t) +
                    FLASH_ALIGNMENT - 1u) & ~((uint32_t)FLASH_ALIGNMENT - 1u));
}

// Rebuild one class's append cursor by scanning its committed record headers.
static inline flash_status log_pool_scan(log_pool_state *pool, uint32_t size_code) {
    const uint16_t expected_length = get_record_len(size_code);
    const uint32_t block_end = pool->active_block + FLASH_BLOCK_SIZE;
    const uint16_t max_records = max_records_for_length(expected_length);
    pool->records_start = records_start_for_length(pool->active_block, expected_length);
    pool->next_address = pool->records_start;
    pool->record_end = pool->next_address;
    pool->record_count = 0;
    pool->pending_tags = 0xFFFFFFFFu;
    pool->tag_map_valid = true;
    flash_record record;

    while (pool->record_count < max_records &&
           pool->next_address + sizeof(record) <= block_end
    ) {
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

        const uint16_t record_index = pool->record_count;
        // Reverse nibble order so backward reads consume the low nibble first.
        const uint8_t tag_shift = (uint8_t)((0b0111u - (record_index & 0b0111u)) << 2);
        // Empty nibbles start as 1s, so write this tag by clearing its zero bits.
        pool->pending_tags &= ~((uint32_t)(~record.logical_id & 0b1111u) << tag_shift);
        pool->next_address += record_size;
        pool->record_end = pool->next_address;
        ++pool->record_count;

        // A tag word is committed only after all eight records it describes.
        // A mismatch disables the accelerator; full-header reads remain correct.
        if (!(pool->record_count & 0b0111u)) {
            uint32_t stored_tags;
            if (HW_FLASH_READ(pool->active_block + sizeof(flash_block_header) +
                              (uint32_t)(record_index >> 3) * sizeof(stored_tags),
                              &stored_tags, sizeof(stored_tags)) ||
                (stored_tags != pool->pending_tags &&
                 !(pool->pending_tags == 0xFFFFFFFFu && stored_tags == FLASH_ERASED_WORD))
            ) {
                pool->tag_map_valid = false;
            }

            pool->pending_tags = 0xFFFFFFFFu;
        }
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
         log_pool_scan(&flash_log_pools[FLASH_LOG_POOL_4], FLASH_RECORD_MARKER_4) != FLASH_OK) ||
        (flash_log_pools[FLASH_LOG_POOL_8].active_block &&
         log_pool_scan(&flash_log_pools[FLASH_LOG_POOL_8], FLASH_RECORD_MARKER_8) != FLASH_OK)
    ) return FLASH_ERROR;

    return FLASH_OK;
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

// GC steps:
// 1. Find the next spare block in circular order, skipping both active blocks,
//    and erase it.
// 2. Copy only the newest record for each ID from this size class into the spare.
// 3. Commit the spare's header only after all record copies succeed.
// 4. Erase the old active block; it becomes the spare for a later GC.
// With more than two blocks, starting each search after the old block rotates
// this class through available blocks; the other class's active block is skipped.
static inline flash_status flash_log_gc(log_pool_state *pool, uint32_t size_code) {
    uint32_t new_block = 0;
    bool found_spare = false;
    const uint32_t old_block = pool->active_block;
    const uint32_t old_index = (old_block - FLASH_BASE_ADDR) / FLASH_BLOCK_SIZE;

    // Start after the source block and search circularly. After each GC the
    // source becomes spare, so this advances the active block through the
    // available blocks instead of repeatedly choosing the lowest free address.
    for (uint32_t step = 1; step < FLASH_BLOCK_COUNT; ++step) {
        uint32_t candidate_index = old_index + step;
        if (candidate_index >= FLASH_BLOCK_COUNT)
            candidate_index -= FLASH_BLOCK_COUNT;

        const uint32_t candidate = FLASH_BASE_ADDR + candidate_index * FLASH_BLOCK_SIZE;
        if (candidate != flash_log_pools[FLASH_LOG_POOL_4].active_block &&
            candidate != flash_log_pools[FLASH_LOG_POOL_8].active_block
        ) {
            new_block = candidate;
            found_spare = true;
            break;
        }
    }
    if (!found_spare)
        return FLASH_FULL;
    if (HW_ERASE_BLOCK(new_block))
        return FLASH_ERROR;

    uint32_t write_address = records_start_for_length(new_block, get_record_len(size_code));
    const uint32_t new_end = new_block + FLASH_BLOCK_SIZE;
    uint32_t address = pool->records_start;
    const uint32_t old_end = pool->record_end;
    const uint16_t old_count = pool->record_count;
    const uint16_t payload_len = get_record_len(size_code);
    const uint32_t record_size = get_record_size(payload_len);
    uint16_t copied_count = 0;
    uint32_t copied_tags = 0xFFFFFFFFu;
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

            const uint8_t tag_shift = (uint8_t)((0b0111u - (copied_count & 0b0111u)) << 2);
            copied_tags &= ~((uint32_t)(~record.logical_id & 0b1111u) << tag_shift);
            write_address += record_size;
            ++copied_count;

            // Write each packed tag word once, after all eight destination records commit.
            if (!(copied_count & 0b0111u)) {
                if (copied_tags != 0xFFFFFFFFu &&
                    HW_FLASH_WRITE(new_block + sizeof(flash_block_header) +
                                   (uint32_t)((copied_count >> 3) - 1u) * sizeof(copied_tags),
                                   &copied_tags, sizeof(copied_tags))
                ) return FLASH_ERROR;
                copied_tags = 0xFFFFFFFFu;
            }
        }
        address += record_size;
    }

    // Commit the size class and generation after all copied records are complete.
    const uint16_t new_generation = (uint16_t)(pool->generation + 1u);
    if (block_header_write(new_block, new_generation, size_code) != FLASH_OK)
        return FLASH_ERROR;

    pool->active_block = new_block;
    pool->generation = new_generation;
    pool->records_start = records_start_for_length(new_block, payload_len);
    pool->next_address = write_address;
    pool->record_end = write_address;
    pool->record_count = copied_count;
    pool->pending_tags = copied_tags;
    pool->tag_map_valid = true;
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
            pool->records_start = records_start_for_length(block, length);
            pool->next_address = pool->records_start;
            pool->record_end = pool->next_address;
            pool->record_count = 0;
            pool->pending_tags = 0xFFFFFFFFu;
            pool->tag_map_valid = true;
            break;
        }
    }

    const uint32_t record_size = get_record_size(length);
    if (record_size > FLASH_BLOCK_SIZE - (pool->records_start - pool->active_block))
        return FLASH_RECORD_TOO_LARGE;

    if (pool->next_address + record_size > pool->active_block + FLASH_BLOCK_SIZE) {
        const flash_status status = flash_log_gc(pool, size_code);

        if (status != FLASH_OK)
            return status;
        // GC may change active_block and next_address; check whether the new block fits.
        if (pool->next_address + record_size > pool->active_block + FLASH_BLOCK_SIZE)
            return FLASH_FULL;
    }

    if (store_record_at(pool->next_address, logical_id, data, length, size_code) != FLASH_OK) {
        pool->next_address = pool->active_block + FLASH_BLOCK_SIZE;
        return FLASH_ERROR;
    }

    // Accumulate the tag in RAM. Flash stores each tag word once, after all eight
    // corresponding records are committed, so a reset cannot leave a half-updated word.
    const uint16_t record_index = pool->record_count;
    // Reverse nibble order so backward reads consume the low nibble first.
    const uint8_t tag_shift = (uint8_t)((0b0111u - (record_index & 0b0111u)) << 2);
    pool->pending_tags &= ~((uint32_t)(~logical_id & 0b1111u) << tag_shift);
    pool->next_address += record_size;
    pool->record_end = pool->next_address;
    ++pool->record_count;

    if (!(pool->record_count & 0b0111u)) {
        if (pool->tag_map_valid &&
            HW_FLASH_WRITE(pool->active_block + sizeof(flash_block_header) +
                           (uint32_t)(record_index >> 3) * sizeof(pool->pending_tags),
                           &pool->pending_tags, sizeof(pool->pending_tags))
        ) pool->tag_map_valid = false;
        pool->pending_tags = 0xFFFFFFFFu;
    }
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

    const uint32_t tag_base = pool->active_block + sizeof(flash_block_header);
    const uint16_t complete_tag_groups = pool->record_count >> 3;
    uint16_t cached_tag_group = 0;
    uint32_t cached_tags = 0;
    bool tag_group_cached = false;
    const uint8_t wanted_tag = (uint8_t)(logical_id & 0b1111u);

    // Newer records are at higher addresses. Scan backward so the first matching
    // ID is the current value. Complete groups use packed tags to skip full headers;
    // the newest incomplete group and any untrusted map use full-header checks.
    uint32_t address = pool->record_end;
    const uint32_t records_start = pool->records_start;
    const uint32_t record_size = get_record_size(requested_length);
    uint16_t record_index = pool->record_count;

    while (address > records_start) {
        if (address - records_start < record_size)
            return FLASH_ERROR;
        address -= record_size;

        if (!record_index) return FLASH_ERROR;
        --record_index;

        const uint16_t tag_group = record_index >> 3;
        bool has_tag = pool->tag_map_valid && tag_group < complete_tag_groups;
        uint8_t record_tag = 0;

        //! 1. Check the tag first so mismatches skip the full record-header read below.
        if (has_tag) {
            if (!tag_group_cached || cached_tag_group != tag_group) {
                if (HW_FLASH_READ(tag_base + (uint32_t)tag_group * sizeof(cached_tags),
                                  &cached_tags, sizeof(cached_tags))
                ) { // A failed hint read must not hide a valid record.
                    pool->tag_map_valid = false;
                    address = pool->record_end;
                    record_index = pool->record_count;
                    tag_group_cached = false;
                    continue;
                }
                cached_tag_group = tag_group;
                tag_group_cached = true;
            }

            if (cached_tags != FLASH_ERASED_WORD) {
                // Example: 0x76543210 stores tags 0 through 7 from newest to oldest.
                // The low nibble gives tag 0; shifting right makes tag 1 the new low nibble.
                record_tag = cached_tags & 0b1111u;
                cached_tags >>= 4;
                if (record_tag != wanted_tag)
                    continue;
            }
            else {
                // An all-ones tag group is unchanged erased flash; check its headers.
                has_tag = false;
            }
        }

        //! 2. Read and validate the header before using its ID, marker, or CRC.
        flash_record record;
        if (HW_FLASH_READ(address, &record, sizeof(record)) ||
            get_record_len(record.marker) != requested_length
        ) return FLASH_ERROR;

        //! 3. Check the tag against the full ID before trusting it to skip records.
        if (has_tag && (record.logical_id & 0b1111u) != record_tag) {
            // Metadata disagrees with the record. Restart without using the hint.
            pool->tag_map_valid = false;
            address = pool->record_end;
            record_index = pool->record_count;
            tag_group_cached = false;
            continue;
        }

        //! 4. The full ID distinguishes a real match from a tag collision.
        if (record.logical_id == logical_id) {
            // Only a matching record needs a capacity check and payload read.
            if (requested_length > capacity)
                return FLASH_BUFFER_TOO_SMALL;

            if (HW_FLASH_READ(address + sizeof(record), data, requested_length) ||
                calculate_crc((const uint8_t *)data, requested_length) != record.crc
            ) return FLASH_ERROR;

            *length = requested_length;
            return FLASH_OK;
        }
    }

    return FLASH_NOT_FOUND;
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
