#include <assert.h>
#include <stdint.h>
#include <openssl/aes.h>
#include <string.h>

#define MESH_GAP_EXT_ADV_SUPPORT 1
#include "../ble_gap.h"
#include "../mesh_crypto.h"
#include <openssl/evp.h>
int GET_RANDOM_BYTES(uint8_t *out, unsigned size);
#include "mesh_crypto_test.h"

int BLE_GAP_BOND_LOAD(uint8_t slot, mesh_gap_bond *bond) {
    (void)slot;
    if (bond) memset(bond, 0, sizeof(*bond));
    return -1;
}
int BLE_GAP_BOND_SAVE(uint8_t slot, const mesh_gap_bond *bond) {
    (void)slot; (void)bond;
    return 0;
}
int BLE_GAP_BOND_DELETE(uint8_t slot) {
    (void)slot;
    return 0;
}

static uint32_t now_ms;
static uint64_t fake_radio_ticks;
static uint8_t fake_radio_ticks_enabled;
static uint8_t fake_radio_tick_autoincrement;
static uint8_t radio_phy_mask = 3, configured_tx_phy, configured_rx_phy;
static uint32_t configured_access_address;
static uint32_t configured_crc_init;
static uint8_t configured_radio_channel;
static unsigned link_rx_count;
static unsigned scan_rx_count, advertising_tx_count;
static uint8_t captured_extended_pdus[16][255];
static uint8_t captured_extended_pdu_lengths[16], captured_extended_channels[16];
static uint64_t captured_extended_ticks[16];
static unsigned captured_extended_count;
static uint8_t force_aux_scan_request;
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
static uint8_t secure_random_passkey_fail, secure_random_passkey_reject;
uint32_t BLE_GAP_CRITICAL_ENTER(void) { return 0; }
void BLE_GAP_CRITICAL_EXIT(uint32_t state) { (void)state; }
static uint8_t secure_random_forced[12], secure_random_force;
static uint8_t ead_randomizer_forced[5], ead_randomizer_force;
int BLE_GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len) {
    if (!secure_random_available || (len == 4 && secure_random_passkey_fail)) return 0;
    if (len == 4 && secure_random_passkey_reject) { memset(out, 0xff, 4); return 1; }
    if (secure_random_disconnect) gap_connection_end();
    if (len == sizeof(ead_randomizer_forced) && ead_randomizer_force) {
        memcpy(out, ead_randomizer_forced, sizeof(ead_randomizer_forced));
        return 1;
    }
    if (secure_random_force) { assert(len == 12); memcpy(out, secure_random_forced, 12); return 1; }
    for (size_t i = 0; i < len; i++) out[i] = secure_random_seed++;
    return 1;
}

static void test_encrypted_advertising_data(void) {
    static const uint8_t key[16] = {
        0x57, 0xa9, 0xda, 0x12, 0xd1, 0x2e, 0x6e, 0x13,
        0x1e, 0x20, 0x61, 0x2a, 0xd1, 0x0a, 0x6a, 0x19
    };
    static const uint8_t iv[8] = {
        0x9e, 0x7a, 0x00, 0xef, 0xb1, 0x7a, 0xe7, 0x46
    };
    static const uint8_t plaintext[] = {
        0x0f, 0x09, 'S', 'h', 'o', 'r', 't', ' ', 'M', 'i', 'n', 'i',
        '-', 'B', 'u', 's', 0x03, 0x19, 0x0a, 0x8c
    };
    static uint8_t vector[] = {
        0x1e, 0x31, 0x18, 0xe1, 0x57, 0xca, 0xde,
        0x74, 0xe4, 0xdc, 0xaf, 0xdc, 0x51, 0xc7, 0x28, 0x28,
        0x10, 0xc2, 0x21, 0x7f, 0x0e, 0x4c, 0xef, 0x43, 0x43, 0x18, 0x1f,
        0xba, 0x00, 0x69, 0xcc
    };
    uint8_t randomizer[5] = {0x18, 0xe1, 0x57, 0xca, 0xde};
    uint8_t output[MESH_GAP_EAD_AD_STRUCTURE_MAX], clear[32];
    size_t output_len = 0, clear_len = 0;
    mesh_gap_ead_key_material_clear();
    assert(!mesh_gap_ead_encrypt(plaintext, sizeof(plaintext), output,
        sizeof(output), &output_len));
    assert(mesh_gap_ead_key_material_set(key, iv));
    assert(mesh_gap_ead_decrypt(vector, sizeof(vector), clear,
        sizeof(clear), &clear_len));
    assert(clear_len == sizeof(plaintext) &&
           !memcmp(clear, plaintext, sizeof(plaintext)));
    const uint8_t malformed_payload[] = {0x04, 0x09, 'x'};
    assert(!mesh_gap_ead_encrypt(malformed_payload,
        sizeof(malformed_payload), output, sizeof(output), &output_len));

    memcpy(ead_randomizer_forced, randomizer, sizeof(randomizer));
    ead_randomizer_force = 1;
    assert(mesh_gap_ead_encrypt(plaintext, sizeof(plaintext), output,
        sizeof(output), &output_len));
    ead_randomizer_force = 0;
    assert(output_len == sizeof(vector) &&
           !memcmp(output, vector, sizeof(vector)));
    secure_random_available = 0;
    assert(!mesh_gap_ead_encrypt(plaintext, sizeof(plaintext), output,
        sizeof(output), &output_len));
    secure_random_available = 1;

    vector[sizeof(vector) - 1] ^= 1;
    assert(!mesh_gap_ead_decrypt(vector, sizeof(vector), clear,
        sizeof(clear), &clear_len));
    vector[sizeof(vector) - 1] ^= 1;
    assert(!mesh_gap_ead_decrypt(vector, sizeof(vector) - 1, clear,
        sizeof(clear), &clear_len));
    assert(!mesh_gap_ead_encrypt(plaintext, sizeof(plaintext), output,
        output_len - 1, &output_len));
    mesh_gap_ead_key_material_clear();
    assert(!mesh_gap_ead_key_material_get(clear));
    assert(!mesh_gap_ead_decrypt(vector, sizeof(vector), clear,
        sizeof(clear), &clear_len));
}
int GET_RANDOM_BYTES(uint8_t *out, unsigned size) {
    return BLE_GAP_RANDOM_SECURE_BYTES(out, size);
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
uint64_t BLE_GAP_HW_TICKS(void) {
    if (!fake_radio_ticks_enabled) return (uint64_t)now_ms * 1000;
    if (fake_radio_tick_autoincrement) return fake_radio_ticks++;
    return fake_radio_ticks;
}
uint64_t HW_TICKS_FROM_US(uint32_t us) { return us; }
void BLE_GAP_HW_STOP(void) {}
void BLE_GAP_HW_INIT(void) {}
void BLE_GAP_HW_SCAN_RX(uint8_t channel) {
    configured_radio_channel = channel;
    scan_rx_count++;
}
uint8_t BLE_GAP_HW_RANDOM_JITTER(void) { return 0; }
void BLE_GAP_HW_PACKET_CLEAR(void) {}
void BLE_GAP_HW_PACKET_READY(void) {}
void BLE_GAP_HW_TX_BUFFER(const uint8_t *frame) { tx_buffer = frame; }
void BLE_GAP_HW_LINK_TX(void) {
    link_tx_count++;
    if (configured_access_address != BLE_ADV_ACCESS_ADDRESS &&
        tx_buffer == gap_radio_ext_adv_frame && captured_extended_count < 16) {
        uint8_t *frame = (uint8_t *)tx_buffer;
        unsigned slot = captured_extended_count++;
        memcpy(captured_extended_pdus[slot], frame, (size_t)frame[1] + 2);
        captured_extended_pdu_lengths[slot] = frame[1] + 2;
        captured_extended_channels[slot] = configured_radio_channel;
        captured_extended_ticks[slot] = fake_radio_ticks;
    }
    if (gap_radio_ext_adv_scan_waiting && gap_radio_ext_adv_scan_response_started &&
        captured_extended_count < 16) {
        uint8_t *frame = (uint8_t *)tx_buffer;
        unsigned slot = captured_extended_count++;
        fake_radio_ticks += HW_TICKS_FROM_US(150);
        memcpy(captured_extended_pdus[slot], frame, (size_t)frame[1] + 2);
        captured_extended_pdu_lengths[slot] = frame[1] + 2;
        captured_extended_channels[slot] = configured_radio_channel;
        captured_extended_ticks[slot] = fake_radio_ticks;
        fake_radio_ticks += ((uint32_t)frame[1] + 10u) * 8u;
        gap_hw_mesh_transmitted();
    }
}
int BLE_GAP_HW_TX_DONE(void) { return 1; }
void BLE_GAP_HW_TX_CLEAR_DONE(void) {}
void BLE_GAP_HW_CRC_INIT(uint32_t crc_init) { configured_crc_init = crc_init; }
void BLE_GAP_HW_LINK_CONFIG(uint32_t access_address, uint8_t channel,
                            uint8_t *frame, uint8_t receive_after_tx, uint8_t tx_phy, uint8_t rx_phy) {
    (void)receive_after_tx;
    configured_access_address = access_address;
    configured_radio_channel = channel;
    tx_buffer = frame;
    configured_tx_phy = tx_phy;
    configured_rx_phy = rx_phy;
}
void BLE_GAP_HW_LINK_RX(void) {
    link_rx_count++;
    if (force_aux_scan_request && gap_radio_ext_adv_scan_waiting) {
        static const uint8_t scanner_address[6] = {
            0x21, 0x22, 0x23, 0x24, 0x25, 0x26
        };
        uint8_t advertiser_address[6], address_type;
        gap_local_address_select(-1, advertiser_address, &address_type);
        memset(rx_frame, 0, sizeof(rx_frame));
        rx_frame[0] = (uint8_t)(0x03 | (address_type << 7));
        rx_frame[1] = 12;
        memcpy(rx_frame + 2, scanner_address, sizeof(scanner_address));
        memcpy(rx_frame + 8, advertiser_address, sizeof(advertiser_address));
        force_aux_scan_request = 0;
        gap_hw_mesh_received();
    }
}
int BLE_GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel) {
    (void)frame; (void)len;
    configured_radio_channel = channel;
    advertising_tx_count++;
    if ((frame[0] & 0x0f) == 0x07 && captured_extended_count < 16) {
        unsigned slot = captured_extended_count++;
        memcpy(captured_extended_pdus[slot], frame, len);
        captured_extended_pdu_lengths[slot] = len;
        captured_extended_channels[slot] = channel;
        captured_extended_ticks[slot] = fake_radio_ticks;
    }
    if (fake_radio_ticks_enabled && fake_radio_tick_autoincrement)
        fake_radio_ticks += ((uint32_t)len + 8u) * 8u;
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
    assert(!mesh_gap_set_static_random_address(
        (uint8_t[6]){0, 0, 0, 0, 0, 0xc0}));
    assert(!mesh_gap_set_static_random_address(
        (uint8_t[6]){0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
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

static void test_connection_timing_configuration(void) {
    mesh_gap_connection_timing timing;
    assert(mesh_gap_connection_timing_get(&timing));
    assert(timing.interval == 24 && timing.latency == 0 &&
           timing.supervision_timeout == 200 &&
           timing.background_scan_interval_ms == 1280 &&
           timing.background_scan_window_ms == 12 &&
           timing.attempt_timeout_ms == 30720);
    assert(!mesh_gap_connection_timing_get(NULL));
    mesh_gap_connection_timing invalid = timing;
    invalid.interval = 5;
    assert(!mesh_gap_connection_timing_set(&invalid));
    invalid = timing;
    invalid.latency = 499;
    invalid.supervision_timeout = 300; // Fails the supervision timeout relation.
    assert(!mesh_gap_connection_timing_set(&invalid));
    invalid = timing;
    invalid.attempt_timeout_ms = 0;
    assert(!mesh_gap_connection_timing_set(&invalid));
    invalid = timing;
    invalid.background_scan_window_ms =
        invalid.background_scan_interval_ms + 1;
    assert(!mesh_gap_connection_timing_set(&invalid));

    timing.interval = 40;
    timing.latency = 2;
    timing.supervision_timeout = 400;
    timing.attempt_timeout_ms = 45000;
    assert(mesh_gap_connection_timing_set(&timing));
    const uint8_t peer[6] = {9, 8, 7, 6, 5, 4};
    assert(mesh_gap_connect_start(peer, 0));
    assert(gap_central_connect.request[24] == 40 &&
           gap_central_connect.request[26] == 2 &&
           gap_central_connect.request[28] == 0x90 &&
           gap_central_connect.request[29] == 1);
    assert(gap_central_connect.deadline_ms == now_ms + 45000);
    assert(!mesh_gap_connection_timing_set(&timing)); // Settings are locked while scanning.
    mesh_gap_connect_cancel();

    timing.attempt_timeout_ms = 250;
    assert(mesh_gap_connection_timing_set(&timing));
    assert(mesh_gap_connect_start(peer, 0));
    now_ms += 251;
    gap_hw_mesh_scan_poll();
    assert(!gap_central_connect.active && !gap_scanning);

    timing.interval = 24;
    timing.latency = 0;
    timing.supervision_timeout = 200;
    timing.background_scan_interval_ms = 1280;
    timing.background_scan_window_ms = 12;
    timing.attempt_timeout_ms = 30720;
    assert(mesh_gap_connection_timing_set(&timing));
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
    now_ms = 0;
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

static void test_accept_list_capacity(void) {
    uint8_t addresses[GAP_ACCEPT_LIST_COUNT + 1][6] = {{0}};
    assert(mesh_gap_accept_list_clear());
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        addresses[i][0] = (uint8_t)(i + 1);
        addresses[i][1] = 0x42;
        addresses[i][5] = 0x20;
        assert(mesh_gap_accept_list_add(addresses[i], 0));
    }
    addresses[GAP_ACCEPT_LIST_COUNT][0] = 0xfe;
    addresses[GAP_ACCEPT_LIST_COUNT][1] = 0x42;
    addresses[GAP_ACCEPT_LIST_COUNT][5] = 0x20;
    assert(!mesh_gap_accept_list_add(addresses[GAP_ACCEPT_LIST_COUNT], 0));
    assert(mesh_gap_accept_list_add(addresses[0], 0)); // Full duplicate is a no-op.
    assert(mesh_gap_accept_list_remove(addresses[GAP_ACCEPT_LIST_COUNT / 2], 0));
    assert(mesh_gap_accept_list_add(addresses[GAP_ACCEPT_LIST_COUNT], 0));
    assert(mesh_gap_accept_list_remove(addresses[GAP_ACCEPT_LIST_COUNT], 0));
    assert(mesh_gap_accept_list_clear());
}

#if MESH_GAP_EXT_ADV_SUPPORT
static void test_extended_advertising_reassembly(void) {
    static const uint8_t primary[] = {
        0x07, 7, 6, 0x18, 0x34, 0x20, 0x05, 0x0f, 0x00
    };
    static const uint8_t auxiliary[] = {
        0x47, 14, 12, 0x19,
        1, 2, 3, 4, 5, 6, 0x34, 0x20, 0x05, 0x11, 0x00, 0x02
    };
    static const uint8_t chain[] = {
        0x07, 6, 3, 0x08, 0x34, 0x20, 0x01, 0x06
    };
    mesh_gap_extended_scan_report report;
    mesh_gap_scan_start();
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary) - 1, -45) == 0); // Truncated PDU.
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -45) == 1);
    assert(!mesh_gap_extended_scan_poll(&report));
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -48) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -50) == 1);
    assert(mesh_gap_extended_scan_poll(&report));
    assert(report.has_address && report.address_type == 1 &&
           !memcmp(report.address, auxiliary + 4, 6));
    assert(report.sid == 2 && report.data_len == 3 && report.rssi == -50 &&
           !memcmp(report.data, (uint8_t[]){0x02, 0x01, 0x06}, 3));
    assert(!mesh_gap_extended_scan_poll(&report));

    uint8_t primary_b[sizeof(primary)], auxiliary_b[sizeof(auxiliary)];
    uint8_t chain_b[sizeof(chain)];
    memcpy(primary_b, primary, sizeof(primary));
    memcpy(auxiliary_b, auxiliary, sizeof(auxiliary));
    memcpy(chain_b, chain, sizeof(chain));
    primary_b[4] = auxiliary_b[10] = chain_b[4] = 0x45;
    primary_b[5] = auxiliary_b[11] = chain_b[5] = 0x30;
    memset(auxiliary_b + 4, 0x22, 6);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary_b, sizeof(primary_b), -41) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -42) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary_b, sizeof(auxiliary_b), -43) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -44) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain_b, sizeof(chain_b), -45) == 1);
    assert(mesh_gap_extended_scan_poll(&report) && report.sid == 2);
    assert(mesh_gap_extended_scan_poll(&report) && report.sid == 3 &&
           !memcmp(report.address, auxiliary_b + 4, 6));

    mesh_gap_scan_stop();
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_GENERAL, 1));
    mesh_gap_scan_start();
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -40) == 1);
    assert(mesh_gap_extended_scan_poll(&report));
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -40) == 1);
    assert(!mesh_gap_extended_scan_poll(&report));
    uint8_t updated_primary[sizeof(primary)], updated_auxiliary[sizeof(auxiliary)];
    uint8_t updated_chain[sizeof(chain)];
    memcpy(updated_primary, primary, sizeof(primary));
    memcpy(updated_auxiliary, auxiliary, sizeof(auxiliary));
    memcpy(updated_chain, chain, sizeof(chain));
    updated_primary[4] = updated_auxiliary[10] = updated_chain[4] = 0x35;
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        updated_primary, sizeof(updated_primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        updated_auxiliary, sizeof(updated_auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        updated_chain, sizeof(updated_chain), -40) == 1);
    assert(mesh_gap_extended_scan_poll(&report) && report.did == 0x35);

    mesh_gap_scan_stop();
    assert(mesh_gap_privacy_filter(1, 0));
    mesh_gap_scan_start();
    uint8_t private_auxiliary[sizeof(auxiliary)];
    memcpy(private_auxiliary, auxiliary, sizeof(auxiliary));
    memcpy(private_auxiliary + 4, test_rpa, sizeof(test_rpa));
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        private_auxiliary, sizeof(private_auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -40) == 1);
    assert(mesh_gap_extended_scan_poll(&report) && report.resolved &&
           report.identity_type == 0 &&
           !memcmp(report.identity_address, test_identity, 6));
    mesh_gap_scan_stop();
    assert(mesh_gap_privacy_filter(0, 0));
    mesh_gap_scan_start();

    uint8_t malformed[sizeof(primary)];
    memcpy(malformed, primary, sizeof(primary));
    malformed[3] = 0x98; // Reserved extended-header flag.
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        malformed, sizeof(malformed), -40) == 0);

    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    now_ms += GAP_EXT_ADV_CHAIN_TIMEOUT_MS + 1;
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -40) == 0);

    uint8_t malformed_chain[sizeof(chain)];
    memcpy(malformed_chain, chain, sizeof(chain));
    malformed_chain[1] = 5;
    malformed_chain[6] = 0; // Invalid zero-length AD structure.
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        malformed_chain, sizeof(malformed_chain) - 1, -40) == 0);
    assert(!mesh_gap_extended_scan_poll(&report));
    mesh_gap_scan_stop();
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_LIMITED, 0));
    mesh_gap_scan_start();
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, sizeof(primary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, sizeof(auxiliary), -40) == 1);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        chain, sizeof(chain), -40) == 1);
    assert(!mesh_gap_extended_scan_poll(&report));
    mesh_gap_scan_stop();
}

