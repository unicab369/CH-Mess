#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_gatt_server.h"

typedef struct {
    uint8_t hash[16];
    uint8_t valid;
    uint16_t service_changed_cccd;
    uint16_t stores;
} peer_store;

static int load_hash(void *context, uint8_t hash[16]) {
    peer_store *store = context;
    if (!store->valid) return 0;
    memcpy(hash, store->hash, 16);
    return 1;
}

static void save_hash(void *context, const uint8_t hash[16]) {
    peer_store *store = context;
    memcpy(store->hash, hash, 16);
    store->valid = 1;
    store->stores++;
}

static uint16_t load_cccd(void *context, uint16_t value_handle) {
    peer_store *store = context;
    return value_handle ? store->service_changed_cccd : 0;
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_standard_service_handles handles;
    peer_store peer = {.service_changed_cccd = 2};
    ble_gatt_server_init(&server, 64);
    assert(ble_gatt_server_add_standard_gatt_service(&server, &handles));
    assert(ble_gatt_server_seal_database(&server));
    ble_gatt_server_set_database_hash_persistence(&server, load_hash,
                                                   save_hash, &peer);
    ble_gatt_server_set_cccd_persistence(&server, load_cccd, NULL, &peer);

    // A first connection records a baseline without sending Service Changed.
    ble_gatt_server_restore_cccds(&server);
    assert(ble_gatt_server_check_database_version(&server));
    assert(peer.valid && peer.stores == 1 &&
           memcmp(peer.hash, server.database_hash, 16) == 0 &&
           !server.event_count);

    // A changed database is retained as pending until its indication confirms.
    peer.hash[0] ^= 0x80;
    ble_gatt_server_link_reset(&server);
    ble_gatt_server_restore_cccds(&server);
    assert(server.database_hash_update_pending && server.event_count == 1 &&
           peer.stores == 1);
    assert(ble_gatt_server_set_cccd(&server,
        handles.service_changed_handle, 0));
    assert(!server.database_hash_update_pending && !server.event_count);
    assert(ble_gatt_server_set_cccd(&server,
        handles.service_changed_handle, 2));
    assert(server.database_hash_update_pending && server.event_count == 1);
    uint8_t indication[BLE_GATT_SERVER_MTU_MAX];
    uint16_t indication_len;
    assert(ble_gatt_server_poll_event(&server, 10, indication,
        sizeof(indication), &indication_len) == 1);
    assert(indication_len == 7 && indication[0] == 0x1d &&
           ble_gatt_server_u16(indication + 3) == 1 &&
           ble_gatt_server_u16(indication + 5) == 0xffff);

    // A disconnect before confirmation leaves the old version persisted.
    ble_gatt_server_link_reset(&server);
    assert(server.database_hash_update_pending == 0 && peer.stores == 1);
    ble_gatt_server_restore_cccds(&server);
    peer.service_changed_cccd = 0;
    ble_gatt_server_restore_cccds(&server);
    assert(!ble_gatt_server_check_database_version(&server));
    assert(peer.stores == 1 && !server.event_count);

    // Enabling indications later through ATT schedules the pending change.
    uint8_t enable_changed[] = {0x12,
        (uint8_t)handles.service_changed_cccd_handle,
        (uint8_t)(handles.service_changed_cccd_handle >> 8), 2, 0};
    uint8_t write_response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t write_response_len;
    assert(ble_gatt_server_att(&server, enable_changed,
        sizeof(enable_changed), write_response, sizeof(write_response),
        &write_response_len) == 1);
    assert(write_response[0] == 0x13);
    assert(server.database_hash_update_pending && server.event_count == 1);
    assert(ble_gatt_server_poll_event(&server, 20, indication,
        sizeof(indication), &indication_len) == 1);
    const uint8_t confirmation[] = {0x1e};
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len;
    assert(ble_gatt_server_att(&server, confirmation, sizeof(confirmation),
        response, sizeof(response), &response_len) == 0);
    assert(!server.database_hash_update_pending && peer.stores == 2 &&
           memcmp(peer.hash, server.database_hash, 16) == 0);
    return 0;
}
