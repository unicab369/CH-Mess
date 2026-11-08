#ifndef FUN_FLASH_LOG_H
#define FUN_FLASH_LOG_H

// Flash layout: each erase block starts with a 4-byte block header
// [generation: 2 bytes | commit: 2 bytes], followed by a packed record bitmap.
// The active block then holds append-only records. Each record starts with an 8-byte
// header [CRC: 2 bytes | logical_id: 2 bytes | marker: 4 bytes], followed by its
// payload and alignment padding.
// GC copies the newest record for each ID into another block before committing it.
//
// [erase block - 4KB (configurable)]
//   [block header: generation (2) | commit (2)]
//   [record bitmap: one byte per record: live (1) | length (1) | ID tag (6)]
//   [record header: CRC (2) | logical_id (2) | marker (4)] [payload] [padding]
//   [record header: CRC (2) | logical_id (2) | marker (4)] [payload] [padding] ...

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
#define FLASH_RECORD_MARKER_4  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA04u)
#define FLASH_RECORD_MARKER_8  (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA08u)

// New marker distinguishes the bitmap layout from older block formats.
#define FLASH_BLOCK_HEADER_COMMIT 0xBA8Bu


// Tracks erase usage and the next write position for one flash block.
typedef struct {
    // Range: 0–65,535 erases; rebase all counts before incrementing at max.
    uint16_t erase_count;  // Number of times this block has been erased.
    uint16_t write_offset; // Offset of the next writable byte in the block.
    bool     erased;       // True if the block is erased and ready to use.
} flash_block;

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
    uint16_t commit;     // FLASH_BLOCK_HEADER_COMMIT when the block copy is complete.
} flash_block_header;

// Each record uses six ID-tag bits, one size bit (4 or 8 bytes), and one live bit.
// This bitmap format therefore supports only 4-byte and 8-byte payload records.
#define FLASH_BITMAP_TAG_BITS       6u
#define FLASH_BITMAP_TAG_MASK       ((1u << FLASH_BITMAP_TAG_BITS) - 1u)
#define FLASH_BITMAP_LIVE_BIT       0u
#define FLASH_BITMAP_LENGTH_BIT     1u
#define FLASH_BITMAP_TAG_SHIFT      2u
#define FLASH_BITMAP_ENTRY_BITS     (FLASH_BITMAP_TAG_BITS + 2u)
#define FLASH_BITMAP_MIN_REC_SIZE  ((sizeof(flash_record) + 4u + FLASH_ALIGNMENT - 1u) & \
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u))
#define FLASH_BITMAP_MAX_RECORDS   (((FLASH_BLOCK_SIZE - sizeof(flash_block_header)) * 8u) / \
                                    (FLASH_BITMAP_MIN_REC_SIZE * 8u + FLASH_BITMAP_ENTRY_BITS))
#define FLASH_BITMAP_RAW_BYTES     ((FLASH_BITMAP_MAX_RECORDS * FLASH_BITMAP_ENTRY_BITS + 7u) / 8u)
#define FLASH_BITMAP_ALIGNMENT     ((FLASH_ALIGNMENT < 4u) ? 4u : FLASH_ALIGNMENT)
#define FLASH_BITMAP_BYTES         (((sizeof(flash_block_header) + FLASH_BITMAP_RAW_BYTES + \
                                      FLASH_BITMAP_ALIGNMENT - 1u) & \
                                     ~((uint32_t)FLASH_BITMAP_ALIGNMENT - 1u)) - \
                                    sizeof(flash_block_header))

// With a 4 KB block and 4-byte minimum payloads, these evaluate to 314 slots
// and a 316-byte bitmap; records then start at byte offset 320.

