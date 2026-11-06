#include <stdint.h>

// Add three scalar arguments.
__attribute__((section(".dataflash_fn"), noinline, used))
uint32_t dataflash_sum(uint32_t first, uint32_t second, uint32_t third) {
    return first + second + third;
}
