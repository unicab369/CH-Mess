#include "ch32fun.h"
#include "iSLER.h"
#include "ble_mesh_crypto.h"
#include "aes_cmm.h"
#include "ble_mesh_provisioning.h"
#include "ble_mesh_network.h"
#include "micro-ecc/uECC.h"
#include <stdio.h>

#define ROM_CFG_MAC_ADDR		((const u32*)0x0007F018)

void ble_mesh_advertise_bearer(uint8_t *wire, size_t wire_len);

// The radio sends a complete advertising PDU, while provisioning supplies an
// AD structure. Keep queued AD structures in order, including delayed ACKs.
#define BLE_MESH_RADIO_QUEUE_SIZE 8
#define BLE_MESH_ADV_ACCESS_ADDRESS 0x8E89BED6u
static struct {
    uint8_t data[PB_MAX_AD_SIZE];
    uint8_t len;
    uint32_t due_ms;
} ble_mesh_radio_queue[BLE_MESH_RADIO_QUEUE_SIZE];

static uint8_t ble_mesh_radio_head, ble_mesh_radio_count;
static uint8_t ble_mesh_radio_ready, BLE_MESH_ADV_POLL_armed, BLE_MESH_ADV_POLL_channel;
static uint32_t BLE_MESH_ADV_POLL_started_ms;
static ISLER_BUF_ATTR uint8_t ble_mesh_radio_frame[8 + PB_MAX_AD_SIZE];

static void ble_mesh_radio_init(void) {
    if (ble_mesh_radio_ready) return;
    iSLERInit(LL_TX_POWER_0_DBM);
    ble_mesh_radio_ready = 1;
}

static int ble_mesh_queue_ad(const uint8_t *adv_data, size_t len, uint32_t due_ms) {
    if (!adv_data || len < 2 || len > PB_MAX_AD_SIZE ||
        (size_t)adv_data[0] + 1 != len ||
        ble_mesh_radio_count == BLE_MESH_RADIO_QUEUE_SIZE
    ) return -1;

    uint8_t slot = (ble_mesh_radio_head + ble_mesh_radio_count) %
                   BLE_MESH_RADIO_QUEUE_SIZE;
    memcpy(ble_mesh_radio_queue[slot].data, adv_data, len);
    ble_mesh_radio_queue[slot].len = (uint8_t)len;
    ble_mesh_radio_queue[slot].due_ms = due_ms;
    ble_mesh_radio_count++;
    return 0;
}

int BLE_MESH_TX(const uint8_t *adv_data, size_t len) {
    ble_mesh_radio_init();
    return ble_mesh_queue_ad(adv_data, len, GET_MILLIS());
}

int BLE_MESH_TX_DELAYED(
    const uint8_t *adv_data, size_t len,
    uint16_t min_delay_ms, uint16_t max_delay_ms
) {
    uint8_t random_byte;
    if (min_delay_ms > max_delay_ms ||
        GET_RANDOM_BYTES(&random_byte, 1) != 1
    ) return -1;

    uint32_t range = (uint32_t)max_delay_ms - min_delay_ms + 1;
    uint32_t delay_ms = min_delay_ms + random_byte % range;
    ble_mesh_radio_init();
    return ble_mesh_queue_ad(adv_data, len, GET_MILLIS() + delay_ms);
}

