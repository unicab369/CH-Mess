#include <assert.h>
#include <stdint.h>
#include <openssl/aes.h>
#include <string.h>

#include "../ble_gap.h"

static uint32_t now_ms;
static uint8_t rx_frame[40], random_seed;
static const uint8_t *tx_buffer;
static int link_tx_count, aes_count;
static uint8_t forced_random[6], force_random;
uint32_t GET_MILLIS(void) { return now_ms; }
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
    aes_count++;
    AES_KEY aes;
    assert(AES_set_encrypt_key(key, 128, &aes) == 0);
    AES_encrypt(in, out, &aes);
}
const uint8_t *BLE_GAP_HW_RX_FRAME(void) { return rx_frame; }
int8_t BLE_GAP_HW_RSSI(void) { return -40; }
uint64_t BLE_GAP_HW_TICKS(void) { return (uint64_t)now_ms * 1000; }
uint64_t HW_TICKS_FROM_US(uint32_t us) { return us; }
void BLE_GAP_HW_STOP(void) {}
void BLE_GAP_HW_PACKET_CLEAR(void) {}
void BLE_GAP_HW_PACKET_READY(void) {}
void BLE_GAP_HW_TX_BUFFER(const uint8_t *frame) { tx_buffer = frame; }
void BLE_GAP_HW_LINK_TX(void) { link_tx_count++; }
int BLE_GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel) {
    (void)frame; (void)len; (void)channel;
    return 1;
}
void BLE_GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]) {
    const uint8_t value[6] = {1, 2, 3, 4, 5, 6};
    memcpy(address, value, sizeof(value));
}
void BLE_GAP_HW_RANDOM_BYTES(uint8_t *out, size_t len) {
    if (force_random) {
        for (size_t i = 0; i < len; i++) out[i] = forced_random[i % 6];
        return;
    }
    static const uint8_t value[4] = {0x78, 0x56, 0x34, 0x12};
    for (size_t i = 0; i < len; i++) out[i] = value[i % sizeof(value)] + (len == 4 ? 0 : random_seed);
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

// Core specification Vol 3 Part H, Appendix D.7; IRK is AES byte order.
static const uint8_t test_irk[16] = {
    0xec, 0x02, 0x34, 0xa3, 0x57, 0xc8, 0xad, 0x05,
    0x34, 0x10, 0x10, 0xa6, 0x0a, 0x39, 0x7d, 0x9b
};
static const uint8_t test_rpa[6] = {0xaa, 0xfb, 0x0d, 0x94, 0x81, 0x70};
static const uint8_t test_identity[6] = {1, 2, 3, 4, 5, 6};

static void test_address_resolution(void) {
    uint8_t hash[3], identity[6], type;
    gap_address_hash(test_irk, test_rpa + 3, hash);
    assert(memcmp(hash, test_rpa, 3) == 0);
    assert(mesh_gap_identity_set(test_identity, 0, test_irk));
    assert(mesh_gap_resolve(test_rpa, 1, identity, &type));
    assert(type == 0 && memcmp(identity, test_identity, 6) == 0);
    assert(mesh_gap_resolve(test_identity, 0, identity, &type));
    assert(!mesh_gap_resolve(test_rpa, 0, identity, &type));
    uint8_t bad_rpa[6];
    memcpy(bad_rpa, test_rpa, 6);
    bad_rpa[0] ^= 1;
    assert(!mesh_gap_resolve(bad_rpa, 1, identity, &type));
    assert(!mesh_gap_resolve(NULL, 1, identity, &type));
    for (uint8_t i = 1; i < GAP_IDENTITY_COUNT; i++) {
        uint8_t peer[6] = {i, 0, 0, 0, 0, 0};
        assert(mesh_gap_identity_set(peer, 0, (uint8_t[16]){0}));
    }
    assert(!mesh_gap_identity_set((uint8_t[]){99, 0, 0, 0, 0, 0}, 0, test_irk));
    assert(mesh_gap_identity_set(test_identity, 0, NULL));
    assert(!mesh_gap_resolve(test_rpa, 1, identity, &type));
    assert(mesh_gap_identity_set(test_identity, 0, test_irk));
}

static void test_private_rotation(void) {
    assert(!mesh_gap_privacy_set(test_irk, 0));
    assert(!mesh_gap_privacy_set(test_irk, 41401));
    assert(mesh_gap_privacy_set(test_irk, 1));
    uint8_t first[6], identity[6], type;
    memcpy(first, gap_random_address, 6);
    assert((first[5] & 0xc0) == 0x40);
    assert(mesh_gap_resolve(first, 1, identity, &type));
    assert(mesh_gap_advertising_start(NULL, 0, 100));
    assert(!mesh_gap_privacy_set(NULL, 0));
    now_ms = 999;
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    now_ms = 1000;
    gap_radio_active_scan_pending = 1;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    gap_radio_active_scan_pending = 0;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) != 0);
    assert(memcmp(gap_advertising.address, gap_random_address, 6) == 0);
    assert(mesh_gap_resolve(gap_random_address, 1, identity, &type));
    mesh_gap_advertising_stop();
    // Initiation retains InitA even if the rotation timeout expires.
    assert(mesh_gap_connect_start(test_identity, 0));
    memcpy(first, gap_random_address, 6);
    now_ms = 2000;
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    mesh_gap_connect_cancel();
    gap_conn.active = 1;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    gap_conn.active = 0;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) != 0);
    assert(mesh_gap_privacy_set(NULL, 0));
    assert(!gap_own_address_type);
}

