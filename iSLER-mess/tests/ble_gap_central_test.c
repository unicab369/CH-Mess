#include <assert.h>
#include <stdint.h>
#include <openssl/aes.h>
#include <string.h>

#include "../ble_gap.h"
#include "../mesh_crypto.h"
#include <openssl/evp.h>

static uint32_t now_ms;
static uint8_t radio_phy_mask = 3, configured_tx_phy, configured_rx_phy;
static uint16_t radio_data_max = MESH_GAP_CONN_DATA_MAX;
static uint8_t rx_frame[6 + (MESH_GAP_CONN_DATA_MAX > 37 ? MESH_GAP_CONN_DATA_MAX : 37)], random_seed;
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
static uint8_t secure_random_available = 1, secure_random_seed, secure_random_disconnect;
uint32_t BLE_GAP_CRITICAL_ENTER(void) { return 0; }
void BLE_GAP_CRITICAL_EXIT(uint32_t state) { (void)state; }
static uint8_t secure_random_forced[12], secure_random_force;
int BLE_GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len) {
    if (!secure_random_available) return 0;
    if (secure_random_disconnect) gap_connection_end();
    if (secure_random_force) { assert(len == 12); memcpy(out, secure_random_forced, 12); return 1; }
    for (size_t i = 0; i < len; i++) out[i] = secure_random_seed++;
    return 1;
}
int BLE_GAP_CCM_ENCRYPT(const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, uint8_t mic[4]) {
    return ccm_encrypt_and_tag(key, nonce, 13, &aad, 1, data, len, data, mic, 4) == CCM_OK;
}
int BLE_GAP_CCM_DECRYPT(const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, const uint8_t mic[4]) {
    return ccm_auth_decrypt(key, nonce, 13, &aad, 1, data, len, mic, 4, data) == CCM_OK;
}
const uint8_t *BLE_GAP_HW_RX_FRAME(void) { return rx_frame; }
uint16_t BLE_GAP_HW_DATA_MAX(void) { return radio_data_max; }
uint8_t BLE_GAP_HW_PHY_MASK(void) { return radio_phy_mask; }
int8_t BLE_GAP_HW_RSSI(void) { return -40; }
uint64_t BLE_GAP_HW_TICKS(void) { return (uint64_t)now_ms * 1000; }
uint64_t HW_TICKS_FROM_US(uint32_t us) { return us; }
void BLE_GAP_HW_STOP(void) {}
void BLE_GAP_HW_PACKET_CLEAR(void) {}
void BLE_GAP_HW_PACKET_READY(void) {}
void BLE_GAP_HW_TX_BUFFER(const uint8_t *frame) { tx_buffer = frame; }
void BLE_GAP_HW_LINK_TX(void) { link_tx_count++; }
int BLE_GAP_HW_TX_DONE(void) { return 1; }
void BLE_GAP_HW_TX_CLEAR_DONE(void) {}
void BLE_GAP_HW_CRC_INIT(uint32_t crc_init) { (void)crc_init; }
void BLE_GAP_HW_LINK_CONFIG(uint32_t access_address, uint8_t channel,
                            uint8_t *frame, uint8_t receive_after_tx, uint8_t tx_phy, uint8_t rx_phy) {
    (void)access_address; (void)channel; (void)receive_after_tx;
    tx_buffer = frame;
    configured_tx_phy = tx_phy;
    configured_rx_phy = rx_phy;
}
void BLE_GAP_HW_LINK_RX(void) {}
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

static void start_test_central_link(void) {
    assert(mesh_gap_connect_start(test_identity, 0));
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active && gap_conn.central_role);
    now_ms += 2;
    mesh_gap_conn_poll();
    assert(gap_conn.rx_armed);
    rx_frame[0] = 0x05; rx_frame[1] = 0; // ACK the first empty Central packet.
    gap_hw_mesh_received();
    assert(!gap_conn.first_event);
    mesh_gap_conn_poll(); // Complete the reply before the next event.
}

static void receive_test_link_packet(uint8_t acknowledged) {
    gap_conn.rx_armed = 1;
    rx_frame[0] = 1 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ acknowledged) << 2);
    rx_frame[1] = 0;
    gap_hw_mesh_received();
}

