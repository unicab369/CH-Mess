#include <assert.h>
#include <stdint.h>

#include "../ble_gatt_services.h"

static uint8_t read_battery_level(
    ble_gatt_server *server, uint16_t handle, uint8_t *value
) {
    uint8_t request[] = {
        ATT_OP_READ_REQ, (uint8_t)handle, (uint8_t)(handle >> 8)
    };
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(server, request, sizeof(request), response,
                           sizeof(response), &response_len) == 1);
    assert(response_len == 2 && response[0] == ATT_OP_READ_RSP);
    *value = response[1];
    return response[0];
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_battery_service battery = {0};
    gatt_server_init(&server, 23);

    assert(gatt_battery_service_add(&server, &battery, 87));
    assert(battery.service_handle && battery.level_handle);
    uint8_t level = 0;
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 87);

    assert(gatt_battery_service_set_level(&battery, 42));
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);
    assert(!gatt_battery_service_set_level(&battery, 101));
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);

    uint8_t write_request[] = {
        ATT_OP_WRITE_REQ, (uint8_t)battery.level_handle,
        (uint8_t)(battery.level_handle >> 8), 50
    };
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(&server, write_request, sizeof(write_request),
        response, sizeof(response), &response_len) == 1);
    assert(response_len == 5 && response[0] == ATT_OP_ERROR_RSP &&
           response[1] == ATT_OP_WRITE_REQ &&
           response[4] == BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED);
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);

    return 0;
}
