#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ISLER_BLE_MESH_ACCESS_H
#define APP_KEY_INDEX_NONE 0xffff
#define DEVICE_KEY_LOCAL 0xfffe
#define MESH_ACCESS_ACK_TIMEOUT_MS 2000u
#define MESH_ACCESS_ACK_RETRY_COUNT 2u
typedef struct {
    uint16_t src, dst, app_key_index;
    uint16_t device_key_owner;
    uint16_t net_key_index;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint32_t opcode;
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

typedef struct {
    uint16_t src, dst, app_key_index, len;
    uint16_t net_key_index;
    uint8_t ttl;
    uint8_t data[380];
} mesh_access_message;

// Use the real network key state machine; access/radio delivery stays mocked.
#include "../ble_mesh_1network.h"

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
    uint8_t block[16];
    for (int i = 0; i < 16; i++) block[i] = in[i] ^ key[i] ^ (uint8_t)(i * 13);
    memcpy(out, block, 16);
}

static mesh_net_state saved_network;
static int network_save_fail, network_save_count;
int BLE_MESH_NETWORK_LOAD_STATE(mesh_net_state *state) {
    *state = saved_network;
    return 1;
}
int BLE_MESH_NETWORK_SAVE_STATE(const mesh_net_state *state) {
    if (network_save_fail) return 0;
    saved_network = *state;
    network_save_count++;
    return 1;
}
int BLE_MESH_NETWORK_STORE_SEQ(uint32_t seq) {
    (void)seq;
    return 1;
}
int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds) {
    (void)seconds;
    return 0;
}
int BLE_MESH_QUEUE_TX(const uint8_t *ad, size_t len) {
    (void)ad; (void)len;
    return 0;
}
int BLE_MESH_QUEUE_RELAY_TX(const uint8_t *ad, size_t len, uint8_t retransmit) {
    (void)ad; (void)len; (void)retransmit;
    return 0;
}
int BLE_MESH_ADV_POLL(uint8_t *ad, size_t *len, int8_t *rssi) {
    (void)ad; (void)len;
    if (rssi) *rssi = 127;
    return 0;
}

#define MESH_TRANSPORT_MAX_LABELS 4
static uint8_t registered_labels;
static uint16_t mesh_virtual_address(const uint8_t label[16]) {
    return (uint16_t)(0x8000 | label[0]);
}
static void mesh_transport_clear_labels(void) { registered_labels = 0; }
static int mesh_label_add(const uint8_t label[16]) {
    (void)label;
    registered_labels++;
    return 1;
}

static mesh_sar_tx_state transport_sar_tx = {5, 2, 2, 7, 1, 2, 9};
static mesh_sar_rx_state transport_sar_rx = {3, 1, 1, 5, 0};
static mesh_sar_tx_state mesh_transport_get_sar_transmitter(void) {
    return transport_sar_tx;
}

static uint32_t now_ms;
static uint32_t last_opcode;
static uint16_t last_dst;
static uint16_t last_src;
static uint8_t last_params[128];
static uint16_t last_app_key_index;
static size_t last_len;
static uint8_t last_ttl;
static int send_count, send_fail, save_fail;
static uint32_t last_expected_response;
static uint8_t applied_on, attention_seconds, reported_on;
static uint16_t applied_element, reported_element;
static int apply_count, report_count;
static int config_report_count;
static mesh_access_pdu polled_access;
static int poll_ready;

uint32_t GET_MILLIS(void) { return now_ms; }
static int mesh_access_queue(uint16_t src, uint16_t dst, uint8_t ttl,
                                uint16_t app_key_index, uint32_t opcode,
                                const uint8_t *params, size_t len,
                                uint8_t mic_64) {
    if (send_fail) return 0;
    send_count++;
    last_src = src;
    last_ttl = ttl;
    assert(mic_64 == 0);
    assert(len <= sizeof(last_params));
    last_dst = dst;
    last_opcode = opcode;
    last_app_key_index = app_key_index;
    last_len = len;
    if (len) memcpy(last_params, params, len);
    return 1;
}
static int mesh_access_queue_acknowledged(uint16_t src, uint16_t dst,
    uint8_t ttl, uint16_t app_key_index, uint32_t request_opcode,
    uint32_t response_opcode, const uint8_t *params, size_t len,
    uint8_t mic_64, uint32_t timeout_ms, uint8_t retry_count) {
    assert(timeout_ms == MESH_ACCESS_ACK_TIMEOUT_MS &&
           retry_count == MESH_ACCESS_ACK_RETRY_COUNT);
    last_expected_response = response_opcode;
    return mesh_access_queue(src, dst, ttl, app_key_index, request_opcode,
        params, len, mic_64);
}
static int mesh_access_queue_on_net(uint16_t net_idx, uint16_t dst,
    uint8_t ttl, uint32_t opcode, const uint8_t *params,
    size_t len) {
    (void)net_idx;
    return mesh_access_queue(mesh_network.state.unicast_address, dst, ttl,
        APP_KEY_INDEX_NONE, opcode, params, len, 0);
}
static int mesh_access_queue_virtual(uint16_t src,
                                              const uint8_t label[16],
                                              uint8_t ttl, uint16_t app_key_index,
                                              uint32_t opcode,
                                              const uint8_t *params, size_t len,
                                              uint8_t mic_64) {
    last_src = src;
    return mesh_access_queue(src, mesh_virtual_address(label), ttl,
                                 app_key_index, opcode, params, len, mic_64);
}
static int mesh_access_poll(mesh_access_message *message,
                                mesh_access_pdu *access) {
    (void)message;
    if (!poll_ready) return 0;
    *access = polled_access;
    poll_ready = 0;
    return 1;
}

#include "../ble_mesh_4models.h"

static mesh_models_state saved;
static int reset_calls, reset_fail;
int BLE_MESH_NODE_RESET(uint16_t dst) {
    if (reset_fail || !mesh_access_queue(mesh_network.state.unicast_address,
        dst, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
        OP_CONFIG_NODE_RESET_STATUS, NULL, 0, 0)) return 0;
    reset_calls++;
    return 1;
}
int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state) {
    *state = saved;
    return 1;
}
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state) {
    if (save_fail) return 0;
    saved = *state;
    return 1;
}
void BLE_MESH_ONOFF_CHANGED(uint16_t element, uint8_t on) {
    applied_element = element;
    applied_on = on;
    apply_count++;
}
void BLE_MESH_ONOFF_STATUS(uint16_t element, uint16_t src, uint8_t present) {
    reported_element = element;
    assert(src == 0x1202);
    reported_on = present;
    report_count++;
}
void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len) {
    (void)src; (void)opcode; (void)params; (void)len;
    config_report_count++;
}
static unsigned health_status_count;
static uint16_t health_status_element, health_status_src;
static uint32_t health_status_opcode;
static size_t health_status_len;
static uint8_t health_status_params[128];
void BLE_MESH_HEALTH_STATUS(uint16_t element, uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len) {
    health_status_count++;
    health_status_element = element;
    health_status_src = src;
    health_status_opcode = opcode;
    health_status_len = len;
    assert(len <= sizeof(health_status_params));
    memcpy(health_status_params, params, len);
}

void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds) {
    (void)element;
    attention_seconds = seconds;
}

static int health_test_calls, health_test_fail;
static uint16_t health_test_element;
static uint8_t health_test_faults[MESH_HEALTH_MAX_FAULTS];
static size_t health_test_count;
int BLE_MESH_HEALTH_TEST(uint16_t element, uint8_t test_id, uint8_t *faults, size_t *len) {
    if (test_id != 0 && test_id != 1) return 0;
    health_test_calls++;
    health_test_element = element;
    if (health_test_fail) return 0;
    assert(*len >= health_test_count);
    memcpy(faults, health_test_faults, health_test_count);
    *len = health_test_count;
    return 1;
}

static int poll_message(const mesh_access_pdu *message) {
    polled_access = *message;
    poll_ready = 1;
    return mesh_models_poll();
}

static int config_message(uint32_t opcode, const uint8_t *params, size_t len) {
    mesh_access_pdu message = {.src = 0x1202, .dst = 0x1201,
        .app_key_index = APP_KEY_INDEX_NONE, .device_key_owner = 0x1201,
        .opcode = opcode, .params = params, .params_len = len};
    return poll_message(&message);
}

// Round-trip the client's wire encoding through the server and dispatcher.
static int config_request(void) {
    uint32_t opcode = last_opcode;
    size_t len = last_len;
    uint8_t params[128];
    memcpy(params, last_params, len);
    return config_message(opcode, params, len);
}