static void test_connection_timing_updates(void) {
    assert(!mesh_gap_connection_update(48, 0, 300));
    start_test_central_link();
    assert(!mesh_gap_connection_update(5, 0, 300));
    assert(!mesh_gap_connection_update(3201, 0, 300));
    assert(!mesh_gap_connection_update(48, 500, 300));
    assert(!mesh_gap_connection_update(48, 0, 9));
    assert(!mesh_gap_connection_update(48, 0, 3201));
    assert(!mesh_gap_connection_update(40, 0, 10)); // Timeout equals 2 intervals.
    gap_conn.central_role = 0;
    assert(!mesh_gap_connection_update(48, 0, 300));
    gap_conn.central_role = 1;
    gap_conn.channel_map_update_pending = 1;
    assert(!mesh_gap_connection_update(48, 0, 300));
    gap_conn.channel_map_update_pending = 0;
    assert(mesh_gap_connection_update(48, 1, 300));
    assert(!mesh_gap_connection_update(48, 1, 300));
    assert(gap_conn.local_update_queued && !gap_conn.update_pending);
    // Do not overwrite a previous packet while it waits for acknowledgement.
    receive_test_link_packet(0);
    assert(gap_conn.local_update_queued && !gap_conn.update_pending);
    receive_test_link_packet(1);
    assert(!gap_conn.local_update_queued && gap_conn.update_pending);
    assert((gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 12);
    assert(memcmp(gap_conn_tx_frame + 2,
        (uint8_t[]){0, 1, 0, 0, 48, 0, 1, 0, 44, 1}, 10) == 0);
    uint16_t instant = gap_conn.update_instant;
    assert((uint16_t)(instant - gap_conn.event_counter) >= 6);
    uint8_t payload[12];
    memcpy(payload, gap_conn_tx_frame + 2, 12);
    receive_test_link_packet(0);
    assert(memcmp(payload, gap_conn_tx_frame + 2, 12) == 0);
    receive_test_link_packet(1);
    while (gap_conn.event_counter != instant) {
        assert(gap_conn.interval == 24);
        receive_test_link_packet(1);
    }
    assert(gap_conn.active && gap_conn.interval == 48 && gap_conn.latency == 1);
    assert(gap_conn.supervision_timeout == 300 && !gap_conn.update_pending);
    uint64_t next_tick = gap_conn.next_event_ticks;
    receive_test_link_packet(1);
    assert(gap_conn.next_event_ticks == next_tick + 48u * 1250);
    gap_connection_end();

    // Generate a fresh Instant when polling sends the first queued update;
    // preserve its lead time across event counter wrap and Peripheral latency.
    start_test_central_link();
    gap_conn.event_counter = 0xfffe;
    gap_conn.latency = 3;
    gap_conn.tx_pending = 0;
    gap_conn.event_replied = 0;
    gap_conn.rx_armed = 0;
    gap_conn.next_event_ticks = (uint64_t)now_ms * 1000;
    assert(mesh_gap_connection_update(72, 0, 300));
    mesh_gap_conn_poll();
    assert(gap_conn.update_pending && !gap_conn.local_update_queued);
    assert((uint16_t)(gap_conn.update_instant - gap_conn.event_counter) == 25);
    assert(gap_conn.update_instant < 0xfffe);
    // Lost acknowledgements must not let the Central apply an unsynchronized
    // update or keep retransmitting an indication with an expired Instant.
    instant = gap_conn.update_instant;
    while (gap_conn.active) receive_test_link_packet(0);
    assert(gap_conn.event_counter == instant);
    assert(!gap_conn.update_pending && !gap_conn.local_update_queued);

    start_test_central_link();
    assert(mesh_gap_disconnect(0x13));
    assert(!mesh_gap_connection_update(48, 0, 300));
    gap_connection_end();
}

static void receive_test_control(uint8_t opcode, const uint8_t *data, uint8_t len) {
    gap_conn.rx_armed = 1;
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) | ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = len + 1;
    rx_frame[2] = opcode;
    if (len) memcpy(rx_frame + 3, data, len);
    gap_hw_mesh_received();
}

