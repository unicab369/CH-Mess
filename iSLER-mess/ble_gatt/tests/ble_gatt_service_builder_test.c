#include <assert.h>
#include <stdint.h>
#define BLE_GATT_SERVER_VALUE_POOL_SIZE 64
#include "../ble_gatt_server.h"

static void add_manual_gatt_service(
    ble_gatt_server *server,
                                   int include_client_features
) {
    ble_gatt_uuid service = {2, {0x01, 0x18}};
    ble_gatt_uuid changed = {2, {0x05, 0x2a}};
    ble_gatt_uuid hash = {2, {0x2a, 0x2b}};
    ble_gatt_uuid features = {2, {0x29, 0x2b}};
    ble_gatt_uuid cccd = {2, {0x02, 0x29}};
    uint8_t zero4[4] = {0}, zero16[16] = {0}, zero1[1] = {0};
    uint16_t h;
    assert(ble_gatt_server_add_service(server, &service, 1, &h));
    assert(ble_gatt_server_add_characteristic(server, &changed,
        BLE_GATT_PROP_INDICATE, 0, zero4, sizeof(zero4), sizeof(zero4),
        NULL, NULL, NULL, NULL, NULL));
    assert(ble_gatt_server_add_descriptor(server, &cccd,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, NULL, 0, 0,
        NULL, NULL, NULL, NULL));
    assert(ble_gatt_server_add_characteristic(server, &hash,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, zero16, sizeof(zero16),
        sizeof(zero16), NULL, NULL, NULL, NULL, NULL));
    if (include_client_features)
        assert(ble_gatt_server_add_characteristic(server, &features,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, zero1, sizeof(zero1),
            sizeof(zero1), NULL, NULL, NULL, NULL, NULL));
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_standard_service_handles handles = {0};
    ble_gatt_server_init(&server, 23);
    uint8_t fill[BLE_GATT_SERVER_VALUE_POOL_SIZE] = {0};
    ble_gatt_uuid filler_uuid = {2, {0x00, 0x2a}};
    assert(ble_gatt_server_add_attribute(&server, &filler_uuid, 0, fill,
        sizeof(fill), sizeof(fill), NULL, NULL, NULL, NULL));
    uint16_t original_count = server.count;
    uint16_t original_handle = server.next_handle;
    assert(!ble_gatt_server_add_standard_gatt_service(&server, &handles));
    assert(server.count == original_count &&
           server.next_handle == original_handle &&
           server.value_used == BLE_GATT_SERVER_VALUE_POOL_SIZE &&
           !server.database_sealed);
    for (uint16_t i = original_count; i < BLE_GATT_SERVER_MAX_ATTRIBUTES; i++)
        assert(server.attributes[i].handle == 0);

    ble_gatt_server_init(&server, 23);
    add_manual_gatt_service(&server, 0);
    assert(!ble_gatt_server_seal_database(&server));
    assert(!server.database_sealed);

    ble_gatt_server_init(&server, 23);
    add_manual_gatt_service(&server, 1);
    assert(ble_gatt_server_seal_database(&server));
    assert(server.database_sealed && server.database_hash_available);
    assert(server.attributes[server.count - 1].flags &
           BLE_GATT_ATTRIBUTE_CLIENT_SUPPORTED_FEATURES);
    return 0;
}