// Compile-time check: the header, bitmap, and minimum-size record slots must
// all fit inside one erase block; otherwise this typedef fails to compile.
typedef char flash_bitmap_must_fit_block[
    sizeof(flash_block_header) + FLASH_BITMAP_BYTES +
    FLASH_BITMAP_MAX_RECORDS * FLASH_BITMAP_MIN_REC_SIZE <= FLASH_BLOCK_SIZE ? 1 : -1
];

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t record_end;    // End of committed records, even if a damaged tail fills the block.
    uint32_t active_block;  // Block selected by its committed generation header.
    uint16_t generation;
    uint16_t record_count;  // Number of committed records in the active block.
    uint8_t bitmap_trusted; // False if bitmap metadata needs the slower header-scan path.
} flash_log;

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

static inline uint8_t get_id_tag(uint16_t logical_id) {
    const uint16_t mixed = logical_id ^ (logical_id >> 6) ^ (logical_id >> 12);
    return (uint8_t)(mixed & FLASH_BITMAP_TAG_MASK);
}

// CH5xx CodeFlash can read an erased word as a value other than 0xFFFFFFFF.
// Establish and verify the all-ones bitmap state before writing any records.
static inline int bitmap_init(uint32_t block) {
#if FLASH_ERASED_WORD == 0xFFFFFFFFu
    (void)block;
    return 0;
#else
    uint32_t fill[8];
    uint32_t check[8];
    for (size_t i = 0; i < sizeof(fill) / sizeof(fill[0]); ++i)
        fill[i] = 0xFFFFFFFFu;

    const uint32_t start = block + sizeof(flash_block_header);

    //! Write the bitmap as all 1s in chunks, then read each chunk back to verify it.
    for (uint32_t offset = 0; offset < FLASH_BITMAP_BYTES; offset += sizeof(fill)) {
        const size_t remaining = FLASH_BITMAP_BYTES - offset;
        const size_t count = remaining < sizeof(fill) ? remaining : sizeof(fill);

        if (HW_FLASH_WRITE(start + offset, fill, count) ||
            HW_FLASH_READ(start + offset, check, count)
        ) return -1;

        for (size_t i = 0; i < count / sizeof(check[0]); ++i)
            if (check[i] != 0xFFFFFFFFu)
                return -1;
    }
    return 0;
#endif
}

static inline int bitmap_write_word(uint32_t address, uint32_t word) {
    if (HW_FLASH_WRITE(address, &word, sizeof(word)))
        return -1;
#if FLASH_ERASED_WORD != 0xFFFFFFFFu
    uint32_t check;
    if (HW_FLASH_READ(address, &check, sizeof(check)) || check != word)
        return -1;
#endif
    return 0;
}

static inline int bitmap_store_record(
    uint32_t block, uint16_t index, uint16_t logical_id, uint16_t length
) {
    if (index >= FLASH_BITMAP_MAX_RECORDS || (length != 4u && length != 8u))
        return -1;
    const uint8_t entry = (uint8_t)((1u << FLASH_BITMAP_LIVE_BIT) |
                                    (length == 8u ? (1u << FLASH_BITMAP_LENGTH_BIT) : 0u) |
                                    (get_id_tag(logical_id) << FLASH_BITMAP_TAG_SHIFT));

    // Read the containing word and update only this record's bitmap byte.
    const uint32_t word_address = block + sizeof(flash_block_header) + (index & ~3u);
    const uint8_t shift = (uint8_t)((index & 3u) * 8u);
    uint32_t word;
    if (HW_FLASH_READ(word_address, &word, sizeof(word)))
        return -1;

    // Flash programming can clear bits, but cannot restore a cleared bit to one.
    const uint8_t current_entry = (uint8_t)(word >> shift);
    if ((current_entry & entry) != entry)
        return -1;

    const uint32_t updated = word & ~((uint32_t)(~entry & 0xffu) << shift);
    return updated == word ? 0 : bitmap_write_word(word_address, updated);
}

