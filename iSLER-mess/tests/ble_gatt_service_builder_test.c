#include <assert.h>
#include <stdint.h>
#define BLE_GATT_SERVER_VALUE_POOL_SIZE 20
#include "../ble_gatt_server.h"

int main(void) {
    ble_gatt_server server;
    ble_gatt_standard_service_handles handles = {0};
    ble_gatt_server_init(&server, 23);
    assert(!ble_gatt_server_add_standard_gatt_service(&server, &handles));
    assert(server.count == 0 && server.next_handle == 1 &&
           server.value_used == 0 && !server.database_sealed);
    for (uint16_t i = 0; i < BLE_GATT_SERVER_MAX_ATTRIBUTES; i++)
        assert(server.attributes[i].handle == 0);
    for (uint16_t i = 0; i < BLE_GATT_SERVER_VALUE_POOL_SIZE; i++)
        assert(server.value_pool[i] == 0);
    return 0;
}
