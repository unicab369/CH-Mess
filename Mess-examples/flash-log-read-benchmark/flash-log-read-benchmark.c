#include <stdint.h>
#include <stdio.h>

#include "../../Mess-libs/flash/flash_log_ch5xx.h"

#define RECORDS_PER_CLASS 128u
#define BENCH_RUNS 128u
#define TAG4_WORDS_PER_CLASS (RECORDS_PER_CLASS / 8u)

volatile uint32_t flash_log_bench_read_calls;
volatile uint32_t flash_log_bench_read_bytes;
volatile uint8_t flash_log_bench_count_reads;
static volatile uint32_t benchmark_sink;
static uint32_t tag2_map_base[FLASH_LOG_POOL_COUNT];
static uint32_t tag8_map_base[FLASH_LOG_POOL_COUNT];
static uint32_t tag4_ram[FLASH_LOG_POOL_COUNT][TAG4_WORDS_PER_CLASS];
static volatile uint32_t scan_header_reads;
static uint32_t baseline_ticks[FLASH_LOG_POOL_COUNT][3];

typedef flash_status (*read_fn)(uint16_t, flash_pool_id, void *, uint16_t, uint16_t *);

static void print_text_column(const char *text, uint8_t width) {
    uint8_t length = 0;
    while (text[length])
        ++length;
    printf("%s", text);
    while (length++ < width)
        printf(" ");
}

static void print_number_column(uint32_t value, uint8_t width) {
    printf("%lu", (unsigned long)value);
    uint8_t digits = 1;
    for (uint32_t remaining = value; remaining >= 10u; remaining /= 10u)
        ++digits;
    while (digits++ < width)
        printf(" ");
}

static void print_table_header(void) {
    print_text_column("Pld", 6);
    print_text_column("Search", 9);
    print_text_column("Ticks", 9);
    print_text_column("Reads", 7);
    print_text_column("B/Search", 10);
    print_text_column("Headers", 9);
    print_text_column("%", 7);
    printf("\n");
}

// Baseline implementation: scan every full record header, newest to oldest.
__attribute__((noinline))
static flash_status read_full_scan(
    uint16_t logical_id, flash_pool_id pool_id, void *data,
    uint16_t capacity, uint16_t *length
) {
    if (pool_id >= FLASH_LOG_POOL_COUNT)
        return FLASH_RECORD_INVALID_SIZE;
    log_pool_state *pool = &flash_log_pools[pool_id];
    const uint16_t payload_len = pool_id == FLASH_LOG_POOL_4 ? 4u : 8u;
    if (!pool->active_block)
        return FLASH_NOT_FOUND;

    const uint32_t stride = get_record_size(payload_len);
    uint32_t address = pool->record_end;
    flash_record record;
    uint8_t payload[8];

    while (address > pool->records_start) {
        address -= stride;
        if (flash_log_bench_count_reads)
            ++scan_header_reads;
        if (HW_FLASH_READ(address, &record, sizeof(record)) ||
            get_record_len(record.marker) != payload_len
        ) return FLASH_ERROR;
        if (record.logical_id != logical_id)
            continue;
        if (payload_len > capacity)
            return FLASH_BUFFER_TOO_SMALL;
        if (HW_FLASH_READ(address + sizeof(record), payload, payload_len) ||
            calculate_crc(payload, payload_len) != record.crc
        ) return FLASH_ERROR;

        __builtin_memcpy(data, payload, payload_len);
        *length = payload_len;
        return FLASH_OK;
    }
    return FLASH_NOT_FOUND;
}