static inline flash_status block_header_write(
    uint32_t address, uint16_t generation
) {
    // Program an erased commit field first, then program the complete 4-byte
    // word. Both writes stay 4-byte aligned; the second write commits the block.
    const flash_block_header pending = {
        .generation = generation,
        .commit = 0xFFFFu
    };
    const flash_block_header committed = {
        .generation = generation,
        .commit = FLASH_BLOCK_HEADER_COMMIT
    };
    return (HW_FLASH_WRITE(address, &pending, sizeof(pending)) ||
            HW_FLASH_WRITE(address, &committed, sizeof(committed)))
            ? FLASH_ERROR : FLASH_OK;
}

// Select the newest committed block and find its next unused record slot.
static inline flash_status flash_log_init(flash_log *log) {
    if (!log)
        return FLASH_ERROR;

    flash_block_header header;
    bool found = false;

    //! Find the newest block whose header says its contents were committed.
    for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
        const uint32_t block = FLASH_BASE_ADDR + (uint32_t)i * FLASH_BLOCK_SIZE;

        if (HW_FLASH_READ(block, &header, sizeof(header)))
            return FLASH_ERROR;
        //! Ignore erased or incomplete headers, such as a GC interrupted before commit.
        if (header.commit != FLASH_BLOCK_HEADER_COMMIT)
            continue;

        // Signed subtraction handles 16-bit wraparound while valid block
        // generations remain fewer than 32768 increments apart.
        if (!found || (int16_t)(header.generation - log->generation) > 0) {
            log->active_block = block;
            log->generation = header.generation;
            found = true;
        }
    }
    if (!found) {
        // No committed block exists; erase the region and start a fresh format.
        for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
            const uint32_t block = FLASH_BASE_ADDR + (uint32_t)i * FLASH_BLOCK_SIZE;
            if (HW_ERASE_BLOCK(block))
                return FLASH_ERROR;
        }
        // Make the first block active. The helper writes its commit marker last.
        log->active_block = FLASH_BASE_ADDR;
        log->generation = 1;

        if (bitmap_init(log->active_block) ||
            block_header_write(log->active_block, log->generation) != FLASH_OK
        ) return FLASH_ERROR;
    }

    flash_record record;
    const uint32_t block_end = log->active_block + FLASH_BLOCK_SIZE;
    log->next_address = log->active_block + sizeof(flash_block_header) + FLASH_BITMAP_BYTES;
    log->record_end = log->next_address;
    log->record_count = 0;
    log->bitmap_trusted = 1;
    //! Scan committed records until the first unused slot.
    while (log->next_address + sizeof(record) <= block_end) {
        if (log->record_count >= FLASH_BITMAP_MAX_RECORDS) {
            log->next_address = block_end;
            break;
        }

        if (HW_FLASH_READ(log->next_address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t record_length = get_record_len(record.marker);

        if (!record_length) {
            // Check whether every header byte still has the flash's erased value.
            const uint8_t *record_bytes = (const uint8_t *)&record;
            bool record_erased = true;

            for (size_t i = 0; i < sizeof(record); ++i) {
                if (record_bytes[i] != (uint8_t)(FLASH_ERASED_WORD >> ((i & 3u) * 8u))) {
                    record_erased = false;
                    break;
                }
            }
            if (record_erased) {
                break;
            } else {
                // An incomplete record consumes the rest of this block.
                log->next_address = block_end;
                break;
            }
        }

        // Check whether this record fits before reading its payload for the CRC.
        if (record_length > block_end - log->next_address - sizeof(record)) {
            log->next_address = block_end;
            break;
        }

        // Check whether this record fits before reading its payload for the CRC.
        const uint32_t record_size = (sizeof(record) + record_length + FLASH_ALIGNMENT - 1u) &
                                    ~((uint32_t)FLASH_ALIGNMENT - 1u);
        if (record_size > block_end - log->next_address)
            break;

        // Calculate the bytes left between next_address and the end of this block.
        const uint32_t block_remaining = FLASH_BLOCK_SIZE -
                                ((log->next_address - log->active_block) & (FLASH_BLOCK_SIZE - 1u));
        if (record_size > block_remaining) {
            log->next_address = block_end;
            break;
        }

        uint8_t crc_chunk[8];
        uint32_t crc_address = log->next_address + sizeof(record);
        uint16_t crc_remaining = record_length;

        uint8_t bitmap_entry;
        if (log->record_count >= FLASH_BITMAP_MAX_RECORDS ||
            HW_FLASH_READ(log->active_block + sizeof(flash_block_header) + log->record_count,
                          &bitmap_entry, sizeof(bitmap_entry))
        )
            return FLASH_ERROR;

        if (((bitmap_entry >> FLASH_BITMAP_LENGTH_BIT) & 1u ? 8u : 4u) != record_length ||
            ((bitmap_entry >> FLASH_BITMAP_TAG_SHIFT) & FLASH_BITMAP_TAG_MASK) !=
                get_id_tag(record.logical_id)
        ) {
            // A committed record without matching bitmap metadata falls back to header scans.
            log->bitmap_trusted = 0;
        }

        // Outdated records cannot be returned, so their payload CRC need not be read at startup.
        if (!log->bitmap_trusted ||
            (bitmap_entry & (1u << FLASH_BITMAP_LIVE_BIT))) {
            //! Payloads are at most 8 bytes, so read and checksum the payload at once.
            if (HW_FLASH_READ(crc_address, crc_chunk, crc_remaining))
                return FLASH_ERROR;

            if (calculate_crc(crc_chunk, crc_remaining) != record.crc) {
                log->next_address = block_end;
                break;
            }
        }

        log->next_address += record_size;
        log->record_end = log->next_address;
        ++log->record_count;
    }

    return FLASH_OK;
}

static inline uint32_t flash_record_size(uint16_t payload_length) {
    return (sizeof(flash_record) + payload_length + FLASH_ALIGNMENT - 1u) &
           ~((uint32_t)FLASH_ALIGNMENT - 1u);
}

static inline flash_status flash_write_at(
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
                       sizeof(record.marker)))
        return FLASH_ERROR;
    return FLASH_OK;
}