static void test_extended_advertising_radio_followup(void) {
    static const uint8_t primary[] = {
        0x07, 7, 6, 0x18, 0x34, 0x20, 0x05, 0x0f, 0x00
    };
    static const uint8_t auxiliary[] = {
        0x47, 14, 12, 0x19,
        1, 2, 3, 4, 5, 6, 0x34, 0x20, 0x05, 0x11, 0x00, 0x02
    };
    static const uint8_t chain[] = {
        0x07, 6, 3, 0x08, 0x34, 0x20, 0x01, 0x06
    };
    mesh_gap_extended_scan_report report;
    now_ms = 25;
    fake_radio_ticks = 500000;
    fake_radio_ticks_enabled = 1;
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
    mesh_gap_scan_start();
    gap_hw_mesh_scan_poll();

    memcpy(rx_frame, primary, sizeof(primary));
    gap_hw_mesh_received();
    gap_hw_mesh_scan_poll();
    int slot = gap_radio_aux_request[0].active ? 0 : 1;
    assert(gap_radio_aux_request[slot].active);
    assert(gap_radio_aux_request[slot].window_start_ticks == 500311 &&
           gap_radio_aux_request[slot].window_end_ticks == 500347);
    fake_radio_ticks = gap_radio_aux_request[slot].window_start_ticks;
    gap_hw_mesh_scan_poll();
    assert(gap_radio_aux_listening);
    assert(configured_access_address == BLE_ADV_ACCESS_ADDRESS);
    assert(configured_radio_channel == 5 && configured_tx_phy == MESH_GAP_PHY_1M &&
           configured_rx_phy == MESH_GAP_PHY_1M);

    memcpy(rx_frame, auxiliary, sizeof(auxiliary));
    gap_hw_mesh_received();
    gap_hw_mesh_scan_poll();
    assert(gap_radio_aux_request[slot].active);
    fake_radio_ticks = gap_radio_aux_request[slot].window_start_ticks;
    gap_hw_mesh_scan_poll();
    assert(gap_radio_aux_listening);

    memcpy(rx_frame, chain, sizeof(chain));
    gap_hw_mesh_received();
    gap_hw_mesh_scan_poll();
    assert(mesh_gap_extended_scan_poll(&report));
    assert(report.has_address && report.sid == 2 && report.data_len == 3 &&
           !memcmp(report.data, (uint8_t[]){0x02, 0x01, 0x06}, 3));

    mesh_gap_scan_stop();
    fake_radio_ticks_enabled = 0;
    gap_hw_mesh_scan_poll();
}
#endif

static void test_scanning_and_advertising_coexistence(void) {
    static const uint8_t data[] = {2, 0x01, 0x06};
    now_ms = 100;
    unsigned scan_before = scan_rx_count;
    unsigned tx_before = advertising_tx_count;
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
    assert(mesh_gap_advertising_start(data, sizeof(data), 100));
    mesh_gap_scan_start();
    gap_hw_mesh_scan_poll();
    assert(gap_radio_rx_armed && scan_rx_count == scan_before + 1);

    uint32_t sent_at = 0;
    uint8_t jitter = 0;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, &sent_at, &jitter) == 0);
    assert(gap_scanning && gap_advertising.enabled &&
           advertising_tx_count == tx_before + 3);
    gap_hw_mesh_scan_poll();
    assert(gap_radio_rx_armed && scan_rx_count == scan_before + 2);

    now_ms = gap_advertising.next_event_ms;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, &sent_at, &jitter) == 0);
    assert(gap_scanning && gap_advertising.enabled &&
           advertising_tx_count == tx_before + 6);

    mesh_gap_scan_stop();
    const uint8_t peer[6] = {1, 2, 3, 4, 5, 6};
    assert(mesh_gap_connect_start(peer, 0));
    gap_hw_mesh_scan_poll();
    now_ms = gap_advertising.next_event_ms;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, &sent_at, &jitter) == 0);
    assert(gap_central_connect.active && gap_scanning &&
           gap_advertising.enabled && advertising_tx_count == tx_before + 9);
    gap_hw_mesh_scan_poll();
    assert(gap_radio_rx_armed && scan_rx_count == scan_before + 4);

    mesh_gap_scan_stop();
    mesh_gap_advertising_stop();
    static const uint8_t scan_response[] = {2, 0x0a, 0};
    assert(mesh_gap_connectable_advertising_start(data, sizeof(data),
        scan_response, sizeof(scan_response), 100));
    assert(!mesh_gap_connect_start(peer, 0));
    mesh_gap_advertising_stop();
    gap_hw_mesh_scan_poll();
}

#if MESH_GAP_EXT_ADV_SUPPORT
static void test_extended_advertising_transmit(void) {
    static const uint8_t data[] = {2, 0x01, 0x06};
    static const uint8_t local_address[6] = {
        0x31, 0x32, 0x33, 0x34, 0x35, 0xc6
    };
    mesh_gap_extended_scan_report report;
    now_ms = 500;
    fake_radio_ticks = 1000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    assert(mesh_gap_privacy_filter(0, 0));
    assert(mesh_gap_set_static_random_address(local_address));
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
    mesh_gap_scan_start();
    assert(mesh_gap_extended_advertising_start(data, sizeof(data), 4, 100));
    assert(!mesh_gap_advertising_start(data, sizeof(data), 100));
    assert(mesh_gap_discoverable() == 1);
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 2);
    const uint8_t *primary = captured_extended_pdus[0];
    const uint8_t *auxiliary = captured_extended_pdus[1];
    assert(captured_extended_pdu_lengths[0] == 9 &&
           captured_extended_channels[0] == 37);
    assert((primary[0] & 0x0f) == 0x07 && primary[1] == 7 &&
           primary[2] == 6 && primary[3] == 0x18 &&
           (primary[5] >> 4) == 4 && primary[6] == 0 &&
           primary[7] == 16 && primary[8] == 0);
    assert(captured_extended_pdu_lengths[1] == 15 &&
           captured_extended_channels[1] == 0);
    assert((auxiliary[0] & 0x0f) == 0x07 && auxiliary[1] == 13 &&
           auxiliary[2] == 9 && auxiliary[3] == 0x09 &&
           !memcmp(auxiliary + 10, primary + 4, 2) &&
           !memcmp(auxiliary + 12, data, sizeof(data)));
    assert(fake_radio_ticks >= 1000480);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, captured_extended_pdu_lengths[0], -40));
    assert(gap_ext_adv_contexts[0].active &&
           gap_ext_adv_contexts[0].adi ==
               ((uint16_t)primary[4] | (uint16_t)primary[5] << 8));
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, captured_extended_pdu_lengths[1], -40));
    assert(!gap_ext_adv_contexts[0].active);
    assert(mesh_gap_extended_scan_poll(&report));
    assert(report.has_address && report.sid == 4 && report.data_len == 3 &&
           !memcmp(report.data, data, sizeof(data)));
    mesh_gap_extended_advertising_stop();
    mesh_gap_scan_stop();
    assert(mesh_gap_use_public_address());
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
    gap_hw_mesh_scan_poll();
    uint8_t maximum_data[MESH_GAP_EXT_ADV_DATA_MAX];
    uint8_t oversized_data[MESH_GAP_EXT_ADV_DATA_MAX + 1];
    memset(maximum_data, 'A', sizeof(maximum_data));
    memset(oversized_data, 'B', sizeof(oversized_data));
    for (size_t offset = 0; offset < sizeof(maximum_data);) {
        size_t chunk = sizeof(maximum_data) - offset;
        if (chunk > 256) chunk = 256;
        maximum_data[offset] = (uint8_t)(chunk - 1);
        maximum_data[offset + 1] = 0x09;
        offset += chunk;
    }
    for (size_t offset = 0; offset < sizeof(oversized_data);) {
        size_t chunk = sizeof(oversized_data) - offset;
        if (chunk > 256) chunk = 256;
        oversized_data[offset] = (uint8_t)(chunk - 1);
        oversized_data[offset + 1] = 0x09;
        offset += chunk;
    }
    assert(mesh_gap_extended_advertising_start(maximum_data,
        sizeof(maximum_data), 4, 100));
    mesh_gap_extended_advertising_stop();
    assert(!mesh_gap_extended_advertising_start(oversized_data,
        sizeof(oversized_data), 4, 100));
}

