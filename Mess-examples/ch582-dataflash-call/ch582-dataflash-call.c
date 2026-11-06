#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>
#include "dataflash_struct.h"

typedef uint32_t (*dataflash_scalar_fn)(uint32_t first, uint32_t second,
                                        uint32_t third);
typedef uint32_t (*codeflash_callback_fn)(uint32_t left, uint32_t right);
typedef uint32_t (*dataflash_callback_fn)(codeflash_callback_fn callback,
                                          uint32_t left, uint32_t right);
typedef void (*dataflash_bytes_fn)(uint8_t *bytes, uint32_t length);
typedef uint32_t (*dataflash_checksum_fn)(const uint8_t *bytes, uint32_t length);
typedef void (*dataflash_struct_fn)(dataflash_record *record);

#ifndef DATAFLASH_SUM_OFFSET
#define DATAFLASH_SUM_OFFSET 0x000u
#define DATAFLASH_CALLBACK_OFFSET 0x100u
#define DATAFLASH_BYTES_OFFSET 0x200u
#define DATAFLASH_CHECKSUM_OFFSET 0x300u
#define DATAFLASH_STRUCT_OFFSET 0x400u
#endif

#define DATAFLASH_BASE_ADDRESS 0x00070000u
#define DATAFLASH_SUM_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_SUM_OFFSET)
#define DATAFLASH_CALLBACK_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_CALLBACK_OFFSET)
#define DATAFLASH_BYTES_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_BYTES_OFFSET)
#define DATAFLASH_CHECKSUM_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_CHECKSUM_OFFSET)
#define DATAFLASH_STRUCT_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_STRUCT_OFFSET)

__attribute__((noinline, used))
static uint32_t codeflash_multiply(uint32_t left, uint32_t right) {
    return left * right;
}

int main(void) {
    SystemInit();

    dataflash_scalar_fn sum = (dataflash_scalar_fn)(uintptr_t)DATAFLASH_SUM_ADDRESS;
    dataflash_callback_fn call_callback =
        (dataflash_callback_fn)(uintptr_t)DATAFLASH_CALLBACK_ADDRESS;
    dataflash_bytes_fn increment = (dataflash_bytes_fn)(uintptr_t)DATAFLASH_BYTES_ADDRESS;
    dataflash_checksum_fn checksum =
        (dataflash_checksum_fn)(uintptr_t)DATAFLASH_CHECKSUM_ADDRESS;
    dataflash_struct_fn update_record =
        (dataflash_struct_fn)(uintptr_t)DATAFLASH_STRUCT_ADDRESS;
    static const uint8_t checksum_data[] = { 10, 20, 30, 40, 50 };

    printf("app running; DataFlash calls begin\n");

    while (1) {
        const volatile uint8_t *sum_code =
            (const volatile uint8_t *)(uintptr_t)DATAFLASH_SUM_ADDRESS;
        printf("sum image bytes: %02x %02x %02x %02x %02x %02x\n",
               sum_code[0], sum_code[1], sum_code[2],
               sum_code[3], sum_code[4], sum_code[5]);

        printf("calling sum at 0x%08lx\n", (unsigned long)DATAFLASH_SUM_ADDRESS);
        uint32_t sum_result = sum(10, 20, 12);
        printf("sum returned %lu\n", (unsigned long)sum_result);

        printf("calling DataFlash callback at 0x%08lx\n",
               (unsigned long)DATAFLASH_CALLBACK_ADDRESS);
        uint32_t callback_result = call_callback(codeflash_multiply, 6, 7);
        printf("CodeFlash callback returned %lu\n\n",
               (unsigned long)callback_result);

        uint8_t bytes[] = { 1, 2, 3, 4, 5, 254 };
        printf("calling bytes at 0x%08lx\n", (unsigned long)DATAFLASH_BYTES_ADDRESS);
        increment(bytes, sizeof(bytes));

        printf("bytes changed array to:");

        for (uint32_t i = 0; i < sizeof(bytes); ++i)
            printf(" %02x", bytes[i]);
        printf("\n\n");

        printf("calling checksum at 0x%08lx\n",
               (unsigned long)DATAFLASH_CHECKSUM_ADDRESS);
        uint32_t checksum_result = checksum(checksum_data, sizeof(checksum_data));
        printf("checksum of CodeFlash data returned %lu\n\n",
               (unsigned long)checksum_result);

        dataflash_record record = { .total = 100, .count = 3, .flags = 0 };
        printf("calling struct update at 0x%08lx\n",
               (unsigned long)DATAFLASH_STRUCT_ADDRESS);
        update_record(&record);
        printf("struct now has total=%lu count=%u flags=0x%02x\n\n",
               (unsigned long)record.total, record.count, record.flags);

        Delay_Ms(2000);
    }
}