// Copy only the newest record for each ID, then commit the destination block.
static inline flash_status flash_log_gc(flash_log *log) {
    // Rotate to the following erase block, wrapping at the end of the log region.
    const uint32_t old_block = log->active_block;
    uint32_t new_block = old_block + FLASH_BLOCK_SIZE;
    if (new_block >= FLASH_LOG_END)
        new_block = FLASH_BASE_ADDR;

    // The destination must be erased before any copied records are written.
    if (HW_ERASE_BLOCK(new_block) || bitmap_init(new_block))
        return FLASH_ERROR;

    // Reserve the block header and bitmap; copied records follow both.
    uint32_t write_address = new_block + sizeof(flash_block_header) + FLASH_BITMAP_BYTES;
    const uint32_t new_end = new_block + FLASH_BLOCK_SIZE;
    uint32_t address = old_block + sizeof(flash_block_header) + FLASH_BITMAP_BYTES;
    flash_record record;
    uint8_t payload[8];

    // Keep the source scan bound fixed while the active log state is unchanged.
    const uint32_t old_end = log->next_address;
    const uint16_t old_record_count = log->record_count;
    uint16_t source_index = 0;
    uint16_t copied_count = 0;
    bool src_bitmap_ok = log->bitmap_trusted != 0;

    // Walk the old log in append order.
    while (source_index < old_record_count && address + sizeof(record) <= old_end) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint32_t src_remaining = old_end - address;
        const uint16_t payload_len = get_record_len(record.marker);

        // Ensure the payload fits after the header.
        if (!payload_len || payload_len > src_remaining - sizeof(record))
            break;

        // Stop if the full record exceeds the remaining source bytes.
        const uint32_t record_size = flash_record_size(payload_len);
        if (record_size > src_remaining)
            break;

        bool outdated = false;
        uint8_t bitmap_entry = 0xffu;

        if (src_bitmap_ok) {
            if (source_index >= FLASH_BITMAP_MAX_RECORDS ||
                HW_FLASH_READ(old_block + sizeof(flash_block_header) + source_index,
                              &bitmap_entry, sizeof(bitmap_entry))
            )
                return FLASH_ERROR;
            if (((bitmap_entry >> FLASH_BITMAP_LENGTH_BIT) & 1u ? 8u : 4u) != payload_len ||
                ((bitmap_entry >> FLASH_BITMAP_TAG_SHIFT) & FLASH_BITMAP_TAG_MASK) !=
                    get_id_tag(record.logical_id)
            ) {
                // Do not trust a mismatched hint; use the full-ID fallback for this GC.
                src_bitmap_ok = false;
            } else {
                outdated = !(bitmap_entry & (1u << FLASH_BITMAP_LIVE_BIT));
            }
        }

        // If metadata is unavailable, preserve correctness by checking later full IDs.
        bool has_newer_record = false;
        // Outdated records need neither duplicate checks nor copying.
        if (!outdated) {
            if (!src_bitmap_ok) {
                uint16_t later_index = (uint16_t)(source_index + 1u);
                uint32_t later_addr = address + record_size;

                //! Search later records; a later record with the same ID replaces this one.
                while (later_index < old_record_count &&
                       later_addr + sizeof(record) <= old_end
                ) {
                    flash_record candidate;
                    if (HW_FLASH_READ(later_addr, &candidate, sizeof(candidate)))
                        return FLASH_ERROR;

                     // Stop if this later record has an invalid marker or extends past written data.
                    const uint16_t later_length = get_record_len(candidate.marker);
                    if (!later_length ||
                        later_length > old_end - later_addr - sizeof(candidate)
                    ) break;

                    //! A valid later record with this ID replaces the current record.
                    if (candidate.logical_id == record.logical_id) {
                        has_newer_record = true;
                        break;
                    }
                    later_addr += flash_record_size(later_length);
                    ++later_index;
                }
            }

            if (!has_newer_record) {
                // Check destination capacity before reading the source payload.
                if (write_address + record_size > new_end)
                    return FLASH_FULL;

                //! Validate the source, then append the copy with its marker last.
                if (HW_FLASH_READ(address + sizeof(record), payload, payload_len) ||
                    calculate_crc(payload, payload_len) != record.crc ||
                    flash_write_at(write_address, record.logical_id, payload,
                                  payload_len, record.marker) != FLASH_OK
                ) return FLASH_ERROR;

                if (bitmap_store_record(new_block, copied_count,
                                              record.logical_id, payload_len))
                    return FLASH_ERROR;

                // Advance the destination cursor to the next free record position.
                write_address += record_size;
                ++copied_count;
            }
        }
        // Advance through the source log whether this record was copied or skipped.
        address += record_size;
        ++source_index;
    }

    // Keep this after the copy loop: startup accepts a block only after this commit.
    const uint16_t new_generation = (uint16_t)(log->generation + 1u);
    if (block_header_write(new_block, new_generation) != FLASH_OK)
        return FLASH_ERROR;

    // The new copy is committed. Switch to it before erasing the old block.
    log->active_block = new_block;
    log->generation = new_generation;
    log->next_address = write_address;
    log->record_end = write_address;
    log->record_count = copied_count;
    log->bitmap_trusted = 1;

    // The committed destination is recoverable now; erase the old copy last.
    return HW_ERASE_BLOCK(old_block) ? FLASH_ERROR : FLASH_OK;
}