static void test_extended_advertising_chain_transmit(void) {
    static const uint8_t local_address[6] = {
        0x31, 0x32, 0x33, 0x34, 0x35, 0xc6
    };
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
    data[0] = 2;
    data[1] = 0x01;
    data[2] = 0x06;
    size_t offset = 3;
    while (offset < sizeof(data)) {
        size_t chunk = sizeof(data) - offset;
        if (chunk > 256) chunk = 256;
        data[offset] = (uint8_t)(chunk - 1);
        data[offset + 1] = 0x09;
        memset(data + offset + 2, 'X', chunk - 2);
        offset += chunk;
    }
    mesh_gap_extended_scan_report report;
    now_ms = 750;
    fake_radio_ticks = 2000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    assert(mesh_gap_privacy_filter(0, 0));
    assert(mesh_gap_set_static_random_address(local_address));
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
    mesh_gap_scan_start();
    assert(mesh_gap_extended_advertising_start(data, sizeof(data), 5, 100));
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 8);

    const uint8_t *primary = captured_extended_pdus[0];
    const uint8_t *auxiliary = captured_extended_pdus[1];
    const uint8_t *last = captured_extended_pdus[7];
    assert(captured_extended_channels[0] == 37 &&
           captured_extended_pdu_lengths[0] == 9 && primary[7] == 16);
    assert(captured_extended_channels[1] == 0 &&
           captured_extended_pdu_lengths[1] == 255 &&
           auxiliary[1] == 253 && auxiliary[2] == 12 &&
           auxiliary[3] == 0x19 && (auxiliary[11] >> 4) == 5 &&
           !memcmp(auxiliary + 10, primary + 4, 2) && auxiliary[12] == 1 &&
           auxiliary[13] == 91);
    assert(captured_extended_ticks[1] - captured_extended_ticks[0] >= 480 &&
           captured_extended_ticks[1] - captured_extended_ticks[0] <= 510);
    for (uint8_t i = 2; i < 7; i++) {
        const uint8_t *chain = captured_extended_pdus[i];
        assert(captured_extended_channels[i] == i - 1 &&
               captured_extended_pdu_lengths[i] == 255 && chain[2] == 6 &&
               chain[3] == 0x18 &&
               !memcmp(chain + 4, auxiliary + 10, 2) &&
               chain[6] == i && chain[7] == 91);
        assert(captured_extended_ticks[i] - captured_extended_ticks[i - 1] >= 2730 &&
               captured_extended_ticks[i] - captured_extended_ticks[i - 1] <= 2760);
    }
    assert(captured_extended_channels[7] == 6 &&
           captured_extended_pdu_lengths[7] == 186 && last[2] == 3 &&
           last[3] == 0x08 &&
           !memcmp(last + 4, auxiliary + 10, 2));

    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, captured_extended_pdu_lengths[0], -40));
    for (uint8_t i = 1; i < captured_extended_count; i++)
        assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
            captured_extended_pdus[i], captured_extended_pdu_lengths[i], -40));
    assert(mesh_gap_extended_scan_poll(&report));
    assert(report.has_address && report.sid == 5 &&
           report.data_len == sizeof(data) &&
           !memcmp(report.data, data, sizeof(data)));
    mesh_gap_extended_advertising_stop();
    mesh_gap_scan_stop();
    assert(mesh_gap_use_public_address());
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
}

static void test_periodic_advertising_transmit(void) {
    static const uint8_t adv_data[] = {2, 0x01, 0x06};
    uint8_t periodic_data[300];
    uint8_t maximum_periodic_data[MESH_GAP_EXT_ADV_DATA_MAX];
    periodic_data[0] = 255; // One 254-byte manufacturer data structure.
    periodic_data[1] = 0xff;
    memset(periodic_data + 2, 0x5a, 254);
    periodic_data[256] = 43; // A second 42-byte manufacturer structure.
    periodic_data[257] = 0xff;
    memset(periodic_data + 258, 0xa5, 42);
    size_t data_offset = 0;
    while (data_offset < sizeof(maximum_periodic_data)) {
        size_t field_data = sizeof(maximum_periodic_data) - data_offset - 1;
        if (field_data > 255) field_data = 255;
        maximum_periodic_data[data_offset] = (uint8_t)field_data;
        maximum_periodic_data[data_offset + 1] = 0xff;
        memset(maximum_periodic_data + data_offset + 2, 0x33,
               field_data - 1);
        data_offset += field_data + 1;
    }
    now_ms = 850;
    fake_radio_ticks = 3000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    assert(mesh_gap_extended_advertising_start(adv_data, sizeof(adv_data),
                                                6, 100));
    assert(!mesh_gap_periodic_advertising_start_set(1, periodic_data,
                                                     sizeof(periodic_data), 6));
    assert(!mesh_gap_periodic_advertising_start_set(0, maximum_periodic_data,
        sizeof(maximum_periodic_data), 6));
    assert(mesh_gap_periodic_advertising_start_set(0, periodic_data,
                                                    sizeof(periodic_data), 6));
    assert(!mesh_gap_periodic_advertising_update_set(0, periodic_data,
                                                      MESH_GAP_EXT_ADV_DATA_MAX + 1));
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 2);
    const uint8_t *aux = captured_extended_pdus[1];
    assert(aux[2] == 27 && aux[3] == 0x29);
    const uint8_t *sync = aux + 12;
    assert(((uint16_t)sync[2] | ((uint16_t)sync[3] << 8)) == 6);
    uint32_t access_address = (uint32_t)sync[9] |
        (uint32_t)sync[10] << 8 | (uint32_t)sync[11] << 16 |
        (uint32_t)sync[12] << 24;
    uint32_t crc_init = (uint32_t)sync[13] |
        (uint32_t)sync[14] << 8 | (uint32_t)sync[15] << 16;
    assert(gap_access_address_valid(access_address) && crc_init != 0);
    uint16_t sync_offset = (uint16_t)sync[0] |
        (uint16_t)(sync[1] & 0x1f) << 8;
    uint32_t sync_unit_us = sync[1] & 0x20 ? 300 : 30;
    uint64_t expected_offset = gap_ext_advertising[0].periodic_next_event_ticks -
        captured_extended_ticks[1];
    assert((uint64_t)sync_offset * sync_unit_us <= expected_offset &&
           expected_offset < (uint64_t)sync_offset * sync_unit_us +
               sync_unit_us);
    assert(sync[4] == 0xff && sync[5] == 0xff && sync[6] == 0xff &&
           sync[7] == 0xff && (sync[8] & 0x1f) == 0x1f);
    uint16_t set_adi = (uint16_t)aux[10] | (uint16_t)aux[11] << 8;
    assert((set_adi >> 12) == 6 && (sync[16] | sync[17]) == 0);
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[0];
    fake_radio_ticks = set->periodic_next_event_ticks;
    uint64_t scheduled_event = set->periodic_next_event_ticks;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 4);
    assert(configured_access_address == access_address &&
           captured_extended_channels[2] ==
               gap_periodic_channel(set, 0) &&
           captured_extended_ticks[2] >= scheduled_event &&
           captured_extended_ticks[2] - scheduled_event <= 5);
    assert(captured_extended_pdu_lengths[2] == 255 &&
           captured_extended_pdus[2][2] == 6 &&
           captured_extended_pdus[2][3] == 0x18 &&
           ((uint16_t)captured_extended_pdus[2][4] |
            (uint16_t)captured_extended_pdus[2][5] << 8) != set_adi &&
           (captured_extended_pdus[2][5] >> 4) == 6);
    assert(captured_extended_pdu_lengths[3] == 60 &&
           captured_extended_pdus[3][2] == 3 &&
           captured_extended_pdus[3][3] == 0x08 &&
           captured_extended_pdus[3][6] == 0x5a);
    assert(set->periodic_event_counter == 1);
    uint64_t interval_ticks = HW_TICKS_FROM_US(
        (uint32_t)set->periodic_interval * 1250u);
    fake_radio_ticks = set->periodic_next_event_ticks + 2 * interval_ticks;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 6 &&
           captured_extended_channels[4] == gap_periodic_channel(set, 3) &&
           set->periodic_event_counter == 4);
    uint32_t saved_access_address = set->periodic_access_address;
    set->periodic_access_address = BLE_ADV_ACCESS_ADDRESS;
    assert(gap_periodic_channel(set, 0) == 25 &&
           gap_periodic_channel(set, 1) == 20 &&
           gap_periodic_channel(set, 2) == 6 &&
           gap_periodic_channel(set, 3) == 21);
    set->periodic_access_address = saved_access_address;
    uint16_t old_periodic_did = set->periodic_did;
    assert(mesh_gap_periodic_advertising_update_set(0, adv_data,
                                                     sizeof(adv_data)) &&
           set->periodic_did != old_periodic_did);
    assert(mesh_gap_periodic_advertising_stop_set(0));
    assert(!mesh_gap_periodic_advertising_stop_set(0));
    mesh_gap_extended_advertising_stop();
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
}

static void test_pawr_advertising_subevents(void) {
    static const uint8_t adv_data[] = {2, 0x01, 0x06};
    static const uint8_t periodic_data[] = {2, 0x09, 'P'};
    now_ms = 875;
    fake_radio_ticks = 4000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    assert(mesh_gap_extended_advertising_start(adv_data, sizeof(adv_data),
                                                3, 100));
    assert(mesh_gap_periodic_advertising_start_set(0, periodic_data,
                                                    sizeof(periodic_data), 24));
    assert(!mesh_gap_periodic_advertising_pawr_set(0, 5, 6, 1, 2));
    assert(!mesh_gap_periodic_advertising_pawr_set(0, 3, 6, 6, 2));
    assert(mesh_gap_periodic_advertising_pawr_set(0, 3, 6, 1, 2));
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[0];
    assert(set->pawr_enabled &&
           gap_access_address_valid(set->periodic_response_access_address));

    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 2);
    const uint8_t *aux = captured_extended_pdus[1];
    assert(aux[2] == 37 && aux[3] == 0x69);
    const uint8_t *timing = aux + 30;
    assert(timing[0] == 9 && timing[1] == 0x32);
    uint32_t response_aa = (uint32_t)timing[2] |
        (uint32_t)timing[3] << 8 | (uint32_t)timing[4] << 16 |
        (uint32_t)timing[5] << 24;
    assert(response_aa == set->periodic_response_access_address);
    assert(response_aa != set->periodic_access_address);
    assert(timing[6] == 3 && timing[7] == 6 && timing[8] == 1 &&
           timing[9] == 2);

    uint64_t event_start = set->periodic_next_event_ticks;
    fake_radio_ticks = event_start;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 5);
    for (uint8_t subevent = 0; subevent < 3; subevent++) {
        const uint8_t *pdu = captured_extended_pdus[2 + subevent];
        uint64_t expected_start = event_start + HW_TICKS_FROM_US(
            (uint32_t)subevent * 7500u);
        assert(pdu[2] == 3 && pdu[3] == 0x08);
        if (subevent == 0)
            assert(pdu[6] == 2 && pdu[7] == 0x09 && pdu[8] == 'P');
        else
            assert(pdu[1] == 4 && captured_extended_pdu_lengths[2 + subevent] == 6);
        assert(captured_extended_channels[2 + subevent] ==
               gap_periodic_channel(set, subevent));
        assert(captured_extended_ticks[2 + subevent] >= expected_start &&
               captured_extended_ticks[2 + subevent] - expected_start <= 5);
    }
    assert(!set->pawr_data_pending && set->periodic_data_len == sizeof(periodic_data));
    assert(set->periodic_event_counter == 1);
    assert(!mesh_gap_periodic_advertising_update_set(0, periodic_data, 247));
    assert(mesh_gap_periodic_advertising_stop_set(0));
    mesh_gap_extended_advertising_stop();
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
}

static void test_pawr_observer_response(void) {
    static const uint8_t response_data[] = {2, 0x09, 'R'};
    static const uint8_t subevent_pdu[] = {
        0x07, 7, 3, 0x08, 0x02, 0x60, 2, 0x01, 0x06
    };
    mesh_gap_periodic_sync_context *sync = &gap_periodic_syncs[0];
    mesh_gap_periodic_report report;
    mesh_gap_scan_stop();
    memset(gap_periodic_syncs, 0, sizeof(gap_periodic_syncs));
    gap_periodic_report_head = gap_periodic_report_count = 0;
    memset(sync, 0, sizeof(*sync));
    sync->used = sync->established = 1;
    sync->handle = 1;
    sync->sid = 6;
    sync->interval = 24;
    sync->event_counter = 12;
    sync->anchor_ticks = 1000000;
    sync->access_address = 0x12345678;
    sync->response_access_address = 0x78563412;
    sync->crc_init = 0x123456;
    sync->phy = MESH_GAP_PHY_1M;
    memset(sync->channel_map, 0xff, sizeof(sync->channel_map));
    sync->channel_map[4] = 0x1f;
    sync->has_pawr_timing = 1;
    sync->pawr_num_subevents = 3;
    sync->pawr_subevent_interval = 6;
    sync->pawr_response_slot_delay = 1;
    sync->pawr_response_slot_spacing = 3;
    sync->last_event_ms = now_ms;
    assert(mesh_gap_periodic_sync_pawr_respond(1, 2, 0, response_data,
                                                sizeof(response_data)));
    assert(sync->pawr_selected_subevent == 2 && sync->pawr_response_pending &&
           sync->next_event_ticks == sync->anchor_ticks + 45000);
    assert(!mesh_gap_periodic_sync_pawr_respond(1, 3, 0, response_data,
                                                sizeof(response_data)));
    assert(!mesh_gap_periodic_sync_pawr_respond(1, 2, 255, response_data,
                                                 sizeof(response_data)));

    fake_radio_ticks = 2000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    memcpy(gap_radio_ext_scan_frame, subevent_pdu, sizeof(subevent_pdu));
    gap_radio_ext_scan_kind = MESH_GAP_EXT_ADV_PERIODIC_PDU;
    gap_radio_periodic_listening_slot = 0;
    gap_radio_periodic_listening = 1;
    gap_radio_rx_armed = 1;
    gap_radio_ext_scan_ticks = fake_radio_ticks + 136;
    gap_radio_ext_scan_rssi = -42;
    gap_radio_ext_scan_ready = 1;
    gap_radio_ext_scan_process();
    assert(captured_extended_count == 1 &&
           configured_access_address == sync->response_access_address &&
           captured_extended_pdu_lengths[0] == 7 &&
           captured_extended_pdus[0][0] == 0x07 &&
           captured_extended_pdus[0][2] == 1 &&
           captured_extended_pdus[0][3] == 0 &&
           !memcmp(captured_extended_pdus[0] + 4, response_data,
                   sizeof(response_data)));
    assert(captured_extended_channels[0] ==
           gap_periodic_channel_for(sync->access_address, sync->channel_map,
                                    (uint16_t)(12 ^ 2)));
    assert(captured_extended_ticks[0] >= 2000000 + 1250 &&
           captured_extended_ticks[0] <= 2000000 + 1253);
    assert(!sync->pawr_response_pending && sync->event_counter == 13 &&
           !sync->event_data_active);
    assert(mesh_gap_periodic_report_poll(&report) && report.handle == 1 &&
           report.event_counter == 12 && report.data_len == 3 &&
           !memcmp(report.data, (uint8_t[]){2, 0x01, 0x06}, 3));
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
    gap_radio_rx_armed = 0;
}

