#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t (*dataflash_scalar_fn)(uint32_t first, uint32_t second,
                                        uint32_t third);
typedef void (*dataflash_bytes_fn)(uint8_t *bytes, uint32_t length);
typedef uint32_t (*dataflash_checksum_fn)(const uint8_t *bytes, uint32_t length);

#ifndef DATAFLASH_SUM_OFFSET
#define DATAFLASH_SUM_OFFSET 0x000u
#define DATAFLASH_WEIGHT_OFFSET 0x100u
#define DATAFLASH_BYTES_OFFSET 0x200u
#define DATAFLASH_CHECKSUM_OFFSET 0x300u
#endif

#define DATAFLASH_BASE_ADDRESS 0x00070000u
#define DATAFLASH_SUM_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_SUM_OFFSET)
#define DATAFLASH_WEIGHT_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_WEIGHT_OFFSET)
#define DATAFLASH_BYTES_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_BYTES_OFFSET)
#define DATAFLASH_CHECKSUM_ADDRESS (DATAFLASH_BASE_ADDRESS + DATAFLASH_CHECKSUM_OFFSET)

int main(void) {
    SystemInit();

    dataflash_scalar_fn sum = (dataflash_scalar_fn)(uintptr_t)DATAFLASH_SUM_ADDRESS;
    dataflash_scalar_fn weight = (dataflash_scalar_fn)(uintptr_t)DATAFLASH_WEIGHT_ADDRESS;
    dataflash_bytes_fn increment = (dataflash_bytes_fn)(uintptr_t)DATAFLASH_BYTES_ADDRESS;
    dataflash_checksum_fn checksum =
        (dataflash_checksum_fn)(uintptr_t)DATAFLASH_CHECKSUM_ADDRESS;
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

        printf("calling weight at 0x%08lx\n", (unsigned long)DATAFLASH_WEIGHT_ADDRESS);
        uint32_t weight_result = weight(10, 20, 12);
        printf("weight returned %lu\n", (unsigned long)weight_result);

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

        Delay_Ms(2000);
    }
}