static void test_scan_identity_filter(void) {
    assert(mesh_gap_privacy_filter(1, 1));
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 1));
    mesh_gap_scan_start();
    uint8_t frame[8] = {0x42, 6}; // ADV_NONCONN_IND with random AdvA
    memcpy(frame + 2, test_rpa, 6);
    gap_receive_report(frame, 6, -40);
    mesh_gap_scan_report report;
    assert(mesh_gap_scan_poll(&report));
    assert(report.resolved && report.identity_type == 0);
    assert(memcmp(report.address, test_rpa, 6) == 0);
    assert(memcmp(report.identity_address, test_identity, 6) == 0);
    // Another RPA with the same identity and payload is a duplicate.
    uint8_t prand[3] = {3, 2, 0x41};
    memcpy(frame + 5, prand, 3);
    gap_address_hash(test_irk, prand, frame + 2);
    gap_receive_report(frame, 6, -40);
    assert(!mesh_gap_scan_poll(&report));
    frame[2] ^= 1;
    gap_receive_report(frame, 6, -40);
    assert(!mesh_gap_scan_poll(&report));
    assert(!mesh_gap_identity_set(test_identity, 0, NULL));
    mesh_gap_scan_stop();
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
}

static void test_radio_privacy_filter(void) {
    mesh_gap_active_scan_start();
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_rpa, 6);
    rx_frame[2] ^= 1;
    int before = link_tx_count;
    gap_hw_mesh_received();
    assert(link_tx_count == before && !gap_radio_active_scan_pending);
    rx_frame[2] ^= 1;
    gap_hw_mesh_received();
    assert(link_tx_count == before + 1 && gap_radio_active_scan_pending);
    assert(memcmp(tx_buffer + 8, test_rpa, 6) == 0);
    mesh_gap_scan_stop();
    gap_radio_active_scan_pending = 0;
    gap_radio_advertising_rx_event = 1;
    gap_radio_adv_frame[0] = 0; // Connectable advertisement with public address
    BLE_GAP_HW_PUBLIC_ADDRESS(gap_radio_adv_frame + 2);
    rx_frame[0] = 0x43; rx_frame[1] = 12; // SCAN_REQ from random address
    memcpy(rx_frame + 8, gap_radio_adv_frame + 2, 6);
    rx_frame[2] ^= 1;
    before = link_tx_count;
    gap_hw_mesh_received();
    assert(link_tx_count == before);
    rx_frame[2] ^= 1;
    gap_hw_mesh_received();
    assert(link_tx_count == before + 1);
    rx_frame[0] = 0x45; rx_frame[1] = 34; // CONNECT_IND from the peer RPA
    rx_frame[2] ^= 1;
    gap_radio_connect_request_ready = 0;
    gap_hw_mesh_received();
    assert(!gap_radio_connect_request_ready);
    rx_frame[2] ^= 1;
    gap_hw_mesh_received();
    assert(gap_radio_connect_request_ready);
    gap_radio_advertising_rx_event = 0;
    gap_radio_connect_request_ready = 0;
    assert(mesh_gap_privacy_filter(0, 0));
}