static void test_periodic_advertising_sync(void) {
    static const uint8_t advertiser[6] = {1, 2, 3, 4, 5, 6};
    uint8_t sync_info_pdu[43] = {0};
    uint8_t sync_ind[11] = {
        0x07, 9, 6, 0x18, 0x02, 0x60, 1, 20, 0,
        2, 0x01
    };
    uint8_t chain_ind[7] = {0x07, 5, 3, 0x08, 0x02, 0x60, 0x06};
    mesh_gap_periodic_sync_event event;
    mesh_gap_periodic_report report;
    now_ms = 900;
    fake_radio_ticks = 1000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 0;
    mesh_gap_scan_stop();
    memset(gap_periodic_syncs, 0, sizeof(gap_periodic_syncs));
    memset(gap_radio_aux_request, 0, sizeof(gap_radio_aux_request));
    int handle = mesh_gap_periodic_sync_start(0, advertiser, 6, 1000);
    assert(handle == 1 && gap_scanning);
    assert(!mesh_gap_periodic_sync_event_poll(&event));
    gap_hw_mesh_scan_poll();

    sync_info_pdu[0] = 0x07;
    sync_info_pdu[1] = 41;
    sync_info_pdu[2] = 37;
    sync_info_pdu[3] = 0x29; // AdvA, ADI, SyncInfo.
    memcpy(sync_info_pdu + 4, advertiser, 6);
    sync_info_pdu[10] = 0x02;
    sync_info_pdu[11] = 0x60; // SID 6, advertising-set DID 2.
    sync_info_pdu[12] = 40; // 1.2 ms SyncOffset in 30 us units.
    sync_info_pdu[14] = 16; // 20 ms fits both PAwR subevents.
    memset(sync_info_pdu + 16, 0xff, 4);
    sync_info_pdu[20] = 0x1f; // All 37 channels; SCA code 0.
    sync_info_pdu[21] = 0x78;
    sync_info_pdu[22] = 0x56;
    sync_info_pdu[23] = 0x34;
    sync_info_pdu[24] = 0x12; // Access Address 0x12345678.
    sync_info_pdu[25] = 0x56;
    sync_info_pdu[26] = 0x34;
    sync_info_pdu[27] = 0x12; // CRCInit 0x123456.
    sync_info_pdu[28] = 5; // First PeriodicEventCounter.
    sync_info_pdu[30] = 9;
    sync_info_pdu[31] = 0x32;
    memcpy(sync_info_pdu + 32, (uint8_t[]){0x12, 0x34, 0x56, 0x78,
        2, 6, 1, 2}, 8);
    sync_info_pdu[40] = 2;
    sync_info_pdu[41] = 0x01;
    sync_info_pdu[42] = 0x06;
    memcpy(gap_radio_ext_scan_frame, sync_info_pdu, sizeof(sync_info_pdu));
    gap_radio_ext_scan_kind = MESH_GAP_EXT_ADV_AUXILIARY_PDU;
    gap_radio_ext_scan_ticks = fake_radio_ticks -
        HW_TICKS_FROM_US(160000); // SyncInfo is already more than six events old.
    gap_radio_ext_scan_rssi = -35;
    gap_radio_ext_scan_ready = 1;
    gap_hw_mesh_scan_poll();
    assert(!gap_periodic_syncs[0].window_active);

    gap_ext_adv_fields decoded_sync_info;
    sync_info_pdu[13] = 0x20; // 300 us units are invalid for this short offset.
    assert(!gap_ext_adv_decode(sync_info_pdu, sizeof(sync_info_pdu),
                               &decoded_sync_info));
    sync_info_pdu[13] = 0x80; // Reserved SyncInfo bit.
    assert(!gap_ext_adv_decode(sync_info_pdu, sizeof(sync_info_pdu),
                               &decoded_sync_info));
    sync_info_pdu[13] = 0;
    memcpy(gap_radio_ext_scan_frame, sync_info_pdu, sizeof(sync_info_pdu));
    gap_radio_ext_scan_kind = MESH_GAP_EXT_ADV_AUXILIARY_PDU;
    gap_radio_ext_scan_ticks = fake_radio_ticks;
    gap_radio_ext_scan_rssi = -35;
    gap_radio_ext_scan_ready = 1;
    gap_hw_mesh_scan_poll();
    assert(gap_periodic_syncs[0].window_active &&
           !gap_periodic_syncs[0].established &&
           gap_periodic_syncs[0].event_counter == 5 &&
           gap_periodic_syncs[0].access_address == 0x12345678 &&
           gap_periodic_syncs[0].has_pawr_timing &&
           gap_periodic_syncs[0].pawr_num_subevents == 2 &&
           gap_periodic_syncs[0].pawr_subevent_interval == 6 &&
           gap_periodic_syncs[0].pawr_response_slot_delay == 1 &&
           gap_periodic_syncs[0].pawr_response_slot_spacing == 2 &&
           gap_periodic_syncs[0].response_access_address == 0x78563412);

    uint64_t first_event = gap_periodic_syncs[0].next_event_ticks;
    fake_radio_ticks = gap_periodic_syncs[0].window_start_ticks;
    gap_hw_mesh_scan_poll();
    assert(gap_radio_periodic_listening &&
           configured_access_address == 0x12345678 &&
           configured_crc_init == 0x123456 &&
           configured_radio_channel ==
               gap_periodic_channel_for(0x12345678,
                   gap_periodic_syncs[0].channel_map, 5));

    fake_radio_ticks = first_event + 152;
    memcpy(rx_frame, sync_ind, sizeof(sync_ind));
    gap_hw_mesh_received();
    gap_hw_mesh_scan_poll();
    assert(gap_periodic_syncs[0].established &&
           gap_periodic_syncs[0].event_counter == 6 &&
           gap_periodic_syncs[0].event_data_active &&
           gap_periodic_syncs[0].window_chain);
    assert(mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_ESTABLISHED &&
           event.handle == handle && event.sid == 6);

    uint64_t chain_window_start = gap_periodic_syncs[0].window_start_ticks;
    fake_radio_ticks = chain_window_start;
    gap_hw_mesh_scan_poll();
    assert(gap_radio_periodic_listening && configured_radio_channel == 1);
    fake_radio_ticks = chain_window_start + 123;
    memcpy(rx_frame, chain_ind, sizeof(chain_ind));
    gap_hw_mesh_received();
    gap_hw_mesh_scan_poll();
    assert(mesh_gap_periodic_report_poll(&report));
    assert(report.handle == handle && report.sid == 6 &&
           report.event_counter == 5 && report.did == 2 &&
           report.rssi == -40 && report.data_len == 3 &&
           !memcmp(report.data, (uint8_t[]){2, 0x01, 0x06}, 3));
    assert(!mesh_gap_periodic_report_poll(&report));
    assert(mesh_gap_periodic_sync_terminate((uint8_t)handle));
    assert(mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_TERMINATED &&
           event.handle == handle && !gap_periodic_syncs[0].used);
    assert(!gap_scanning);

    int pending = mesh_gap_periodic_sync_start(0, advertiser, 7, 1000);
    assert(pending == 1 && mesh_gap_periodic_sync_cancel((uint8_t)pending));
    assert(mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_CANCELLED &&
           event.handle == pending);
    assert(!mesh_gap_periodic_sync_cancel((uint8_t)pending));

    now_ms = 1900;
    fake_radio_ticks = 2000000;
    int missed_handle = mesh_gap_periodic_sync_start(0, advertiser, 7, 1000);
    assert(missed_handle == 1);
    gap_hw_mesh_scan_poll();
    sync_info_pdu[11] = 0x70; // Same SyncInfo, now SID 7.
    memcpy(gap_radio_ext_scan_frame, sync_info_pdu, sizeof(sync_info_pdu));
    gap_radio_ext_scan_kind = MESH_GAP_EXT_ADV_AUXILIARY_PDU;
    gap_radio_ext_scan_ticks = fake_radio_ticks;
    gap_radio_ext_scan_rssi = -35;
    gap_radio_ext_scan_ready = 1;
    gap_hw_mesh_scan_poll();
    for (uint8_t missed = 0; missed < 6; missed++) {
        assert(gap_periodic_syncs[0].used);
        fake_radio_ticks = gap_periodic_syncs[0].window_start_ticks;
        gap_hw_mesh_scan_poll();
        assert(gap_radio_periodic_listening);
        fake_radio_ticks = gap_periodic_syncs[0].window_end_ticks + 1;
        gap_hw_mesh_scan_poll();
    }
    assert(!gap_periodic_syncs[0].used &&
           mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_LOST &&
           event.handle == missed_handle);
    fake_radio_ticks_enabled = 0;
}

