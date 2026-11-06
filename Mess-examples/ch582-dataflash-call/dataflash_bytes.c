#include <stdint.h>

// Increment each byte in the caller's buffer in place.
__attribute__((section(".dataflash_fn"), noinline, used))
void dataflash_bytes(uint8_t *bytes, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i)
        bytes[i] += 1;
}
