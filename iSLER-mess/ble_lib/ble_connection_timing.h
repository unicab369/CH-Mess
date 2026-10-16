// Shared LE connection timing parameter validation.
#ifndef BLE_CONNECTION_TIMING_H
#define BLE_CONNECTION_TIMING_H

#include <stdint.h>

static inline int ble_connection_timing_ranges_valid(
    uint16_t interval_min, uint16_t interval_max,
    uint16_t latency, uint16_t timeout
) {
    return interval_min >= 6 && interval_min <= interval_max &&
        interval_max <= 3200 && latency <= 499 &&
        timeout >= 10 && timeout <= 3200;
}

#endif
