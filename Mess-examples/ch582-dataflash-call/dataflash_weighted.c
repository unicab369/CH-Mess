#include <stdint.h>

// Add three scalar arguments, counting the second one twice.
__attribute__((section(".dataflash_fn"), noinline, used))
uint32_t dataflash_weighted(uint32_t first, uint32_t second, uint32_t third) {
    return first + second * 2 + third;
}
