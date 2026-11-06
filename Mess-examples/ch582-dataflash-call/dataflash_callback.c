#include <stdint.h>

typedef uint32_t (*codeflash_callback_fn)(uint32_t left, uint32_t right);

// Call the function pointer supplied by the CodeFlash application.
__attribute__((section(".dataflash_fn"), noinline, used))
uint32_t dataflash_call_callback(codeflash_callback_fn callback,
                                 uint32_t left, uint32_t right) {
    return callback(left, right);
}