static void test_pawr_timing_decode(void) {
    uint8_t pdu[40] = {0};
    pdu[0] = 0x07;
    pdu[1] = 38;
    pdu[2] = 37;
    pdu[3] = 0x29; // AdvA, ADI, SyncInfo, followed by ACAD.
    memcpy(pdu + 4, (uint8_t[]){1, 2, 3, 4, 5, 6}, 6);
    pdu[10] = 6 << 4;
    pdu[12] = 40; // SyncOffset: 1.2 ms in 30 us units.
    pdu[14] = 8; // 10 ms periodic interval.
    memset(pdu + 16, 0xff, 4);
    pdu[20] = 0x1f;
    memcpy(pdu + 21, (uint8_t[]){0x78, 0x56, 0x34, 0x12}, 4);
    memcpy(pdu + 25, (uint8_t[]){0x56, 0x34, 0x12}, 3);
    pdu[28] = 5;
    pdu[30] = 9; // Eight-byte value plus AD type.
    pdu[31] = 0x32; // Periodic Advertising Response Timing Information.
    memcpy(pdu + 32, (uint8_t[]){0x12, 0x34, 0x56, 0x78,
        2, 6, 1, 2}, 8);

    gap_ext_adv_fields fields;
    assert(gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
    assert(fields.has_pawr_timing && fields.response_access_address ==
        0x78563412 && fields.pawr_num_subevents == 2 &&
        fields.pawr_subevent_interval == 6 &&
        fields.pawr_response_slot_delay == 1 &&
        fields.pawr_response_slot_spacing == 2);

    pdu[36] = 0;
    assert(!gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
    pdu[36] = 2;
    pdu[37] = 5; // Invalid subevent interval for two subevents.
    assert(!gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
    pdu[37] = 6;
    pdu[38] = 0xff; // Reserved response-slot delay.
    assert(!gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
    pdu[38] = 1;
    pdu[39] = 1; // Response-slot spacing is at least 0.25 ms.
    assert(!gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
    pdu[39] = 2;
    pdu[32] = pdu[21]; pdu[33] = pdu[22];
    pdu[34] = pdu[23]; pdu[35] = pdu[24]; // RspAA must differ from Sync AA.
    assert(!gap_ext_adv_decode(pdu, sizeof(pdu), &fields));
}

static void test_periodic_sync_connection_arbitration(void) {
    now_ms = 4000;
    fake_radio_ticks = 4000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 0;
    memset(&gap_conn, 0, sizeof(gap_conn));
    memset(gap_periodic_syncs, 0, sizeof(gap_periodic_syncs));
    gap_radio_rx_armed = gap_radio_active_scan_pending = 0;
    gap_radio_periodic_listening = gap_radio_aux_listening = 0;
    gap_radio_ext_scan_ready = 0;
    gap_radio_scan_generation = gap_scan_generation;
    gap_conn.active = 1;
    gap_conn.interval = 24;
    gap_conn.data_capacity = 27;
    gap_conn.last_rx_ms = now_ms;
    gap_conn.next_event_ticks = fake_radio_ticks + HW_TICKS_FROM_US(20000);

    mesh_gap_periodic_sync_context *sync = &gap_periodic_syncs[0];
    sync->used = sync->established = 1;
    sync->handle = 1;
    sync->interval = 8;
    sync->event_counter = 3;
    sync->timeout_ms = 10000;
    sync->last_event_ms = now_ms;
    sync->access_address = 0x12345678;
    sync->crc_init = 0x123456;
    sync->channel_map[0] = sync->channel_map[1] =
        sync->channel_map[2] = sync->channel_map[3] = 0xff;
    sync->channel_map[4] = 0x1f;
    sync->anchor_ticks = fake_radio_ticks;
    sync->next_event_ticks = fake_radio_ticks + HW_TICKS_FROM_US(1000);

    // A periodic event outside the connection guard window can use the radio.
    gap_hw_mesh_scan_poll();
    fake_radio_ticks = sync->window_start_ticks;
    gap_hw_mesh_scan_poll();
    assert(gap_radio_periodic_listening);

    // A connection event preempts that listener and records the missed event.
    gap_radio_connection_take_radio();
    assert(!gap_radio_periodic_listening && !gap_radio_rx_armed &&
           sync->missed_events == 1 && sync->event_counter == 4);

    // A window intersecting the guarded connection event is skipped up front.
    sync->next_event_ticks = fake_radio_ticks + HW_TICKS_FROM_US(1000);
    sync->window_active = 0;
    gap_conn.next_event_ticks = fake_radio_ticks + HW_TICKS_FROM_US(2200);
    gap_hw_mesh_scan_poll();
    fake_radio_ticks = sync->window_start_ticks;
    gap_hw_mesh_scan_poll();
    assert(!gap_radio_periodic_listening && sync->missed_events == 2 &&
           sync->event_counter == 5);

    memset(&gap_conn, 0, sizeof(gap_conn));
    memset(gap_periodic_syncs, 0, sizeof(gap_periodic_syncs));
    fake_radio_ticks_enabled = 0;
}

static void test_extended_advertising_multiple_sets(void) {
    static const uint8_t data0[] = {2, 0x09, 'A'};
    static const uint8_t data1[] = {2, 0x01, 0x06, 2, 0x09, 'B'};
    static const uint8_t invalid_data[] = {1, 0x09};
    now_ms = 1000;
    fake_radio_ticks = 3000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    gap_ext_advertising_next_set = 0;

    assert(mesh_gap_extended_advertising_start_set(0, data0,
        sizeof(data0), 3, 100));
    assert(mesh_gap_extended_advertising_start_set(1, data1,
        sizeof(data1), 8, 100));
    assert(!mesh_gap_extended_advertising_start_set(
        MESH_GAP_EXT_ADV_SET_COUNT, invalid_data, sizeof(invalid_data),
        9, 100));
    assert(!mesh_gap_extended_advertising_start_set(0, data1,
        sizeof(data1), 9, 100));
    assert(mesh_gap_discoverable());

    // Both sets are due; successive poll calls must service each set once.
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(captured_extended_count == 4);
    assert((captured_extended_pdus[0][5] >> 4) == 3);
    assert((captured_extended_pdus[2][5] >> 4) == 8);
    assert((captured_extended_pdus[1][11] >> 4) == 3 &&
           !memcmp(captured_extended_pdus[1] + 12, data0, sizeof(data0)));
    assert((captured_extended_pdus[3][11] >> 4) == 8 &&
           !memcmp(captured_extended_pdus[3] + 12, data1, sizeof(data1)));
    assert(gap_ext_advertising_next_set == 0);

    assert(mesh_gap_extended_advertising_stop_set(0));
    assert(!gap_ext_advertising[0].enabled && gap_ext_advertising[1].enabled);
    assert(mesh_gap_discoverable());
    assert(mesh_gap_extended_advertising_stop_set(1));
    assert(!GAP_EXT_ADVERTISING_ENABLED);
    assert(!mesh_gap_discoverable());
    assert(!mesh_gap_extended_advertising_stop_set(
        MESH_GAP_EXT_ADV_SET_COUNT));
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
}

static void test_extended_scannable_advertising(void) {
    static const uint8_t local_address[6] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0xc6
    };
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
    data[0] = 2;
    data[1] = 0x01;
    data[2] = 0x06;
    size_t offset = 3;
    while (offset < sizeof(data)) {
        size_t chunk = sizeof(data) - offset;
        if (chunk > 256) chunk = 256;
        data[offset] = (uint8_t)(chunk - 1);
        data[offset + 1] = 0x09;
        memset(data + offset + 2, 'R', chunk - 2);
        offset += chunk;
    }

    mesh_gap_extended_scan_report report;
    now_ms = 1100;
    fake_radio_ticks = 4000000;
    fake_radio_ticks_enabled = 1;
    fake_radio_tick_autoincrement = 1;
    captured_extended_count = 0;
    assert(mesh_gap_set_static_random_address(local_address));
    assert(mesh_gap_privacy_filter(0, 0));
    assert(mesh_gap_advertising_filter_policy(0, 0));
    assert(mesh_gap_scan_configure(20, 20, MESH_GAP_DISCOVERY_ALL, 0));
    mesh_gap_scan_start();
    assert(mesh_gap_extended_scannable_advertising_start_set(0, data,
        sizeof(data), 6, 100));
    force_aux_scan_request = 1;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(!force_aux_scan_request && captured_extended_count == 9);

    const uint8_t *primary = captured_extended_pdus[0];
    const uint8_t *auxiliary = captured_extended_pdus[1];
    const uint8_t *scan_response = captured_extended_pdus[2];
    assert(primary[2] == 0x86 && primary[3] == 0x18);
    assert(auxiliary[1] == 10 && auxiliary[2] == 0x89 &&
           auxiliary[3] == 0x09 &&
           !memcmp(auxiliary + 10, primary + 4, 2));
    assert(scan_response[1] == 253 && scan_response[2] == 12 &&
           scan_response[3] == 0x19 &&
           !memcmp(scan_response + 10, primary + 4, 2) &&
           !memcmp(scan_response + 15, data, MESH_GAP_EXT_ADV_FIRST_PDU_DATA_MAX));
    assert(captured_extended_channels[2] == 0 &&
           captured_extended_channels[3] == 1);
    assert(captured_extended_ticks[3] - captured_extended_ticks[2] >= 2730 &&
           captured_extended_ticks[3] - captured_extended_ticks[2] <= 2760);
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_PRIMARY_PDU,
        primary, captured_extended_pdu_lengths[0], -40));
    assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
        auxiliary, captured_extended_pdu_lengths[1], -40));
    for (uint8_t i = 2; i < captured_extended_count; i++) {
        if (i == 8) {
            assert(gap_ext_adv_contexts[0].data_len == 1470);
            assert(!memcmp(gap_ext_adv_contexts[0].data, data, 1470));
            assert(!memcmp(captured_extended_pdus[i] + 6, data + 1470, 180));
        }
        assert(mesh_gap_extended_scan_receive(MESH_GAP_EXT_ADV_AUXILIARY_PDU,
            captured_extended_pdus[i], captured_extended_pdu_lengths[i], -40));
    }
    assert(gap_ext_adv_report_count == 1);
    assert(mesh_gap_extended_scan_poll(&report));
    assert(report.sid == 6 && report.data_len == sizeof(data) &&
           !memcmp(report.data, data, sizeof(data)));

    assert(mesh_gap_extended_advertising_stop_set(0));
    mesh_gap_scan_stop();
    static const uint8_t allowed_peer[6] = {
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56
    };
    static const uint8_t short_response[] = {2, 0x09, 'F'};
    assert(mesh_gap_accept_list_add(allowed_peer, 0));
    assert(mesh_gap_advertising_filter_policy(1, 0));
    captured_extended_count = 0;
    assert(mesh_gap_extended_scannable_advertising_start_set(0,
        short_response, sizeof(short_response), 7, 100));
    force_aux_scan_request = 1;
    assert(gap_hw_mesh_send_due(NULL, 0, now_ms, NULL, NULL) == 0);
    assert(!force_aux_scan_request && captured_extended_count == 2 &&
           !gap_radio_ext_adv_scan_response_started);
    assert(mesh_gap_extended_advertising_stop_set(0));
    assert(mesh_gap_accept_list_clear());
    assert(mesh_gap_advertising_filter_policy(0, 0));
    assert(mesh_gap_use_public_address());
    fake_radio_tick_autoincrement = 0;
    fake_radio_ticks_enabled = 0;
}
#endif

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

    assert(mesh_gap_accept_list_clear());
    assert(mesh_gap_accept_list_add(test_identity, 0));
    assert(!mesh_gap_advertising_filter_policy(2, 0));
    assert(mesh_gap_advertising_filter_policy(1, 1));
    gap_radio_advertising_rx_event = 1;
    gap_radio_adv_frame[0] = 0; // Undirected connectable advertising.
    BLE_GAP_HW_PUBLIC_ADDRESS(gap_radio_adv_frame + 2);
    gap_radio_scan_response_started = 0;
    rx_frame[0] = 0x43; rx_frame[1] = 12;
    const uint8_t unknown_peer[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    memcpy(rx_frame + 2, unknown_peer, 6);
    memcpy(rx_frame + 8, gap_radio_adv_frame + 2, 6);
    before = link_tx_count;
    gap_hw_mesh_received();
    assert(link_tx_count == before && !gap_radio_scan_response_started);

    memcpy(rx_frame + 2, test_rpa, 6);
    gap_radio_rx_ready = 0;
    gap_hw_mesh_received();
    assert(link_tx_count == before + 1 && gap_radio_scan_response_started);
    gap_radio_scan_response_started = 0;

    rx_frame[0] = 0x45; rx_frame[1] = 34;
    memcpy(rx_frame + 2, unknown_peer, 6);
    gap_radio_connect_request_ready = 0;
    gap_hw_mesh_received();
    assert(!gap_radio_connect_request_ready);
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_radio_connect_request_ready);
    gap_radio_advertising_rx_event = 0;
    gap_radio_connect_request_ready = 0;
    assert(mesh_gap_advertising_filter_policy(0, 0));
    assert(mesh_gap_accept_list_clear());
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