// Benchmark scanner shared by 4-bit and 8-bit tags. Only the tag width and table
// location differ; both variants use the same candidate checks and CRC path.
__attribute__((noinline))
static flash_status read_tag_scan(
    uint16_t logical_id, flash_pool_id pool_id, void *data,
    uint16_t capacity, uint16_t *length, uint8_t tag_bits,
    const uint32_t *ram_tag_words
) {
    if (pool_id >= FLASH_LOG_POOL_COUNT)
        return FLASH_RECORD_INVALID_SIZE;
    log_pool_state *pool = &flash_log_pools[pool_id];
    const uint16_t payload_len = pool_id == FLASH_LOG_POOL_4 ? 4u : 8u;
    if (!pool->active_block)
        return FLASH_NOT_FOUND;

    const uint8_t entries_per_word = (uint8_t)(32u / tag_bits);
    const uint8_t tag_group_shift = tag_bits == 2u ? 4u : tag_bits == 4u ? 3u : 2u;
    const uint8_t tag_position_mask = entries_per_word - 1u;
    const uint8_t tag_order_xor = tag_bits == 4u ? 0b0111u : 0u;
    const uint8_t tag_value_shift = tag_bits == 2u ? 1u : tag_bits == 4u ? 2u : 3u;
    const uint8_t tag_mask = (uint8_t)((1u << tag_bits) - 1u);
    const uint8_t wanted_tag = (uint8_t)logical_id & tag_mask;
    const uint32_t tag_base = tag_bits == 4u
        ? pool->active_block + sizeof(flash_block_header)
        : tag_bits == 2u ? tag2_map_base[pool_id]
                         : tag8_map_base[pool_id];
    const uint32_t stride = get_record_size(payload_len);
    uint32_t address = pool->record_end;
    uint16_t cached_group = UINT16_MAX;
    uint32_t cached_tags = 0;
    flash_record record;
    uint8_t payload[8];

    while (address > pool->records_start) {
        address -= stride;
        const uint16_t record_index = (uint16_t)(
            (address - pool->records_start) / stride);
        const uint16_t tag_group = record_index >> tag_group_shift;
        if (tag_group != cached_group) {
            if (ram_tag_words) {
                cached_tags = ram_tag_words[tag_group];
            }
            else if (HW_FLASH_READ(tag_base + (uint32_t)tag_group * sizeof(cached_tags),
                                   &cached_tags, sizeof(cached_tags))) {
                return FLASH_ERROR;
            }
            cached_group = tag_group;
        }

        // Apply the table's packing order using the precomputed mask and shifts.
        const uint8_t tag_position = (uint8_t)((record_index & tag_position_mask) ^ tag_order_xor);
        const uint8_t tag_shift = (uint8_t)(tag_position << tag_value_shift);
        const uint8_t record_tag = (uint8_t)(
            (cached_tags >> tag_shift) & tag_mask);
        if (record_tag != wanted_tag)
            continue;

        if (flash_log_bench_count_reads)
            ++scan_header_reads;
        if (HW_FLASH_READ(address, &record, sizeof(record)) ||
            get_record_len(record.marker) != payload_len
        ) return FLASH_ERROR;
        if (((uint8_t)record.logical_id & tag_mask) != record_tag)
            return FLASH_ERROR;
        if (record.logical_id != logical_id)
            continue;
        if (payload_len > capacity)
            return FLASH_BUFFER_TOO_SMALL;
        if (HW_FLASH_READ(address + sizeof(record), payload, payload_len) ||
            calculate_crc(payload, payload_len) != record.crc
        ) return FLASH_ERROR;

        __builtin_memcpy(data, payload, payload_len);
        *length = payload_len;
        return FLASH_OK;
    }
    return FLASH_NOT_FOUND;
}

static flash_status read_tag4_same(
    uint16_t id, flash_pool_id pool_id, void *data, uint16_t capacity, uint16_t *length
) {
    return read_tag_scan(id, pool_id, data, capacity, length, 4u, NULL);
}

static flash_status read_tag2_same(
    uint16_t id, flash_pool_id pool_id, void *data, uint16_t capacity, uint16_t *length
) {
    return read_tag_scan(id, pool_id, data, capacity, length, 2u, NULL);
}