static void test_connection_parameter_requests(void) {
    const uint8_t features[8] = {0x0e};
    uint8_t parameters[23] = {48, 0, 60, 0, 1, 0, 44, 1, 10, 0, 0};
    memset(parameters + 11, 0xff, 12);
    assert(!mesh_gap_connection_request(48, 60, 1, 300));
    start_test_central_link();
    assert(!mesh_gap_connection_request(5, 60, 1, 300));
    assert(!mesh_gap_connection_request(61, 60, 1, 300));
    assert(!mesh_gap_connection_request(48, 3201, 1, 300));
    assert(!mesh_gap_connection_request(48, 60, 500, 300));
    assert(!mesh_gap_connection_request(48, 60, 1, 9));
    assert(!mesh_gap_connection_request(48, 60, 1, 3201));
    assert(!mesh_gap_connection_request(40, 40, 0, 10));
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    assert(mesh_gap_connection_status() == MESH_GAP_CONNECTION_PENDING);
    assert(!mesh_gap_connection_update(48, 1, 300));
    assert(!mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    assert(gap_conn.feature_request_pending && gap_conn_tx_frame[2] == 0x08);
    receive_test_control(0x09, features, sizeof(features));
    assert(gap_conn.features_known && gap_conn.params_pending);
    assert(gap_conn_tx_frame[2] == 0x0f && gap_conn_tx_frame[1] == 24);
    assert(memcmp(gap_conn_tx_frame + 3, parameters, 8) == 0);
    for (uint8_t i = 14; i < 26; i++) assert(gap_conn_tx_frame[i] == 0xff);
    receive_test_control(0x10, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0 && gap_conn.update_pending);
    assert(gap_conn.update_interval == 50); // Honor preferred periodicity.
    uint16_t instant = gap_conn.update_instant;
    while (gap_conn.event_counter != instant) receive_test_link_packet(1);
    assert(gap_conn.interval == 50 && mesh_gap_connection_status() == 0);
    gap_connection_end();

    // A Peripheral may initiate the request but waits for the Central's update.
    start_test_central_link();
    gap_conn.central_role = 0;
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x0e);
    receive_test_control(0x09, features, sizeof(features));
    assert(gap_conn_tx_frame[2] == 0x0f && gap_conn.params_pending);
    receive_test_control(0x11, (uint8_t[]){0x08, 0x20}, 2);
    assert(gap_conn.params_pending); // Unrelated rejection cannot cancel it.
    receive_test_control(0x11, (uint8_t[]){0x0f, 0x20}, 2);
    assert(!gap_conn.params_pending && mesh_gap_connection_status() == 0x20);
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    uint8_t update[11] = {1, 0, 0, 54, 0, 1, 0, 44, 1, 0, 0};
    instant = gap_conn.event_counter + 7;
    update[9] = (uint8_t)instant;
    update[10] = (uint8_t)(instant >> 8);
    receive_test_control(0x00, update, sizeof(update));
    assert(!gap_conn.params_pending && gap_conn.update_pending);
    while (gap_conn.event_counter != instant) receive_test_link_packet(1);
    assert(gap_conn.interval == 54 && mesh_gap_connection_status() == 0);
    gap_connection_end();

    // Responding to a Central request sends PARAM_RSP, never UPDATE_IND.
    start_test_central_link();
    gap_conn.central_role = 0;
    receive_test_control(0x08, features, sizeof(features));
    assert(gap_conn_tx_frame[2] == 9 && gap_conn_tx_frame[3] == 0x0e);
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    receive_test_control(0x0f, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0x10);
    assert(memcmp(gap_conn_tx_frame + 3, parameters, sizeof(parameters)) == 0);
    assert(gap_conn.params_pending && !gap_conn.params_local);
    assert(mesh_gap_connection_status() == 0x23); // Central wins the collision.
    gap_connection_end();

    // A Central accepts a Peripheral request directly with UPDATE_IND.
    start_test_central_link();
    receive_test_control(0x0e, features, sizeof(features));
    assert(gap_conn_tx_frame[2] == 9 && gap_conn_tx_frame[3] == 0x0e);
    parameters[0] = 5;
    receive_test_control(0x0f, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x1e);
    parameters[0] = 48;
    parameters[11] = 1; parameters[12] = 0;
    parameters[13] = 1; parameters[14] = 0; // Duplicate offset hints.
    receive_test_control(0x0f, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x1e);
    memset(parameters + 11, 0xff, 12);
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    receive_test_control(0x0f, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x23);
    assert(gap_conn.params_pending && gap_conn.params_local);
    receive_test_control(0x07, (uint8_t[]){0x0f}, 1);
    assert(!gap_conn.params_pending && mesh_gap_connection_status() == 0x1a);
    gap_connection_end();
    start_test_central_link();
    receive_test_control(0x0f, parameters, sizeof(parameters));
    assert(gap_conn_tx_frame[2] == 0 && gap_conn.update_pending);
    gap_connection_end();

    start_test_central_link();
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    receive_test_control(0x09, (uint8_t[8]){0}, 8);
    assert(!gap_conn.local_params_queued && mesh_gap_connection_status() == 0x1a);
    assert(!mesh_gap_connection_request(48, 60, 1, 300));
    gap_connection_end();

    // A responsive link with a stalled procedure still has a 40-second timeout.
    start_test_central_link();
    gap_conn.features_known = 1; gap_conn.peer_features = 0x0e;
    assert(mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    now_ms = gap_conn.params_started_ms + 40000;
    gap_conn.last_rx_ms = now_ms;
    mesh_gap_conn_poll();
    assert(!gap_conn.active && mesh_gap_connection_status() == 0x22);
}

// Test real wire fields, asymmetric peer limits, collisions and LL retries.
static void test_data_length(void) {
    const uint16_t capacity = MESH_GAP_CONN_DATA_MAX;
    uint8_t limits[8] = {251, 0, 0x48, 0x08, 251, 0, 0x48, 0x08};
    uint8_t data[MESH_GAP_CONN_DATA_MAX];
    memset(data, 0xa5, sizeof(data));
    start_test_central_link();
    mesh_gap_data_length state = mesh_gap_data_length_get();
    assert(state.tx_octets == 27 && state.rx_octets == 27 && state.tx_time == 328);
    assert(!mesh_gap_data_length_set(26));
    assert(!mesh_gap_data_length_set(capacity + 1));
    assert(!mesh_gap_send_data(1, data, 28));
    assert(mesh_gap_data_length_set(capacity));
    assert(!mesh_gap_connection_request(48, 60, 1, 300));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x14 && gap_conn_tx_frame[1] == 9);
    assert(gap_conn_tx_frame[3] == capacity && gap_conn_tx_frame[7] == capacity);
    uint16_t duration = (uint16_t)gap_conn_tx_frame[4] << 8 | gap_conn_tx_frame[3];
    assert(duration == capacity);
    duration = (uint16_t)gap_conn_tx_frame[6] << 8 | gap_conn_tx_frame[5];
    assert(duration == (capacity + 14) * 8);
    receive_test_link_packet(0);
    assert(gap_conn_tx_frame[2] == 0x14 && gap_conn.length_pending);
    // Simultaneous requests are answered while our own response remains pending.
    receive_test_control(0x14, limits, 8);
    assert(gap_conn_tx_frame[2] == 0x15 && gap_conn.length_pending);
    receive_test_control(0x15, limits, 8);
    assert(mesh_gap_data_length_status() == 0);
    state = mesh_gap_data_length_get();
    assert(state.tx_octets == capacity && state.rx_octets == capacity);
    assert(mesh_gap_send_data(1, data, capacity));
    // A shrink leaves the already queued fragment intact.
    limits[0] = 27; limits[1] = 0;
    limits[2] = 0x48; limits[3] = 0x01;
    receive_test_control(0x14, limits, 8);
    assert(gap_conn.tx_queued && gap_conn.tx_len == capacity);
    assert(mesh_gap_data_length_get().tx_octets == 27);
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[1] == capacity);
    receive_test_link_packet(1);
    assert(!mesh_gap_send_data(1, data, 28));
    // Receive a full negotiated fragment, including >27-byte configured builds.
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 1 | (gap_conn.expected_rx_sn << 3) | ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = capacity;
    memcpy(rx_frame + 2, data, capacity);
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    uint8_t llid;
    size_t received_len = sizeof(data);
    assert(mesh_gap_receive_data(&llid, data, &received_len) == 1);
    assert(received_len == capacity && llid == 1);
    // Time limits constrain data even when the peer allows more octets.
    limits[0] = 251;
    receive_test_control(0x14, limits, 8);
    assert(mesh_gap_data_length_get().tx_time == 328);
    if (capacity > 31) {
        assert(mesh_gap_send_data(1, data, 31));
        receive_test_link_packet(1);
        receive_test_link_packet(1);
        assert(!mesh_gap_send_data(1, data, 32));
    }
    // An unsolicited response must not change the negotiated parameters.
    limits[4] = 27;
    receive_test_control(0x15, limits, 8);
    assert(mesh_gap_data_length_get().rx_octets == capacity);
    limits[0] = 26;
    receive_test_control(0x14, limits, 8);
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x1e);
    gap_connection_end();

    start_test_central_link();
    assert(mesh_gap_data_length_get().tx_octets == 27);
    assert(mesh_gap_data_length_set(capacity));
    receive_test_link_packet(1);
    receive_test_control(0x07, (uint8_t[]){0x14}, 1);
    assert(!gap_conn.length_pending && mesh_gap_data_length_status() == 0x1a);
    assert(mesh_gap_data_length_set(capacity));
    receive_test_link_packet(1);
    receive_test_control(0x11, (uint8_t[]){0x14, 0x1e}, 2);
    assert(mesh_gap_data_length_status() == 0x1e);
    gap_connection_end();

    // Peripheral receives and initiates the same data length procedure.
    start_test_central_link();
    gap_conn.central_role = 0;
    limits[0] = limits[4] = 251;
    limits[2] = limits[6] = 0x48; limits[3] = limits[7] = 0x08;
    receive_test_control(0x14, limits, 8);
    assert(gap_conn_tx_frame[2] == 0x15);
    assert(mesh_gap_data_length_set(capacity));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x14);
    receive_test_control(0x15, limits, 8);
    assert(mesh_gap_data_length_status() == 0);
    gap_connection_end();

    start_test_central_link();
    gap_conn.features_known = 1; gap_conn.peer_features = 0x0e;
    assert(!mesh_gap_data_length_set(capacity));
    assert(mesh_gap_data_length_status() == 0x1a);
    gap_conn.features_known = 0;
    assert(mesh_gap_data_length_set(capacity));
    receive_test_link_packet(1);
    now_ms = gap_conn.length_started_ms + 40000;
    gap_conn.last_rx_ms = now_ms;
    mesh_gap_conn_poll();
    assert(!gap_conn.active && mesh_gap_data_length_status() == 0x22);

    // A smaller hardware capacity overrides a larger configured buffer.
    radio_data_max = 27;
    start_test_central_link();
    assert(!mesh_gap_data_length_set(28));
    receive_test_control(0x14, limits, 8);
    assert(gap_conn_tx_frame[3] == 27 && mesh_gap_data_length_get().rx_octets == 27);
    gap_connection_end();
    radio_data_max = MESH_GAP_CONN_DATA_MAX;
}

