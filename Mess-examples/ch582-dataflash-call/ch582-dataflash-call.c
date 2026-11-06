#include "ch32fun.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t (*dataflash_function)(uint32_t first, uint32_t second, uint32_t third);

#define DATAFLASH_FUNCTION_ADDRESS 0x00070000u

int main(void) {
    SystemInit();

    dataflash_function volatile call_dataflash =
                                (dataflash_function)(uintptr_t)DATAFLASH_FUNCTION_ADDRESS;
    const uint32_t result = call_dataflash(10, 20, 12);

    printf("DataFlash function at 0x%08lx returned %lu\n",
           (unsigned long)DATAFLASH_FUNCTION_ADDRESS,
           (unsigned long)result);

    for (;;) { }
}