static void test_connect_by_identity(void) {
    assert(mesh_gap_connect_start(test_identity, 0));
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active && gap_conn.central_role);
    assert(!gap_central_connect.active);
    assert(gap_central_connect.request[0] & 0x80);
    assert(memcmp(gap_central_connect.request + 8, test_rpa, 6) == 0);
    gap_connection_end();
}

static void test_directed_connect_target(void) {
    random_seed++;
    assert(mesh_gap_privacy_set(test_irk, 1));
    assert(mesh_gap_connect_start(test_identity, 0));
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0xc1; rx_frame[1] = 12; // ADV_DIRECT_IND with random addresses
    memcpy(rx_frame + 2, test_rpa, 6);
    memcpy(rx_frame + 8, test_rpa, 6); // RPA matching our local IRK
    rx_frame[8] ^= 1;
    gap_hw_mesh_received();
    assert(!gap_conn.active && gap_central_connect.active);
    rx_frame[8] ^= 1;
    rx_frame[1] = 11;
    gap_hw_mesh_received();
    assert(!gap_conn.active && gap_central_connect.active);
    rx_frame[1] = 12;
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_central_connect.active);
    gap_connection_end();
    assert(mesh_gap_privacy_set(NULL, 0));
    assert(mesh_gap_connect_start(test_identity, 0));
    rx_frame[0] = 0x41; // Public TargetA, random AdvA
    BLE_GAP_HW_PUBLIC_ADDRESS(rx_frame + 8);
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_central_connect.active);
    gap_connection_end();
}