static void test_general_connection_establishment(void) {
    assert(!mesh_gap_connect_general_start(2));
    assert(mesh_gap_connect_general_start(1));
    assert(gap_central_connect.active && gap_central_connect.any_peer);
    assert(gap_scanning && gap_active_scanning);

    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x42; // Non-connectable advertisement must be ignored.
    rx_frame[1] = 6;
    const uint8_t peer[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    memcpy(rx_frame + 2, peer, sizeof(peer));
    gap_hw_mesh_received();
    assert(gap_central_connect.active && !gap_conn.active);

    rx_frame[0] = 0x00; // Connectable undirected advertisement.
    gap_hw_mesh_received();
    assert(gap_conn.active && gap_conn.central_role);
    assert(!gap_central_connect.active && !gap_scanning);
    assert(memcmp(gap_conn.peer_identity_address, peer, 6) == 0);
    assert(memcmp(gap_central_connect.request + 8, peer, 6) == 0);
    gap_connection_end();
}

static void test_selective_connection_establishment(void) {
    const uint8_t other_peer[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    assert(mesh_gap_accept_list_clear());
    assert(!mesh_gap_accept_list_add(NULL, 0));
    assert(!mesh_gap_accept_list_add(test_identity, 2));
    assert(!mesh_gap_accept_list_add((uint8_t[]){0, 0, 0, 0, 0, 0xc0}, 1));
    assert(!mesh_gap_accept_list_add(
        (uint8_t[]){0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 1));
    assert(!mesh_gap_identity_set((uint8_t[]){0, 0, 0, 0, 0, 0xc0}, 1,
                                  test_irk));
    assert(!mesh_gap_identity_set(
        (uint8_t[]){0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 1, test_irk));
    assert(mesh_gap_accept_list_add(test_identity, 0));
    assert(mesh_gap_accept_list_add(test_identity, 0)); // Duplicate is harmless.
    assert(mesh_gap_connect_selective_start(0));
    assert(gap_central_connect.active && gap_central_connect.selective &&
           !gap_central_connect.any_peer && gap_scanning && !gap_active_scanning);
    assert(!mesh_gap_accept_list_remove(test_identity, 0)); // Frozen while scanning.
    assert(!mesh_gap_accept_list_add(other_peer, 0));

    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x00;
    rx_frame[1] = 6;
    memcpy(rx_frame + 2, other_peer, 6);
    gap_hw_mesh_received();
    assert(gap_central_connect.active && !gap_conn.active);

    // A listed identity also matches its resolvable private address.
    rx_frame[0] = 0x40;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active && gap_conn.central_role && !gap_central_connect.active);
    assert(memcmp(gap_conn.peer_identity_address, test_identity, 6) == 0);
    assert(memcmp(gap_central_connect.request + 8, test_rpa, 6) == 0);
    gap_connection_end();

    assert(mesh_gap_accept_list_remove(test_identity, 0));
    assert(!mesh_gap_connect_selective_start(0));
    assert(mesh_gap_accept_list_clear());
}

static void test_auto_connection_establishment(void) {
    assert(mesh_gap_accept_list_clear());
    assert(mesh_gap_accept_list_add(test_identity, 0));
    assert(mesh_gap_connect_auto_start());
    assert(gap_central_connect.active && gap_central_connect.auto_connect &&
           gap_scanning && !gap_active_scanning);
    now_ms += 12000;
    gap_hw_mesh_scan_poll();
    assert(gap_central_connect.active && gap_scanning); // Background mode has no attempt timeout.
    mesh_gap_connect_cancel();
    assert(!gap_central_connect.active && !gap_central_connect.auto_connect &&
           !gap_scanning);

    assert(mesh_gap_connect_auto_start());
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 0x40; rx_frame[1] = 6;
    memcpy(rx_frame + 2, test_rpa, 6);
    gap_hw_mesh_received();
    assert(gap_conn.active && gap_conn.central_role && !gap_central_connect.active);
    assert(memcmp(gap_conn.peer_identity_address, test_identity, 6) == 0);
    gap_connection_end();
    assert(mesh_gap_accept_list_clear());
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

static void test_randomized_private_rotation(void) {
    now_ms = 0;
    assert(!mesh_gap_privacy_set_randomized(NULL, 1, 4));
    assert(!mesh_gap_privacy_set_randomized(test_irk, 0, 4));
    assert(!mesh_gap_privacy_set_randomized(test_irk, 1, 3601));
    assert(!mesh_gap_privacy_set_randomized(test_irk, 5, 4));
    secure_random_available = 0;
    assert(!mesh_gap_privacy_set_randomized(test_irk, 1, 4));
    secure_random_available = 1;

    random_seed++;
    assert(mesh_gap_privacy_set_randomized(test_irk, 1, 4));
    assert(gap_privacy.timeout_s >= 1 && gap_privacy.timeout_s <= 4);
    assert(gap_privacy.next_rotation_ms == now_ms +
           (uint32_t)gap_privacy.timeout_s * 1000);
    now_ms = gap_privacy.next_rotation_ms;
    uint8_t current_address[6];
    memcpy(current_address, gap_random_address, sizeof(current_address));
    uint32_t current_deadline = gap_privacy.next_rotation_ms;
    random_seed++;
    secure_random_available = 0;
    gap_privacy_poll(now_ms);
    secure_random_available = 1;
    assert(!memcmp(current_address, gap_random_address, sizeof(current_address)));
    assert(gap_privacy.next_rotation_ms == current_deadline);
    random_seed++;
    gap_privacy_poll(now_ms);
    assert(memcmp(current_address, gap_random_address, sizeof(current_address)));
    assert(gap_privacy.timeout_s >= 1 && gap_privacy.timeout_s <= 4);
    assert(gap_privacy.next_rotation_ms == now_ms +
           (uint32_t)gap_privacy.timeout_s * 1000);
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

static void test_extended_length_control_pdu(void) {
    start_test_central_link();
    gap_conn.data_length.rx_octets = 251; // Simulate a completed DLE exchange.
    gap_conn.rx_armed = 1;
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = 36;
    rx_frame[2] = 0x2a; // LL_PERIODIC_SYNC_WR_IND is still unsupported.
    memset(rx_frame + 3, 0, 35);
    gap_hw_mesh_received();
    assert(gap_conn.active && tx_buffer[1] == 2 && tx_buffer[2] == 0x07 &&
           tx_buffer[3] == 0x2a); // Large unsupported control gets UNKNOWN_RSP.
    gap_connection_end();
}

static void test_periodic_sync_transfer_receive(void) {
    static const uint8_t advertiser[6] = {1, 2, 3, 4, 5, 6};
    static const uint8_t first_periodic_event[] = {
        0x07, 9, 6, 0x18, 0x02, 0x60, 1, 20, 0, 2, 0x01
    };
    uint8_t valid_transfer[37];
    mesh_gap_periodic_sync_event event;
    now_ms = 3000;
    fake_radio_ticks_enabled = 0;
    assert(mesh_gap_periodic_sync_transfer_enable(1, 1000));
    start_test_central_link();
    gap_conn.data_length.rx_octets = 251;

    uint16_t current_event = gap_conn.event_counter;
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = 35;
    memset(rx_frame + 2, 0, 35);
    rx_frame[2] = 0x1c; // LL_PERIODIC_SYNC_IND.
    rx_frame[3] = 0x34; rx_frame[4] = 0x12; // Transfer ID.
    rx_frame[5] = 40; // SyncOffset: 1.2 ms in 30 us units.
    rx_frame[7] = 8; // 10 ms periodic interval.
    memset(rx_frame + 9, 0xff, 4);
    rx_frame[13] = 0x1f;
    rx_frame[14] = 0x78; rx_frame[15] = 0x56;
    rx_frame[16] = 0x34; rx_frame[17] = 0x12;
    rx_frame[18] = 0x56; rx_frame[19] = 0x34;
    rx_frame[20] = 0x12;
    rx_frame[21] = 5; // PeriodicEventCounter.
    rx_frame[23] = (uint8_t)(current_event + 1);
    rx_frame[24] = (uint8_t)((current_event + 1) >> 8);
    rx_frame[25] = 5; // lastPaEventCounter matches SyncInfo.
    rx_frame[27] = 6; // SID 6, public advertiser, SCA code 0.
    rx_frame[28] = 1; // LE 1M PHY.
    memcpy(rx_frame + 29, advertiser, sizeof(advertiser));
    rx_frame[35] = (uint8_t)current_event;
    rx_frame[36] = (uint8_t)(current_event >> 8);
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    memcpy(valid_transfer, rx_frame, sizeof(valid_transfer));
    assert(gap_conn.active && gap_periodic_syncs[0].used &&
           !gap_periodic_syncs[0].established &&
           gap_periodic_syncs[0].handle == 1 &&
           gap_periodic_syncs[0].sid == 6 &&
           gap_periodic_syncs[0].phy == MESH_GAP_PHY_1M &&
           gap_periodic_syncs[0].event_counter == 5 &&
           gap_periodic_syncs[0].widening_ppm == 1500 &&
           gap_periodic_syncs[0].window_active &&
           !memcmp(gap_periodic_syncs[0].address, advertiser, 6));

    uint64_t first_event = gap_periodic_syncs[0].next_event_ticks;
    assert(gap_periodic_sync_receive(0, first_periodic_event,
        sizeof(first_periodic_event), MESH_GAP_PHY_1M, -40,
        first_event + 152));
    assert(mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_ESTABLISHED &&
           event.handle == 1 && event.sid == 6);
    assert(!mesh_gap_periodic_sync_transfer(1, 0x1234));
    gap_conn.data_length.tx_octets = 251;
    gap_conn.data_length.tx_time = 2120;
    assert(mesh_gap_periodic_sync_transfer(1, 0x1234));
    receive_test_link_packet(1);
    assert(tx_buffer[1] == 35 && tx_buffer[2] == 0x1c &&
           tx_buffer[3] == 0x34 && tx_buffer[4] == 0x12 &&
           tx_buffer[7] == 8 && tx_buffer[28] == MESH_GAP_PHY_1M &&
           !memcmp(tx_buffer + 29, advertiser, sizeof(advertiser)) &&
           !(tx_buffer[6] & 0x80));
    uint8_t transferred_sync_info[30] = {
        0x07, 28, 27, 0x29
    };
    memcpy(transferred_sync_info + 4, tx_buffer + 29, 6);
    transferred_sync_info[11] = (uint8_t)(tx_buffer[27] & 0x0f) << 4;
    memcpy(transferred_sync_info + 12, tx_buffer + 5, 18);
    gap_ext_adv_fields transferred_fields;
    assert(gap_ext_adv_decode(transferred_sync_info,
        sizeof(transferred_sync_info), &transferred_fields) &&
        transferred_fields.sid == 6 &&
        transferred_fields.sync_interval == 8 &&
        transferred_fields.sync_access_address == 0x12345678 &&
        transferred_fields.sync_crc_init == 0x123456);
    assert(mesh_gap_periodic_sync_terminate(1));
    assert(mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_TERMINATED);

    // Invalid PHY and malformed SyncInfo must not allocate sync state.
    memset(rx_frame, 0, sizeof(rx_frame));
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[1] = 35;
    rx_frame[2] = 0x1c;
    rx_frame[28] = 3; // Reserved periodic PHY encoding.
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_periodic_syncs[0].used);
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ 1) << 2);
    rx_frame[28] = 1;
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    assert(gap_conn.active && !gap_periodic_syncs[0].used);

    // An accepted transfer that never receives its first PA event times out.
    assert(mesh_gap_periodic_sync_transfer_enable(1, 100));
    memcpy(rx_frame, valid_transfer, sizeof(valid_transfer));
    rx_frame[0] = 3 | (gap_conn.expected_rx_sn << 3) |
        ((gap_conn.tx_sn ^ 1) << 2);
    uint16_t timeout_event = gap_conn.event_counter;
    rx_frame[23] = (uint8_t)(timeout_event + 1);
    rx_frame[24] = (uint8_t)((timeout_event + 1) >> 8);
    rx_frame[35] = (uint8_t)timeout_event;
    rx_frame[36] = (uint8_t)(timeout_event >> 8);
    gap_conn.rx_armed = 1;
    gap_hw_mesh_received();
    assert(gap_periodic_syncs[0].used && !gap_periodic_syncs[0].established);
    now_ms += 100;
    gap_hw_mesh_scan_poll();
    assert(!gap_periodic_syncs[0].used &&
           mesh_gap_periodic_sync_event_poll(&event) &&
           event.type == MESH_GAP_PERIODIC_SYNC_LOST);
    gap_connection_end();
    assert(mesh_gap_periodic_sync_transfer_enable(0, 0));
    fake_radio_ticks_enabled = 0;
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

// Feed SMP through the shared RX slot, including fragmented L2CAP packets.
static void receive_test_smp(const uint8_t *p, uint8_t len, uint8_t split) {
    uint8_t frame[69] = {len, 0, 6, 0};
    memcpy(frame + 4, p, len);
    uint8_t total = len + 4, offset = 0;
    while (offset < total) {
        uint8_t chunk = offset ? 27 : split ? split : 27;
        if (chunk > total - offset) chunk = total - offset;
        gap_conn.tx_queued = 0;
        gap_conn.rx_llid = offset ? 1 : 2;
        gap_conn.rx_len = chunk;
        memcpy(gap_conn.rx_data, frame + offset, chunk);
        gap_conn.rx_ready = 1;
        mesh_gap_smp_poll();
        assert(!gap_conn.rx_ready);
        offset += chunk;
    }
}

static void test_smp_pairing(void) {
    start_test_central_link();
    // Published Bluetooth c1 test vector, using exact on-air byte order.
    uint8_t request[7] = {1,1,0,0,16,7,7}, response[7] = {2,3,0,0,8,0,5};
    uint8_t ia[6] = {0xa6,0xa5,0xa4,0xa3,0xa2,0xa1};
    uint8_t ra[6] = {0xb6,0xb5,0xb4,0xb3,0xb2,0xb1};
    uint8_t random[16] = {0xe0,0x2e,0x70,0xc6,0x4e,0x27,0x88,0x63,
                          0x0e,0x6f,0xad,0x56,0x21,0xd5,0x83,0x57};
    uint8_t expected[16] = {0x86,0x3b,0xf1,0xbe,0xc5,0x4d,0xa7,0xd2,
                            0xea,0x88,0x89,0x87,0xef,0x3f,0x1e,0x1e}, confirm[16];
    memcpy(gap_smp.request, request, 7); memcpy(gap_smp.response, response, 7);
    memcpy(gap_conn.initiator, ia, 6); memcpy(gap_conn.responder, ra, 6);
    gap_conn.initiator_type = 1; gap_conn.responder_type = 0;
    gap_smp_confirm(random, confirm);
    assert(!memcmp(confirm, expected, 16));
    uint8_t s1_input[16] = {0,0xff,0xee,0xdd,0xcc,0xbb,0xaa,0x99,
                            0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11};
    uint8_t s1_expected[16] = {0x62,0xa0,0x6d,0x79,0xae,0x16,0x42,0x5b,
                              0x9b,0xf4,0xb0,0xe8,0xf0,0xe1,0x1f,0x9a};
    gap_smp_e(s1_input, confirm); assert(!memcmp(confirm, s1_expected, 16));
    gap_connection_end();

    for (uint8_t central = 0; central < 2; central++) {
        start_test_central_link(); gap_conn.central_role = central;
        mesh_gap_pairing_set(1);
        assert(mesh_gap_pair());
        uint8_t features[7] = {central ? 2 : 1, 3, 0, 0, central ? 16 : 7, 0, 0};
        receive_test_smp(features, 7, 5);
        assert(gap_smp.phase == GAP_SMP_CONFIRM);
        uint8_t peer_random[17] = {4}, peer_confirm[17] = {3};
        for (unsigned i = 1; i < 17; i++) peer_random[i] = (uint8_t)(0xb0 + i);
        gap_smp_confirm(peer_random + 1, peer_confirm + 1);
        receive_test_smp(peer_confirm, 17, 0);
        assert(gap_smp.phase == GAP_SMP_RANDOM);
        receive_test_smp(peer_random, 17, 9);
        assert(gap_smp.phase == GAP_SMP_ENCRYPT);
        for (unsigned i = gap_smp.key_size; i < 16; i++) assert(!gap_smp.stk[i]);
        if (central) {
            mesh_gap_smp_poll();
            assert(gap_security.phase == GAP_ENC_QUEUED);
            assert(!memcmp(gap_security.ltk, gap_smp.stk, 16));
        } else {
            gap_conn.tx_queued = 0;
            mesh_gap_smp_poll(); // Queue our Pairing Random before ENC_REQ.
            gap_conn.tx_queued = 0;
            gap_security.phase = GAP_ENC_KEY_REQUEST;
            memset(gap_security.random, 0, 8); gap_security.ediv = 0;
            mesh_gap_smp_poll();
            assert(gap_security.phase == GAP_ENC_START_QUEUED);
        }
        // The encryption engine is covered independently by test_link_encryption.
        gap_security.phase = 0; gap_security.status = 0;
        gap_security.tx_enabled = gap_security.rx_enabled = 1;
        mesh_gap_smp_poll();
        assert(mesh_gap_pairing_status() == 0 && !gap_smp.phase);
        for (unsigned i = 0; i < 16; i++) assert(!gap_smp.stk[i] && !gap_smp.random[i]);
        gap_connection_end();
    }
    start_test_central_link(); gap_conn.central_role = 0;
    mesh_gap_pairing_set(0);
    uint8_t features[7] = {1,3,0,0,16,0,0};
    receive_test_smp(features, 7, 0);
    assert(gap_smp.status == 5 && gap_smp.tx[4] == 5);
    gap_smp.tx_len = 0;
    mesh_gap_pairing_set(1);
    features[3] = 4;
    receive_test_smp(features, 7, 0); assert(gap_smp.status == 3);
    features[3] = 0; gap_smp.tx_len = 0;
    secure_random_available = 0;
    receive_test_smp(features, 7, 0); assert(gap_smp.status == 8);
    secure_random_available = 1; gap_smp.tx_len = 0;
    receive_test_smp(features, 7, 0);
    uint8_t bad_confirm[17] = {3}, peer_random[17] = {4};
    receive_test_smp(bad_confirm, 17, 0);
    receive_test_smp(peer_random, 17, 0);
    assert(gap_smp.status == 4 && gap_security.phase == GAP_ENC_IDLE);
    gap_smp.tx_len = 0;
    receive_test_smp(features, 7, 0);
    now_ms = gap_smp.started_ms + 30000;
    mesh_gap_smp_poll();
    assert(gap_smp.blocked && !gap_smp.tx_len && !mesh_gap_pair());
    receive_test_smp(features, 7, 0); assert(!gap_smp.tx_len);
    gap_connection_end();
    start_test_central_link();
    assert(!gap_smp.blocked);
    // ATT must remain available to the application/GATT consumer.
    uint8_t att[5] = {1,0,4,0,0x0a}, out[27], llid;
    gap_conn.rx_llid = 2; gap_conn.rx_len = 5;
    memcpy(gap_conn.rx_data, att, 5); gap_conn.rx_ready = 1;
    size_t len = sizeof(out);
    assert(mesh_gap_receive_data(&llid, out, &len) == 1);
    assert(llid == 2 && len == 5 && !memcmp(att, out, 5));
    // SMP waits for all fragments of an application PDU to be queued.
    mesh_gap_pairing_set(1);
    uint8_t first[5] = {3,0,4,0,0x0a}, tail[2] = {0,0};
    gap_conn.tx_queued = 0;
    assert(mesh_gap_send_data(2, first, 5));
    assert(mesh_gap_pair());
    gap_conn.tx_queued = 0;
    mesh_gap_smp_poll();
    assert(gap_smp.tx_len && !gap_conn.tx_queued);
    assert(mesh_gap_send_data(1, tail, 2));
    gap_conn.tx_queued = 0;
    mesh_gap_smp_poll();
    assert(!gap_smp.tx_len && gap_conn.tx_queued && gap_conn.tx_data[2] == 6);
    mesh_gap_pairing_set(0);
    gap_connection_end();
}

// Independent AES calculation with the user's TK, rather than assuming TK=0.
static void test_passkey_confirm(uint32_t passkey, const uint8_t random[16], uint8_t out[16]) {
    uint8_t key[16] = {0}, p1[16], p2[16] = {0}, block[16], encrypted[16];
    for (unsigned i = 0; i < 4; i++) key[15 - i] = (uint8_t)(passkey >> (8 * i));
    p1[0] = gap_conn.initiator_type; p1[1] = gap_conn.responder_type;
    memcpy(p1 + 2, gap_smp.request, 7); memcpy(p1 + 9, gap_smp.response, 7);
    memcpy(p2, gap_conn.responder, 6); memcpy(p2 + 6, gap_conn.initiator, 6);
    for (unsigned i = 0; i < 16; i++) block[15 - i] = random[i] ^ p1[i];
    AES_KEY aes;
    assert(AES_set_encrypt_key(key, 128, &aes) == 0);
    AES_encrypt(block, encrypted, &aes);
    for (unsigned i = 0; i < 16; i++) block[i] = encrypted[i] ^ p2[15 - i];
    AES_encrypt(block, encrypted, &aes);
    for (unsigned i = 0; i < 16; i++) out[i] = encrypted[15 - i];
}

static void test_passkey_pairing(void) {
    assert(!mesh_gap_security_set(5, 0, 16));
    assert(!mesh_gap_security_set(MESH_GAP_IO_NONE, 1, 16));
    assert(!mesh_gap_security_set(2, 1, 6));
    assert(!mesh_gap_passkey_reply(19655));
    mesh_gap_pairing_set(1);
    for (uint8_t central = 0; central < 2; central++) {
        for (uint8_t local_io = 0; local_io < 5; local_io++) {
            if (local_io == MESH_GAP_IO_NONE) continue;
            for (uint8_t peer_io = 0; peer_io < 5; peer_io++) {
                start_test_central_link(); gap_conn.central_role = central;
                assert(mesh_gap_security_set(local_io, 1, 16));
                assert(mesh_gap_pair());
                uint8_t features[7] = {central ? 2 : 1, peer_io, 0, 4, 16, 0, 0};
                receive_test_smp(features, 7, 0);
                if (peer_io == MESH_GAP_IO_NONE || (local_io < 2 && peer_io < 2)) {
                    assert(mesh_gap_pairing_status() == 3);
                    gap_connection_end(); continue;
                }
                uint8_t input = local_io == 2 ||
                    (local_io == 4 && (peer_io < 2 || (peer_io == 4 && !central)));
                uint32_t passkey = 19655;
                assert(mesh_gap_passkey(&passkey) == (input ? MESH_GAP_PASSKEY_INPUT : MESH_GAP_PASSKEY_DISPLAY));
                assert(passkey <= 999999);
                assert(!mesh_gap_authenticated() && !mesh_gap_key_size());
                assert(!mesh_gap_security_set(0, 0, 7));
                uint8_t peer_random[17] = {4}, confirm[17] = {3};
                for (unsigned i = 1; i < 17; i++) peer_random[i] = (uint8_t)(0x80 + i);
                test_passkey_confirm(passkey, peer_random + 1, confirm + 1);
                // The Peripheral may receive a confirm while the user is typing.
                if (input && !central) {
                    receive_test_smp(confirm, 17, 0);
                    assert(gap_smp.phase == GAP_SMP_PASSKEY && gap_smp.confirm_received);
                }
                if (input) {
                    assert(!mesh_gap_passkey_reply(1000000));
                    assert(mesh_gap_passkey_reply(passkey));
                    mesh_gap_smp_poll();
                    assert(!mesh_gap_passkey_reply(passkey));
                }
                if (!(input && !central)) receive_test_smp(confirm, 17, 0);
                assert(gap_smp.phase == GAP_SMP_RANDOM);
                uint8_t expected_key[16], s1_input[16], key[16] = {0};
                memcpy(s1_input, central ? gap_smp.random : peer_random + 1, 8);
                memcpy(s1_input + 8, central ? peer_random + 1 : gap_smp.random, 8);
                for (unsigned i = 0; i < 4; i++) key[15 - i] = (uint8_t)(passkey >> (8 * i));
                uint8_t be_input[16], be_key[16];
                for (unsigned i = 0; i < 16; i++) be_input[i] = s1_input[15 - i];
                AES_KEY aes; assert(AES_set_encrypt_key(key, 128, &aes) == 0);
                AES_encrypt(be_input, be_key, &aes);
                for (unsigned i = 0; i < 16; i++) expected_key[i] = be_key[15 - i];
                receive_test_smp(peer_random, 17, 0);
                assert(gap_smp.phase == GAP_SMP_ENCRYPT);
                assert(!memcmp(expected_key, gap_smp.stk, 16));
                for (unsigned i = 0; i < 16; i++) assert(!gap_smp.tk[i]);
                gap_conn.tx_queued = 0;
                mesh_gap_smp_poll();
                if (!central) {
                    gap_conn.tx_queued = 0;
                    gap_security.phase = GAP_ENC_KEY_REQUEST;
                    memset(gap_security.random, 0, 8); gap_security.ediv = 0;
                    mesh_gap_smp_poll();
                    assert(gap_smp.encryption_started);
                }
                gap_security.phase = 0; gap_security.status = 0;
                gap_security.tx_enabled = gap_security.rx_enabled = 1;
                mesh_gap_smp_poll();
                assert(mesh_gap_authenticated() && mesh_gap_key_size() == 16);
                assert(!mesh_gap_passkey(NULL));
                gap_connection_end();
                assert(!mesh_gap_authenticated() && !mesh_gap_key_size());
            }
        }
    }
    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_security_set(2, 1, 16));
    uint8_t features[7] = {1,0,0,4,8,0,0};
    receive_test_smp(features, 7, 0); assert(gap_smp.status == 6);
    gap_smp.tx_len = 0; features[4] = 16;
    receive_test_smp(features, 7, 0);
    assert(mesh_gap_pair_cancel() && gap_smp.status == 1 && !mesh_gap_passkey(NULL));
    assert(!mesh_gap_pair_cancel());
    gap_smp.tx_len = 0;
    receive_test_smp(features, 7, 0);
    assert(mesh_gap_passkey_reply(19655)); mesh_gap_smp_poll();
    uint8_t wrong_confirm[17] = {3}, peer_random[17] = {4};
    test_passkey_confirm(19656, peer_random + 1, wrong_confirm + 1);
    receive_test_smp(wrong_confirm, 17, 0);
    receive_test_smp(peer_random, 17, 0);
    assert(gap_smp.status == 4 && !mesh_gap_authenticated());
    gap_smp.tx_len = 0;
    receive_test_smp(features, 7, 0);
    now_ms = gap_smp.started_ms + 30000;
    mesh_gap_smp_poll();
    assert(gap_smp.blocked && !mesh_gap_passkey_reply(19655));
    for (unsigned i = 0; i < 16; i++) assert(!gap_smp.tk[i]);
    gap_connection_end();
    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_security_set(0, 1, 16));
    features[1] = 2;
    secure_random_passkey_fail = 1;
    receive_test_smp(features, 7, 0);
    assert(gap_smp.status == 8 && !mesh_gap_passkey(NULL));
    secure_random_passkey_fail = 0; gap_smp.tx_len = 0;
    secure_random_passkey_reject = 1;
    receive_test_smp(features, 7, 0);
    assert(gap_smp.status == 8); // Bounded rejection sampling, no biased fallback.
    secure_random_passkey_reject = 0; gap_smp.tx_len = 0;
    secure_random_disconnect = 1;
    receive_test_smp(features, 7, 0);
    secure_random_disconnect = 0;
    assert(!gap_conn.active);
    for (unsigned i = 0; i < 16; i++) assert(!gap_smp.random[i] && !gap_smp.tk[i]);
    // A Peripheral's authentication request must also reach the Central's preq.
    start_test_central_link(); assert(mesh_gap_security_set(0, 0, 7));
    uint8_t request[2] = {11,4};
    receive_test_smp(request, 2, 0);
    assert(gap_smp.request[3] == 4 && gap_smp.tx[7] == 4);
    gap_connection_end();
    assert(mesh_gap_security_set(MESH_GAP_IO_NONE, 0, 7));
    mesh_gap_pairing_set(0);
}

static void test_secure_connections_just_works(void) {
    int vector_result = ble_sc_crypto_test();
    if (vector_result) fprintf(stderr, "SC vector failed: %d\n", vector_result);
    assert(vector_result == 0);
    start_test_central_link();
    mesh_gap_pairing_set(1);
    assert(mesh_gap_security_set(MESH_GAP_IO_NONE, 0, 16));
    assert(mesh_gap_secure_connections_set(1));
    assert(mesh_gap_pair());
    assert(gap_smp.request[3] == 8);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send Pairing Request.
    const uint8_t response[7] = {2, MESH_GAP_IO_NONE, 0, 8, 16, 0, 0};
    receive_test_smp(response, sizeof(response), 0);
    assert(gap_smp.phase == GAP_SMP_SC_PUBLIC_KEY && gap_smp.tx_len == 69);
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        assert(gap_conn.tx_llid == (fragment ? 1 : 2));
    }
    assert(!gap_smp.tx_len && !gap_conn.tx_l2cap_remaining);
    uint8_t peer_private[32] = {0}, peer_public[64], public_pdu[65] = {12};
    peer_private[31] = 7;
    assert(uECC_compute_public_key(peer_private, peer_public, uECC_secp256r1()));
    gap_sc_reverse(public_pdu + 1, peer_public, 32);
    gap_sc_reverse(public_pdu + 33, peer_public + 32, 32);
    receive_test_smp(public_pdu, sizeof(public_pdu), 0);
    assert(gap_smp.phase == GAP_SMP_SC_CONFIRM && !gap_smp.tx_len);
    uint8_t expected_dhkey[32];
    assert(uECC_shared_secret(gap_smp.sc.public_key, peer_private,
                              expected_dhkey, uECC_secp256r1()));
    assert(!memcmp(gap_smp.sc.dhkey, expected_dhkey, 32));
    uint8_t peer_nonce[17] = {4}, peer_confirm[17] = {3};
    for (uint8_t i = 1; i < sizeof(peer_nonce); i++) peer_nonce[i] = i + 70;
    gap_sc_confirm_value(peer_public, gap_smp.sc.public_key,
                         peer_nonce + 1, 0, peer_confirm + 1);
    receive_test_smp(peer_confirm, sizeof(peer_confirm), 0);
    assert(gap_smp.phase == GAP_SMP_SC_RANDOM);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send our Pairing Random.
    receive_test_smp(peer_nonce, sizeof(peer_nonce), 0);
    assert(gap_smp.phase == GAP_SMP_SC_DHKEY && gap_smp.tx[4] == 13);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send our DHKey Check.
    uint8_t peer_check[17] = {13};
    gap_sc_dhkey_check(0, peer_check + 1);
    receive_test_smp(peer_check, sizeof(peer_check), 0);
    assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_security.phase == GAP_ENC_QUEUED);
    gap_security.phase = 0; gap_security.status = 0;
    gap_security.tx_enabled = gap_security.rx_enabled = 1;
    mesh_gap_smp_poll();
    assert(mesh_gap_pairing_status() == 0 && !gap_smp.phase &&
           !gap_conn.authenticated && gap_conn.encryption_key_size == 16);
    gap_connection_end();

    start_test_central_link();
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    receive_test_smp(response, sizeof(response), 0);
    uint8_t bad_public[65] = {12};
    receive_test_smp(bad_public, sizeof(bad_public), 0);
    assert(mesh_gap_pairing_status() == 0x0b);
    gap_connection_end();

    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send Security Request.
    const uint8_t request[7] = {1, MESH_GAP_IO_NONE, 0, 8, 16, 0, 0};
    receive_test_smp(request, sizeof(request), 0);
    assert(gap_smp.phase == GAP_SMP_SC_PUBLIC_KEY && gap_smp.tx[4] == 2);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send Pairing Response.
    receive_test_smp(public_pdu, sizeof(public_pdu), 0);
    assert(gap_smp.phase == GAP_SMP_SC_CONFIRM && gap_smp.tx[4] == 12);
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        assert(gap_conn.tx_llid == (fragment ? 1 : 2));
    }
    assert(gap_smp.phase == GAP_SMP_SC_RANDOM && gap_smp.tx[4] == 3);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    receive_test_smp(peer_nonce, sizeof(peer_nonce), 0);
    assert(gap_smp.phase == GAP_SMP_SC_DHKEY && gap_smp.tx[4] == 4);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t central_check[17] = {13};
    gap_sc_dhkey_check(1, central_check + 1);
    receive_test_smp(central_check, sizeof(central_check), 0);
    assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT && gap_smp.tx[4] == 13);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    gap_security.phase = GAP_ENC_KEY_REQUEST;
    memset(gap_security.random, 0, 8); gap_security.ediv = 0;
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_security.phase == GAP_ENC_START_QUEUED);
    gap_security.phase = 0; gap_security.status = 0;
    gap_security.tx_enabled = gap_security.rx_enabled = 1;
    mesh_gap_smp_poll();
    assert(mesh_gap_pairing_status() == 0 && !gap_smp.phase &&
           !gap_conn.authenticated && gap_conn.encryption_key_size == 16);
    gap_connection_end();
    assert(mesh_gap_secure_connections_set(0));
    mesh_gap_pairing_set(0);
}

