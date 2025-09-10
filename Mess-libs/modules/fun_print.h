#include "ch32fun.h"
#include <stdint.h>


void print_bits(uint32_t value, uint8_t bit_count) {
    for (int i = bit_count - 1; i >= 0; i--) {
        const uint8_t bit = (value & (1 << i)) ? 1: 0;
        printf("%d", bit);
    }
    printf("\n");
}


void print_separator() {
    printf("-------------------------------\n");
}