static void test_foundation_configuration(void) {
    memset(&saved, 0, sizeof(saved));
    memset(&mesh_models, 0, sizeof(mesh_models));
    memset(&mesh_network.state.app_keys, 0, sizeof(mesh_network.state.app_keys));
    saved.default_ttl = MODEL_TTL;
    saved.sar_transmitter = MESH_SAR_TRANSMITTER_DEFAULT;
    saved.sar_receiver = MESH_SAR_RX_DEFAULT;
    saved.onoff_server_bindings = saved.onoff_client_bindings = saved.health_server_bindings = 1;
    saved.other[0] = (mesh_element_bindings){1, 1, 1, 0};
    mesh_network.state.app_keys[0].used = 1;
    mesh_network.state.app_keys[0].index = 0x234;
    mesh_network.state.app_keys[1].used = 1;
    mesh_network.state.app_keys[1].index = 0x235;
    now_ms = 0;
    assert(mesh_models_init());

    assert(mesh_get_composition(0x1201, 0xff) && config_request());
    assert(last_opcode == OP_CONFIG_COMPOSITION_STATUS && last_len == 41);
    assert(last_params[0] == 0 && last_params[7] == MESH_NETWORK_REPLAY_SLOTS);
    assert(last_params[9] == 1 && last_params[13] == 7 && last_params[31] == 4);
    assert(last_params[15] == 0 && last_params[17] == 1 &&
           last_params[19] == MESH_MODEL_SAR_CONFIG_SERVER);
    assert(last_params[21] == MESH_MODEL_HEALTH_SERVER &&
           last_params[23] == MESH_MODEL_HEALTH_CLIENT);
    assert(last_params[25] == 0 && last_params[26] == 0x10);
    assert(last_params[33] == MESH_MODEL_HEALTH_SERVER &&
           last_params[35] == MESH_MODEL_HEALTH_CLIENT);
    assert(last_app_key_index == DEVICE_KEY_LOCAL);
    mesh_network.state.element_count = 1;
    assert(mesh_get_composition(0x1201, 0) && config_request() && last_len == 29);
    mesh_network.state.element_count = 2;
    assert(config_message(OP_CONFIG_COMPOSITION_GET, NULL, 0) == 0);

    assert(mesh_get_sar_transmitter(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_SAR_TRANSMITTER_STATUS && last_len == 5 &&
           last_params[0] == MESH_CONFIG_SUCCESS && last_params[1] == 0x25 &&
           last_params[2] == 0x72 && last_params[3] == 0x21 && last_params[4] == 9);
    mesh_sar_tx_state sar = {3, 4, 5, 6, 7, 8, 9};
    assert(mesh_set_sar_transmitter(0x1201, &sar) && config_request());
    assert(last_opcode == OP_CONFIG_SAR_TRANSMITTER_STATUS && last_params[0] == 0 &&
           last_params[1] == 0x43 && last_params[2] == 0x65 &&
           last_params[3] == 0x87 && last_params[4] == 9);
    assert(saved.sar_transmitter.segment_interval_step == 3 &&
           saved.sar_transmitter.unicast_retrans_count == 4 &&
           mesh_transport_get_sar_transmitter().segment_interval_step == 3);
    uint8_t invalid_sar[4] = {0, 0, 0, 0xf0};
    assert(!config_message(OP_CONFIG_SAR_TRANSMITTER_SET, invalid_sar, sizeof(invalid_sar)));
    save_fail = 1;
    sar.segment_interval_step = 7;
    assert(mesh_set_sar_transmitter(0x1201, &sar) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && last_params[1] == 0x43);
    assert(saved.sar_transmitter.segment_interval_step == 3 &&
           mesh_transport_get_sar_transmitter().segment_interval_step == 3);
    save_fail = 0;
    sar = MESH_SAR_TRANSMITTER_DEFAULT;
    assert(mesh_set_sar_transmitter(0x1201, &sar) && config_request());

    assert(mesh_get_sar_receiver(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_SAR_RECEIVER_STATUS && last_len == 4 &&
           last_params[0] == MESH_CONFIG_SUCCESS && last_params[1] == 0x23 &&
           last_params[2] == 0x51 && last_params[3] == 0);
    mesh_sar_rx_state sar_rx = {17, 6, 12, 9, 3};
    assert(mesh_set_sar_receiver(0x1201, &sar_rx) && config_request());
    assert(last_opcode == OP_CONFIG_SAR_RECEIVER_STATUS && last_params[0] == 0 &&
           last_params[1] == 0xd1 && last_params[2] == 0x9c &&
           last_params[3] == 3);
    assert(saved.sar_receiver.segments_threshold == 17 &&
           transport_sar_rx.segment_interval_step == 9);
    uint8_t invalid_sar_rx[3] = {0, 0, 0xfc};
    assert(!config_message(OP_CONFIG_SAR_RECEIVER_SET, invalid_sar_rx,
                           sizeof(invalid_sar_rx)));
    save_fail = 1;
    sar_rx.discard_timeout = 7;
    assert(mesh_set_sar_receiver(0x1201, &sar_rx) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE &&
           saved.sar_receiver.discard_timeout == 12 &&
           transport_sar_rx.discard_timeout == 12);
    save_fail = 0;
    sar_rx = MESH_SAR_RX_DEFAULT;
    assert(mesh_set_sar_receiver(0x1201, &sar_rx) && config_request());

    assert(!mesh_set_default_ttl(0x1201, 1));
    assert(!mesh_set_default_ttl(0x1201, 128));
    uint8_t ttl = 1;
    assert(!config_message(OP_CONFIG_DEFAULT_TTL_SET, &ttl, 1));
    assert(mesh_set_default_ttl(0x1201, 7) && config_request());
    assert(last_opcode == OP_CONFIG_DEFAULT_TTL_STATUS && last_params[0] == 7 && last_ttl == 7);
    save_fail = 1; ttl = 3;
    assert(!config_message(OP_CONFIG_DEFAULT_TTL_SET, &ttl, 1));
    assert(saved.default_ttl == 7 && mesh_models.state.default_ttl == 7);
    save_fail = 0;
    assert(mesh_get_default_ttl(0x1201) && config_request() && last_params[0] == 7);
    assert(mesh_onoff_get(0x1201, 0x1202, 0x234) && last_ttl == 7 &&
           last_expected_response == OP_ONOFF_STATUS);
    assert(mesh_set_default_ttl(0x1201, 0) && config_request() && last_ttl == 0);
    assert(mesh_set_default_ttl(0x1201, 7) && config_request());

    uint8_t label[16] = {9}, collision[16] = {9, 1};
    assert(mesh_model_group_change(0x1201, MESH_MODEL_ONOFF_SERVER, 0xc001, 1) == 0);
    assert(mesh_model_group_change(0x1202, MESH_MODEL_ONOFF_SERVER, 0xc002, 1) == 0);
    assert(mesh_model_label_add(0x1201, MESH_MODEL_ONOFF_SERVER, label) == 0);
    assert(mesh_model_label_add(0x1201, MESH_MODEL_ONOFF_SERVER, collision) == 0);
    assert(mesh_get_subscriptions(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER) && config_request());
    assert(last_opcode == OP_CONFIG_SIG_MODEL_SUB_LIST && last_len == 9);
    assert(last_params[5] == 1 && last_params[6] == 0xc0 && last_params[7] == 9 && last_params[8] == 0x80);
    mesh_models_state before = saved;
    save_fail = 1;
    assert(mesh_replace_subscription(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER, 0xc003, NULL));
    assert(config_request() && last_params[0] == MESH_CONFIG_STORAGE_FAILURE);
    assert(!memcmp(&before, &saved, sizeof(saved)) && registered_labels == 2);
    save_fail = 0;
    assert(mesh_replace_subscription(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER, 0xc003, NULL) && config_request());
    assert(saved.group_count == 2 && saved.virtual_count == 0 && registered_labels == 0);
    assert(saved.groups[0].element == 1 && saved.groups[0].address == 0xc002);
    assert(mesh_replace_subscription(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER, 0, label) && config_request());
    assert(saved.group_count == 1 && saved.virtual_count == 1 && registered_labels == 1);
    assert(last_params[3] == 9 && last_params[4] == 0x80);
    assert(mesh_replace_subscription(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER, 0, NULL) && config_request());
    assert(saved.group_count == 1 && saved.virtual_count == 0);
    assert(mesh_get_subscriptions(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER) && config_request() && last_len == 5);
    uint8_t invalid_model[] = {1, 0x12, 0xff, 0x7f};
    assert(config_message(OP_CONFIG_SIG_SUB_GET, invalid_model, 4));
    assert(last_params[0] == MESH_CONFIG_INVALID_MODEL && last_len == 5);
    invalid_model[0] = 0;
    assert(config_message(OP_CONFIG_SIG_SUB_GET, invalid_model, 4));
    assert(last_params[0] == MESH_CONFIG_INVALID_ADDRESS);
    assert(!mesh_replace_subscription(0x1201, 0x1201, MESH_MODEL_ONOFF_SERVER, 0x8001, NULL));

    mesh_publication pub = {.address = 0xc010, .app_idx = 0x234,
        .ttl = 0xff, .period = 0x41, .retransmit = 10}; // 1 s; 2 retries, 100 ms apart.
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(last_opcode == OP_CONFIG_MODEL_PUB_STATUS && last_len == 12 && last_params[0] == 0);
    assert(last_params[3] == 0x10 && last_params[4] == 0xc0 && last_params[7] == 0xff);
    assert(mesh_get_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER) && config_request());
    uint8_t config_model[] = {1, 0x12, 0, 0};
    assert(config_message(OP_CONFIG_MODEL_PUB_GET, config_model, 4));
    assert(last_params[0] == MESH_CONFIG_INVALID_PUBLICATION);
    config_model[0] = 2;
    assert(config_message(OP_CONFIG_MODEL_PUB_GET, config_model, 4));
    assert(last_params[0] == MESH_CONFIG_INVALID_MODEL);
    pub.app_idx = 0x235;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_BINDING && saved.publications[1][0].app_idx == 0x234);
    pub.app_idx = 0x236;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_APPKEY);
    pub.app_idx = 0x234;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub));
    uint8_t wire[25]; memcpy(wire, last_params, last_len);
    wire[5] |= 0x10;
    assert(config_message(OP_CONFIG_MODEL_PUB_SET, wire, 11) && last_params[0] == MESH_CONFIG_FEATURE_NOT_SUPPORTED);
    wire[5] &= ~0x10; wire[6] = 128;
    assert(config_message(OP_CONFIG_MODEL_PUB_SET, wire, 11) && last_params[0] == MESH_CONFIG_INVALID_PUBLICATION);
    wire[6] = 0xff; wire[5] |= 0x80;
    assert(!config_message(OP_CONFIG_MODEL_PUB_SET, wire, 11));
    wire[5] &= ~0x80; wire[3] = 0x80;
    assert(config_message(OP_CONFIG_MODEL_PUB_SET, wire, 11) && last_params[0] == MESH_CONFIG_INVALID_ADDRESS);
    wire[3] = 0xc0;
    assert(!config_message(OP_CONFIG_MODEL_PUB_SET, wire, 10));
    pub.address = 0xc011;
    save_fail = 1;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && saved.publications[1][0].address == 0xc010);
    save_fail = 0;
    assert(mesh_models_init()); // Persisted settings restore the periodic timer.
    int count = send_count;
    now_ms = 999; mesh_models_poll(); assert(send_count == count);
    send_fail = 1; now_ms = 1000; mesh_models_poll();
    assert(send_count == count && mesh_models.publications[1][0].remaining == 3);
    send_fail = 0; now_ms = 1001; mesh_models_poll();
    assert(send_count == count + 1 && last_src == 0x1202 && last_dst == 0xc010 && last_ttl == 7);
    assert(last_opcode == OP_ONOFF_STATUS && last_params[0] == 0 && last_app_key_index == 0x234);
    now_ms = 1100; mesh_models_poll(); assert(send_count == count + 1);
    now_ms = 1101; mesh_models_poll(); assert(send_count == count + 2);
    now_ms = 1201; mesh_models_poll(); assert(send_count == count + 3);
    now_ms = 1301; mesh_models_poll(); assert(send_count == count + 3);
    // State changes publish once, and duplicate transactions do not start a new publication.
    uint8_t on[] = {1, 42};
    mesh_access_pdu message = {.src = 0x1301, .dst = 0x1202, .app_key_index = 0x234,
        .opcode = OP_ONOFF_SET_UNACK, .params = on, .params_len = 2};
    assert(poll_message(&message));
    mesh_models_poll();
    assert(last_opcode == OP_ONOFF_STATUS && last_params[0] == 1);
    uint32_t retry_at = mesh_models.publications[1][0].retransmit_at_ms;
    assert(poll_message(&message));
    assert(mesh_models.publications[1][0].retransmit_at_ms == retry_at);
    // Disabling publication cancels retries and ignores other fields.
    pub.address = 0; pub.app_idx = 0xffff; pub.ttl = 0xfe;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(saved.publications[1][0].address == 0 && last_params[5] == 0 && last_params[7] == 0);
    count = send_count; now_ms += 2000; mesh_models_poll(); assert(send_count == count);

    // Virtual publication uses its UUID rather than relying on a subscription.
    pub = (mesh_publication){.app_idx = 0x234, .ttl = 4, .has_label = 1};
    memcpy(pub.label, label, 16);
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_SERVER, &pub) && config_request());
    assert(saved.publications[1][0].address == 0x8009 && saved.publications[1][0].has_label);
    uint8_t present = 1;
    assert(mesh_publication_begin(1, MESH_MODEL_ONOFF_SERVER, OP_ONOFF_STATUS, &present, 1));
    mesh_models_poll(); assert(last_dst == 0x8009 && last_ttl == 4);
    uint8_t unbind[] = {2, 0x12, 0x34, 2, 0, 0x10};
    assert(config_message(OP_CONFIG_MODEL_APP_UNBIND, unbind, 6));
    assert(saved.publications[1][0].address == 0);

    // Client periodic messages start only after the application publishes.
    pub = (mesh_publication){.address = 0xc100, .app_idx = 0x234, .ttl = 4,
                             .period = 0x41, .retransmit = 1};
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_CLIENT, &pub) && config_request());
    count = send_count; now_ms += 1000; mesh_models_poll();
    assert(send_count == count);
    assert(mesh_onoff_publish(0x1202, 0, 0));
    mesh_models_poll();
    uint8_t tid = last_params[1];
    now_ms += 50; mesh_models_poll();
    assert(last_opcode == OP_ONOFF_SET_UNACK && last_params[1] == tid);
    now_ms += 950; mesh_models_poll();
    assert(last_params[1] != tid && last_params[0] == 0);
    tid = last_params[1];
    // A received configuration request gets the TX slot before a due publication.
    now_ms += 50;
    assert(config_message(OP_CONFIG_DEFAULT_TTL_GET, NULL, 0));
    assert(last_opcode == OP_CONFIG_DEFAULT_TTL_STATUS);
    mesh_models_poll();
    assert(last_opcode == OP_ONOFF_SET_UNACK && last_params[1] == tid);

    // TTL 1 is local delivery, with no Mesh advertising packet.
    pub = (mesh_publication){.address = 0x1201, .app_idx = 0x234, .ttl = 1, .retransmit = 1};
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_CLIENT, &pub) && config_request());
    assert(mesh_onoff_publish(0x1202, 1, 0));
    count = send_count;
    assert(mesh_models_poll());
    assert(send_count == count && mesh_models.onoff_server[0].onoff == 1);
    int prior = apply_count;
    now_ms += 50;
    assert(mesh_models_poll());
    assert(send_count == count && apply_count == prior); // Retry uses the same TID.
    assert(!mesh_onoff_publish(0x1203, 1, 0));
    assert(!mesh_onoff_publish(0x1202, 2, 0));
    pub.address = 0;
    assert(mesh_set_publication(0x1201, 0x1202, MESH_MODEL_ONOFF_CLIENT, &pub) && config_request());

    // Health publishes an empty current-fault list; timers survive clock wrap.
    now_ms = UINT32_MAX - 50;
    pub = (mesh_publication){.address = 0xc100, .app_idx = 0x234, .ttl = 0xff, .period = 1};
    assert(mesh_set_publication(0x1201, 0x1201, MESH_MODEL_HEALTH_SERVER, &pub) && config_request());
    count = send_count;
    now_ms = 48; mesh_models_poll(); assert(send_count == count);
    now_ms = 49; mesh_models_poll();
    assert(send_count == count + 1 && last_opcode == OP_HEALTH_CURRENT_STATUS && last_len == 3);
    assert(last_params[0] == 0 && last_params[1] == (uint8_t)MESH_COMPANY_ID);
    uint8_t delete_key[] = {0x23, 0x41, 0x23};
    assert(config_message(OP_CONFIG_APPKEY_DELETE, delete_key, 3));
    assert(saved.publications[0][2].address == 0 && saved.health_server_bindings == 0);
    count = send_count; now_ms += 1000; mesh_models_poll(); assert(send_count == count);

    // New status types reach the Config Client only with the sender's Device Key.
    const uint32_t statuses[] = {OP_CONFIG_COMPOSITION_STATUS, OP_CONFIG_DEFAULT_TTL_STATUS,
        OP_CONFIG_MODEL_PUB_STATUS, OP_CONFIG_SIG_MODEL_SUB_LIST,
        OP_CONFIG_SAR_TRANSMITTER_STATUS, OP_CONFIG_SAR_RECEIVER_STATUS};
    message = (mesh_access_pdu){.src = 0x1202, .dst = 0x1201,
        .app_key_index = APP_KEY_INDEX_NONE, .device_key_owner = 0x1202};
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        message.opcode = statuses[i];
        prior = config_report_count;
        assert(poll_message(&message) && config_report_count == prior + 1);
        message.device_key_owner = 0x1301;
        assert(!poll_message(&message));
        message.device_key_owner = 0x1202;
    }
}