static void test_secure_connections_numeric_comparison(void) {
    assert(mesh_gap_security_set(MESH_GAP_IO_DISPLAY_YES_NO, 1, 16));
    assert(mesh_gap_secure_connections_set(1));
    for (uint8_t accept = 0; accept < 2; accept++) {
        start_test_central_link();
        mesh_gap_pairing_set(1);
        assert(mesh_gap_pair());
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        const uint8_t response[7] = {
            2, MESH_GAP_IO_DISPLAY_YES_NO, 0, 12, 16, 0, 0
        };
        receive_test_smp(response, sizeof(response), 0);
        assert(gap_smp.sc.numeric_required);
        for (uint8_t fragment = 0; fragment < 3; fragment++) {
            gap_conn.tx_queued = gap_conn.tx_pending = 0;
            mesh_gap_smp_poll();
        }
        uint8_t peer_private[32] = {0}, peer_public[64], public_pdu[65] = {12};
        peer_private[31] = 9;
        assert(uECC_compute_public_key(peer_private, peer_public, uECC_secp256r1()));
        gap_sc_reverse(public_pdu + 1, peer_public, 32);
        gap_sc_reverse(public_pdu + 33, peer_public + 32, 32);
        receive_test_smp(public_pdu, sizeof(public_pdu), 0);
        uint8_t peer_nonce[17] = {4}, peer_confirm[17] = {3};
        for (uint8_t i = 1; i < sizeof(peer_nonce); i++) peer_nonce[i] = i + 90;
        gap_sc_confirm_value(peer_public, gap_smp.sc.public_key,
                             peer_nonce + 1, 0, peer_confirm + 1);
        receive_test_smp(peer_confirm, sizeof(peer_confirm), 0);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        receive_test_smp(peer_nonce, sizeof(peer_nonce), 0);
        assert(gap_smp.phase == GAP_SMP_SC_USER && !gap_smp.tx_len);
        uint32_t value;
        assert(mesh_gap_numeric_comparison(&value) && value < 1000000);
        assert(!mesh_gap_authenticated());
        assert(mesh_gap_numeric_comparison_reply(accept));
        assert(!mesh_gap_numeric_comparison_reply(accept));
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        if (!accept) {
            assert(mesh_gap_pairing_status() == 0x0c);
            gap_connection_end();
            continue;
        }
        assert(gap_smp.phase == GAP_SMP_SC_DHKEY && gap_conn.tx_queued);
        uint8_t peer_check[17] = {13};
        gap_sc_dhkey_check(0, peer_check + 1);
        receive_test_smp(peer_check, sizeof(peer_check), 0);
        assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        assert(gap_security.phase == GAP_ENC_QUEUED);
        gap_security.phase = 0; gap_security.status = 0;
        gap_security.tx_enabled = gap_security.rx_enabled = 1;
        mesh_gap_smp_poll();
        assert(mesh_gap_pairing_status() == 0 && mesh_gap_authenticated());
        gap_connection_end();
    }
    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    const uint8_t request[7] = {
        1, MESH_GAP_IO_DISPLAY_YES_NO, 0, 12, 16, 0, 0
    };
    receive_test_smp(request, sizeof(request), 0);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t peer_private[32] = {0}, peer_public[64], public_pdu[65] = {12};
    peer_private[31] = 11;
    assert(uECC_compute_public_key(peer_private, peer_public, uECC_secp256r1()));
    gap_sc_reverse(public_pdu + 1, peer_public, 32);
    gap_sc_reverse(public_pdu + 33, peer_public + 32, 32);
    receive_test_smp(public_pdu, sizeof(public_pdu), 0);
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
    }
    assert(gap_smp.phase == GAP_SMP_SC_RANDOM && gap_smp.tx[4] == 3);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t peer_nonce[17] = {4};
    for (uint8_t i = 1; i < sizeof(peer_nonce); i++) peer_nonce[i] = i + 110;
    receive_test_smp(peer_nonce, sizeof(peer_nonce), 0);
    assert(gap_smp.phase == GAP_SMP_SC_USER && gap_smp.tx[4] == 4);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll(); // Send Nb before the Central's DHKey Check arrives.
    uint8_t central_check[17] = {13};
    gap_sc_dhkey_check(1, central_check + 1);
    receive_test_smp(central_check, sizeof(central_check), 0);
    assert(gap_smp.sc.peer_check_received && gap_smp.phase == GAP_SMP_SC_USER);
    uint32_t value;
    assert(mesh_gap_numeric_comparison(&value) && value < 1000000);
    assert(mesh_gap_numeric_comparison_reply(1));
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT && gap_conn.tx_queued);
    gap_security.phase = GAP_ENC_KEY_REQUEST;
    memset(gap_security.random, 0, 8); gap_security.ediv = 0;
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_security.phase == GAP_ENC_START_QUEUED);
    gap_security.phase = 0; gap_security.status = 0;
    gap_security.tx_enabled = gap_security.rx_enabled = 1;
    mesh_gap_smp_poll();
    assert(mesh_gap_pairing_status() == 0 && mesh_gap_authenticated());
    gap_connection_end();
    assert(mesh_gap_secure_connections_set(0));
    assert(mesh_gap_security_set(MESH_GAP_IO_NONE, 0, 7));
    mesh_gap_pairing_set(0);
}

