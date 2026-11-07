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

#ifndef FLASH_REC_MAP_LEN
// One byte per recent record; 0 disables the reverse lookup map.
#define FLASH_REC_MAP_LEN 32
#endif

#if FLASH_REC_MAP_LEN > 0 && \
    (FLASH_REC_MAP_LEN & (FLASH_REC_MAP_LEN - 1)) != 0
#error "FLASH_REC_MAP_LEN must be a power of two or zero"
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
#define FLASH_RECORD_MARKER_16 (((uint32_t)FLASH_RECORD_COMMIT << 16) | 0xBA10u)

#define FLASH_BLOCK_HEADER_COMMIT 0xB10CCAFEu


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
// Size: 8 bytes, stored at the start of an erase block.
typedef struct {
    uint32_t generation; // Increases when GC installs a copied block.
    uint32_t commit;     // FLASH_BLOCK_HEADER_COMMIT when the block copy is complete.
} flash_block_header;

typedef struct {
    uint32_t next_address;  // Address where the next record will be written.
    uint32_t active_block;  // Block selected by its committed generation header.
    uint32_t generation;
#if FLASH_REC_MAP_LEN > 0
    uint32_t map_end;       // End address of the newest mapped record.
    uint16_t map_count;
    uint16_t map_next;
    uint8_t record_map[FLASH_REC_MAP_LEN];
#endif
} flash_log;

// Hardware interface; implement these functions for the target flash device.
int HW_FLASH_READ(uint32_t address, void *dst, size_t length);
int HW_FLASH_WRITE(uint32_t address, const void *src, size_t length);
uint32_t HW_ERASE_BLOCK(uint32_t address);

