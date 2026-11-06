#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t (*scalar_fn)(uint32_t first, uint32_t second, uint32_t third);
typedef void (*bytes_fn)(uint8_t *bytes, uint32_t length);

#ifndef DF_SUM_OFF
#define DF_SUM_OFF 0x000u
#define DF_WEIGHT_OFF 0x100u
#define DF_BYTES_OFF 0x200u
#endif

#define DF_BASE 0x00070000u
#define DF_SUM_ADDR (DF_BASE + DF_SUM_OFF)
#define DF_WEIGHT_ADDR (DF_BASE + DF_WEIGHT_OFF)
#define DF_BYTES_ADDR (DF_BASE + DF_BYTES_OFF)

int main(void) {
    SystemInit();

    scalar_fn sum = (scalar_fn)(uintptr_t)DF_SUM_ADDR;
    scalar_fn weight = (scalar_fn)(uintptr_t)DF_WEIGHT_ADDR;
    bytes_fn increment = (bytes_fn)(uintptr_t)DF_BYTES_ADDR;

    printf("app running; DataFlash calls begin\n");

    while (1) {
        const volatile uint8_t *sum_code =
            (const volatile uint8_t *)(uintptr_t)DF_SUM_ADDR;
        printf("sum image bytes: %02x %02x %02x %02x %02x %02x\n",
               sum_code[0], sum_code[1], sum_code[2],
               sum_code[3], sum_code[4], sum_code[5]);

        printf("calling sum at 0x%08lx\n", (unsigned long)DF_SUM_ADDR);
        uint32_t sum_result = sum(10, 20, 12);
        printf("sum returned %lu\n", (unsigned long)sum_result);

        printf("calling weight at 0x%08lx\n", (unsigned long)DF_WEIGHT_ADDR);
        uint32_t weight_result = weight(10, 20, 12);
        printf("weight returned %lu\n", (unsigned long)weight_result);

        uint8_t bytes[] = { 1, 2, 3, 4, 5, 254 };
        printf("calling bytes at 0x%08lx\n", (unsigned long)DF_BYTES_ADDR);
        increment(bytes, sizeof(bytes));

        printf("bytes changed array to:");

        for (uint32_t i = 0; i < sizeof(bytes); ++i)
            printf(" %02x", bytes[i]);
        printf("\n");

        Delay_Ms(2000);
    }
}