static flash_status read_tag8_same(
    uint16_t id, flash_pool_id pool_id, void *data, uint16_t capacity, uint16_t *length
) {
    return read_tag_scan(id, pool_id, data, capacity, length, 8u, NULL);
}

static flash_status read_tag4_ram(
    uint16_t id, flash_pool_id pool_id, void *data, uint16_t capacity, uint16_t *length
) {
    return read_tag_scan(id, pool_id, data, capacity, length, 4u,
                         tag4_ram[pool_id]);
}

// Store one 8-bit ID tag per record before the 2-bit table at the block end.
static flash_status prepare_tag8_map(uint8_t pool_index) {
    log_pool_state *pool = &flash_log_pools[pool_index];
    const uint32_t map_bytes = ((uint32_t)pool->record_count + 3u) & ~3u;
    const uint32_t tag2_groups = ((uint32_t)pool->record_count + 15u) / 16u;
    const uint32_t tag2_bytes = tag2_groups * sizeof(uint32_t);
    const uint32_t map_base = pool->active_block + FLASH_BLOCK_SIZE -
                              tag2_bytes - map_bytes;
    if (map_base < pool->record_end)
        return FLASH_FULL;

    const uint32_t size_code = pool_index == FLASH_LOG_POOL_4
                             ? FLASH_RECORD_MARKER_4 : FLASH_RECORD_MARKER_8;
    const uint16_t payload_len = get_record_len(size_code);
    const uint32_t stride = get_record_size(payload_len);
    const uint16_t group_count = (uint16_t)((pool->record_count + 3u) / 4u);
    for (uint16_t group = 0; group < group_count; ++group) {
        uint32_t tags = 0xFFFFFFFFu;
        for (uint8_t byte = 0; byte < 4; ++byte) {
            const uint16_t index = (uint16_t)(group * 4u + byte);
            if (index >= pool->record_count)
                break;

            flash_record record;
            const uint32_t address = pool->records_start + (uint32_t)index * stride;
            if (HW_FLASH_READ(address, &record, sizeof(record)))
                return FLASH_ERROR;
            const uint32_t shift = (uint32_t)byte * 8u;
            tags = (tags & ~(0xFFu << shift)) |
                   ((uint32_t)(uint8_t)record.logical_id << shift);
        }

        if (HW_FLASH_WRITE(map_base + (uint32_t)group * sizeof(tags),
                           &tags, sizeof(tags)))
            return FLASH_ERROR;
    }

    tag8_map_base[pool_index] = map_base;
    return FLASH_OK;
}

// Pack sixteen 2-bit ID tags into each 32-bit word at the block end.
static flash_status prepare_tag2_map(uint8_t pool_index) {
    log_pool_state *pool = &flash_log_pools[pool_index];
    const uint16_t group_count = (uint16_t)((pool->record_count + 15u) / 16u);
    const uint32_t map_bytes = (uint32_t)group_count * sizeof(uint32_t);
    const uint32_t map_base = pool->active_block + FLASH_BLOCK_SIZE - map_bytes;
    if (map_base < pool->record_end)
        return FLASH_FULL;

    const uint32_t size_code = pool_index == FLASH_LOG_POOL_4
                             ? FLASH_RECORD_MARKER_4 : FLASH_RECORD_MARKER_8;
    const uint32_t stride = get_record_size(get_record_len(size_code));
    for (uint16_t group = 0; group < group_count; ++group) {
        uint32_t tags = 0xFFFFFFFFu;
        for (uint8_t slot = 0; slot < 16; ++slot) {
            const uint16_t index = (uint16_t)(group * 16u + slot);
            if (index >= pool->record_count)
                break;

            flash_record record;
            const uint32_t address = pool->records_start + (uint32_t)index * stride;
            if (HW_FLASH_READ(address, &record, sizeof(record)))
                return FLASH_ERROR;
            const uint32_t shift = (uint32_t)slot * 2u;
            tags = (tags & ~(3u << shift)) |
                   (((uint32_t)record.logical_id & 3u) << shift);
        }

        if (HW_FLASH_WRITE(map_base + (uint32_t)group * sizeof(tags),
                           &tags, sizeof(tags)))
            return FLASH_ERROR;
    }

    tag2_map_base[pool_index] = map_base;
    return FLASH_OK;
}