static void test_peer_privacy_modes(void) {
    uint8_t identity[6], type;
    assert(gap_identities[gap_identity_find(test_identity, 0)].privacy_mode ==
           MESH_GAP_PRIVACY_NETWORK);
    assert(!mesh_gap_identity_privacy(NULL, 0, MESH_GAP_PRIVACY_DEVICE));
    assert(!mesh_gap_identity_privacy(test_identity, 2, MESH_GAP_PRIVACY_DEVICE));
    assert(!mesh_gap_identity_privacy(test_identity, 0, 2));
    assert(!mesh_gap_identity_privacy(test_rpa, 1, MESH_GAP_PRIVACY_DEVICE));
    assert(!mesh_gap_identity_privacy((uint8_t[]){99, 0, 0, 0, 0, 0}, 0, 1));
    // Resolution identifies host-selected peers independently of RX policy.
    assert(mesh_gap_resolve(test_identity, 0, identity, &type));
    assert(mesh_gap_privacy_filter(0, 0));
    mesh_gap_scan_start();
    uint8_t frame[8] = {2, 6};
    memcpy(frame + 2, test_identity, 6);
    mesh_gap_scan_report report;
    gap_receive_report(frame, 6, -40);
    assert(!mesh_gap_scan_poll(&report));
    assert(!mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_DEVICE));
    frame[0] = 0x42;
    memcpy(frame + 2, test_rpa, 6);
    gap_receive_report(frame, 6, -40);
    assert(mesh_gap_scan_poll(&report) && report.resolved);
    // Unlisted peers continue to obey the optional filter setting.
    frame[2] ^= 1;
    gap_receive_report(frame, 6, -40);
    assert(mesh_gap_scan_poll(&report) && !report.resolved);
    mesh_gap_scan_stop();
    assert(mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_DEVICE));
    assert(mesh_gap_identity_set(test_identity, 0, test_irk)); // Preserve mode
    mesh_gap_scan_start();
    frame[0] = 2;
    memcpy(frame + 2, test_identity, 6);
    gap_receive_report(frame, 6, -40);
    assert(mesh_gap_scan_poll(&report) && report.resolved);
    mesh_gap_scan_stop();
    assert(mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_NETWORK));
    // All-zero IRKs still allow identity addresses in network mode.
    assert(mesh_gap_identity_set(test_identity, 0, (uint8_t[16]){0}));
    mesh_gap_scan_start();
    gap_receive_report(frame, 6, -40);
    assert(mesh_gap_scan_poll(&report));
    mesh_gap_scan_stop();
    assert(mesh_gap_identity_set(test_identity, 0, test_irk));

    // An active scan must not send SCAN_REQ to the prohibited identity.
    mesh_gap_active_scan_start();
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_identity, 6);
    int before = link_tx_count;
    gap_hw_mesh_received();
    assert(link_tx_count == before && !gap_radio_active_scan_pending);
    mesh_gap_scan_stop();

    // Incoming SCAN_REQ and CONNECT_IND obey the peer mode without filters.
    gap_radio_advertising_rx_event = 1;
    gap_radio_adv_frame[0] = 0;
    BLE_GAP_HW_PUBLIC_ADDRESS(gap_radio_adv_frame + 2);
    rx_frame[0] = 3; rx_frame[1] = 12;
    memcpy(rx_frame + 8, gap_radio_adv_frame + 2, 6);
    gap_hw_mesh_received();
    assert(link_tx_count == before);
    rx_frame[0] = 5; rx_frame[1] = 34;
    gap_radio_connect_request_ready = 0;
    gap_hw_mesh_received();
    assert(!gap_radio_connect_request_ready);
    rx_frame[0] = 0x45;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_radio_connect_request_ready);
    gap_radio_advertising_rx_event = 0;
    gap_radio_connect_request_ready = 0;
    assert(mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_DEVICE));
    gap_radio_advertising_rx_event = 1;
    rx_frame[0] = 3; rx_frame[1] = 12;
    memcpy(rx_frame + 2, test_identity, 6);
    gap_hw_mesh_received();
    assert(link_tx_count == before + 1);
    rx_frame[0] = 5; rx_frame[1] = 34;
    gap_hw_mesh_received();
    assert(gap_radio_connect_request_ready);
    gap_radio_advertising_rx_event = 0;
    gap_radio_connect_request_ready = 0;
    assert(mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_NETWORK));

    // Host requests use the identity; only an allowed on-air address connects.
    assert(mesh_gap_connect_start(test_identity, 0));
    rx_frame[0] = 0; rx_frame[1] = 6;
    gap_hw_mesh_received();
    assert(!gap_conn.active && gap_central_connect.active);
    rx_frame[0] = 0x40;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_central_connect.active);
    gap_connection_end();
    assert(mesh_gap_identity_privacy(test_identity, 0, MESH_GAP_PRIVACY_DEVICE));
    assert(mesh_gap_connect_start(test_identity, 0));
    rx_frame[0] = 0;
    memcpy(rx_frame + 2, test_identity, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active);
    gap_connection_end();
}

