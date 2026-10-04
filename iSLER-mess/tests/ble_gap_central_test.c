#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../ble_gap.h"

uint32_t GET_MILLIS(void) { return 0; }
void BLE_GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]) {
    const uint8_t value[6] = {1, 2, 3, 4, 5, 6};
    memcpy(address, value, sizeof(value));
}
void BLE_GAP_HW_RANDOM_BYTES(uint8_t *out, size_t len) {
    static const uint8_t value[4] = {0x78, 0x56, 0x34, 0x12};
    for (size_t i = 0; i < len; i++) out[i] = value[i % sizeof(value)];
}

static void test_access_address_rules(void) {
    assert(gap_access_address_valid(0x12345678));
    assert(!gap_access_address_valid(BLE_ADV_ACCESS_ADDRESS));
    assert(!gap_access_address_valid(BLE_ADV_ACCESS_ADDRESS ^ 1));
    assert(!gap_access_address_valid(0x01010101));
    assert(!gap_access_address_valid(0x00000000));
}

static void test_connect_request(void) {
    const uint8_t peer[6] = {0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6};
    assert(mesh_gap_connect_start(peer, 0));
    assert(gap_central_connect.active && gap_scanning && !gap_active_scanning);
    const uint8_t *request = gap_central_connect.request;
    assert(request[0] == 0x05 && request[1] == 34);
    assert(memcmp(request + 2, (uint8_t[]){1, 2, 3, 4, 5, 6}, 6) == 0);
    assert(memcmp(request + 8, peer, 6) == 0);
    assert(gap_access_address_valid((uint32_t)request[14] |
        (uint32_t)request[15] << 8 | (uint32_t)request[16] << 16 |
        (uint32_t)request[17] << 24));
    assert(request[21] == 1 && request[24] == 24 && request[28] == 200);
    assert(request[30] == 0xff && request[33] == 0xff && request[34] == 0x1f);
    assert(!mesh_gap_connect_start(peer, 0));
    mesh_gap_connect_cancel();
    assert(!gap_central_connect.active && !gap_scanning);

    const uint8_t local_random[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0xc6};
    assert(mesh_gap_set_static_random_address(local_random));
    assert(mesh_gap_connect_start(peer, 1));
    request = gap_central_connect.request;
    assert(request[0] == 0xc5);
    assert(memcmp(request + 2, local_random, 6) == 0);
    mesh_gap_connect_cancel();
}

int main(void) {
    test_access_address_rules();
    test_connect_request();
    return 0;
}