// Copy the production packed 4-bit tag words into RAM for the RAM-tag test path.
static flash_status load_tag4_map_to_ram(uint8_t pool_index) {
    log_pool_state *pool = &flash_log_pools[pool_index];
    const uint16_t word_count = (uint16_t)((pool->record_count + 7u) / 8u);
    if (word_count > TAG4_WORDS_PER_CLASS)
        return FLASH_FULL;

    for (uint16_t group = 0; group < word_count; ++group) {
        if (HW_FLASH_READ(pool->active_block + sizeof(flash_block_header) +
                          (uint32_t)group * sizeof(uint32_t),
                          &tag4_ram[pool_index][group], sizeof(uint32_t)))
            return FLASH_ERROR;
    }
    return FLASH_OK;
}

static flash_status write_dataset(flash_pool_id pool_id, uint16_t payload_len) {
    uint8_t payload[8];
    for (uint16_t id = 0; id < RECORDS_PER_CLASS; ++id) {
        for (uint16_t i = 0; i < payload_len; ++i)
            payload[i] = (uint8_t)(id + i * 17u);
        const flash_status status = flash_log_write(id, payload, pool_id);
        if (status != FLASH_OK)
            return status;
    }
    return FLASH_OK;
}

static void benchmark_case(
    const char *class_name, const char *case_name,
    uint16_t wanted_id, flash_pool_id pool_id,
    read_fn reader, bool show_header_count
) {
    uint8_t output[8];
    uint16_t length = 0;
    uint32_t checksum = 0;

    flash_log_bench_count_reads = 0;
    const uint32_t start = funSysTick32();
    flash_status status = FLASH_OK;
    for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
        status = reader(wanted_id, pool_id, output, sizeof(output), &length);
        checksum += (uint32_t)status + length + output[0];
    }
    const uint32_t ticks = (uint32_t)(funSysTick32() - start) / BENCH_RUNS;
    benchmark_sink = checksum;

    const uint8_t pool_index = pool_id;
    const uint8_t case_index = wanted_id == RECORDS_PER_CLASS - 1u ? 0u
                             : wanted_id == 0u ? 1u : 2u;
    if (reader == read_full_scan)
        baseline_ticks[pool_index][case_index] = ticks;
    const uint32_t baseline = baseline_ticks[pool_index][case_index];
    const char *percent_sign = ticks < baseline ? "+" : ticks > baseline ? "-" : "";
    const uint32_t percent = ticks < baseline
        ? (baseline - ticks) * 100u / baseline
        : ticks > baseline ? (ticks - baseline) * 100u / baseline : 0u;

    // Count one separate search so counter updates do not inflate timed results.
    flash_log_bench_read_calls = 0;
    flash_log_bench_read_bytes = 0;
    scan_header_reads = 0;
    flash_log_bench_count_reads = 1;
    (void)reader(wanted_id, pool_id, output, sizeof(output), &length);
    flash_log_bench_count_reads = 0;
    const uint32_t read_calls = flash_log_bench_read_calls;
    const uint32_t read_bytes = flash_log_bench_read_bytes;

    print_text_column(class_name, 6);
    print_text_column(case_name, 9);
    print_number_column(ticks, 9);
    print_number_column(read_calls, 7);
    print_number_column(read_bytes, 10);
    if (show_header_count) {
        print_number_column(scan_header_reads, 9);
    }
    else {
        print_text_column("-", 9);
    }
    print_text_column(percent_sign, 1);
    print_number_column(percent, 6);
    printf("\n");
}