static void test_peer_local_keys(void) {
    uint8_t local_irk[16], hash[3], first[6];
    memcpy(local_irk, test_irk, 16);
    local_irk[0] ^= 1;
    int slot = gap_identity_find(test_identity, 0);
    assert(mesh_gap_use_public_address());
    assert(!mesh_gap_identity_local_key(NULL, 0, local_irk));
    assert(!mesh_gap_identity_local_key(test_identity, 2, local_irk));
    assert(!mesh_gap_identity_local_key(test_rpa, 1, local_irk));
    assert(mesh_gap_identity_local_key(test_identity, 0, local_irk));
    assert(mesh_gap_identity_set(test_identity, 0, test_irk));
    assert(memcmp(gap_identities[slot].local_irk, local_irk, 16) == 0);
    random_seed++;
    assert(mesh_gap_privacy_set(test_irk, 1));
    assert(mesh_gap_connect_start(test_identity, 0));
    assert(gap_central_connect.request[0] & 0x40);
    gap_address_hash(local_irk, gap_central_connect.request + 5, hash);
    assert(memcmp(hash, gap_central_connect.request + 2, 3) == 0);
    assert(!mesh_gap_identity_local_key(test_identity, 0, NULL));
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0xc1; rx_frame[1] = 12;
    memcpy(rx_frame + 2, test_rpa, 6);
    memcpy(rx_frame + 8, test_rpa, 6); // Target from global key must fail.
    gap_hw_mesh_received();
    assert(gap_central_connect.active && !gap_conn.active);
    gap_address_hash(local_irk, rx_frame + 11, rx_frame + 8);
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_central_connect.active);
    gap_connection_end();

    mesh_gap_active_scan_start();
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    gap_hw_mesh_received();
    assert(gap_radio_active_scan_pending);
    gap_address_hash(local_irk, gap_radio_scan_request + 5, hash);
    assert(memcmp(hash, gap_radio_scan_request + 2, 3) == 0);
    memcpy(first, gap_identities[slot].local_address, 6);
    now_ms = gap_privacy.next_rotation_ms;
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_identities[slot].local_address, 6) == 0);
    mesh_gap_scan_stop();
    gap_radio_active_scan_pending = 0;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_identities[slot].local_address, 6) != 0);

    assert(mesh_gap_directed_advertising_start(test_identity, 0, 100));
    assert(gap_advertising.peer_slot == slot && gap_advertising.address_type == 1);
    gap_address_hash(local_irk, gap_advertising.address + 3, hash);
    assert(memcmp(hash, gap_advertising.address, 3) == 0);
    gap_address_hash(test_irk, gap_advertising.target_address + 3, hash);
    assert(gap_advertising.target_type == 1);
    assert(memcmp(hash, gap_advertising.target_address, 3) == 0);
    memcpy(first, gap_advertising.address, 6);
    random_seed++;
    now_ms = gap_privacy.next_rotation_ms;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_advertising.address, 6) != 0);
    gap_address_hash(local_irk, gap_advertising.address + 3, hash);
    assert(memcmp(hash, gap_advertising.address, 3) == 0);
    // The peer may initiate with a different RPA from the directed TargetA.
    gap_radio_advertising_rx_event = 1;
    gap_radio_adv_frame[0] = 0xc1;
    memcpy(gap_radio_adv_frame + 2, gap_advertising.address, 6);
    memcpy(gap_radio_adv_frame + 8, gap_advertising.target_address, 6);
    rx_frame[0] = 0xc5; rx_frame[1] = 34;
    memcpy(rx_frame + 2, test_rpa, 6);
    memcpy(rx_frame + 8, gap_advertising.address, 6);
    gap_radio_connect_request_ready = 0;
    gap_hw_mesh_received();
    assert(gap_radio_connect_request_ready);
    gap_radio_advertising_rx_event = 0;
    gap_radio_connect_request_ready = 0;
    mesh_gap_advertising_stop();

    // An explicit zero key uses the configured local identity, never an RPA.
    assert(mesh_gap_identity_local_key(test_identity, 0, (uint8_t[16]){0}));
    assert(mesh_gap_connect_start(test_identity, 0));
    assert(!(gap_central_connect.request[0] & 0x40));
    assert(memcmp(gap_central_connect.request + 2, test_identity, 6) == 0);
    rx_frame[0] = 0xc1; rx_frame[1] = 12;
    memcpy(rx_frame + 8, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_central_connect.active && !gap_conn.active);
    mesh_gap_connect_cancel();
    assert(mesh_gap_privacy_set(NULL, 0));
    uint8_t static_identity[6] = {9, 8, 7, 6, 5, 0xc4};
    assert(mesh_gap_set_static_random_address(static_identity));
    random_seed++;
    assert(mesh_gap_privacy_set(test_irk, 1));
    assert(mesh_gap_connect_start(test_identity, 0));
    assert(gap_central_connect.request[0] & 0x40);
    assert(memcmp(gap_central_connect.request + 2, static_identity, 6) == 0);
    mesh_gap_connect_cancel();
    // Null removes the override and restores the global RPA.
    assert(mesh_gap_identity_local_key(test_identity, 0, NULL));
    assert(mesh_gap_connect_start(test_identity, 0));
    assert(memcmp(gap_central_connect.request + 2, gap_random_address, 6) == 0);
    mesh_gap_connect_cancel();
    assert(mesh_gap_privacy_set(NULL, 0));
}