static void test_channel_map_updates(void) {
    const uint8_t channels[5] = {3, 0, 0, 0, 0};
    const uint8_t invalid[5] = {3, 0, 0, 0, 0x20};
    assert(!mesh_gap_channel_map_set(channels));
    start_test_central_link();
    assert(!mesh_gap_channel_map_set(NULL));
    assert(!mesh_gap_channel_map_set((uint8_t[5]){0}));
    assert(!mesh_gap_channel_map_set((uint8_t[5]){1}));
    assert(!mesh_gap_channel_map_set(invalid));
    assert(mesh_gap_channel_map_set(gap_conn.channel_map));
    assert(!gap_conn.local_map_queued && mesh_gap_connection_status() == 0);
    uint8_t old_map[5];
    memcpy(old_map, gap_conn.channel_map, 5);
    assert(mesh_gap_channel_map_set(channels));
    assert(mesh_gap_connection_status() == MESH_GAP_CONNECTION_PENDING);
    assert(!mesh_gap_channel_map_set(channels));
    assert(!mesh_gap_connection_update(48, 0, 200));
    assert(!mesh_gap_connection_request(48, 60, 0, 200));
    assert(!mesh_gap_data_length_set(27));
    // Earlier unacknowledged packets delay construction of the map PDU.
    receive_test_link_packet(0);
    assert(gap_conn.local_map_queued && !gap_conn.channel_map_update_pending);
    gap_conn.event_counter = 0xfffe;
    gap_conn.latency = 3;
    receive_test_link_packet(1);
    assert(!gap_conn.local_map_queued && gap_conn.channel_map_update_pending);
    assert(gap_conn_tx_frame[2] == 0x01 && gap_conn_tx_frame[1] == 8);
    assert(memcmp(gap_conn_tx_frame + 3, channels, 5) == 0);
    uint16_t instant = (uint16_t)gap_conn_tx_frame[8] |
        (uint16_t)gap_conn_tx_frame[9] << 8;
    assert(instant == 23); // The Instant wraps safely through event zero.
    assert(memcmp(gap_conn.channel_map, old_map, 5) == 0);
    uint8_t pending_frame[10];
    memcpy(pending_frame, gap_conn_tx_frame, sizeof(pending_frame));
    receive_test_link_packet(0);
    assert(memcmp(pending_frame + 1, gap_conn_tx_frame + 1, 9) == 0);
    while (gap_conn.event_counter != instant) {
        assert(memcmp(gap_conn.channel_map, old_map, 5) == 0);
        receive_test_link_packet(1);
    }
    assert(!gap_conn.channel_map_update_pending && mesh_gap_connection_status() == 0);
    assert(memcmp(gap_conn.channel_map, channels, 5) == 0);
    assert(gap_conn.used_count == 2 && gap_conn.used_channels[0] == 0 &&
        gap_conn.used_channels[1] == 1);
    gap_connection_end();

    // A peer timing request must not start a second procedure before our map is sent.
    start_test_central_link();
    assert(mesh_gap_channel_map_set(channels));
    uint8_t timing[23] = {48, 0, 60, 0, 0, 0, 200, 0};
    memset(timing + 11, 0xff, 12);
    receive_test_control(0x0f, timing, sizeof(timing));
    assert(gap_conn_tx_frame[2] == 0x11 && !gap_conn.update_pending);
    assert(gap_conn.local_map_queued && mesh_gap_connection_status() == MESH_GAP_CONNECTION_PENDING);
    gap_connection_end();

    // The Central foreground send path also builds a queued map update.
    start_test_central_link();
    assert(mesh_gap_channel_map_set(channels));
    gap_conn.tx_pending = gap_conn.event_replied = gap_conn.rx_armed = 0;
    now_ms = (uint32_t)((gap_conn.next_event_ticks + 999) / 1000);
    mesh_gap_conn_poll();
    assert(gap_conn_tx_frame[2] == 0x01 && gap_conn.channel_map_update_pending);
    // Reaching the Instant without an ACK must close the link.
    gap_conn.event_counter = gap_conn.channel_map_update_instant;
    gap_connection_update_apply(0);
    assert(!gap_conn.active && mesh_gap_connection_status() == 0x28);
    assert(!gap_conn.local_map_queued && !gap_conn.channel_map_update_pending);

    // Only the Central may initiate; a Peripheral applies a received update.
    start_test_central_link();
    gap_conn.central_role = 0;
    assert(!mesh_gap_channel_map_set(channels));
    uint8_t parameters[7] = {3, 0, 0, 0, 0, 0, 0};
    instant = gap_conn.event_counter + 7;
    parameters[5] = (uint8_t)instant;
    parameters[6] = (uint8_t)(instant >> 8);
    receive_test_control(0x01, parameters, sizeof(parameters));
    assert(gap_conn.channel_map_update_pending);
    while (gap_conn.event_counter != instant) receive_test_link_packet(1);
    assert(memcmp(gap_conn.channel_map, channels, 5) == 0);
    gap_connection_end();
}

