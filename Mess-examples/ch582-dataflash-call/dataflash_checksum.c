#include <stdint.h>

// Read bytes from the caller's buffer and return their 32-bit sum.
__attribute__((section(".dataflash_fn"), noinline, used))
uint32_t dataflash_checksum(const uint8_t *bytes, uint32_t length) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; ++i)
        sum += bytes[i];
    return sum;
}