// Append one value, committing the record only after its header and payload are written.
static inline flash_status flash_log_write(
    flash_log *log, uint16_t logical_id, const void *data, uint32_t marker
) {
    if (!log || !data) return FLASH_ERROR;

    const uint16_t length = get_record_len(marker);
    if (!length)
        return FLASH_RECORD_INVALID_SIZE;

    // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
    const uint32_t total = (sizeof(flash_record) + length + FLASH_ALIGNMENT - 1u) &
                            ~((uint32_t)FLASH_ALIGNMENT - 1u);
    if (total > FLASH_BLOCK_SIZE)
        return FLASH_RECORD_TOO_LARGE;

    const uint32_t max_offset = FLASH_BLOCK_SIZE - total;

    //! Run GC if data space or bitmap slots are exhausted.
    bool no_room = log->next_address - log->active_block > max_offset;
    if (no_room || log->record_count >= FLASH_BITMAP_MAX_RECORDS) {
        const flash_status gc_status = flash_log_gc(log);
        if (gc_status != FLASH_OK)
            return gc_status;

        // GC changes the active block and next address, so check the new space again.
        no_room = log->next_address - log->active_block > max_offset;
        if (no_room || log->record_count >= FLASH_BITMAP_MAX_RECORDS)
            return FLASH_FULL;
    }

    if (flash_write_at(log->next_address, logical_id,
        (const uint8_t *)data, length, marker) != FLASH_OK
    ) {
        log->next_address = log->active_block + FLASH_BLOCK_SIZE;
        return FLASH_ERROR;
    }

    const uint16_t record_index = log->record_count;
    ++log->record_count;

    if (log->bitmap_trusted) {
        if (bitmap_store_record(log->active_block, record_index, logical_id, length)) {
            log->bitmap_trusted = 0;
        } else {
            // Scan older entries: the tag avoids reading most full record headers.
            const uint8_t tag = get_id_tag(logical_id);
            uint32_t old_address = log->active_block + sizeof(flash_block_header) + FLASH_BITMAP_BYTES;

            for (uint16_t i = 0; i < record_index; ++i) {
                uint8_t entry;
                if (i >= FLASH_BITMAP_MAX_RECORDS ||
                    HW_FLASH_READ(log->active_block + sizeof(flash_block_header) + i,
                                  &entry, sizeof(entry))) {
                    log->bitmap_trusted = 0;
                    break;
                }

                const uint16_t old_length = (entry & (1u << FLASH_BITMAP_LENGTH_BIT)) ? 8u : 4u;
                const bool is_live = (entry & (1u << FLASH_BITMAP_LIVE_BIT)) != 0;
                const bool tag_match = ((entry >> FLASH_BITMAP_TAG_SHIFT) & FLASH_BITMAP_TAG_MASK) == tag;

                //! Read the old header only if it is live and its tag matches.
                if (is_live && tag_match) {
                    flash_record old_record;

                    //! Validate the header before trusting its tag or changing its bitmap entry.
                    if (HW_FLASH_READ(old_address, &old_record, sizeof(old_record)) ||
                        get_record_len(old_record.marker) != old_length) {
                        log->bitmap_trusted = 0;
                        break;
                    }

                    if (old_record.logical_id == logical_id) {
                        // Clear this record's live bit in its containing 32-bit bitmap word.
                        const uint32_t word_address = log->active_block + sizeof(flash_block_header) +
                                                      (i & ~3u);
                        const uint32_t mask = 1u << ((i & 3u) * 8u + FLASH_BITMAP_LIVE_BIT);
                        uint32_t word;
                        if (i >= FLASH_BITMAP_MAX_RECORDS ||
                            HW_FLASH_READ(word_address, &word, sizeof(word))) {
                            log->bitmap_trusted = 0;
                            break;
                        }
                        if (word & mask) {
                            word &= ~mask;
                            if (bitmap_write_word(word_address, word)) {
                                log->bitmap_trusted = 0;
                                break;
                            }
                        }
                    } else if (get_id_tag(old_record.logical_id) != tag) {
                        // The header's tag disagrees with the bitmap hint; use full scans.
                        log->bitmap_trusted = 0;
                        break;
                    }
                }

                old_address += flash_record_size(old_length);
            }
        }
    }

    log->next_address += total;
    log->record_end = log->next_address;
    return FLASH_OK;
}