static void test_phy_updates(void) {
    assert(!mesh_gap_phy_set(2, 2));
    start_test_central_link();
    uint8_t tx, rx;
    mesh_gap_phy_get(&tx, &rx);
    assert(tx == 1 && rx == 1);
    assert(!mesh_gap_phy_set(0, 2) && !mesh_gap_phy_set(4, 4));
    assert(mesh_gap_phy_set(2, 2));
    assert(!mesh_gap_connection_update(48, 0, 200));
    assert(!mesh_gap_channel_map_set((uint8_t[5]){3}));
    assert(!mesh_gap_data_length_set(27));
    receive_test_link_packet(0);
    assert(gap_conn.phy_queued && !gap_conn.phy_pending);
    gap_conn.event_counter = 0xfffd;
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x16 && gap_conn_tx_frame[3] == 2);
    receive_test_link_packet(0);
    assert(gap_conn_tx_frame[2] == 0x16 && mesh_gap_phy_status() == 0xff);
    // Central wins a simultaneous PHY request and keeps its pending request.
    receive_test_control(0x16, (uint8_t[]){2, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x23);
    assert(gap_conn.phy_pending);
    receive_test_control(0x17, (uint8_t[]){2, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x18 && gap_conn_tx_frame[3] == 2 && gap_conn_tx_frame[4] == 2);
    uint16_t instant = gap_conn.phy_instant;
    assert(gap_conn.phy_update_pending && instant < 10);
    while (gap_conn.event_counter != instant) {
        assert(gap_conn.tx_phy == 1 && gap_conn.rx_phy == 1);
        receive_test_link_packet(1);
    }
    mesh_gap_phy_get(&tx, &rx);
    assert(tx == 2 && rx == 2 && mesh_gap_phy_status() == 0);
    gap_conn.rx_armed = gap_conn.event_replied = 0;
    now_ms = (uint32_t)((gap_conn.next_event_ticks + 999) / 1000);
    mesh_gap_conn_poll();
    assert(configured_tx_phy == 2 && configured_rx_phy == 2);
    // A peer's asymmetric preference can change only one direction.
    receive_test_control(0x16, (uint8_t[]){1, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x18 && gap_conn_tx_frame[3] == 0 && gap_conn_tx_frame[4] == 0);
    // No shared preference: unchanged rates and immediate completion.
    assert(!gap_conn.phy_update_pending);
    gap_connection_end();

    // The Central can select different supported rates in each direction.
    start_test_central_link();
    receive_test_control(0x16, (uint8_t[]){1, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x18 && gap_conn_tx_frame[3] == 2 && gap_conn_tx_frame[4] == 0);
    instant = gap_conn.phy_instant;
    while (gap_conn.event_counter != instant) receive_test_link_packet(1);
    assert(gap_conn.tx_phy == 2 && gap_conn.rx_phy == 1);
    gap_connection_end();

    // A single identical peer preference cannot yield a new asymmetric link.
    start_test_central_link();
    assert(mesh_gap_phy_set(2, 1));
    receive_test_link_packet(1);
    receive_test_control(0x17, (uint8_t[]){2, 2}, 2);
    assert(gap_conn_tx_frame[3] == 0 && gap_conn_tx_frame[4] == 0);
    assert(mesh_gap_phy_status() == 0 && !gap_conn.phy_update_pending);
    gap_connection_end();

    // The foreground Central send path also starts a queued PHY request.
    start_test_central_link();
    assert(mesh_gap_phy_set(2, 2));
    gap_conn.tx_pending = gap_conn.event_replied = gap_conn.rx_armed = 0;
    now_ms = (uint32_t)((gap_conn.next_event_ticks + 999) / 1000);
    mesh_gap_conn_poll();
    assert(gap_conn_tx_frame[2] == 0x16 && gap_conn.phy_pending);
    gap_connection_end();

    // Peripheral responds and applies independent directions at the Instant.
    start_test_central_link();
    gap_conn.central_role = 0;
    assert(mesh_gap_phy_set(2, 2));
    receive_test_link_packet(1);
    receive_test_control(0x16, (uint8_t[]){2, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x17 && gap_conn.phy_pending);
    instant = gap_conn.event_counter + 7;
    receive_test_control(0x18, (uint8_t[]){2, 1, (uint8_t)instant, (uint8_t)(instant >> 8)}, 4);
    while (gap_conn.event_counter != instant) receive_test_link_packet(1);
    mesh_gap_phy_get(&tx, &rx);
    assert(tx == 1 && rx == 2);
    assert(mesh_gap_phy_status() == 0);
    gap_connection_end();

    // Unsupported UPDATE fields leave that direction unchanged.
    start_test_central_link(); gap_conn.central_role = 0;
    receive_test_control(0x18, (uint8_t[]){4, 3, 0, 0}, 4);
    assert(!gap_conn.phy_update_pending && gap_conn.tx_phy == 1 && gap_conn.rx_phy == 1);
    // Past Instants and unacknowledged Central updates close the link.
    receive_test_control(0x18, (uint8_t[]){2, 2, 0, 0}, 4);
    assert(!gap_conn.active && mesh_gap_phy_status() == 0x28);
    start_test_central_link();
    receive_test_control(0x16, (uint8_t[]){2, 2}, 2);
    assert(gap_conn.phy_update_pending);
    while (gap_conn.active) receive_test_link_packet(0);
    assert(mesh_gap_phy_status() == 0x28);

    start_test_central_link();
    assert(mesh_gap_phy_set(2, 2));
    receive_test_link_packet(1);
    receive_test_control(0x07, (uint8_t[]){0x16}, 1);
    assert(mesh_gap_phy_status() == 0x1a && !gap_conn.phy_pending);
    assert(mesh_gap_phy_set(2, 2));
    receive_test_link_packet(1);
    receive_test_control(0x11, (uint8_t[]){0x16, 0x20}, 2);
    assert(mesh_gap_phy_status() == 0x20);
    assert(mesh_gap_phy_set(2, 2));
    receive_test_link_packet(1);
    now_ms = gap_conn.phy_started_ms + 40000;
    gap_conn.last_rx_ms = now_ms;
    mesh_gap_conn_poll();
    assert(!gap_conn.active && mesh_gap_phy_status() == 0x22);

    // Feature exchange advertises 2M in byte 1, and caches the peer's support.
    start_test_central_link();
    receive_test_control(0x0e, (uint8_t[]){0x2e, 1, 0, 0, 0, 0, 0, 0}, 8);
    assert(gap_conn_tx_frame[2] == 0x09 && gap_conn_tx_frame[4] == 1);
    assert(mesh_gap_phy_set(2, 2));
    gap_connection_end();
    start_test_central_link();
    receive_test_control(0x0e, (uint8_t[8]){0x2e}, 8);
    assert(!mesh_gap_phy_set(2, 2) && mesh_gap_phy_status() == 0x1a);
    gap_connection_end();

    radio_phy_mask = 1;
    start_test_central_link();
    assert(!mesh_gap_phy_set(2, 2));
    receive_test_control(0x0e, (uint8_t[8]){0x2e}, 8);
    assert(gap_conn_tx_frame[4] == 0);
    receive_test_control(0x16, (uint8_t[]){2, 2}, 2);
    assert(gap_conn_tx_frame[2] == 0x07);
    gap_connection_end();
    radio_phy_mask = 3;
}

// Core Vol 6 Part C, section 1: encryption-start and data packet sample vectors.
static const uint8_t encryption_ltk[16] = {
    0xbf, 0x01, 0xfb, 0x9d, 0x4e, 0xf3, 0xbc, 0x36,
    0xd8, 0x74, 0xf5, 0x39, 0x41, 0x38, 0x68, 0x4c
};
static const uint8_t encryption_random[8] = {0x90, 0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab};
static const uint8_t central_entropy[12] = {
    0x13, 0x02, 0xf1, 0xe0, 0xdf, 0xce, 0xbd, 0xac, 0x24, 0xab, 0xdc, 0xba
};
static const uint8_t peripheral_entropy[12] = {
    0x79, 0x68, 0x57, 0x46, 0x35, 0x24, 0x13, 0x02, 0xbe, 0xba, 0xaf, 0xde
};

// Independently encrypt peer packets with OpenSSL to check the project's CCM
// against a second implementation, including AAD, direction and counter layout.
static void receive_secure_test_pdu(uint8_t llid, const uint8_t *payload,
                                     size_t len, uint64_t counter, uint8_t duplicate,
                                     uint8_t acknowledged, uint8_t tampered) {
    uint8_t nonce[13], aad;
    rx_frame[0] = llid | ((gap_conn.expected_rx_sn ^ duplicate) << 3) |
        ((gap_conn.tx_sn ^ acknowledged) << 2);
    rx_frame[1] = (uint8_t)(len + 4);
    for (uint8_t i = 0; i < 5; i++) nonce[i] = (uint8_t)(counter >> (8 * i));
    if (!gap_conn.central_role) nonce[4] |= 0x80;
    memcpy(nonce + 5, gap_security.iv, 8);
    aad = rx_frame[0] & 0xe3;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    assert(ctx);
    int produced;
    assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL));
    assert(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN, 13, NULL));
    assert(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, 4, NULL));
    assert(EVP_EncryptInit_ex(ctx, NULL, NULL, gap_security.session_key, nonce));
    assert(EVP_EncryptUpdate(ctx, NULL, &produced, NULL, (int)len));
    assert(EVP_EncryptUpdate(ctx, NULL, &produced, &aad, 1));
    assert(EVP_EncryptUpdate(ctx, rx_frame + 2, &produced, payload, (int)len));
    assert(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, 4, rx_frame + 2 + len));
    EVP_CIPHER_CTX_free(ctx);
    if (tampered) rx_frame[2 + len] ^= 1;
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
}