static void test_secure_connections_passkey(void) {
    assert(mesh_gap_security_set(MESH_GAP_IO_KEYBOARD_ONLY, 1, 16));
    assert(mesh_gap_secure_connections_set(1));
    start_test_central_link();
    mesh_gap_pairing_set(1);
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    const uint8_t response[7] = {
        2, MESH_GAP_IO_DISPLAY_ONLY, 0, 12, 16, 0, 0
    };
    receive_test_smp(response, sizeof(response), 0);
    assert(gap_smp.sc.passkey_required &&
           gap_smp.passkey_action == MESH_GAP_PASSKEY_INPUT);
    assert(mesh_gap_passkey_reply(123456));
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
    }
    uint8_t peer_private[32] = {0}, peer_public[64], public_pdu[65] = {12};
    peer_private[31] = 13;
    assert(uECC_compute_public_key(peer_private, peer_public, uECC_secp256r1()));
    gap_sc_reverse(public_pdu + 1, peer_public, 32);
    gap_sc_reverse(public_pdu + 33, peer_public + 32, 32);
    receive_test_smp(public_pdu, sizeof(public_pdu), 0);
    assert(gap_smp.phase == GAP_SMP_SC_PASSKEY);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t previous_nonce[16] = {0};
    for (uint8_t round = 0; round < 20; round++) {
        uint8_t z = 0x80 | ((123456u >> round) & 1);
        uint8_t local_confirm[16];
        gap_sc_confirm_value(gap_smp.sc.public_key, peer_public,
                             gap_smp.random, z, local_confirm);
        assert(gap_smp.phase == GAP_SMP_SC_CONFIRM && gap_smp.tx[4] == 3 &&
               !memcmp(gap_smp.tx + 5, local_confirm, 16));
        if (round) assert(memcmp(previous_nonce, gap_smp.random, 16));
        memcpy(previous_nonce, gap_smp.random, 16);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        uint8_t peer_nonce[17] = {4}, peer_confirm[17] = {3};
        for (uint8_t i = 1; i < sizeof(peer_nonce); i++)
            peer_nonce[i] = (uint8_t)(round * 7 + i);
        gap_sc_confirm_value(peer_public, gap_smp.sc.public_key,
                             peer_nonce + 1, z, peer_confirm + 1);
        receive_test_smp(peer_confirm, sizeof(peer_confirm), 0);
        assert(gap_smp.phase == GAP_SMP_SC_RANDOM && gap_smp.tx[4] == 4);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        receive_test_smp(peer_nonce, sizeof(peer_nonce), 0);
        assert(gap_smp.sc.passkey_round == (round < 19 ? round + 1 : round));
    }
    assert(gap_smp.phase == GAP_SMP_SC_DHKEY && gap_smp.tx[4] == 13);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t peer_check[17] = {13};
    gap_sc_dhkey_check(0, peer_check + 1);
    receive_test_smp(peer_check, sizeof(peer_check), 0);
    assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_security.phase == GAP_ENC_QUEUED);
    gap_security.phase = 0; gap_security.status = 0;
    gap_security.tx_enabled = gap_security.rx_enabled = 1;
    mesh_gap_smp_poll();
    assert(mesh_gap_pairing_status() == 0 && mesh_gap_authenticated());
    gap_connection_end();
    assert(mesh_gap_security_set(MESH_GAP_IO_DISPLAY_ONLY, 1, 16));
    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    const uint8_t request[7] = {
        1, MESH_GAP_IO_KEYBOARD_ONLY, 0, 12, 16, 0, 0
    };
    receive_test_smp(request, sizeof(request), 0);
    uint32_t passkey;
    assert(gap_smp.sc.passkey_required &&
           mesh_gap_passkey(&passkey) == MESH_GAP_PASSKEY_DISPLAY &&
           passkey < 1000000);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    uint8_t central_private[32] = {0}, central_public[64], central_pdu[65] = {12};
    central_private[31] = 15;
    assert(uECC_compute_public_key(central_private, central_public, uECC_secp256r1()));
    gap_sc_reverse(central_pdu + 1, central_public, 32);
    gap_sc_reverse(central_pdu + 33, central_public + 32, 32);
    receive_test_smp(central_pdu, sizeof(central_pdu), 0);
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
    }
    assert(gap_smp.phase == GAP_SMP_SC_PASSKEY);
    for (uint8_t round = 0; round < 20; round++) {
        uint8_t z = 0x80 | ((passkey >> round) & 1);
        uint8_t central_nonce[17] = {4}, central_confirm[17] = {3};
        for (uint8_t i = 1; i < sizeof(central_nonce); i++)
            central_nonce[i] = (uint8_t)(round * 9 + i);
        gap_sc_confirm_value(central_public, gap_smp.sc.public_key,
                             central_nonce + 1, z, central_confirm + 1);
        receive_test_smp(central_confirm, sizeof(central_confirm), 0);
        assert(gap_smp.confirm_received && gap_smp.phase == GAP_SMP_SC_PASSKEY);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        uint8_t expected_confirm[16];
        gap_sc_confirm_value(gap_smp.sc.public_key, central_public,
                             gap_smp.random, z, expected_confirm);
        assert(gap_smp.phase == GAP_SMP_SC_RANDOM && gap_smp.tx[4] == 3 &&
               !memcmp(gap_smp.tx + 5, expected_confirm, 16));
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
        receive_test_smp(central_nonce, sizeof(central_nonce), 0);
        assert(gap_smp.tx[4] == 4);
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
    }
    assert(gap_smp.phase == GAP_SMP_SC_DHKEY);
    uint8_t central_check[17] = {13};
    gap_sc_dhkey_check(1, central_check + 1);
    receive_test_smp(central_check, sizeof(central_check), 0);
    assert(gap_smp.phase == GAP_SMP_SC_ENCRYPT && gap_smp.tx[4] == 13);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    gap_security.phase = GAP_ENC_KEY_REQUEST;
    memset(gap_security.random, 0, 8); gap_security.ediv = 0;
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_security.phase == GAP_ENC_START_QUEUED);
    gap_security.phase = 0; gap_security.status = 0;
    gap_security.tx_enabled = gap_security.rx_enabled = 1;
    mesh_gap_smp_poll();
    assert(mesh_gap_pairing_status() == 0 && mesh_gap_authenticated());
    gap_connection_end();
    assert(mesh_gap_security_set(MESH_GAP_IO_KEYBOARD_ONLY, 1, 16));
    start_test_central_link(); gap_conn.central_role = 0;
    assert(mesh_gap_pair());
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    const uint8_t input_request[7] = {
        1, MESH_GAP_IO_DISPLAY_ONLY, 0, 12, 16, 0, 0
    };
    receive_test_smp(input_request, sizeof(input_request), 0);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    receive_test_smp(central_pdu, sizeof(central_pdu), 0);
    for (uint8_t fragment = 0; fragment < 3; fragment++) {
        gap_conn.tx_queued = gap_conn.tx_pending = 0;
        mesh_gap_smp_poll();
    }
    assert(gap_smp.phase == GAP_SMP_SC_PASSKEY &&
           gap_smp.passkey_action == MESH_GAP_PASSKEY_INPUT);
    uint8_t input_nonce[17] = {4}, input_confirm[17] = {3};
    for (uint8_t i = 1; i < sizeof(input_nonce); i++) input_nonce[i] = i + 40;
    gap_sc_confirm_value(central_public, gap_smp.sc.public_key,
                         input_nonce + 1, 0x80, input_confirm + 1);
    receive_test_smp(input_confirm, sizeof(input_confirm), 0);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    assert(gap_smp.confirm_received && !gap_smp.tx_len);
    assert(mesh_gap_passkey_reply(654321)); // Different low bit from the peer.
    mesh_gap_smp_poll();
    assert(gap_smp.phase == GAP_SMP_SC_RANDOM && gap_smp.tx[4] == 3);
    gap_conn.tx_queued = gap_conn.tx_pending = 0;
    mesh_gap_smp_poll();
    receive_test_smp(input_nonce, sizeof(input_nonce), 0);
    assert(mesh_gap_pairing_status() == 4 && !mesh_gap_authenticated());
    gap_connection_end();
    assert(mesh_gap_secure_connections_set(0));
    assert(mesh_gap_security_set(MESH_GAP_IO_NONE, 0, 7));
    mesh_gap_pairing_set(0);
}

int main(void) {
    test_encrypted_advertising_data();
    test_access_address_rules();
    test_connect_request();
    test_connection_timing_configuration();
    test_address_resolution();
    test_extended_length_control_pdu();
    test_private_rotation();
    test_randomized_private_rotation();
    test_accept_list_capacity();
#if MESH_GAP_EXT_ADV_SUPPORT
    test_extended_advertising_reassembly();
    test_extended_advertising_radio_followup();
    test_extended_advertising_transmit();
    test_extended_advertising_chain_transmit();
    test_periodic_advertising_transmit();
    test_pawr_advertising_subevents();
    test_pawr_observer_response();
    test_periodic_advertising_sync();
    test_pawr_timing_decode();
    test_periodic_sync_connection_arbitration();
    test_periodic_sync_transfer_receive();
    test_extended_advertising_multiple_sets();
    test_extended_scannable_advertising();
#endif
    test_scanning_and_advertising_coexistence();
    test_scan_identity_filter();
    test_radio_privacy_filter();
    test_connect_by_identity();
    test_general_connection_establishment();
    test_selective_connection_establishment();
    test_auto_connection_establishment();
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
    test_smp_pairing();
    test_passkey_pairing();
    test_secure_connections_just_works();
    test_secure_connections_numeric_comparison();
    test_secure_connections_passkey();
    return 0;
}