// Fast read for callers that guarantee valid pointers and arguments.
static inline flash_status flash_log_read_fast(
    flash_log *log, uint16_t logical_id, void *data,
    uint16_t capacity, uint16_t *length
) {
    // Search from the newest record backward; the bitmap skips obsolete records and most IDs.
    if (log->bitmap_trusted) {
        // Begin at the end of valid record data and one past its last bitmap entry.
        uint32_t address = log->record_end;
        uint16_t index = log->record_count;
        // Compute the requested ID's short tag for quick candidate filtering.
        const uint8_t tag = get_id_tag(logical_id);

        // Each pass examines one record, moving toward older entries.
        while (index) {
            uint8_t entry;
            // Select the next older record's bitmap entry.
            --index;
            if (index >= FLASH_BITMAP_MAX_RECORDS ||
                HW_FLASH_READ(log->active_block + sizeof(flash_block_header) + index,
                              &entry, sizeof(entry))
            )
                return FLASH_ERROR;

            //! The length bit tells us the record size so we can step backward.
            const uint16_t payload_len = (entry & (1u << FLASH_BITMAP_LENGTH_BIT)) ? 8u : 4u;
            address -= flash_record_size(payload_len);

            //! Skip obsolete entries and tag mismatches; check the next older record.
            const bool is_live = (entry & (1u << FLASH_BITMAP_LIVE_BIT)) != 0;
            const bool tag_matches = ((entry >> FLASH_BITMAP_TAG_SHIFT) & FLASH_BITMAP_TAG_MASK) == tag;
            if (!is_live || !tag_matches)
                continue;

            flash_record candidate;
            if (HW_FLASH_READ(address, &candidate, sizeof(candidate)))
                return FLASH_ERROR;

            //! 1. Validate bitmap length and tag against the record before treating it as a
            //    tag collision; inconsistent metadata requires the full header scan below.
            if (get_record_len(candidate.marker) != payload_len ||
                get_id_tag(candidate.logical_id) != tag
            ) {
                log->bitmap_trusted = 0;
                break;
            }

            //! 2. Compare the full ID only after the bitmap entry is known to be consistent.
            if (candidate.logical_id != logical_id)
                continue;

            //! 3. Check capacity only after the full ID matches, so a tag collision cannot
            //    report a buffer error for a different record.
            if (payload_len > capacity)
                return FLASH_BUFFER_TOO_SMALL;

            //! 4. Read the payload and verify its CRC before returning the value.
            if (HW_FLASH_READ(address + sizeof(candidate), data, payload_len) ||
                calculate_crc((const uint8_t *)data, payload_len) != candidate.crc
            ) return FLASH_ERROR;

            *length = payload_len;
            return FLASH_OK;
        }

        // If bitmap_trusted is true, and the search finds no matching ID, returns FLASH_NOT_FOUND.
        if (log->bitmap_trusted)
            return FLASH_NOT_FOUND;
    }

    uint32_t address =
        log->active_block + sizeof(flash_block_header) + FLASH_BITMAP_BYTES;
    uint32_t new_address = 0;
    uint16_t new_crc = 0;
    uint16_t new_length = 0;
    bool found = false;
    flash_record record;

    //! Scan in address order; the last matching record is the newest.
    // Stop before reading a header that extends beyond the written records.
    while (address + sizeof(record) <= log->next_address) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t record_length = get_record_len(record.marker);
        if (!record_length ||
            record_length > log->next_address - address - sizeof(record)
        ) break;

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
        address += record_size;
    }

    if (!found) return FLASH_NOT_FOUND;
    if (new_length > capacity)
        return FLASH_BUFFER_TOO_SMALL;

    //! Read the selected payload and verify its CRC before returning it.
    if ((new_length && HW_FLASH_READ(new_address + sizeof(record), data, new_length)) ||
        calculate_crc((const uint8_t *)data, new_length) != new_crc
    ) return FLASH_ERROR;

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