static void start_test_encrypted_central(void) {
    start_test_central_link();
    memcpy(secure_random_forced, central_entropy, 12);
    secure_random_force = 1;
    assert(mesh_gap_encrypt(encryption_ltk, encryption_random, 0x2474));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x03 && gap_conn_tx_frame[1] == 23);
    assert(memcmp(gap_conn_tx_frame + 3, encryption_random, 8) == 0);
    assert(gap_conn_tx_frame[11] == 0x74 && gap_conn_tx_frame[12] == 0x24);
    receive_test_control(0x04, peripheral_entropy, 12);
    const uint8_t session[16] = {
        0x99, 0xad, 0x1b, 0x52, 0x26, 0xa3, 0x7e, 0x3e,
        0x05, 0x8e, 0x3b, 0x8e, 0x27, 0xc2, 0xc6, 0x66
    };
    assert(memcmp(gap_security.session_key, session, 16) == 0);
    receive_test_control(0x05, NULL, 0);
    assert(tx_buffer[1] == 5);
    assert(memcmp(tx_buffer + 2, (uint8_t[]){0x9f, 0xcd, 0xa7, 0xf4, 0x48}, 5) == 0);
    assert(!mesh_gap_encrypted() && gap_security.tx_counter == 1);
    // An unacknowledged plaintext START request retries the same encrypted response.
    rx_frame[0] = 3 | ((gap_conn.expected_rx_sn ^ 1) << 3) | (gap_conn.tx_sn << 2);
    rx_frame[1] = 1; rx_frame[2] = 0x05; gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    assert(memcmp(tx_buffer + 2, (uint8_t[]){0x9f, 0xcd, 0xa7, 0xf4, 0x48}, 5) == 0);
    assert(gap_security.tx_counter == 1);
    receive_secure_test_pdu(3, (uint8_t[]){0x06}, 1, 0, 0, 1, 0);
    assert(mesh_gap_encrypted() && mesh_gap_security_status() == 0);
    assert(gap_security.rx_counter == 1 && gap_security.tx_counter == 1);
    secure_random_force = 0;
}

