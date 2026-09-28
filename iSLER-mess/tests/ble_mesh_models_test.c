#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ISLER_BLE_MESH_ACCESS_H
#define APP_KEY_INDEX_NONE 0xffff
typedef struct {
    uint16_t src, dst, app_key_index;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint32_t opcode;
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

typedef struct {
    uint16_t src, dst, app_key_index, len;
    uint8_t ttl;
    uint8_t data[380];
} mesh_access_message;

#define MESH_MAX_APP_KEYS 4
#define MESH_MAX_ELEMENTS 2
typedef struct {
    uint8_t key[16], new_key[16];
    uint16_t index;
    uint8_t used, has_new_key;
} mesh_app_key;
typedef struct {
    uint16_t net_key_index, unicast_address;
    uint8_t element_count;
    uint8_t key_refresh_phase;
    mesh_app_key app_keys[MESH_MAX_APP_KEYS];
} mesh_net_state;

static int mesh_app_key_slot(const mesh_net_state *state, uint16_t index) {
    for (int i = 0; i < MESH_MAX_APP_KEYS; i++)
        if (state->app_keys[i].used && state->app_keys[i].index == index) return i;
    return -1;
}

static struct {
    mesh_net_state state;
    uint8_t ready;
} mesh_network;
static int mesh_local_element(uint16_t address) {
    return address >= mesh_network.state.unicast_address &&
           address - mesh_network.state.unicast_address <
               mesh_network.state.element_count;
}

#define MESH_TRANSPORT_MAX_LABELS 4
static uint8_t registered_labels;
static uint16_t ble_mesh_virtual_address(const uint8_t label[16]) {
    return (uint16_t)(0x8000 | label[0]);
}
static void ble_mesh_transport_clear_labels(void) { registered_labels = 0; }
static int ble_mesh_label_add(const uint8_t label[16]) {
    (void)label;
    registered_labels++;
    return 1;
}

static uint32_t now_ms;
static uint32_t last_opcode;
static uint16_t last_dst;
static uint16_t last_src;
static uint8_t last_params[32];
static uint16_t last_app_key_index;
static size_t last_len;
static uint8_t applied_on, attention_seconds, reported_on;
static uint16_t applied_element, reported_element;
static int apply_count, report_count;
static mesh_access_pdu polled_access;
static int poll_ready;

static uint32_t GET_MILLIS(void) { return now_ms; }
static int mesh_commit(const mesh_net_state *state) {
    mesh_network.state = *state;
    return 1;
}
static int ble_mesh_stage_app_key(uint16_t index, const uint8_t key[16]) {
    int slot = mesh_app_key_slot(&mesh_network.state, index);
    if (slot < 0 || mesh_network.state.key_refresh_phase != 1) return 0;
    memcpy(mesh_network.state.app_keys[slot].new_key, key, 16);
    mesh_network.state.app_keys[slot].has_new_key = 1;
    return 1;
}
static int ble_mesh_access_queue(uint16_t src, uint16_t dst, uint8_t ttl,
                                uint16_t app_key_index, uint32_t opcode,
                                const uint8_t *params, size_t len,
                                uint8_t mic_64) {
    last_src = src;
    (void)ttl;
    assert(mic_64 == 0);
    assert(len <= sizeof(last_params));
    last_dst = dst;
    last_opcode = opcode;
    last_app_key_index = app_key_index;
    last_len = len;
    if (len) memcpy(last_params, params, len);
    return 1;
}
static int ble_mesh_access_queue_virtual(uint16_t src,
                                              const uint8_t label[16],
                                              uint8_t ttl, uint16_t app_key_index,
                                              uint32_t opcode,
                                              const uint8_t *params, size_t len,
                                              uint8_t mic_64) {
    last_src = src;
    return ble_mesh_access_queue(src, ble_mesh_virtual_address(label), ttl,
                                 app_key_index, opcode, params, len, mic_64);
}
static int ble_mesh_access_poll(mesh_access_message *message,
                                mesh_access_pdu *access) {
    (void)message;
    if (!poll_ready) return 0;
    *access = polled_access;
    poll_ready = 0;
    return 1;
}

#include "../ble_mesh_4models.h"

static mesh_models_state saved;
int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state) {
    *state = saved;
    return 1;
}
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state) {
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
}
void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds) {
    (void)element;
    attention_seconds = seconds;
}

