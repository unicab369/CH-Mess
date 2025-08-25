#include "ch32fun.h"
#include <stdio.h>

typedef struct {
    uint8_t *values;
    uint8_t size;
    uint8_t index;
} stepper8_t;

typedef struct {
    uint16_t *values;
    uint8_t size;
    uint8_t index;
} stepper16_t;

typedef struct {
    uint32_t *values;
    uint8_t size;
    uint8_t index;
} stepper32_t;

//! stepper8
void stepper8_tick(stepper8_t *s, uint8_t skip) {
    s->index = (s->index + skip) % s->size;
}

uint8_t stepper8_currentValue(stepper8_t *s) {
    return s->values[s->index];
}

uint8_t stepper8_step(stepper8_t *s, uint8_t skip) {
    uint8_t ret = stepper8_currentValue(s);
    stepper8_tick(s, skip);
    return ret;
}

//! stepper16
void stepper16_tick(stepper16_t *s, uint8_t skip) {
    s->index = (s->index + skip) % s->size;
}

uint16_t stepper16_currentValue(stepper16_t *s) {
    return s->values[s->index];
}

uint16_t stepper16_step(stepper16_t *s, uint8_t skip) {
    uint16_t ret = stepper16_currentValue(s);
    stepper16_tick(s, skip);
    return ret;
}

//! stepper32
void stepper32_tick(stepper32_t *s, uint8_t skip) {
    s->index = (s->index + skip) % s->size;
}

uint32_t stepper32_currentValue(stepper32_t *s) {
    return s->values[s->index];
}

uint32_t stepper32_step(stepper32_t *s, uint8_t skip) {
    uint32_t ret = stepper32_currentValue(s);
    stepper32_tick(s, skip);
    return ret;
}