#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t (*scalar_fn)(uint32_t first, uint32_t second, uint32_t third);
typedef void (*bytes_fn)(uint8_t *bytes, uint32_t length);

#ifndef DF_SUM_OFF
#define DF_SUM_OFF 0x400u
#define DF_WEIGHT_OFF 0x500u
#define DF_BYTES_OFF 0x600u
#endif

#define DF_BASE 0x00070000u
#define DF_SUM_ADDR (DF_BASE + DF_SUM_OFF)
#define DF_WEIGHT_ADDR (DF_BASE + DF_WEIGHT_OFF)
#define DF_BYTES_ADDR (DF_BASE + DF_BYTES_OFF)

int main(void) {
    SystemInit();

    scalar_fn volatile sum = (scalar_fn)(uintptr_t)DF_SUM_ADDR;
    scalar_fn volatile weight = (scalar_fn)(uintptr_t)DF_WEIGHT_ADDR;
    bytes_fn volatile increment = (bytes_fn)(uintptr_t)DF_BYTES_ADDR;

    printf("sum at 0x%08lx returned %lu\n", (unsigned long)DF_SUM_ADDR,
                                            (unsigned long)sum(10, 20, 12));
    printf("weight at 0x%08lx returned %lu\n", (unsigned long)DF_WEIGHT_ADDR,
                                                (unsigned long)weight(10, 20, 12));

    uint8_t bytes[] = { 1, 2, 254 };
    increment(bytes, sizeof(bytes));

    printf("bytes at 0x%08lx changed array to:", (unsigned long)DF_BYTES_ADDR);

    for (uint32_t i = 0; i < sizeof(bytes); ++i)
        printf(" %02x", bytes[i]);
    printf("\n");

    for (;;) { }
}