static int poll_message(const mesh_access_pdu *message) {
    polled_access = *message;
    poll_ready = 1;
    return ble_mesh_models_poll();
}

int main(void) {
    mesh_network.ready = 1;
    mesh_network.state.unicast_address = 0x1201;
    mesh_network.state.element_count = 2;
    mesh_network.state.net_key_index = 0x123;
    assert(ble_mesh_models_init() == 1);

    uint8_t client_key[16] = {0x55};
    assert(ble_mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 0) == 1);
    assert(last_opcode == OP_CONFIG_APPKEY_ADD && last_len == 19);
    assert(ble_mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 1) == 1);
    assert(last_opcode == OP_CONFIG_APPKEY_UPDATE && last_len == 19);
    assert(ble_mesh_add_or_update_app_key(0x1202, 0x123, 0x234, client_key, 2) == 0);

    uint8_t add[19] = {0x23, 0x41, 0x23}; // NetKey 0x123, AppKey 0x234
    memset(add + 3, 0x55, 16);
    mesh_access_pdu message = {
        .src = 0x1202, .dst = 0x1201, .app_key_index = APP_KEY_INDEX_NONE,
        .ttl = 5, .opcode = OP_CONFIG_APPKEY_ADD,
        .params = add, .params_len = sizeof(add)
    };
    assert(poll_message(&message) == 1);
    assert(mesh_network.state.app_keys[0].used &&
           mesh_network.state.app_keys[0].index == 0x234);
    assert(last_opcode == OP_CONFIG_APPKEY_STATUS &&
           last_app_key_index == APP_KEY_INDEX_NONE &&
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
    assert(ble_mesh_onoff_set(0x1201, 0x1202, 0x234, 0, 1) == 1);
    assert(last_opcode == OP_ONOFF_SET && last_len == 2 &&
           last_params[0] == 0);
    uint8_t virtual_label[16] = {1};
    assert(ble_mesh_onoff_get_virtual(0x1201, virtual_label, 0x234) == 1);
    assert(last_dst == 0x8001 && last_opcode == OP_ONOFF_GET);
    assert(ble_mesh_onoff_set_virtual(0x1201, virtual_label, 0x234, 1, 0) == 1);
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
    ble_mesh_models_poll();
    assert(attention_seconds == 0);

    polled_access = message;
    polled_access.opcode = OP_ONOFF_STATUS;
    polled_access.params = &status;
    polled_access.params_len = 1;
    poll_ready = 1;
    assert(ble_mesh_models_poll() == 1);
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
    assert(ble_mesh_onoff_get(0x1201, 0x1202, 0x235) == 1);
    assert(last_app_key_index == 0x235);
    assert(ble_mesh_onoff_get(0x1201, 0x1202, 0x236) == 0);
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
    assert(ble_mesh_onoff_get(0x1201, 0x1202, 0x235) == 0);

    // Reusing the slot must not restore the deleted key's model bindings.
    message.opcode = OP_CONFIG_APPKEY_ADD;
    message.params = second;
    message.params_len = sizeof(second);
    assert(poll_message(&message) == 1);
    assert(mesh_network.state.app_keys[1].used &&
           saved.onoff_client_bindings == 1);
    assert(ble_mesh_onoff_get(0x1201, 0x1202, 0x235) == 0);

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
    assert(ble_mesh_onoff_set(0x1202, 0x1300, 0x234, 1, 0) == 1);
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
    return 0;
}