static void test_key_configuration(void) {
    mesh_net_state initial = {.net_key_index = 0xabc, .unicast_address = 0x1201,
                              .element_count = 2};
    memset(initial.net_key, 0x11, 16);
    initial.app_keys[0].used = initial.app_keys[1].used = 1;
    initial.app_keys[0].index = 0x234;
    initial.app_keys[1].index = 0x235;
    initial.app_keys[0].net_idx = initial.app_keys[1].net_idx = 0xabc;
    memset(initial.app_keys[0].key, 0x22, 16);
    memset(initial.app_keys[1].key, 0x33, 16);
    assert(mesh_network_init(&initial));
    saved_network = initial;
    memset(&saved, 0, sizeof(saved));
    memset(&mesh_models, 0, sizeof(mesh_models));
    saved.default_ttl = 5;
    saved.onoff_server_bindings = 1;
    saved.publications[0][0] = (mesh_publication){.address = 0xc001, .app_idx = 0x234};
    assert(mesh_models_init());
    mesh_models_state models_before = saved;
    uint8_t new_key[16], new_app[16], third_key[16];
    memset(new_key, 0x44, 16); memset(new_app, 0x55, 16); memset(third_key, 0x66, 16);

    assert(mesh_netkey_get(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_NETKEY_LIST && last_len == 2);
    assert(last_params[0] == 0xbc && last_params[1] == 0x0a);
    assert(last_src == 0x1201 && last_dst == 0x1202 && last_app_key_index == DEVICE_KEY_LOCAL);
    assert(!mesh_netkey_add_or_update(0x1201, 0x1000, new_key, 1));
    assert(!mesh_netkey_add_or_update(0x1201, 0xabc, NULL, 0));
    assert(!mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 2));
    assert(!mesh_netkey_delete(0x1201, 0x1000));
    assert(!mesh_netkey_get_phase(0x1201, 0x1000));
    assert(!mesh_netkey_set_phase(0x1201, 0xabc, 1));
    assert(!mesh_netkey_set_phase(0x1201, 0xabc, 4));
    assert(!mesh_netkey_set_phase(0x1201, 0x1000, 2));

    int writes = network_save_count;
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, initial.net_key, 0) && config_request());
    assert(last_opcode == OP_CONFIG_NETKEY_STATUS && last_len == 3 && last_params[0] == 0);
    assert(network_save_count == writes);
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_KEY_ALREADY_STORED);
    assert(mesh_netkey_add_or_update(0x1201, 0xabd, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_subnet_slot(&mesh_network.state, 0xabd) > 0);
    assert(mesh_netkey_add_or_update(0x1201, 0xabe, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_netkey_add_or_update(0x1201, 0xabf, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_netkey_add_or_update(0x1201, 0xac0, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_INSUFFICIENT_RESOURCES);
    assert(mesh_netkey_delete(0x1201, 0xabd) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_netkey_delete(0x1201, 0xabe) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_netkey_delete(0x1201, 0xabf) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(network_save_count == writes + 6 && !memcmp(&mesh_network.state, &initial, sizeof(initial)));
    assert(mesh_netkey_delete(0x1201, 0xabc) && config_request());
    assert(last_params[0] == MESH_CONFIG_CANNOT_REMOVE);
    writes = network_save_count;

    uint8_t malformed[18] = {0xbc, 0x1a};
    assert(!config_message(OP_CONFIG_NETKEY_ADD, malformed, sizeof(malformed)));
    assert(!config_message(OP_CONFIG_NETKEY_UPDATE, malformed, sizeof(malformed)));
    assert(!config_message(OP_CONFIG_NETKEY_DELETE, malformed, 2));
    malformed[1] = 0x0a;
    assert(!config_message(OP_CONFIG_NETKEY_ADD, malformed, 17));
    assert(!config_message(OP_CONFIG_NETKEY_GET, malformed, 1));
    assert(!config_message(OP_CONFIG_NETKEY_DELETE, malformed, 3));
    uint8_t bad_phase[] = {0xbc, 0x0a, 0};
    assert(!config_message(OP_CONFIG_KEY_PHASE_SET, bad_phase, 3));
    bad_phase[2] = 4; assert(!config_message(OP_CONFIG_KEY_PHASE_SET, bad_phase, 3));
    bad_phase[2] = 2; bad_phase[1] = 0x1a;
    assert(!config_message(OP_CONFIG_KEY_PHASE_GET, bad_phase, 2));
    assert(!config_message(OP_CONFIG_KEY_PHASE_SET, bad_phase, 3));
    bad_phase[1] = 0x0a;
    assert(!config_message(OP_CONFIG_KEY_PHASE_GET, bad_phase, 3));
    assert(!config_message(OP_CONFIG_KEY_PHASE_SET, bad_phase, 2));
    assert(network_save_count == writes);

    assert(mesh_netkey_get_phase(0x1201, 0xabc) && config_request());
    assert(last_opcode == OP_CONFIG_KEY_PHASE_STATUS && last_len == 4 && last_params[3] == 0);
    assert(mesh_netkey_get_phase(0x1201, 0xabd) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY && last_params[3] == 0);
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 2) && config_request());
    assert(last_params[0] == 0x0b && last_params[3] == 0); // Cannot Update wire status.
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS && network_save_count == writes);
    assert(mesh_netkey_add_or_update(0x1201, 0xabd, new_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY);
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, initial.net_key, 1) && config_request());
    assert(last_params[0] == 0x06); // Key Index Already Stored wire status.

    // Persist before exposing the new keys, and report storage failures separately.
    network_save_fail = 1;
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE);
    assert(!memcmp(&saved_network, &initial, sizeof(initial)));
    assert(!memcmp(&mesh_network.state, &initial, sizeof(initial)));
    network_save_fail = 0;
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS && mesh_network.state.key_refresh_phase == 1);
    assert(mesh_network.state.has_new_key && !memcmp(saved_network.new_net_key, new_key, 16));
    assert(!memcmp(mesh_network.state.net_key, initial.net_key, 16));
    writes = network_save_count;
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_KEY_ALREADY_STORED);
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, initial.net_key, 0) && config_request());
    assert(last_params[0] == 0 && network_save_count == writes);
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 1) && config_request());
    assert(last_params[0] == 0 && network_save_count == writes); // Same Phase 1 update is redundant.
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, third_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_CANNOT_UPDATE);
    assert(mesh_netkey_get_phase(0x1201, 0xabc) && config_request() && last_params[3] == 1);

    network_save_fail = 1;
    assert(mesh_add_or_update_app_key(0x1201, 0xabc, 0x234, new_app, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && !mesh_network.state.app_keys[0].has_new_key);
    network_save_fail = 0;
    assert(mesh_add_or_update_app_key(0x1201, 0xabc, 0x234, new_app, 1) && config_request());
    assert(last_params[0] == 0 && mesh_network.state.app_keys[0].has_new_key);
    writes = network_save_count;
    assert(mesh_add_or_update_app_key(0x1201, 0xabc, 0x234, new_app, 1) && config_request());
    assert(last_params[0] == 0 && network_save_count == writes);
    assert(mesh_add_or_update_app_key(0x1201, 0xabc, 0x234, third_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_CANNOT_UPDATE);
    assert(mesh_network_restore() && mesh_models_init());
    assert(mesh_netkey_get_phase(0x1201, 0xabc) && config_request() && last_params[3] == 1);

    network_save_fail = 1;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 2) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && last_params[3] == 1);
    network_save_fail = 0;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 2) && config_request());
    assert(last_params[0] == 0 && last_params[3] == 2 && saved_network.key_refresh_phase == 2);
    writes = network_save_count;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 2) && config_request());
    assert(last_params[0] == 0 && network_save_count == writes);
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, new_key, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_CANNOT_UPDATE);
    assert(mesh_add_or_update_app_key(0x1201, 0xabc, 0x234, new_app, 1) && config_request());
    assert(last_params[0] == MESH_CONFIG_CANNOT_UPDATE);
    assert(mesh_netkey_set_phase(0x1201, 0xabd, 3) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY && mesh_network.state.key_refresh_phase == 2);
    network_save_fail = 1;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && last_params[3] == 2);
    network_save_fail = 0;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == 0 && last_params[3] == 0 && !mesh_network.state.has_new_key);
    assert(!memcmp(mesh_network.state.net_key, new_key, 16));
    assert(!memcmp(mesh_network.state.app_keys[0].key, new_app, 16));
    assert(!memcmp(mesh_network.state.app_keys[1].key, initial.app_keys[1].key, 16));
    assert(!mesh_network.state.app_keys[0].has_new_key);
    uint8_t zero[16] = {0};
    assert(!memcmp(mesh_network.state.new_net_key, zero, 16));
    assert(!memcmp(mesh_network.state.app_keys[0].new_key, zero, 16));
    assert(!memcmp(&saved, &models_before, sizeof(saved))); // Bindings/publication indexes remain usable.
    writes = network_save_count;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == 0 && network_save_count == writes);

    // Transition 3 may skip Phase 2; a Phase 2 provisionee already has only the new key.
    assert(mesh_netkey_add_or_update(0x1201, 0xabc, third_key, 1) && config_request());
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[3] == 0 && !memcmp(mesh_network.state.net_key, third_key, 16));
    initial = mesh_network.state;
    initial.phase2_provisioned = 1;
    assert(mesh_network_init(&initial)); saved_network = initial;
    assert(mesh_netkey_get_phase(0x1201, 0xabc) && config_request());
    assert(last_params[3] == 2);
    writes = network_save_count;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 2) && config_request());
    assert(last_params[0] == 0 && last_params[3] == 2 && network_save_count == writes);
    network_save_fail = 1;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && last_params[3] == 2);
    network_save_fail = 0;
    assert(mesh_netkey_set_phase(0x1201, 0xabc, 3) && config_request());
    assert(last_params[0] == 0 && last_params[3] == 0 && !mesh_network.state.phase2_provisioned);
    assert(!memcmp(mesh_network.state.net_key, third_key, 16));

    // Incoming key-management requests need this node's DevKey and the primary element.
    uint8_t update[18] = {0xbc, 0x0a}; memcpy(update + 2, new_key, 16);
    mesh_access_pdu message = {.src = 0x1202, .dst = 0x1201,
        .device_key_owner = 0x1202, .app_key_index = APP_KEY_INDEX_NONE,
        .net_key_index = 0xabc,
        .opcode = OP_CONFIG_NETKEY_UPDATE, .params = update, .params_len = sizeof(update)};
    assert(!poll_message(&message));
    message.device_key_owner = 0x1201; message.app_key_index = 0x234;
    assert(!poll_message(&message));
    message.app_key_index = APP_KEY_INDEX_NONE; message.dst = 0x1202;
    message.device_key_owner = 0x1202;
    assert(!poll_message(&message));
    assert(mesh_network.state.key_refresh_phase == 0);
    const uint32_t statuses[] = {OP_CONFIG_NETKEY_STATUS, OP_CONFIG_NETKEY_LIST, OP_CONFIG_KEY_PHASE_STATUS};
    message.dst = 0x1201; message.params = NULL; message.params_len = 0;
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        message.opcode = statuses[i];
        int reports = config_report_count;
        assert(poll_message(&message) && config_report_count == reports + 1);
        message.device_key_owner = 0x1201;
        assert(!poll_message(&message));
        message.device_key_owner = 0x1202;
    }

    assert(mesh_netkey_add_or_update(0x1201, 0xabd, new_key, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_netkey_delete(0x1201, 0xabc) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_network.state.net_key_index == 0xabd);
    assert(mesh_subnet_slot(&mesh_network.state, 0xabc) < 0);
    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
        assert(!mesh_network.state.app_keys[i].used);
}