// Check that a program operation stays within a physical page, when required.
#define FLASH_CRC_INITIAL 0xFFFFu
// Calculate the finalized CRC-16/MODBUS (reflected polynomial 0xA001).
static inline uint16_t flash_crc_calculate(const uint8_t *bytes, size_t length) {
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

static inline uint16_t flash_record_len(uint32_t marker) {
    switch (marker) {
    case FLASH_RECORD_MARKER_4:  return 4;
    case FLASH_RECORD_MARKER_8:  return 8;
    case FLASH_RECORD_MARKER_16: return 16;
    default: return 0;
    }
}

#if FLASH_REC_MAP_LEN > 0
// Keep a size code and six-bit ID hint for each of the newest contiguous records.
static inline void flash_map_store(
    flash_log *log, uint16_t logical_id, uint32_t address, uint16_t size
) {
    // If this record doesn't follow the previous one in flash, discard the map.
    if (log->map_count && address != log->map_end) {
        log->map_count = 0;
        log->map_next = 0;
    }

    //! Store a six-bit ID hint and a two-bit code for the payload size.
    const uint8_t id_hint = (uint8_t)((logical_id ^ (logical_id >> 6) ^
                                       (logical_id >> 12)) & 0x3fu);
    const uint8_t size_code = size == 4 ? 0 : size == 8 ? 1 : 2;
    log->record_map[log->map_next] = (uint8_t)((id_hint << 2) | size_code);

    //! Advance the circular map slot and cap its count at its capacity.
    log->map_next = (log->map_next + 1u) & (FLASH_REC_MAP_LEN - 1u);
    if (log->map_count < FLASH_REC_MAP_LEN)
        ++log->map_count;

    //! Remember where the next contiguous record should begin.
    log->map_end = address + ((sizeof(flash_record) + size + FLASH_ALIGNMENT - 1u) &
                              ~((uint32_t)FLASH_ALIGNMENT - 1u));
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

static inline flash_status block_header_write(
    uint32_t address, uint32_t generation
) {
    const flash_block_header header = {
        .generation = generation,
        .commit = FLASH_BLOCK_HEADER_COMMIT
    };
    if (HW_FLASH_WRITE(address, &header.generation, sizeof(header.generation)) ||
        HW_FLASH_WRITE(address + offsetof(flash_block_header, commit),
                       &header.commit, sizeof(header.commit)))
        return FLASH_ERROR;
    return FLASH_OK;
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

        // Signed subtraction handles generation wraparound when blocks are compared.
        if (!found || (int32_t)(header.generation - log->generation) > 0) {
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
        if (block_header_write(log->active_block, log->generation) != FLASH_OK)
            return FLASH_ERROR;
    }

    flash_record record;
    const uint32_t block_end = log->active_block + FLASH_BLOCK_SIZE;
    log->next_address = log->active_block + sizeof(flash_block_header);
#if FLASH_REC_MAP_LEN > 0
    log->map_count = 0;
    log->map_next = 0;
    log->map_end = log->next_address;
#endif

    //! Scan records in the selected block until the first unused slot.
    while (log->next_address + sizeof(record) <= block_end) {
        if (HW_FLASH_READ(log->next_address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint16_t record_length = flash_record_len(record.marker);

        if (!record_length) {
            if (flash_record_is_erased(&record)) {
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

        uint8_t crc_chunk[16];
        uint32_t crc_address = log->next_address + sizeof(record);
        uint16_t crc_remaining = record_length;

        //! Payloads are at most 16 bytes, so read and checksum the payload at once.
        if (HW_FLASH_READ(crc_address, crc_chunk, crc_remaining))
            return FLASH_ERROR;

        if (flash_crc_calculate(crc_chunk, crc_remaining) != record.crc) {
            log->next_address = block_end;
            break;
        }

#if FLASH_REC_MAP_LEN > 0
        flash_map_store(log, record.logical_id, log->next_address, record_length);
#endif

        log->next_address += record_size;
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
        .crc = flash_crc_calculate(data, length)
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
    if (HW_ERASE_BLOCK(new_block))
        return FLASH_ERROR;

    // Reserve the destination header; records are copied after it.
    uint32_t write_address = new_block + sizeof(flash_block_header);
    const uint32_t new_end = new_block + FLASH_BLOCK_SIZE;
    uint32_t address = old_block + sizeof(flash_block_header);
    flash_record record;
    uint8_t payload[16];
    // Keep the source scan bound fixed while the active log state is unchanged.
    const uint32_t old_end = log->next_address;

    // Walk the old log in append order.
    while (address + sizeof(record) <= old_end) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;

        const uint32_t src_remaining = old_end - address;
        const uint16_t payload_len = flash_record_len(record.marker);

        // Ensure the payload fits after the header.
        if (!payload_len || payload_len > src_remaining - sizeof(record))
            break;

        // Stop if the full record exceeds the remaining source bytes.
        const uint32_t record_size = flash_record_size(payload_len);
        if (record_size > src_remaining)
            break;

        // True when a later record has the same ID, so this older value can be skipped.
        bool has_newer_record = false;
        uint32_t later_address = address + record_size;

        // Search later records for the same ID; if found, copy that newer record instead.
        while (later_address + sizeof(record) <= old_end) {
            flash_record candidate;
            if (HW_FLASH_READ(later_address, &candidate, sizeof(candidate)))
                return FLASH_ERROR;

            const uint16_t later_length = flash_record_len(candidate.marker);
            if (!later_length || later_length > old_end - later_address - sizeof(candidate))
                break;
            if (candidate.logical_id == record.logical_id) {
                has_newer_record = true;
                break;
            }
            later_address += flash_record_size(later_length);
        }

        if (!has_newer_record) {
            // Check destination capacity before reading the source payload.
            if (write_address + record_size > new_end)
                return FLASH_FULL;

            // Validate the source, then append the copy with its marker last.
            if (HW_FLASH_READ(address + sizeof(record), payload, payload_len) ||
                flash_crc_calculate(payload, payload_len) != record.crc ||
                flash_write_at(write_address, record.logical_id, payload,
                                      payload_len, record.marker) != FLASH_OK
            ) return FLASH_ERROR;

            // Advance the destination cursor to the next free record position.
            write_address += record_size;
        }
        // Advance through the source log whether this record was copied or skipped.
        address += record_size;
    }

    // Keep this after the copy loop: startup accepts a block only after this commit.
    const uint32_t new_generation = log->generation + 1u;
    if (block_header_write(new_block, new_generation) != FLASH_OK)
        return FLASH_ERROR;

    // The new copy is committed. Switch to it before erasing the old block.
    log->active_block = new_block;
    log->generation = new_generation;
    log->next_address = write_address;

#if FLASH_REC_MAP_LEN > 0
    // Rebuild the RAM reverse map so it points into the newly active block.
    log->map_count = 0;
    log->map_next = 0;
    log->map_end = new_block + sizeof(flash_block_header);
    address = log->map_end;

    // Read headers only while a full header remains in the copied records.
    while (address + sizeof(record) <= log->next_address) {
        if (HW_FLASH_READ(address, &record, sizeof(record)))
            return FLASH_ERROR;
        const uint16_t length = flash_record_len(record.marker);
        if (!length) break;

        flash_map_store(log, record.logical_id, address, length);
        address += flash_record_size(length);
    }
#endif

    // The committed destination is recoverable now; erase the old copy last.
    if (HW_ERASE_BLOCK(old_block))
        return FLASH_ERROR;
    return FLASH_OK;
}

// Append one value, committing the record only after its header and payload are written.
static inline flash_status flash_log_write(
    flash_log *log, uint16_t logical_id, const void *data, uint32_t marker
) {
    if (!log || !data) return FLASH_ERROR;

    const uint16_t length = flash_record_len(marker);
    if (!length)
        return FLASH_RECORD_INVALID_SIZE;

    // Equivalent to ((size + FLASH_ALIGNMENT - 1) / FLASH_ALIGNMENT) * FLASH_ALIGNMENT.
    const uint32_t total = (sizeof(flash_record) + length + FLASH_ALIGNMENT - 1u) &
                            ~((uint32_t)FLASH_ALIGNMENT - 1u);
    if (total > FLASH_BLOCK_SIZE)
        return FLASH_RECORD_TOO_LARGE;

    if (log->next_address + total > log->active_block + FLASH_BLOCK_SIZE) {
        const flash_status gc_status = flash_log_gc(log);
        if (gc_status != FLASH_OK)
            return gc_status;
        if (log->next_address + total > log->active_block + FLASH_BLOCK_SIZE)
            return FLASH_FULL;
    }

    if (flash_write_at(log->next_address, logical_id, (const uint8_t *)data,
                              length, marker) != FLASH_OK) {
        log->next_address = log->active_block + FLASH_BLOCK_SIZE;
        return FLASH_ERROR;
    }

    log->next_address += total;
#if FLASH_REC_MAP_LEN > 0
    flash_map_store(log, logical_id, log->next_address - total, length);
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
#if FLASH_REC_MAP_LEN > 0
    // Start just past the newest mapped record and walk toward older records.
    uint32_t candidate_addr = log->map_end;
    // Each map byte packs a six-bit ID hint (which can collide) and a two-bit payload-size code.
    const uint8_t id_hint = (uint8_t)((logical_id ^ (logical_id >> 6) ^
                                       (logical_id >> 12)) & 0x3fu);

    // map_next changes as records are added; it points to the next slot to write.
    // Walk backward from the latest entry, wrapping around the ring as needed.
    const uint16_t map_count = log->map_count;

    for (uint16_t n = 0; n < map_count; ++n) {
        // The map is a ring; map_next points to the next slot to overwrite.
        const uint16_t index = (log->map_next - 1u - n) & (FLASH_REC_MAP_LEN - 1u);

        //! The map entry contains a six-bit ID hint and a two-bit payload-size code.
        const uint8_t map_entry = log->record_map[index];
        //! The low two bits encode payload sizes 4, 8, and 16.
        const uint16_t payload_len = (uint16_t)(4u << (map_entry & 3u));
        //! Move back to this record's start so the next iteration can check the older record.
        candidate_addr -= (sizeof(flash_record) + payload_len + FLASH_ALIGNMENT - 1u) &
                       ~((uint32_t)FLASH_ALIGNMENT - 1u);

        //! Skip this entry if its ID hint doesn't match; check older entries.
        if ((map_entry >> 2) != id_hint)
            continue;

        //! Read the header first so a hint collision needs no payload read.
        flash_record candidate;
        if (HW_FLASH_READ(candidate_addr, &candidate, sizeof(candidate)))
            return FLASH_ERROR;

        //! Confirm the full header before using this payload.
        if (candidate.logical_id != logical_id ||
            flash_record_len(candidate.marker) != payload_len
        ) continue;

        //! confirm the full ID before reporting that this matching record needs a larger buffer.
        if (payload_len > capacity)
            return FLASH_BUFFER_TOO_SMALL;

        //! Read the matched record's payload into the caller's buffer.
        if (HW_FLASH_READ(candidate_addr + sizeof(candidate), data, payload_len))
            return FLASH_ERROR;
        if (flash_crc_calculate((const uint8_t *)data, payload_len) != candidate.crc)
            return FLASH_ERROR;
        *length = payload_len;
        return FLASH_OK;
    }
#endif

    // An ID older than the mapped range still needs the full flash scan.
    // Scan records and remember the newest matching logical ID.
    uint32_t address = log->active_block + sizeof(flash_block_header);
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

        const uint16_t record_length = flash_record_len(record.marker);
        if (!record_length) break;
        if (record_length > log->next_address - address - sizeof(record)) {
            break;
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
        address += record_size;
    }

    if (!found) return FLASH_NOT_FOUND;
    if (new_length > capacity)
        return FLASH_BUFFER_TOO_SMALL;

    //! Read the selected payload and verify its CRC before returning it.
    if ((new_length && HW_FLASH_READ(new_address + sizeof(record), data, new_length)) ||
        flash_crc_calculate((const uint8_t *)data, new_length) != new_crc
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
