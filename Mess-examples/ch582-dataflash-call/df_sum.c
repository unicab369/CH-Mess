#include <stdint.h>

// Add three scalar arguments.
__attribute__((section(".df_fn"), noinline, used))
uint32_t df_fn(uint32_t first, uint32_t second, uint32_t third) {
    return first + second + third;
}