static void test_node_settings(void) {
    mesh_net_state state = {.unicast_address = 0x1201, .element_count = 2,
                            .net_key_index = 0x123, .beacon = 1};
    assert(mesh_network_init(&state));
    saved_network = state;
    memset(&saved, 0, sizeof(saved));
    memset(&mesh_models, 0, sizeof(mesh_models));
    saved.default_ttl = 5;
    assert(mesh_models_init());
    assert(mesh_get_beacon(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_BEACON_STATUS && last_len == 1 && last_params[0] == 1);
    assert(!mesh_set_beacon(0x1201, 2));
    uint8_t params[] = {2, 0xff, 1};
    assert(!config_message(OP_CONFIG_BEACON_SET, params, 1));
    assert(!config_message(OP_CONFIG_BEACON_GET, params, 1));
    assert(!config_message(OP_CONFIG_BEACON_SET, NULL, 0));
    network_save_fail = 1;
    assert(mesh_set_beacon(0x1201, 0) && !config_request());
    assert(saved_network.beacon == 1 && mesh_network.state.beacon == 1);
    network_save_fail = 0;
    now_ms += 100;
    assert(mesh_set_beacon(0x1201, 0) && config_request());
    assert(last_params[0] == 0 && saved_network.beacon == 0);
    assert(mesh_network.beacon.last_sent_ms == now_ms);
    assert(mesh_network_restore() && mesh_get_beacon(0x1201) && config_request());
    assert(last_params[0] == 0);
    assert(mesh_set_beacon(0x1201, 1) && config_request());
    assert(last_params[0] == 1 && saved_network.beacon == 1);
    int writes = network_save_count;
    assert(mesh_set_beacon(0x1201, 1) && config_request());
    assert(network_save_count == writes);

    assert(mesh_get_net_transmit(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_NET_TRANSMIT_STATUS && last_len == 1 && last_params[0] == 0);
    assert(!mesh_set_net_transmit(0x1201, 8, 0));
    assert(!mesh_set_net_transmit(0x1201, 0, 32));
    assert(!config_message(OP_CONFIG_NET_TRANSMIT_GET, params, 1));
    assert(!config_message(OP_CONFIG_NET_TRANSMIT_SET, NULL, 0));
    assert(!config_message(OP_CONFIG_NET_TRANSMIT_SET, params, 2));
    network_save_fail = 1;
    assert(mesh_set_net_transmit(0x1201, 7, 31) && !config_request());
    assert(mesh_network.state.network_transmit == 0 && saved_network.network_transmit == 0);
    network_save_fail = 0;
    assert(mesh_set_net_transmit(0x1201, 7, 31) && config_request());
    assert(last_params[0] == 0xff && saved_network.network_transmit == 0xff);
    assert(mesh_network_restore() && mesh_get_net_transmit(0x1201) && config_request());
    assert(last_params[0] == 0xff);
    writes = network_save_count;
    assert(mesh_set_net_transmit(0x1201, 7, 31) && config_request());
    assert(network_save_count == writes);
    assert(mesh_set_net_transmit(0x1201, 2, 4) && config_request());
    assert(last_params[0] == 34 && saved_network.network_transmit == 34);
    assert(last_app_key_index == DEVICE_KEY_LOCAL && last_src == 0x1201 && last_dst == 0x1202);

    // Relay settings are saved and reported; Proxy and Friend remain unsupported.
    writes = network_save_count;
    assert(mesh_get_relay(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_RELAY_STATUS && last_len == 2 &&
           last_params[0] == 0 && last_params[1] == 0);
    params[0] = 1; params[1] = 0x9a;
    assert(config_message(OP_CONFIG_RELAY_SET, params, 2));
    assert(last_params[0] == 1 && last_params[1] == 0x9a &&
           saved_network.relay == 1 && saved_network.relay_retransmit == 0x9a);
    assert(mesh_get_relay(0x1201) && config_request() &&
           last_params[0] == 1 && last_params[1] == 0x9a);
    params[0] = 0; params[1] = 0;
    assert(config_message(OP_CONFIG_RELAY_SET, params, 2));
    assert(mesh_network_restore() && mesh_network.state.relay == 0 &&
           mesh_network.state.relay_retransmit == 0);
    params[0] = 2; assert(!config_message(OP_CONFIG_RELAY_SET, params, 2));
    assert(!config_message(OP_CONFIG_RELAY_SET, params, 1));
    assert(!config_message(OP_CONFIG_RELAY_GET, params, 1));
    assert(mesh_get_proxy(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_PROXY_STATUS && last_len == 1 && last_params[0] == 2);
    assert(mesh_get_friend(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_FRIEND_STATUS && last_len == 1 && last_params[0] == 2);
    const uint32_t feature_sets[] = {OP_CONFIG_PROXY_SET, OP_CONFIG_FRIEND_SET};
    for (size_t i = 0; i < sizeof(feature_sets) / sizeof(feature_sets[0]); i++) {
        params[0] = 1; assert(config_message(feature_sets[i], params, 1) && last_params[0] == 2);
        params[0] = 0; assert(config_message(feature_sets[i], params, 1) && last_params[0] == 2);
        params[0] = 2; assert(!config_message(feature_sets[i], params, 1));
        assert(!config_message(feature_sets[i], NULL, 0));
    }
    assert(network_save_count == writes + 2 && saved_network.network_transmit == 34 &&
           saved_network.beacon == 1 && saved_network.relay == 0);
    assert(mesh_set_relay(0x1201, 1, 7, 31) && config_request());
    assert(last_opcode == OP_CONFIG_RELAY_STATUS && last_len == 2 &&
           last_params[0] == 1 && last_params[1] == 0xff);
    assert(!mesh_set_relay(0x1201, 2, 0, 0));
    assert(!mesh_set_relay(0x1201, 1, 8, 0));
    assert(!mesh_set_relay(0x1201, 1, 0, 32));
    writes = network_save_count;

    assert(!mesh_get_node_identity(0x1201, 0x1000));
    assert(mesh_get_node_identity(0x1201, 0x123) && config_request());
    assert(last_opcode == OP_CONFIG_NODE_IDENTITY_STATUS && last_len == 4);
    assert(last_params[0] == 0 && last_params[1] == 0x23 && last_params[2] == 1 && last_params[3] == 2);
    assert(mesh_get_node_identity(0x1201, 0x124) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY && last_params[3] == 0);
    uint8_t identity[] = {0x23, 1, 1};
    assert(config_message(OP_CONFIG_NODE_IDENTITY_SET, identity, 3));
    assert(last_params[0] == MESH_CONFIG_FEATURE_NOT_SUPPORTED && last_params[3] == 2);
    identity[0] = 0x24;
    assert(config_message(OP_CONFIG_NODE_IDENTITY_SET, identity, 3));
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY && last_params[3] == 1);
    identity[2] = 2;
    assert(!config_message(OP_CONFIG_NODE_IDENTITY_SET, identity, 3));
    identity[2] = 0xff;
    assert(!config_message(OP_CONFIG_NODE_IDENTITY_SET, identity, 3));
    identity[1] = 0xf1;
    assert(!config_message(OP_CONFIG_NODE_IDENTITY_GET, identity, 2));
    assert(!config_message(OP_CONFIG_NODE_IDENTITY_SET, identity, 2));
    assert(network_save_count == writes);

    uint8_t disable = 0;
    mesh_access_pdu message = {.src = 0x1202, .dst = 0x1201,
        .device_key_owner = 0x1202, .app_key_index = APP_KEY_INDEX_NONE,
        .opcode = OP_CONFIG_BEACON_SET, .params = &disable, .params_len = 1};
    assert(!poll_message(&message));
    message.device_key_owner = 0x1201; message.app_key_index = 0x234;
    assert(!poll_message(&message));
    message.app_key_index = APP_KEY_INDEX_NONE; message.dst = 0x1202; message.device_key_owner = 0x1202;
    assert(!poll_message(&message));
    assert(saved_network.beacon == 1);
    const uint32_t statuses[] = {OP_CONFIG_BEACON_STATUS, OP_CONFIG_NET_TRANSMIT_STATUS,
        OP_CONFIG_RELAY_STATUS, OP_CONFIG_PROXY_STATUS, OP_CONFIG_FRIEND_STATUS, OP_CONFIG_NODE_IDENTITY_STATUS};
    message.dst = 0x1201;
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        message.opcode = statuses[i];
        int reports = config_report_count;
        assert(poll_message(&message) && config_report_count == reports + 1);
        message.device_key_owner = 0x1201;
        assert(!poll_message(&message));
        message.device_key_owner = 0x1202;
    }
}

// Exercise Heartbeat reception through the authenticated network receive path.
static void receive_heartbeat(const mesh_net_message *hb) {
    static uint32_t sequence;
    uint32_t seq = sequence++;
    uint32_t iv = mesh_network.state.iv_index;
    uint8_t pdu[21] = {mesh_network.old_key.nid, (uint8_t)(0x80 | hb->ttl),
        (uint8_t)(seq >> 16), (uint8_t)(seq >> 8), (uint8_t)seq,
        (uint8_t)(hb->src >> 8), (uint8_t)hb->src};
    uint8_t plain[6] = {(uint8_t)(hb->dst >> 8), (uint8_t)hb->dst};
    memcpy(plain + 2, hb->transport, 4);
    uint8_t nonce[13];
    mesh_nonce(nonce, pdu + 1, iv);
    assert(ccm_encrypt_and_tag(mesh_network.old_key.encryption_key, nonce, 13,
        NULL, 0, plain, sizeof(plain), pdu + 7, pdu + 13, 8) == CCM_OK);
    mesh_obfuscate(&mesh_network.old_key, pdu, iv);
    mesh_net_message received;
    assert(mesh_net_receive(pdu, sizeof(pdu), &received) == 0);
}

static void test_health(void) {
    mesh_net_state state = {.unicast_address = 0x1201, .element_count = 2};
    state.app_keys[0].used = 1; state.app_keys[0].index = 0x234;
    state.app_keys[1].used = 1; state.app_keys[1].index = 0x235;
    assert(mesh_network_init(&state));
    memset(&saved, 0, sizeof(saved)); memset(&mesh_models, 0, sizeof(mesh_models));
    saved.default_ttl = 5; saved.health_server_bindings = 1; saved.other[0].health_server_bindings = 1;
    assert(mesh_models_init());
    now_ms = UINT32_MAX - 199;
    uint8_t company[] = {(uint8_t)MESH_COMPANY_ID, (uint8_t)(MESH_COMPANY_ID >> 8)};
    mesh_access_pdu request = {.src = 0x1301, .dst = 0x1201, .app_key_index = 0x234,
        .opcode = OP_HEALTH_FAULT_GET, .params = company, .params_len = 2};
    assert(poll_message(&request) && last_opcode == OP_HEALTH_FAULT_STATUS && last_len == 3);
    assert(last_params[0] == 0 && !memcmp(last_params + 1, company, 2));
    const uint8_t faults[] = {1, 2, 1};
    assert(mesh_health_faults(0x1201, 0, faults, sizeof(faults)));
    assert(mesh_models.health_server[0].current_count == 2 && mesh_models.health_server[0].registered_count == 2);
    assert(poll_message(&request) && last_len == 5 && last_params[3] == 1 && last_params[4] == 2);
    assert(mesh_health_faults(0x1201, 1, NULL, 0));
    assert(poll_message(&request) && last_len == 5 && last_params[0] == 1);
    assert(mesh_models.health_server[0].current_count == 0); // History survives recovery.
    const uint8_t active[] = {3};
    assert(mesh_health_faults(0x1201, 1, active, 1));
    request.opcode = OP_HEALTH_FAULT_CLEAR;
    assert(poll_message(&request) && last_len == 3 && last_params[0] == 1);
    assert(mesh_models.health_server[0].registered_count == 0 && mesh_models.health_server[0].current_count == 1);
    assert(mesh_health_faults(0x1201, 1, active, 1));
    request.opcode = OP_HEALTH_FAULT_CLEAR_UNACK;
    int count = send_count;
    assert(poll_message(&request) && send_count == count && mesh_models.health_server[0].registered_count == 0);

    uint8_t test[] = {1, (uint8_t)MESH_COMPANY_ID, (uint8_t)(MESH_COMPANY_ID >> 8)};
    health_test_faults[0] = 4; health_test_count = 1;
    request.opcode = OP_HEALTH_FAULT_TEST; request.params = test; request.params_len = 3;
    assert(poll_message(&request) && health_test_calls == 1 && health_test_element == 0x1201);
    assert(last_len == 4 && last_params[0] == 1 && last_params[3] == 4);
    request.dst = 0x1202; test[0] = 0; health_test_faults[0] = 5;
    assert(poll_message(&request) && last_src == 0x1202 && health_test_element == 0x1202);
    assert(mesh_models.health_server[0].current[0] == 4 && mesh_models.health_server[1].current[0] == 5);
    request.opcode = OP_HEALTH_FAULT_TEST_UNACK;
    count = send_count; assert(poll_message(&request) && send_count == count);
    test[0] = 2; int calls = health_test_calls;
    assert(!poll_message(&request) && health_test_calls == calls);
    test[0] = 1; test[1] ^= 1;
    assert(!poll_message(&request) && health_test_calls == calls);
    test[1] ^= 1; health_test_fail = 1;
    assert(!poll_message(&request) && mesh_models.health_server[1].current[0] == 5);
    health_test_fail = 0;
    request.params_len = 2; assert(!poll_message(&request));
    request.opcode = OP_HEALTH_FAULT_GET; request.params = test + 1; request.params_len = 2;
    request.app_key_index = 0x235;
    assert(!poll_message(&request)); // Key exists but is not bound to this Health Server.
    request.app_key_index = APP_KEY_INDEX_NONE; request.device_key_owner = 0x1202;
    assert(!poll_message(&request));
    request.app_key_index = 0x234; request.device_key_owner = 0;
    test[1] ^= 1; assert(!poll_message(&request)); test[1] ^= 1;

    uint8_t divisor = 2;
    request.opcode = OP_HEALTH_PERIOD_SET; request.params = &divisor; request.params_len = 1;
    save_fail = 1; assert(!poll_message(&request) && saved.health_period[1] == 0);
    save_fail = 0; assert(poll_message(&request) && last_opcode == OP_HEALTH_PERIOD_STATUS && last_params[0] == 2);
    assert(saved.health_period[0] == 0 && saved.health_period[1] == 2);
    divisor = 16; assert(!poll_message(&request) && saved.health_period[1] == 2);
    divisor = 15; request.opcode = OP_HEALTH_PERIOD_SET_UNACK;
    count = send_count; assert(poll_message(&request) && send_count == count && saved.health_period[1] == 15);
    request.opcode = OP_HEALTH_PERIOD_GET; request.params = NULL; request.params_len = 0;
    assert(poll_message(&request) && last_params[0] == 15);
    request.dst = 0x1201; assert(poll_message(&request) && last_params[0] == 0);
    saved.health_period[0] = 16; mesh_models.ready = 0; assert(!mesh_models_init());
    saved.health_period[0] = 2; assert(mesh_models_init());
    assert(mesh_models.state.health_period[1] == 15 && mesh_models.health_server[1].current_count == 0);
    assert(mesh_models.health_server[0].registered_count == 0); // Faults are runtime data.

    mesh_publication pub = {.address = 0xc001, .app_idx = 0x234, .ttl = 5, .period = 0x48, .retransmit = 1};
    assert(mesh_set_publication(0x1201, 0x1201, MESH_MODEL_HEALTH_SERVER, &pub) && config_request());
    const uint8_t full[] = {1, 2, 3, 4, 5};
    assert(mesh_health_faults(0x1201, 1, full, sizeof(full)));
    count = send_count; assert(!mesh_models_poll() && send_count == count + 1);
    assert(last_opcode == OP_HEALTH_CURRENT_STATUS && last_len == 8 && last_params[0] == 1);
    assert(!memcmp(last_params + 3, full, sizeof(full)) && mesh_health_period(0) == 2000);
    now_ms += 50; count = send_count; mesh_models_poll(); assert(send_count == count + 1 && last_len == 8);
    now_ms += 1949; count = send_count; mesh_models_poll(); assert(send_count == count);
    now_ms++; mesh_models_poll(); assert(send_count == count + 1 && last_len == 8);
    uint8_t zero = 0, extra = 6;
    assert(!mesh_health_faults(0x1201, 1, &zero, 1));
    assert(!mesh_health_faults(0x1201, 1, &extra, 1)); // Full history rejects atomically.
    assert(mesh_models.health_server[0].current_count == 5 && mesh_models.health_server[0].current[0] == 1);
    assert(!mesh_health_faults(0x1203, 0, NULL, 0));
    assert(!mesh_health_faults(0x1201, 0, NULL, 1));
    uint8_t too_many[MESH_HEALTH_MAX_FAULTS + 1] = {1};
    assert(!mesh_health_faults(0x1201, 0, too_many, sizeof(too_many)));
    assert(mesh_health_faults(0x1201, 1, NULL, 0));
    count = send_count; mesh_models_poll(); assert(send_count == count + 1 && last_len == 3);
    assert(mesh_health_period(0) == 8000);
    now_ms += 50; mesh_models_poll();
    now_ms += 7949; count = send_count; mesh_models_poll(); assert(send_count == count);
    now_ms++; mesh_models_poll(); assert(send_count == count + 1 && last_len == 3);
    mesh_models.state.health_period[0] = 15;
    assert(mesh_health_faults(0x1201, 1, full, sizeof(full)) && mesh_health_period(0) == 100);
    pub.period = 0;
    assert(mesh_set_publication(0x1201, 0x1201, MESH_MODEL_HEALTH_SERVER, &pub) && config_request());
    count = send_count; mesh_models_poll(); assert(send_count == count + 1); // Changes publish even without a period.
    now_ms += 50; mesh_models_poll();
    now_ms += 1000; count = send_count; mesh_models_poll(); assert(send_count == count);
}

static void test_health_client(void) {
    mesh_net_state state = {.unicast_address = 0x1201, .element_count = 2};
    state.app_keys[0].used = 1; state.app_keys[0].index = 0x234;
    state.app_keys[1].used = 1; state.app_keys[1].index = 0x235;
    assert(mesh_network_init(&state));
    memset(&saved, 0, sizeof(saved)); memset(&mesh_models, 0, sizeof(mesh_models));
    saved.default_ttl = 5;
    saved.health_server_bindings = 1; // A server binding cannot authorize the client.
    assert(mesh_models_init());
    assert(!mesh_health_fault_get(0x1201, 0x1301, 0x234, 0x1234));
    for (uint16_t element = 0x1201; element <= 0x1202; element++) {
        assert(mesh_model_binding(0x1201, element, 0x234, MESH_MODEL_HEALTH_CLIENT, 1) && config_request());
        assert(last_opcode == OP_CONFIG_MODEL_APP_STATUS && last_params[0] == MESH_CONFIG_SUCCESS);
        assert(mesh_get_bindings(0x1201, element, MESH_MODEL_HEALTH_CLIENT) && config_request());
        assert(last_opcode == OP_CONFIG_SIG_MODEL_APP_LIST && last_len == 7);
        assert(last_params[5] == 0x34 && last_params[6] == 2);
    }
    assert(saved.health_client_bindings == 1 && saved.other[0].health_client_bindings == 1);
    mesh_models.ready = 0;
    assert(mesh_models_init()); // Restore both persisted client bindings.
    assert(mesh_health_fault_get(0x1202, 0x1301, 0x234, 0x1234));
    assert(last_src == 0x1202 && last_dst == 0x1301 && last_app_key_index == 0x234 && last_ttl == 5);
    assert(last_expected_response == OP_HEALTH_FAULT_STATUS);
    assert(last_opcode == OP_HEALTH_FAULT_GET && last_len == 2 && last_params[0] == 0x34 && last_params[1] == 0x12);
    assert(!mesh_health_fault_get(0x1203, 0x1301, 0x234, 0x1234));
    assert(!mesh_health_fault_get(0x1201, 0x1301, 0x235, 0x1234));
    assert(!mesh_health_fault_get(0x1201, 0x1301, APP_KEY_INDEX_NONE, 0x1234));
    for (uint8_t ack = 0; ack < 2; ack++) {
        assert(mesh_health_fault_clear(0x1201, 0x1301, 0x234, 0x1234, ack));
        assert(last_opcode == (ack ? OP_HEALTH_FAULT_CLEAR : OP_HEALTH_FAULT_CLEAR_UNACK) && last_len == 2);
        if (ack) assert(last_expected_response == OP_HEALTH_FAULT_STATUS);
        assert(last_params[0] == 0x34 && last_params[1] == 0x12);
        assert(mesh_health_fault_test(0x1201, 0x1301, 0x234, 0x1234, 7, ack));
        assert(last_opcode == (ack ? OP_HEALTH_FAULT_TEST : OP_HEALTH_FAULT_TEST_UNACK) && last_len == 3);
        if (ack) assert(last_expected_response == OP_HEALTH_FAULT_STATUS);
        assert(last_params[0] == 7 && last_params[1] == 0x34 && last_params[2] == 0x12);
        assert(mesh_health_period_set(0x1201, 0xc001, 0x234, 15, ack));
        assert(last_opcode == (ack ? OP_HEALTH_PERIOD_SET : OP_HEALTH_PERIOD_SET_UNACK) && last_len == 1 && last_params[0] == 15);
        assert(mesh_health_attention_set(0x1201, 0x1301, 0x234, 255, ack));
        assert(last_opcode == (ack ? OP_HEALTH_ATTENTION_SET : OP_HEALTH_ATTENTION_SET_UNACK) && last_len == 1 && last_params[0] == 255);
        if (ack) assert(last_expected_response == OP_HEALTH_ATTENTION_STATUS);
    }
    assert(!mesh_health_period_set(0x1201, 0x1301, 0x234, 16, 1));
    assert(mesh_health_period_get(0x1201, 0x1301, 0x234) && last_opcode == OP_HEALTH_PERIOD_GET && !last_len);
    assert(mesh_health_attention_get(0x1201, 0x1301, 0x234) && last_opcode == OP_HEALTH_ATTENTION_GET && !last_len);
    send_fail = 1;
    assert(!mesh_health_fault_get(0x1201, 0x1301, 0x234, 0x1234));
    send_fail = 0;

    uint8_t faults[3 + MESH_HEALTH_MAX_FAULTS + 1] = {7, 0x34, 0x12};
    for (size_t i = 3; i < sizeof(faults); i++) faults[i] = (uint8_t)(i - 2);
    mesh_access_pdu message = {.src = 0x1301, .dst = 0x1202, .app_key_index = 0x234,
        .opcode = OP_HEALTH_CURRENT_STATUS, .params = faults, .params_len = sizeof(faults)};
    health_status_count = 0;
    assert(poll_message(&message) && health_status_count == 1);
    assert(health_status_element == 0x1202 && health_status_src == 0x1301 && health_status_opcode == OP_HEALTH_CURRENT_STATUS);
    assert(health_status_len == sizeof(faults) && !memcmp(health_status_params, faults, sizeof(faults)));
    message.opcode = OP_HEALTH_FAULT_STATUS; message.params_len = 3;
    assert(poll_message(&message) && health_status_count == 2); // Empty remote history is valid.
    message.params_len = 2;
    assert(!poll_message(&message));
    uint8_t value = 15;
    message.opcode = OP_HEALTH_PERIOD_STATUS; message.params = &value; message.params_len = 1;
    assert(poll_message(&message) && health_status_params[0] == 15);
    value = 16; assert(!poll_message(&message));
    message.opcode = OP_HEALTH_ATTENTION_STATUS; value = 255;
    assert(poll_message(&message) && health_status_params[0] == 255);
    message.params_len = 0; assert(!poll_message(&message));
    message.params = faults; message.params_len = 2; assert(!poll_message(&message));
    message.opcode = OP_HEALTH_CURRENT_STATUS; message.params_len = sizeof(faults);
    message.app_key_index = 0x235; assert(!poll_message(&message));
    message.app_key_index = APP_KEY_INDEX_NONE; message.device_key_owner = message.src;
    message.dst = 0x1201; assert(!poll_message(&message));
    message.device_key_owner = message.dst; assert(!poll_message(&message));
    message.app_key_index = 0x234; message.device_key_owner = 0;
    assert(poll_message(&message) && health_status_element == 0x1201);

    for (uint16_t element = 0x1201; element <= 0x1202; element++)
        assert(mesh_group_subscription(0x1201, element, MESH_MODEL_HEALTH_CLIENT, 0xc001, 1) && config_request());
    message.dst = 0xc001;
    unsigned count = health_status_count;
    assert(poll_message(&message) && health_status_count == count + 2);
    message.dst = 0xc002; assert(!poll_message(&message));
    uint8_t label[16] = {0x81, 0x82};
    assert(mesh_config_virtual_sub(0x1201, 0x1202, MESH_MODEL_HEALTH_CLIENT, label, 1) && config_request());
    message.dst = mesh_virtual_address(label); message.has_label = 1;
    memcpy(message.label, label, 16);
    count = health_status_count;
    assert(poll_message(&message) && health_status_count == count + 1 && health_status_element == 0x1202);
    message.label[0] ^= 1; message.dst = mesh_virtual_address(message.label);
    assert(!poll_message(&message));
    save_fail = 1;
    assert(mesh_model_binding(0x1201, 0x1202, 0x234, MESH_MODEL_HEALTH_CLIENT, 0) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && saved.other[0].health_client_bindings == 1);
    save_fail = 0;
    assert(mesh_model_binding(0x1201, 0x1202, 0x234, MESH_MODEL_HEALTH_CLIENT, 0) && config_request());
    assert(!mesh_health_attention_get(0x1202, 0x1301, 0x234));
    assert(mesh_model_binding(0x1201, 0x1202, 0x234, MESH_MODEL_HEALTH_CLIENT, 1) && config_request());
    assert(mesh_delete_app_key(0x1201, 0, 0x234) && config_request());
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    assert(!saved.health_client_bindings && !saved.other[0].health_client_bindings);
    assert(!mesh_health_fault_get(0x1201, 0x1301, 0x234, 0x1234));
    saved.health_client_bindings = 0x80;
    assert(!mesh_models_init()); // Reject bindings outside the key-slot mask.
    saved.health_client_bindings = 0; saved.other[0].health_client_bindings = 0x80;
    assert(!mesh_models_init());
    saved.other[0].health_client_bindings = 0;
    assert(mesh_models_init());
}

static void test_heartbeat_configuration(void) {
    mesh_net_state state = {.unicast_address = 0x1201, .element_count = 1, .net_key_index = 0x123};
    now_ms = 0;
    assert(mesh_network_init(&state));
    saved_network = state;
    memset(&saved, 0, sizeof(saved));
    memset(&mesh_models, 0, sizeof(mesh_models));
    saved.default_ttl = 5;
    assert(mesh_models_init());
    assert(mesh_get_heartbeat_pub(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_HEARTBEAT_PUB_STATUS && last_len == 10);
    for (unsigned i = 0; i < last_len; i++) assert(last_params[i] == 0);
    mesh_heartbeat_publication pub = {.dst = 0xc001, .net_idx = 0x123,
        .count_log = 3, .period_log = 2, .ttl = 5, .features = 0xffff};
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && last_len == 9 && config_request());
    assert(last_params[0] == 0 && last_params[1] == 1 && last_params[2] == 0xc0);
    assert(last_params[3] == 3 && last_params[4] == 2 && last_params[5] == 5);
    assert(last_params[6] == 0 && last_params[7] == 0 && last_params[8] == 0x23 && last_params[9] == 1);
    assert(saved_network.heartbeat.dst == 0xc001 && saved_network.heartbeat.features == 0);
    assert(mesh_network.heartbeat.remaining == 4);
    mesh_net_message net;
    mesh_net_poll(&net); assert(mesh_network.heartbeat.remaining == 3);
    assert(mesh_get_heartbeat_pub(0x1201) && config_request() && last_params[3] == 3);
    mesh_network.heartbeat.remaining = 2;
    assert(mesh_get_heartbeat_pub(0x1201) && config_request() && last_params[3] == 2);
    assert(mesh_network_restore() && mesh_network.state.heartbeat.dst == 0xc001);
    assert(mesh_network.heartbeat.remaining == 0); // Finite counts do not resume after reboot.
    pub.count_log = 0xff;
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && config_request());
    assert(mesh_network_restore() && mesh_network.heartbeat.remaining == 0xffff);
    pub.count_log = 0x11;
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && config_request());
    assert(last_params[3] == 0x11 && mesh_network.heartbeat.remaining == 0xfffe);
    pub.dst = 0xc002; network_save_fail = 1;
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && config_request());
    assert(last_params[0] == MESH_CONFIG_STORAGE_FAILURE && mesh_network.state.heartbeat.dst == 0xc001);
    network_save_fail = 0;
    pub.net_idx = 0x124;
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && config_request());
    assert(last_params[0] == MESH_CONFIG_INVALID_NETKEY && mesh_network.state.heartbeat.dst == 0xc001);
    uint8_t invalid[] = {0, 0x80, 1, 1, 5, 0, 0, 0x23, 1};
    assert(config_message(OP_CONFIG_HEARTBEAT_PUB_SET, invalid, sizeof(invalid)));
    assert(last_params[0] == MESH_CONFIG_INVALID_ADDRESS);
    invalid[1] = 0xc0; invalid[2] = 0x12;
    assert(config_message(OP_CONFIG_HEARTBEAT_PUB_SET, invalid, sizeof(invalid)));
    assert(last_params[0] == MESH_CONFIG_CANNOT_SET);
    invalid[2] = 1; invalid[3] = 0x12;
    assert(config_message(OP_CONFIG_HEARTBEAT_PUB_SET, invalid, sizeof(invalid)));
    assert(last_params[0] == MESH_CONFIG_CANNOT_SET);
    invalid[3] = 1; invalid[4] = 0x80;
    assert(!config_message(OP_CONFIG_HEARTBEAT_PUB_SET, invalid, sizeof(invalid)));
    assert(!config_message(OP_CONFIG_HEARTBEAT_PUB_GET, invalid, 1));
    assert(!config_message(OP_CONFIG_HEARTBEAT_PUB_SET, invalid, 8));
    pub = (mesh_heartbeat_publication){0};
    assert(mesh_set_heartbeat_pub(0x1201, &pub) && config_request());
    assert(mesh_network.state.heartbeat.dst == 0 && mesh_network.heartbeat.remaining == 0);

    assert(mesh_get_heartbeat_sub(0x1201) && config_request() && last_len == 9);
    for (unsigned i = 0; i < last_len; i++) assert(last_params[i] == 0);
    now_ms = UINT32_MAX - 499;
    assert(mesh_set_heartbeat_sub(0x1201, 0x1202, 0xc001, 3) && config_request());
    assert(last_params[5] == 3 && last_params[6] == 0 && last_params[7] == 0x7f && last_params[8] == 0);
    mesh_net_message hb = {.ctl = 1, .src = 0x1202, .dst = 0xc001,
        .ttl = 3, .transport_len = 4, .transport = {0x0a, 5, 0, 1}};
    receive_heartbeat(&hb);
    assert(mesh_network.heartbeat.count == 1 && mesh_network.heartbeat.min_hops == 3);
    hb.ttl = 5; receive_heartbeat(&hb);
    hb.ttl = 1; receive_heartbeat(&hb);
    assert(mesh_network.heartbeat.count == 3 && mesh_network.heartbeat.min_hops == 1 && mesh_network.heartbeat.max_hops == 5);
    now_ms = 501;
    assert(mesh_get_heartbeat_sub(0x1201) && config_request());
    assert(last_params[5] == 2 && last_params[6] == 2 && last_params[7] == 1 && last_params[8] == 5);
    hb.src = 0x1203; receive_heartbeat(&hb); assert(mesh_network.heartbeat.count == 3);
    hb.src = 0x1202; hb.dst = 0xc002; receive_heartbeat(&hb); assert(mesh_network.heartbeat.count == 3);
    hb.dst = 0xc001; hb.ttl = 6; receive_heartbeat(&hb); assert(mesh_network.heartbeat.count == 3);
    hb.ttl = 1; mesh_network.heartbeat.count = 0xfffe;
    receive_heartbeat(&hb); receive_heartbeat(&hb); assert(mesh_network.heartbeat.count == 0xffff);
    assert(mesh_get_heartbeat_sub(0x1201) && config_request() && last_params[6] == 0xff);
    now_ms = 3500;
    receive_heartbeat(&hb); assert(mesh_network.heartbeat.count == 0xffff);
    assert(mesh_get_heartbeat_sub(0x1201) && config_request() && last_params[5] == 0);
    assert(mesh_set_heartbeat_sub(0x1201, 0x1202, 0xc001, 0) && config_request());
    assert(last_params[5] == 0 && last_params[6] == 0xff && last_params[7] == 0x7f);
    assert(mesh_network.heartbeat.count == 0 && mesh_network.heartbeat.src == 0x1202);
    assert(mesh_set_heartbeat_sub(0x1201, 0, 0xc001, 2) && config_request());
    assert(last_params[1] == 0 && last_params[3] == 0 && last_params[5] == 0 && last_params[7] == 0x7f);
    assert(mesh_get_heartbeat_sub(0x1201) && config_request() && last_params[7] == 0);
    assert(!mesh_set_heartbeat_sub(0x1201, 0xc001, 0xc001, 2));
    assert(!mesh_set_heartbeat_sub(0x1201, 0x1202, 0x1202, 2));
    uint8_t bad_sub[] = {2, 0x12, 2, 0x12, 2};
    assert(!config_message(OP_CONFIG_HEARTBEAT_SUB_SET, bad_sub, sizeof(bad_sub)));
    bad_sub[2] = 1; bad_sub[3] = 0xc0; bad_sub[4] = 0x12;
    assert(!config_message(OP_CONFIG_HEARTBEAT_SUB_SET, bad_sub, sizeof(bad_sub)));
    assert(!config_message(OP_CONFIG_HEARTBEAT_SUB_SET, NULL, 0));
    assert(mesh_set_heartbeat_sub(0x1201, 0x1202, 0x1201, 0x11) && config_request());
    assert(last_params[5] == 0x11);
    assert(mesh_network_restore() && mesh_network.heartbeat.src == 0);

    const uint32_t statuses[] = {OP_CONFIG_NODE_RESET_STATUS, OP_CONFIG_HEARTBEAT_PUB_STATUS,
        OP_CONFIG_HEARTBEAT_SUB_STATUS};
    mesh_access_pdu message = {.src = 0x1202, .dst = 0x1201,
        .app_key_index = APP_KEY_INDEX_NONE, .device_key_owner = 0x1202};
    for (unsigned i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        message.opcode = statuses[i];
        int reports = config_report_count;
        assert(poll_message(&message) && config_report_count == reports + 1);
        message.device_key_owner = 0x1201; assert(!poll_message(&message));
        message.device_key_owner = 0x1202;
    }
    message.opcode = OP_CONFIG_NODE_RESET;
    assert(!poll_message(&message) && reset_calls == 0);
    message.device_key_owner = 0x1201; message.app_key_index = 0x234;
    assert(!poll_message(&message) && reset_calls == 0);
    message.app_key_index = APP_KEY_INDEX_NONE; message.dst = 0x1202; message.device_key_owner = 0x1202;
    assert(!poll_message(&message) && reset_calls == 0);
    assert(!config_message(OP_CONFIG_NODE_RESET, bad_sub, 1));
    send_fail = 1; assert(!config_message(OP_CONFIG_NODE_RESET, NULL, 0) && reset_calls == 0);
    send_fail = 0; reset_fail = 1;
    assert(!config_message(OP_CONFIG_NODE_RESET, NULL, 0) && reset_calls == 0);
    reset_fail = 0;
    assert(mesh_reset_node(0x1201) && config_request());
    assert(last_opcode == OP_CONFIG_NODE_RESET_STATUS && last_len == 0 && last_app_key_index == DEVICE_KEY_LOCAL);
    assert(reset_calls == 1 && mesh_models.reset_pending);
    assert(!mesh_models_poll()); // Pause model traffic while the reply drains.
}