static void benchmark_class(
    const char *class_name, flash_pool_id pool_id,
    read_fn reader, bool show_header_count
) {
    benchmark_case(class_name, "newest", RECORDS_PER_CLASS - 1u,
                   pool_id, reader, show_header_count);
    benchmark_case(class_name, "oldest", 0u, pool_id, reader, show_header_count);
    benchmark_case(class_name, "missing", RECORDS_PER_CLASS,
                   pool_id, reader, show_header_count);
}

static void benchmark_read_paths(void) {
    printf("\nPld: payload size (4B or 8B).\n");
    printf("Search: newest record, oldest record, or missing ID.\n");
    printf("Ticks: SysTick ticks per search.\n");
    printf("Reads: flash read calls per search.\n");
    printf("B/Search: bytes read per search.\n");
    printf("Headers: full record headers read.\n");
    printf("%%: faster (+) or slower (-) than the full-header baseline.\n");

    printf("\nBaseline full-header scan:\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, read_full_scan, true);
    benchmark_class("8B", FLASH_LOG_POOL_8, read_full_scan, true);

    printf("\nProduction flash_log_read_fast (4-bit tags):\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, flash_log_read_fast, false);
    benchmark_class("8B", FLASH_LOG_POOL_8, flash_log_read_fast, false);

    printf("\nSame scanner, 4-bit tags:\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, read_tag4_same, true);
    benchmark_class("8B", FLASH_LOG_POOL_8, read_tag4_same, true);

    printf("\nSame scanner, 4-bit tags copied to RAM:\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, read_tag4_ram, true);
    benchmark_class("8B", FLASH_LOG_POOL_8, read_tag4_ram, true);

    printf("\nSame scanner, 8-bit tags:\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, read_tag8_same, true);
    benchmark_class("8B", FLASH_LOG_POOL_8, read_tag8_same, true);

    printf("\nSame scanner, 2-bit tags:\n");
    print_table_header();
    benchmark_class("4B", FLASH_LOG_POOL_4, read_tag2_same, true);
    benchmark_class("8B", FLASH_LOG_POOL_8, read_tag2_same, true);
}

int main(void) {
    SystemInit();

    printf("Flash-log read benchmark (%u records/class, %u runs/search)\n",
           RECORDS_PER_CLASS, BENCH_RUNS);
    printf("This example erases its reserved 3-block region on every startup.\n");
    for (uint32_t i = 0; i < FLASH_BLOCK_COUNT; ++i) {
        if (HW_ERASE_BLOCK(FLASH_BASE_ADDR + i * FLASH_BLOCK_SIZE)) {
            printf("erase failed at block %lu\n", (unsigned long)i);
            while (1) { }
        }
    }

    flash_status status = flash_log_init();
    if (status == FLASH_OK)
        status = write_dataset(FLASH_LOG_POOL_4, 4);
    if (status == FLASH_OK)
        status = write_dataset(FLASH_LOG_POOL_8, 8);
    if (status == FLASH_OK)
        status = prepare_tag2_map(FLASH_LOG_POOL_4);
    if (status == FLASH_OK)
        status = prepare_tag2_map(FLASH_LOG_POOL_8);
    if (status == FLASH_OK)
        status = prepare_tag8_map(FLASH_LOG_POOL_4);
    if (status == FLASH_OK)
        status = prepare_tag8_map(FLASH_LOG_POOL_8);
    if (status == FLASH_OK)
        status = load_tag4_map_to_ram(FLASH_LOG_POOL_4);
    if (status == FLASH_OK)
        status = load_tag4_map_to_ram(FLASH_LOG_POOL_8);
    if (status != FLASH_OK) {
        printf("dataset setup failed: %u\n", status);
        while (1) { }
    }

    printf("Dataset ready: separate 4-byte and 8-byte pools, same IDs.\n");
    benchmark_read_paths();

    while (1) { }
}