int BLE_MESH_ADV_POLL(uint8_t *adv_data, size_t *len) {
    if (!adv_data || !len) return -1;
    ble_mesh_radio_init();
    uint32_t now = GET_MILLIS();
    int received = 0;

    if (rx_ready) {
        const uint8_t *frame = (const uint8_t *)LLE_BUF;
        uint8_t payload_len = frame[1];
        BLE_MESH_ADV_POLL_armed = 0;
        rx_ready = 0;

        // An ADV_NONCONN_IND payload is AdvA (6 bytes) followed by AD data.
        if ((frame[0] & 0x0F) == 0x02 && payload_len >= 8 &&
            payload_len <= 37
        ) {
            size_t end = (size_t)payload_len + 2;
            for (size_t offset = 8; offset < end;) {
                uint8_t ad_len = frame[offset];
                if (ad_len == 0 || offset + ad_len + 1 > end) break;
                if (frame[offset + 1] == MESH_PROV_AD_TYPE ||
                    frame[offset + 1] == MESH_BEACON_AD_TYPE ||
                    frame[offset + 1] == MESH_NETWORK_AD_TYPE
                ) {
                    if ((size_t)ad_len + 1 > *len) return -1;
                    memcpy(adv_data, frame + offset, (size_t)ad_len + 1);
                    *len = (size_t)ad_len + 1;
                    received = 1;
                    break;
                }
                offset += (size_t)ad_len + 1;
            }
        }
    }

    if (ble_mesh_radio_count &&
        (int32_t)(now - ble_mesh_radio_queue[ble_mesh_radio_head].due_ms) >= 0
    ) {
        // The factory MAC is stored most-significant byte first in ROM.
        const uint8_t *mac = (const uint8_t *)ROM_CFG_MAC_ADDR;
        ble_mesh_radio_frame[0] = 0x02;
        ble_mesh_radio_frame[1] = 0;
        for (uint8_t i = 0; i < 6; i++) ble_mesh_radio_frame[7 - i] = mac[i];
        memcpy(ble_mesh_radio_frame + 8,
               ble_mesh_radio_queue[ble_mesh_radio_head].data,
               ble_mesh_radio_queue[ble_mesh_radio_head].len);
        size_t frame_len = 8 + ble_mesh_radio_queue[ble_mesh_radio_head].len;
        ble_mesh_radio_head = (ble_mesh_radio_head + 1) % BLE_MESH_RADIO_QUEUE_SIZE;
        ble_mesh_radio_count--;
        BLE_MESH_ADV_POLL_armed = 0;

        for (uint8_t channel = 37; channel <= 39; channel++) {
            iSLERTX(BLE_MESH_ADV_ACCESS_ADDRESS, ble_mesh_radio_frame,
                    frame_len, channel, PHY_1M);
            if (!tx_done) return -1;
        }
    }

    if (!BLE_MESH_ADV_POLL_armed || (uint32_t)(now - BLE_MESH_ADV_POLL_started_ms) >= 20u) {
        uint8_t channel = 37 + BLE_MESH_ADV_POLL_channel;
        BLE_MESH_ADV_POLL_channel = (BLE_MESH_ADV_POLL_channel + 1) % 3;
        iSLERRX(BLE_MESH_ADV_ACCESS_ADDRESS, channel, PHY_1M);
        BLE_MESH_ADV_POLL_started_ms = GET_MILLIS();
        BLE_MESH_ADV_POLL_armed = 1;
    }
    return received;
}

int GET_LOCAL_UUID(uint8_t device_uuid[16]) {
    if (!device_uuid) return -1;

    // Use the factory MAC as the stable, device-specific part of the UUID.
    memset(device_uuid, 0, 16);
    for (int i = 0; i < 6; i++) {
        device_uuid[i] = ((const uint8_t *)ROM_CFG_MAC_ADDR)[i];
    }
    return 0;
}

void PROV_ATTENTION_START(uint8_t seconds) {
    (void)seconds;
}

void PROV_ATTENTION_STOP(void) {
}

int PROVISIONER_GET_DATA(prov_data *data) {
    (void)data;
    return -1;
}

int PROVISIONER_STORE_NODE_DEVKEY(
    const uint8_t device_key[16], uint16_t unicast_address
) {
    (void)device_key;
    (void)unicast_address;
    return -1;
}

int PROVISIONEE_STORE_DATA(const prov_data *data, const uint8_t device_key[16]) {
    if (!data || !device_key) return -1;
    mesh_network_state state = {0};
    memcpy(state.net_key, data->net_key, 16);
    state.net_key_index = data->net_key_index;
    memcpy(state.dev_key, device_key, 16);
    state.iv_index = data->iv_index;
    state.phase2_provisioned = (data->flags & 1u) != 0;
    state.iv_update = (data->flags & 2u) != 0;
    state.iv_skip_min_time = state.iv_update;
    state.unicast_address = data->unicast_address;
    uint64_t seconds;
    if (BLE_MESH_NETWORK_TIME_SECONDS(&seconds) == 1) {
        state.iv_time_valid = 1;
        state.iv_state_start_time = seconds;
    }
    if (BLE_MESH_NETWORK_SAVE_STATE(&state) != 1) return -1;
    return ble_mesh_network_init(&state) ? 0 : -1;
}

