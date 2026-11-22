#ifndef DATAFLASH_FUNCTIONS_H
#define DATAFLASH_FUNCTIONS_H

#include <stdint.h>

typedef uint32_t (*dataflash_scalar_fn)(uint32_t first, uint32_t second,
                                        uint32_t third);
typedef uint32_t (*codeflash_callback_fn)(uint32_t left, uint32_t right);
typedef uint32_t (*dataflash_callback_fn)(codeflash_callback_fn callback,
                                          uint32_t left, uint32_t right);
typedef void (*dataflash_bytes_fn)(uint8_t *bytes, uint32_t length);
typedef uint32_t (*dataflash_checksum_fn)(const uint8_t *bytes, uint32_t length);

typedef struct {
    uint32_t total;
    uint16_t count;
    uint8_t flags;
} dataflash_record;
typedef void (*dataflash_struct_fn)(dataflash_record *record);

#if defined(DATAFLASH_FUNCTIONS_IMAGE)

__attribute__((section(".dataflash_sum"), noinline, used))
uint32_t dataflash_sum(uint32_t first, uint32_t second, uint32_t third) {
    return first + second + third;
}

__attribute__((section(".dataflash_callback"), noinline, used))
uint32_t dataflash_call_callback(codeflash_callback_fn callback,
                                 uint32_t left, uint32_t right) {
    return callback(left, right);
}

__attribute__((section(".dataflash_bytes"), noinline, used))
void dataflash_increment_bytes(uint8_t *bytes, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i)
        bytes[i] += 1;
}

__attribute__((section(".dataflash_checksum"), noinline, used))
uint32_t dataflash_checksum(const uint8_t *bytes, uint32_t length) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; ++i)
        sum += bytes[i];
    return sum;
}

__attribute__((section(".dataflash_struct"), noinline, used))
void dataflash_update_record(dataflash_record *record) {
    record->total += record->count;
    record->count++;
    record->flags |= 1u;
}

#endif

#endif
