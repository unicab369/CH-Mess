#ifndef DATAFLASH_STRUCT_H
#define DATAFLASH_STRUCT_H

#include <stdint.h>

typedef struct {
    uint32_t total;
    uint16_t count;
    uint8_t flags;
} dataflash_record;

#ifdef DATAFLASH_STRUCT_IMAGE
// Built from this header as a standalone DataFlash function image.
__attribute__((section(".dataflash_fn"), noinline, used))
void dataflash_update_record(dataflash_record *record) {
    record->total += record->count;
    record->count++;
    record->flags |= 1u;
}
#else
// The app uses this declaration with its fixed-address function pointer.
void dataflash_update_record(dataflash_record *record);
#endif

#endif
