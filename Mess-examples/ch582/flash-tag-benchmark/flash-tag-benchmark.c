#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>

#include "tag-benchmark-data.h"

#define RECORD_COUNT_4B 341u
#define RECORD_COUNT_8B 255u
#define BENCH_RUNS 512u

static volatile uint32_t benchmark_sink;

// Return the number of full-ID checks in the high half and the matching record
// index in the low half (0xffff means not found).
__attribute__((noinline))
static uint32_t scan_tag4(uint16_t wanted_id, uint16_t record_count) {
    const volatile uint32_t *tags = tag4_words;
    const volatile uint32_t *headers = record_header_words;
    const uint8_t wanted_tag = (uint8_t)(wanted_id & 0x0Fu);
    uint16_t full_id_checks = 0;

    int32_t index = (int32_t)record_count - 1;
    while (index >= 0) {
        const int32_t group = index / 8;
        const int32_t first_index = group * 8;
        const uint32_t packed = tags[group];

        for (int32_t nibble = index - first_index; nibble >= 0; --nibble) {
            const uint16_t record_index = (uint16_t)(first_index + nibble);
            if (((packed >> (nibble * 4)) & 0x0Fu) != wanted_tag)
                continue;
            ++full_id_checks;
            const uint32_t crc_and_id = headers[record_index * 2u];
            const uint32_t marker = headers[record_index * 2u + 1u];
            (void)marker;
            if ((uint16_t)(crc_and_id >> 16) == wanted_id)
                return ((uint32_t)full_id_checks << 16) | record_index;
        }
        index = first_index - 1;
    }

    return ((uint32_t)full_id_checks << 16) | 0xFFFFu;
}

__attribute__((noinline))
static uint32_t scan_tag8(uint16_t wanted_id, uint16_t record_count) {
    const volatile uint32_t *tags = tag8_words;
    const volatile uint32_t *headers = record_header_words;
    const uint8_t wanted_tag = (uint8_t)wanted_id;
    uint16_t full_id_checks = 0;

    int32_t index = (int32_t)record_count - 1;
    while (index >= 0) {
        const int32_t group = index / 4;
        const int32_t first_index = group * 4;
        const uint32_t packed = tags[group];

        for (int32_t byte = index - first_index; byte >= 0; --byte) {
            const uint16_t record_index = (uint16_t)(first_index + byte);
            if (((packed >> (byte * 8)) & 0xFFu) != wanted_tag)
                continue;
            ++full_id_checks;
            const uint32_t crc_and_id = headers[record_index * 2u];
            const uint32_t marker = headers[record_index * 2u + 1u];
            (void)marker;
            if ((uint16_t)(crc_and_id >> 16) == wanted_id)
                return ((uint32_t)full_id_checks << 16) | record_index;
        }
        index = first_index - 1;
    }

    return ((uint32_t)full_id_checks << 16) | 0xFFFFu;
}

static uint32_t time_tag4(uint16_t wanted_id, uint16_t record_count) {
    uint32_t checksum = 0;
    const uint32_t start = funSysTick32();
    for (uint32_t i = 0; i < BENCH_RUNS; ++i)
        checksum += scan_tag4(wanted_id, record_count);
    const uint32_t elapsed = (uint32_t)(funSysTick32() - start);
    benchmark_sink = checksum;
    return elapsed / BENCH_RUNS;
}

static uint32_t time_tag8(uint16_t wanted_id, uint16_t record_count) {
    uint32_t checksum = 0;
    const uint32_t start = funSysTick32();
    for (uint32_t i = 0; i < BENCH_RUNS; ++i)
        checksum += scan_tag8(wanted_id, record_count);
    const uint32_t elapsed = (uint32_t)(funSysTick32() - start);
    benchmark_sink = checksum;
    return elapsed / BENCH_RUNS;
}

static void print_case(const char *name, uint16_t wanted_id, uint16_t record_count) {
    const uint32_t result4 = scan_tag4(wanted_id, record_count);
    const uint32_t result8 = scan_tag8(wanted_id, record_count);
    const uint32_t ticks4 = time_tag4(wanted_id, record_count);
    const uint32_t ticks8 = time_tag8(wanted_id, record_count);

    printf("%s (ID %u, %u records):\n", name, wanted_id, record_count);
    printf("  4-bit: %lu SysTick ticks/search, %lu full-ID checks, index %u\n",
           (unsigned long)ticks4, (unsigned long)(result4 >> 16),
           (unsigned)(result4 & 0xFFFFu));
    printf("  8-bit: %lu SysTick ticks/search, %lu full-ID checks, index %u\n",
           (unsigned long)ticks8, (unsigned long)(result8 >> 16),
           (unsigned)(result8 & 0xFFFFu));
}

static void benchmark_class(const char *name, uint16_t record_count) {
    printf("\n%s payload class (%u records):\n", name, record_count);
    print_case("Newest hit", (uint16_t)(record_count - 1u), record_count);
    print_case("Oldest hit", 0u, record_count);
    print_case("Missing ID", record_count, record_count);
}

int main(void) {
    SystemInit();

    printf("Tag scan benchmark: %u runs/search\n", BENCH_RUNS);
    printf("Test IDs and tag tables are const data in CodeFlash.\n");
    printf("Tags are the low 4 or 8 ID bits; searches run newest-to-oldest.\n");

    benchmark_class("4-byte", RECORD_COUNT_4B);
    benchmark_class("8-byte", RECORD_COUNT_8B);

    while (1) { }
}