int main(void) {
    mesh_network.ready = 1;
    mesh_network.state.unicast_address = 0x1201;
    mesh_network.state.element_count = 2;
    mesh_network.state.net_key_index = 0x123;
    assert(mesh_models_init() == 1);

    uint8_t client_key[16] = {0x55};
    assert(mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 0) == 1);
    assert(last_opcode == OP_CONFIG_APPKEY_ADD && last_len == 19);
    assert(mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 1) == 1);
    assert(last_opcode == OP_CONFIG_APPKEY_UPDATE && last_len == 19);
    assert(mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 2) == 0);

    assert(mesh_model_binding(0x1202, 0x1202, 0x234, MESH_MODEL_ONOFF_SERVER, 1) == 1);
    assert(last_opcode == OP_CONFIG_MODEL_APP_BIND && last_len == 6);
    assert(mesh_model_binding(0x1202, 0x1202, 0x234, MESH_MODEL_ONOFF_SERVER, 0) == 1);
    assert(last_opcode == OP_CONFIG_MODEL_APP_UNBIND && last_len == 6);

    uint8_t add[19] = {0x23, 0x41, 0x23}; // NetKey 0x123, AppKey 0x234
    memset(add + 3, 0x55, 16);
    mesh_access_pdu message = {
        .src = 0x1202, .dst = 0x1201, .app_key_index = APP_KEY_INDEX_NONE,
        .device_key_owner = 0x1201,
        .ttl = 5, .opcode = OP_CONFIG_APPKEY_ADD,
        .params = add, .params_len = sizeof(add)
    };
    message.device_key_owner = message.src;
    assert(poll_message(&message) == 0); // Remote key cannot configure this node.
    assert(!mesh_network.state.app_keys[0].used);
    message.device_key_owner = message.dst;
    assert(poll_message(&message) == 1);
    assert(mesh_network.state.app_keys[0].used &&
           mesh_network.state.app_keys[0].index == 0x234);
    assert(last_opcode == OP_CONFIG_APPKEY_STATUS &&
           last_app_key_index == DEVICE_KEY_LOCAL &&
           last_params[0] == MESH_CONFIG_SUCCESS && last_dst == 0x1202);

    uint8_t second[19] = {0x23, 0x51, 0x23}; // AppKey 0x235
    memset(second + 3, 0x66, 16);
    message.params = second;
    assert(poll_message(&message) == 1);
    assert(mesh_network.state.app_keys[1].used &&
           mesh_network.state.app_keys[1].index == 0x235 &&
           last_params[0] == MESH_CONFIG_SUCCESS);
    uint8_t get_keys[2] = {0x23, 0x01};
    message.opcode = OP_CONFIG_APPKEY_GET;
    message.params = get_keys;
    message.params_len = sizeof(get_keys);
    assert(poll_message(&message) == 1);
    assert(last_opcode == OP_CONFIG_APPKEY_LIST && last_len == 6 &&
           last_params[0] == MESH_CONFIG_SUCCESS &&
           last_params[3] == 0x34 && last_params[4] == 0x52 &&
           last_params[5] == 0x23);
    message.params = add;

    uint8_t bind[6] = {0x01, 0x12, 0x34, 0x02, 0x00, 0x10};
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(poll_message(&message) == 1);
    assert(saved.onoff_server_bindings == 1 &&
           last_opcode == OP_CONFIG_MODEL_APP_STATUS &&
           last_params[0] == MESH_CONFIG_SUCCESS);

    uint8_t on[2] = {1, 7};
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    assert(poll_message(&message) == 1);
    assert(applied_on == 1 && apply_count == 1 &&
           last_opcode == OP_ONOFF_STATUS && last_params[0] == 1 &&
           last_app_key_index == 0x234);
    assert(poll_message(&message) == 1); // repeated TID
    assert(apply_count == 1);
    message.app_key_index = 0x235;
    assert(poll_message(&message) == 0); // unbound AppKey
    message.app_key_index = APP_KEY_INDEX_NONE;
    assert(poll_message(&message) == 0); // DevKey cannot set OnOff

    bind[4] = 0x01; // Generic OnOff Client
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(poll_message(&message) == 1);
    assert(saved.onoff_client_bindings == 1);
    assert(mesh_onoff_set(0x1201, 0x1202, 0x234, 0, 1) == 1);
    assert(last_opcode == OP_ONOFF_SET && last_len == 2 &&
           last_params[0] == 0 &&
           last_expected_response == OP_ONOFF_STATUS);
    uint8_t virtual_label[16] = {1};
    assert(mesh_onoff_get_virtual(0x1201, virtual_label, 0x234) == 1);
    assert(last_dst == 0x8001 && last_opcode == OP_ONOFF_GET);
    assert(mesh_onoff_set_virtual(0x1201, virtual_label, 0x234, 1, 0) == 1);
    assert(last_dst == 0x8001 && last_opcode == OP_ONOFF_SET_UNACK);

    uint8_t status = 1;
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_STATUS;
    message.params = &status;
    message.params_len = 1;
    assert(poll_message(&message) == 1);
    assert(report_count == 1 && reported_on == 1);

    bind[4] = 0x02;
    bind[5] = 0x00; // Health Server
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(poll_message(&message) == 1);
    uint8_t seconds = 2;
    message.app_key_index = 0x234;
    message.opcode = OP_HEALTH_ATTENTION_SET;
    message.params = &seconds;
    message.params_len = 1;
    assert(poll_message(&message) == 1);
    assert(attention_seconds == 2 &&
           last_opcode == OP_HEALTH_ATTENTION_STATUS);
    now_ms = 2000;
    mesh_models_poll();
    assert(attention_seconds == 0);

    polled_access = message;
    polled_access.opcode = OP_ONOFF_STATUS;
    polled_access.params = &status;
    polled_access.params_len = 1;
    poll_ready = 1;
    assert(mesh_models_poll() == 1);
    assert(report_count == 2);

    uint8_t label[16] = {1};
    uint8_t sub[20] = {0x01, 0x12};
    memcpy(sub + 2, label, 16);
    sub[18] = 0x00;
    sub[19] = 0x10; // Generic OnOff Server
    message.dst = 0x1201;
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_SUB_VIRTUAL_ADD;
    message.params = sub;
    message.params_len = sizeof(sub);
    assert(poll_message(&message) == 1);
    assert(saved.virtual_count == 1 && registered_labels == 1 &&
           last_opcode == OP_CONFIG_MODEL_SUB_STATUS &&
           last_params[0] == MESH_CONFIG_SUCCESS &&
           last_params[3] == 0x01 && last_params[4] == 0x80);

    message.dst = 0x8001;
    message.has_label = 1;
    memcpy(message.label, label, 16);
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 8;
    assert(poll_message(&message) == 1);
    assert(apply_count == 2);
    message.label[0] = 2;
    message.dst = 0x8002;
    on[1] = 9;
    assert(poll_message(&message) == 0);
    assert(apply_count == 2);

    message.dst = 0x1201;
    message.has_label = 0;
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE;
    message.params = sub;
    message.params_len = sizeof(sub);
    assert(poll_message(&message) == 1);
    assert(saved.virtual_count == 0 && registered_labels == 0);

    // Bind the second key independently, then remove its permission.
    uint8_t bind_second[6] = {0x01, 0x12, 0x35, 0x02, 0x00, 0x10};
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind_second;
    message.params_len = sizeof(bind_second);
    assert(poll_message(&message) == 1);
    assert(saved.onoff_server_bindings == 3);
    message.app_key_index = 0x235;
    message.opcode = OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 10;
    assert(poll_message(&message) == 1);
    assert(apply_count == 3 && last_app_key_index == 0x235);
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_APP_UNBIND;
    message.params = bind_second;
    message.params_len = sizeof(bind_second);
    assert(poll_message(&message) == 1);
    assert(saved.onoff_server_bindings == 1);
    message.app_key_index = 0x235;
    message.opcode = OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 11;
    assert(poll_message(&message) == 0);

    bind_second[4] = 0x01; // Bind the second key to the OnOff Client.
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind_second;
    message.params_len = sizeof(bind_second);
    assert(poll_message(&message) == 1);
    assert(saved.onoff_client_bindings == 3);
    assert(mesh_onoff_get(0x1201, 0x1202, 0x235) == 1);
    assert(last_app_key_index == 0x235);
    assert(mesh_onoff_get(0x1201, 0x1202, 0x236) == 0);
    uint8_t get_model_keys[4] = {0x01, 0x12, 0x01, 0x10};
    message.opcode = OP_CONFIG_SIG_MODEL_APP_GET;
    message.params = get_model_keys;
    message.params_len = sizeof(get_model_keys);
    assert(poll_message(&message) == 1);
    assert(last_opcode == OP_CONFIG_SIG_MODEL_APP_LIST && last_len == 8 &&
           last_params[0] == MESH_CONFIG_SUCCESS &&
           last_params[5] == 0x34 && last_params[6] == 0x52 &&
           last_params[7] == 0x23);

    uint8_t remove[3] = {0x23, 0x51, 0x23};
    message.opcode = OP_CONFIG_APPKEY_DELETE;
    message.params = remove;
    message.params_len = sizeof(remove);
    assert(poll_message(&message) == 1);
    assert(!mesh_network.state.app_keys[1].used &&
           saved.onoff_client_bindings == 1 &&
           last_params[0] == MESH_CONFIG_SUCCESS);
    assert(mesh_onoff_get(0x1201, 0x1202, 0x235) == 0);

    // Reusing the slot must not restore the deleted key's model bindings.
    message.opcode = OP_CONFIG_APPKEY_ADD;
    message.params = second;
    message.params_len = sizeof(second);
    assert(poll_message(&message) == 1);
    assert(mesh_network.state.app_keys[1].used &&
           saved.onoff_client_bindings == 1);
    assert(mesh_onoff_get(0x1201, 0x1202, 0x235) == 0);

    // AppKey Update is accepted for an existing key during refresh phase 1.
    uint8_t update[19] = {0x23, 0x51, 0x23};
    memset(update + 3, 0xaa, 16);
    message.opcode = OP_CONFIG_APPKEY_UPDATE;
    message.params = update;
    message.params_len = sizeof(update);
    assert(poll_message(&message) == 1);
    assert(last_params[0] == MESH_CONFIG_CANNOT_UPDATE);
    mesh_network.state.key_refresh_phase = 1;
    assert(poll_message(&message) == 1);
    assert(last_params[0] == MESH_CONFIG_SUCCESS &&
           mesh_network.state.app_keys[1].has_new_key &&
           memcmp(mesh_network.state.app_keys[1].new_key, update + 3, 16) == 0);

    uint8_t extra[19] = {0x23, 0x61, 0x23};
    memset(extra + 3, 0x77, 16);
    message.opcode = OP_CONFIG_APPKEY_ADD;
    message.params = extra;
    assert(poll_message(&message) == 1);
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    extra[1] = 0x71;
    extra[3] = 0x88;
    assert(poll_message(&message) == 1);
    assert(last_params[0] == MESH_CONFIG_SUCCESS);
    extra[1] = 0x81;
    extra[3] = 0x99;
    assert(poll_message(&message) == 1);
    assert(last_params[0] == MESH_CONFIG_INSUFFICIENT_RESOURCES);

    // The second element has its own binding and state.
    uint8_t bind_element2[6] = {0x02, 0x12, 0x34, 0x02, 0x00, 0x10};
    message.opcode = OP_CONFIG_MODEL_APP_BIND;
    message.params = bind_element2;
    message.params_len = sizeof(bind_element2);
    assert(poll_message(&message) == 1);
    assert(saved.other[0].onoff_server_bindings == 1);
    bind_element2[4] = 0x01; // Client model on the second element.
    assert(poll_message(&message) == 1);
    assert(mesh_onoff_set(0x1202, 0x1300, 0x234, 1, 0) == 1);
    assert(last_src == 0x1202 && last_opcode == OP_ONOFF_SET_UNACK);
    bind_element2[4] = 0x00;
    message.dst = 0x1202;
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 12;
    assert(poll_message(&message) == 1);
    assert(applied_element == 0x1202 && last_src == 0x1202);

    // One group message reaches both subscribed element instances.
    uint8_t group[6] = {0x01, 0x12, 0x01, 0xc0, 0x00, 0x10};
    message.dst = 0x1201;
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_SUB_ADD;
    message.params = group;
    message.params_len = sizeof(group);
    assert(poll_message(&message) == 1);
    group[0] = 0x02;
    assert(poll_message(&message) == 1);
    assert(saved.group_count == 2);
    int prior_count = apply_count;
    message.dst = 0xc001;
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_SET_UNACK;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 13;
    assert(poll_message(&message) == 1);
    assert(apply_count == prior_count + 2);

    // Removing one subscription leaves the other element subscribed.
    message.dst = 0x1201;
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_MODEL_SUB_DELETE;
    message.params = group;
    message.params_len = sizeof(group);
    assert(poll_message(&message) == 1);
    assert(saved.group_count == 1);
    message.dst = 0xc001;
    message.app_key_index = 0x234;
    message.opcode = OP_ONOFF_SET_UNACK;
    message.params = on;
    message.params_len = sizeof(on);
    on[1] = 14;
    prior_count = apply_count;
    assert(poll_message(&message) == 1);
    assert(apply_count == prior_count + 1 && applied_element == 0x1201);

    message.dst = 0x1201;
    message.app_key_index = APP_KEY_INDEX_NONE;
    message.opcode = OP_CONFIG_APPKEY_STATUS;
    message.params = last_params;
    message.params_len = 4;
    message.device_key_owner = message.dst;
    assert(poll_message(&message) == 0); // Client replies cannot use our key.
    assert(config_report_count == 0);
    message.device_key_owner = message.src;
    assert(poll_message(&message) == 1 && config_report_count == 1);
    message.app_key_index = 0x234;
    assert(poll_message(&message) == 0); // AppKey cannot authorize configuration.
    test_foundation_configuration();
    test_key_configuration();
    test_node_settings();
    test_health();
    test_health_client();
    test_heartbeat_configuration();
    return 0;
}