// Storage interfaces: implement these with nonvolatile storage before use.
int BLE_MESH_NETWORK_LOAD_STATE(mesh_network_state *state) {
    (void)state;
    return 0;
}

int BLE_MESH_NETWORK_SAVE_STATE(const mesh_network_state *state) {
    (void)state;
    return 0;
}

// Persist the next sequence number in the same state loaded by
// BLE_MESH_NETWORK_LOAD_STATE. Return 1 only after it is durably stored;
// a RAM-only implementation could reuse a nonce after reboot.
int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq) {
    (void)next_seq;
    return 0;
}

// Supply a trusted monotonic second count that survives reboot. Until a clock
// is available, IV Update timing remains disabled rather than skipping its
// required minimum durations.
int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds) {
    (void)seconds;
    return 0;
}

// Temporary test stub: replace before using provisioning with real devices.
int GET_RANDOM_BYTES(uint8_t *out, unsigned len) {
    memset(out, 22, len);
    return 1;
}

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

int ECDH_GENERATE_KPAIR(uint8_t private_key[32], uint8_t public_key[64]) {
    // micro-ecc needs an RNG callback before it can generate a private key.
    uECC_set_rng(GET_RANDOM_BYTES);
    return uECC_make_key(public_key, private_key, uECC_secp256r1()) ? 0 : -1;
}

int ECDH_COMPUTE_DHKEY(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
) {
    uECC_Curve curve = uECC_secp256r1();
    // This validates the peer's key; it does not verify a signature.
    if (!uECC_valid_public_key(peer_public_key, curve)) return -1;
    return uECC_shared_secret(peer_public_key, private_key, dhkey, curve) ? 0 : -1;
}

int AUTH_COMPUTE_CONFIRMATION(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32], uint8_t confirmation_salt[16],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t confirmation_key[16], input[32], t[16];

    // s1(confirm_inputs) is AES-CMAC with an all-zero key.
    aes_cmac(zero, confirm_inputs, PROV_CONFIRM_INPUTS_LEN, confirmation_salt);

    // k1 derives the confirmation key from the DHKey and confirmation salt.
    aes_cmac(confirmation_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prck", 4, confirmation_key);
    memcpy(input, random, 16);
    memcpy(input + 16, auth_value, 16);
    aes_cmac(confirmation_key, input, sizeof(input), confirmation);
    return 0;
}

int AUTH_DERIVE_SESSION(
    const uint8_t dhkey[32], const uint8_t confirmation_salt[16],
    const uint8_t provisioner_random[16],
    const uint8_t provisionee_random[16],
    uint8_t session_key[16], uint8_t session_nonce[13],
    uint8_t device_key[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t input[48], provisioning_salt[16], nonce_key[16], t[16];

    memcpy(input, confirmation_salt, 16);
    memcpy(input + 16, provisioner_random, 16);
    memcpy(input + 32, provisionee_random, 16);

    // s1(confirmation_salt || both random values) uses an all-zero key.
    aes_cmac(zero, input, sizeof(input), provisioning_salt);

    // k1 uses the same first CMAC result for all three derived keys.
    aes_cmac(provisioning_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prsk", 4, session_key);
    aes_cmac(t, (const uint8_t *)"prsn", 4, nonce_key);
    memcpy(session_nonce, nonce_key + 3, 13);
    aes_cmac(t, (const uint8_t *)"prdk", 4, device_key);
    return 0;
}

// The provisioning state machine supplies a 25-byte data PDU and an 8-byte MIC.
// AES-CCM is already available here, so these two crypto interfaces can be wired now.
int AUTH_ENCRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t plain[25], uint8_t encrypted[25], uint8_t mic[8]
) {
    return ccm_encrypt_and_tag(session_key, session_nonce, 13, NULL, 0,
                               plain, 25, encrypted, mic, 8);
}

int AUTH_DECRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t encrypted[25], const uint8_t mic[8], uint8_t plain[25]
) {
    return ccm_auth_decrypt(session_key, session_nonce, 13, NULL, 0,
                            encrypted, 25, mic, 8, plain);
}

// Transport and access
// Message states
