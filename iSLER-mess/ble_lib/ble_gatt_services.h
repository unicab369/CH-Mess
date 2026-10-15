#ifndef BLE_GATT_SERVICES_H
#define BLE_GATT_SERVICES_H

// Reusable Bluetooth SIG GATT services built on the generic GATT server.
#include "ble_gatt_server.h"

typedef struct {
    uint16_t service_handle;
    uint16_t level_handle;
    uint8_t level;
} ble_gatt_battery_service;

static uint8_t gatt_battery_level_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (!offset && *inout_len) {
        out[0] = ((ble_gatt_battery_service *)context)->level;
        *inout_len = 1;
    } else {
        *inout_len = 0;
    }
    return 0;
}

// Add the standard Battery Service with a read-only Battery Level value.
// The service state must remain alive while the GATT server uses it.
static inline int gatt_battery_service_add(
    ble_gatt_server *server,
    ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100) return 0;

    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid level_uuid = {2, {0x19, 0x2a}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    battery->level = level;

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &battery->service_handle) &&
        gatt_server_add_characteristic(server, &level_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &battery->level, 1, 1, gatt_battery_level_read, NULL, battery,
            NULL, &battery->level_handle);
    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(battery, 0, sizeof(*battery));
        return 0;
    }
    return 1;
}

static inline int gatt_battery_service_set_level(
    ble_gatt_battery_service *battery, uint8_t level
) {
    if (!battery || !battery->level_handle || level > 100) return 0;
    battery->level = level;
    return 1;
}

#endif
