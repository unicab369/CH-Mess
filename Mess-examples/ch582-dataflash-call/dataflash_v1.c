#include <stdint.h>

// Initial implementation: 10 + 20 + 12 returns 42.
__attribute__((section(".dataflash_fn"), noinline, used))
uint32_t dataflash_function(uint32_t first, uint32_t second, uint32_t third) {
    return first + second + third;
}