static void test_link_encryption(void) {
    assert(!mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    assert(!mesh_gap_key_request(NULL, NULL) && !mesh_gap_key_reply(encryption_ltk));
    start_test_encrypted_central();
    const uint8_t central_data[27] = {
        0x17, 0x00, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
        0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x30
    };
    const uint8_t encrypted_data[31] = {
        0x7a, 0x70, 0xd6, 0x64, 0x15, 0x22, 0x6d, 0xf2, 0x6b, 0x17, 0x83, 0x9a,
        0x06, 0x04, 0x05, 0x59, 0x6b, 0xd6, 0x56, 0x4f, 0x79, 0x6b, 0x5b, 0x9c,
        0xe6, 0xff, 0x32, 0xf7, 0x5a, 0x6d, 0x33
    };
    assert(mesh_gap_send_data(2, central_data, sizeof(central_data)));
    receive_test_link_packet(1);
    assert(tx_buffer[1] == 31 && memcmp(tx_buffer + 2, encrypted_data, 31) == 0);
    assert(gap_security.tx_counter == 2);
    receive_test_link_packet(0);
    assert(gap_security.tx_counter == 2 && memcmp(tx_buffer + 2, encrypted_data, 31) == 0);
    receive_secure_test_pdu(2, central_data, 27, 1, 0, 1, 0);
    assert(gap_conn.rx_ready && gap_security.rx_counter == 2);
    receive_secure_test_pdu(2, central_data, 27, 1, 1, 1, 0);
    assert(gap_security.rx_counter == 2);
    // A full RX slot does not consume the next packet counter; retry after draining.
    receive_secure_test_pdu(1, central_data, 27, 2, 0, 1, 0);
    assert(gap_security.rx_counter == 2);
    uint8_t output[27], llid; size_t len = sizeof(output);
    assert(mesh_gap_receive_data(&llid, output, &len) == 1 && llid == 2 && len == 27);
    assert(memcmp(output, central_data, 27) == 0);
    receive_secure_test_pdu(1, central_data, 27, 2, 0, 1, 0);
    assert(gap_security.rx_counter == 3 && gap_conn.rx_ready);
    receive_secure_test_pdu(3, (uint8_t[]){0x12}, 1, 3, 0, 1, 0);
    assert(gap_conn_tx_frame[2] == 0x13 && tx_buffer[1] == 5);
    // Authentication failure disconnects before a packet reaches the application.
    receive_secure_test_pdu(1, central_data, 27, 4, 0, 1, 1);
    assert(!gap_conn.active && mesh_gap_security_status() == 0x3d);
    assert(memcmp(gap_security.session_key, (uint8_t[16]){0}, 16) == 0);
    assert(memcmp(gap_conn.rx_data, (uint8_t[MESH_GAP_CONN_DATA_MAX]){0}, MESH_GAP_CONN_DATA_MAX) == 0);

    // Refresh uses the old key and IV through the pause exchange, then resets counters.
    start_test_encrypted_central();
    uint8_t old_iv[8]; memcpy(old_iv, gap_security.iv, 8);
    assert(mesh_gap_encrypt(encryption_ltk, encryption_random, 0x1234));
    assert(!mesh_gap_send_data(1, central_data, 1));
    assert(memcmp(gap_security.iv, old_iv, 8) == 0);
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x0a && tx_buffer[1] == 5);
    receive_secure_test_pdu(3, (uint8_t[]){0x0b}, 1, 1, 0, 1, 0);
    assert(gap_conn_tx_frame[2] == 0x0b && tx_buffer[1] == 1);
    assert(!gap_security.tx_enabled && !gap_security.rx_enabled);
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x03 && gap_conn_tx_frame[11] == 0x34);
    receive_test_control(0x04, peripheral_entropy, 12);
    receive_test_control(0x05, NULL, 0);
    assert(gap_security.tx_counter == 1);
    receive_secure_test_pdu(3, (uint8_t[]){0x06}, 1, 0, 0, 1, 0);
    assert(mesh_gap_encrypted() && gap_security.rx_counter == 1);
    gap_connection_end();

    // Peripheral host key lookup, start response and refresh rejection.
    start_test_central_link(); gap_conn.central_role = 0;
    memcpy(secure_random_forced, peripheral_entropy, 12); secure_random_force = 1;
    uint8_t request[22];
    memcpy(request, encryption_random, 8); request[8] = 0x74; request[9] = 0x24;
    memcpy(request + 10, central_entropy, 12);
    receive_test_control(0x03, request, sizeof(request));
    assert(gap_conn_tx_frame[2] == 0x04 && gap_conn_tx_frame[1] == 13);
    uint8_t requested_random[8]; uint16_t ediv;
    assert(mesh_gap_key_request(requested_random, &ediv));
    assert(ediv == 0x2474 && memcmp(requested_random, encryption_random, 8) == 0);
    assert(mesh_gap_key_reply(encryption_ltk));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x05 && tx_buffer[1] == 1);
    receive_secure_test_pdu(3, (uint8_t[]){0x06}, 1, 0, 0, 1, 0);
    assert(mesh_gap_encrypted() && tx_buffer[1] == 5);
    assert(memcmp(tx_buffer + 2, (uint8_t[]){0xa3, 0x4c, 0x13, 0xa4, 0x15}, 5) == 0);
    receive_secure_test_pdu(3, (uint8_t[]){0x0a}, 1, 1, 0, 1, 0);
    assert(gap_conn_tx_frame[2] == 0x0b && !gap_security.rx_enabled);
    uint64_t transmitted = gap_security.tx_counter;
    receive_secure_test_pdu(3, (uint8_t[]){0x0a}, 1, 1, 1, 0, 0);
    assert(gap_security.tx_counter == transmitted);
    receive_test_control(0x0b, NULL, 0);
    assert(gap_security.phase == GAP_ENC_PERIPHERAL_RESTART);
    receive_test_control(0x03, request, sizeof(request));
    assert(mesh_gap_key_reply(NULL));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x02 && gap_conn_tx_frame[3] == 0x06);
    receive_test_link_packet(1);
    assert(!gap_conn.active && mesh_gap_security_status() == 0x06);
    secure_random_force = 0;

    // Missing key during an initial start rejects without marking the link encrypted.
    start_test_central_link(); gap_conn.central_role = 0;
    receive_test_control(0x03, request, sizeof(request));
    assert(mesh_gap_key_reply(NULL));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x06);
    assert(!mesh_gap_encrypted() && !mesh_gap_key_request(NULL, NULL));
    gap_connection_end();
    start_test_central_link();
    assert(mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    receive_test_link_packet(1);
    receive_test_control(0x04, peripheral_entropy, 12);
    receive_test_control(0x11, (uint8_t[]){0x03, 0x06}, 2);
    assert(mesh_gap_security_status() == 0x06 && !gap_security.phase && gap_conn.active);
    gap_connection_end();

    // An unexpected data PDU during the handshake is never delivered.
    start_test_central_link();
    assert(mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    receive_test_link_packet(1);
    rx_frame[0] = 1 | (gap_conn.expected_rx_sn << 3) | ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = 1; rx_frame[2] = 0x55; gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    assert(!gap_conn.active && mesh_gap_security_status() == 0x3d);

    // Negotiated larger payloads retain room for all four MIC bytes on the wire.
    start_test_encrypted_central();
    assert(mesh_gap_data_length_set(MESH_GAP_CONN_DATA_MAX));
    receive_test_link_packet(1);
    assert(gap_conn_tx_frame[2] == 0x14);
    receive_secure_test_pdu(3, (uint8_t[]){0x15, 251, 0, 0x48, 0x08, 251, 0, 0x48, 0x08},
                            9, 1, 0, 1, 0);
    uint8_t large[MESH_GAP_CONN_DATA_MAX]; memset(large, 0xab, sizeof(large));
    assert(mesh_gap_send_data(1, large, sizeof(large)));
    receive_test_link_packet(1);
    assert(tx_buffer[1] == MESH_GAP_CONN_DATA_MAX + 4);
    receive_secure_test_pdu(1, large, sizeof(large), 2, 0, 1, 0);
    size_t large_len = sizeof(large);
    assert(mesh_gap_receive_data(NULL, large, &large_len) == 1 && large_len == sizeof(large));
    gap_connection_end();

    // A disconnect during entropy collection must not commit the key afterwards.
    start_test_central_link();
    secure_random_disconnect = 1;
    assert(!mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    secure_random_disconnect = 0;
    assert(!gap_conn.active && !gap_security.phase);
    assert(memcmp(gap_security.ltk, (uint8_t[16]){0}, 16) == 0);

    // Resource failure and timeout never fall back to predictable randomness.
    start_test_central_link(); secure_random_available = 0;
    assert(!mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    assert(mesh_gap_security_status() == 0x1f && !gap_security.phase);
    gap_conn.central_role = 0;
    receive_test_control(0x03, request, sizeof(request));
    assert(gap_conn_tx_frame[2] == 0x11 && gap_conn_tx_frame[4] == 0x1f);
    secure_random_available = 1;
    gap_connection_end();
    start_test_central_link();
    assert(mesh_gap_encrypt(encryption_ltk, encryption_random, 0));
    receive_test_link_packet(1);
    now_ms = gap_security.started_ms + 40000;
    gap_conn.last_rx_ms = now_ms;
    mesh_gap_conn_poll();
    assert(!gap_conn.active && mesh_gap_security_status() == 0x22);
    start_test_encrypted_central();
    gap_security.tx_counter = UINT64_C(1) << 39;
    assert(mesh_gap_send_data(1, central_data, 1));
    receive_test_link_packet(1);
    assert(!gap_conn.active && mesh_gap_security_status() == 0x3d);
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
    test_connection_timing_updates();
    test_connection_parameter_requests();
    test_data_length();
    test_channel_map_updates();
    test_phy_updates();
    test_link_encryption();
    return 0;
}