static void test_nonresolvable_private_addresses(void) {
    uint8_t first[6], identity[6], type;
    assert(mesh_gap_use_public_address());
    assert(mesh_gap_identity_local_key(test_identity, 0, test_irk));
    int before = aes_count;
    assert(!mesh_gap_privacy_set(NULL, 41401));
    random_seed++;
    assert(mesh_gap_privacy_set(NULL, 1));
    assert(gap_privacy.enabled && !gap_privacy.resolvable);
    assert((gap_random_address[5] & 0xc0) == 0);
    assert(aes_count == before);
    assert(!mesh_gap_resolve(gap_random_address, 1, identity, &type));
    assert(!mesh_gap_directed_advertising_start(test_identity, 0, 100));
    assert(mesh_gap_advertising_start(NULL, 0, 100));
    memcpy(first, gap_random_address, 6);
    now_ms = gap_privacy.next_rotation_ms - 1;
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    now_ms++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) != 0);
    assert(memcmp(gap_advertising.address, gap_random_address, 6) == 0);
    assert(aes_count == before);
    mesh_gap_advertising_stop();
    // Local peer keys cannot replace a selected NRPA when initiating.
    assert(mesh_gap_connect_start(test_identity, 0));
    assert((gap_central_connect.request[7] & 0xc0) == 0);
    assert(memcmp(gap_central_connect.request + 2, gap_random_address, 6) == 0);
    mesh_gap_connect_cancel();
    // Outstanding active scan exchanges retain their NRPA through timeout.
    mesh_gap_active_scan_start();
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_radio_active_scan_pending);
    assert(memcmp(gap_radio_scan_request + 2, gap_random_address, 6) == 0);
    memcpy(first, gap_random_address, 6);
    now_ms = gap_privacy.next_rotation_ms;
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) == 0);
    mesh_gap_scan_stop();
    gap_radio_active_scan_pending = 0;
    before = aes_count;
    gap_privacy_poll(now_ms);
    assert(memcmp(first, gap_random_address, 6) != 0 && aes_count == before);
    memcpy(first, gap_random_address, 6);
    // Reject degenerate random values and equality with the public/old address.
    force_random = 1;
    memset(forced_random, 0, 6);
    assert(!mesh_gap_privacy_set(NULL, 1));
    memset(forced_random, 0xff, 6);
    assert(!mesh_gap_privacy_set(NULL, 1));
    BLE_GAP_HW_PUBLIC_ADDRESS(forced_random);
    assert(!mesh_gap_privacy_set(NULL, 1));
    memcpy(forced_random, first, 6);
    assert(!mesh_gap_privacy_set(NULL, 1));
    assert(memcmp(first, gap_random_address, 6) == 0);
    assert(aes_count == before);
    force_random = 0;
    // RPA selection still works after NRPA mode, and zero timeout disables it.
    assert(mesh_gap_privacy_set(test_irk, 1));
    assert(gap_privacy.resolvable && (gap_random_address[5] & 0xc0) == 0x40);
    assert(mesh_gap_privacy_set(NULL, 0));
    assert(!gap_privacy.enabled && !gap_own_address_type);
}

int main(void) {
    test_access_address_rules();
    test_connect_request();
    test_address_resolution();
    test_private_rotation();
    test_scan_identity_filter();
    test_radio_privacy_filter();
    test_connect_by_identity();
    test_directed_connect_target();
    test_peer_privacy_modes();
    test_peer_local_keys();
    test_nonresolvable_private_addresses();
    return 0;
}
