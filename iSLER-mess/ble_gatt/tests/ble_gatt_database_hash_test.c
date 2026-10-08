#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_gatt_server.h"

static ble_gatt_uuid uuid16(uint16_t value) {
    ble_gatt_uuid uuid = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
    return uuid;
}

int main(void) {
    static const uint8_t expected[16] = {
        0x89,0x4e,0xcf,0x1c,0x23,0xc7,0x1f,0x82,
        0xdd,0xbf,0x83,0x41,0xb1,0xb2,0x4c,0x62
    };
    ble_gatt_server server;
    ble_gatt_server_init(&server, 64);
    ble_gatt_uuid service_uuid = uuid16(0x1801);
    ble_gatt_uuid value_uuid = uuid16(0x2a19);
    ble_gatt_uuid hash_uuid = uuid16(0x2b2a);
    uint8_t value = 0xa5, initial_hash[16] = {0};
    uint16_t service, declaration, value_handle;
    uint16_t hash_declaration, hash_handle;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, &value, 1, 1,
        NULL, NULL, NULL, &declaration, &value_handle));
    assert(ble_gatt_server_add_characteristic(&server, &hash_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, initial_hash,
        sizeof(initial_hash), sizeof(initial_hash), NULL, NULL, NULL,
        &hash_declaration, &hash_handle));

    uint8_t hash[16];
    assert(ble_gatt_server_compute_database_hash(&server, hash));
    assert(memcmp(hash, expected, sizeof(hash)) == 0);
    assert(ble_gatt_server_seal_database(&server));
    ble_gatt_attribute *attribute = ble_gatt_server_find(&server, hash_handle);
    assert(attribute && memcmp(ble_gatt_attribute_value(&server, attribute),
                               expected, sizeof(expected)) == 0);
    assert(!ble_gatt_server_compute_database_hash(&server, NULL));
    return 0;
}
