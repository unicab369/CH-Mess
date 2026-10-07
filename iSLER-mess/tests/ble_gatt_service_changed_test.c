#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_gatt_server.h"

static int att(ble_gatt_server *server, const uint8_t *request,
               uint16_t request_len, uint8_t *response,
               uint16_t *response_len) {
    return ble_gatt_server_att(server, request, request_len, response,
                               BLE_GATT_SERVER_MTU_MAX, response_len);
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_standard_service_handles handles;
    ble_gatt_server_init(&server, 64);
    assert(ble_gatt_server_add_standard_gatt_service(&server, &handles));
    assert(handles.service_handle == 1 &&
           handles.service_changed_handle == 3 &&
           handles.service_changed_cccd_handle == 4 &&
           handles.database_hash_handle == 6);
    assert(ble_gatt_server_seal_database(&server));

    ble_gatt_attribute *hash = ble_gatt_server_find(
        &server, handles.database_hash_handle);
    assert(hash && hash->value_len == 16);
    const uint8_t *hash_value = ble_gatt_attribute_value(&server, hash);
    uint8_t nonzero = 0;
    for (uint8_t i = 0; i < 16; i++) nonzero |= hash_value[i];
    assert(nonzero);

    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len;
    const uint8_t read_hash_by_uuid[] = {
        0x08, 1, 0, 0xff, 0xff, 0x2a, 0x2b
    };
    assert(att(&server, read_hash_by_uuid, sizeof(read_hash_by_uuid),
               response, &response_len) == 1);
    assert(response[0] == 0x09 && response[1] == 18 && response_len == 20 &&
           ble_gatt_server_u16(response + 2) == handles.database_hash_handle &&
           memcmp(response + 4, hash_value, 16) == 0);

    uint8_t read_changed[] = {0x0a,
        (uint8_t)handles.service_changed_handle,
        (uint8_t)(handles.service_changed_handle >> 8)};
    assert(att(&server, read_changed, sizeof(read_changed), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_READ_NOT_PERMITTED);

    assert(!ble_gatt_server_service_changed(&server, 0, 5));
    assert(!ble_gatt_server_service_changed(&server, 6, 5));
    assert(!ble_gatt_server_service_changed(&server, 2, 5));
    assert(ble_gatt_server_set_cccd(&server,
        handles.service_changed_handle, 2));
    assert(ble_gatt_server_service_changed(&server, 2, 5));
    uint8_t indication[BLE_GATT_SERVER_MTU_MAX];
    uint16_t indication_len;
    assert(ble_gatt_server_poll_event(&server, 100, indication,
        sizeof(indication), &indication_len) == 1);
    assert(indication_len == 7 && indication[0] == 0x1d &&
           ble_gatt_server_u16(indication + 1) ==
               handles.service_changed_handle &&
           ble_gatt_server_u16(indication + 3) == 2 &&
           ble_gatt_server_u16(indication + 5) == 5);
    const uint8_t confirmation[] = {0x1e};
    assert(att(&server, confirmation, sizeof(confirmation), response,
               &response_len) == 0);
    assert(!server.indication_pending);

    ble_gatt_standard_service_handles duplicate;
    assert(!ble_gatt_server_add_standard_gatt_service(&server, &duplicate));
    assert(server.database_sealed);
    return 0;
}
