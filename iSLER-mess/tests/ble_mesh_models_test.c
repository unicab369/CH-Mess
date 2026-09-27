#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ISLER_BLE_MESH_ACCESS_H
typedef struct {
    uint16_t src, dst, app_key_index;
    uint8_t ttl;
    uint32_t opcode;
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

typedef struct {
    uint16_t net_key_index, app_key_index, unicast_address;
    uint8_t app_key[16], has_app_key;
} mesh_net_state;

static struct {
    mesh_net_state state;
    uint8_t ready;
} mesh_network;

static uint32_t now_ms;
static uint32_t last_opcode;
static uint16_t last_dst;
static uint8_t last_params[32], last_device_key;
static size_t last_len;
static uint8_t applied_on, attention_seconds, reported_on;
static int apply_count, report_count;

static uint32_t GET_MILLIS(void) { return now_ms; }
static int mesh_commit(const mesh_net_state *state) {
    mesh_network.state = *state;
    return 1;
}
static int ble_mesh_access_send(uint16_t dst, uint8_t ttl,
                                uint8_t use_device_key, uint32_t opcode,
                                const uint8_t *params, size_t len) {
    (void)ttl;
    assert(len <= sizeof(last_params));
    last_dst = dst;
    last_opcode = opcode;
    last_device_key = use_device_key;
    last_len = len;
    if (len) memcpy(last_params, params, len);
    return 1;
}
static int ble_mesh_access_poll(void) { return 0; }

#include "../ble_mesh_models.h"

static mesh_models_state saved;
int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state) {
    *state = saved;
    return 1;
}
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state) {
    saved = *state;
    return 1;
}
void BLE_MESH_ONOFF_CHANGED(uint8_t on) {
    applied_on = on;
    apply_count++;
}
void BLE_MESH_ONOFF_STATUS(uint16_t src, uint8_t present) {
    assert(src == 0x1202);
    reported_on = present;
    report_count++;
}
void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len) {
    (void)src; (void)opcode; (void)params; (void)len;
}
void BLE_MESH_HEALTH_ATTENTION(uint8_t seconds) {
    attention_seconds = seconds;
}

int main(void) {
    mesh_network.ready = 1;
    mesh_network.state.unicast_address = 0x1201;
    mesh_network.state.net_key_index = 0x123;
    assert(ble_mesh_models_init() == 1);

    uint8_t add[19] = {0x23, 0x41, 0x23}; // NetKey 0x123, AppKey 0x234
    memset(add + 3, 0x55, 16);
    mesh_access_pdu message = {
        .src = 0x1202, .dst = 0x1201, .app_key_index = 0xffff,
        .ttl = 5, .opcode = MESH_OP_CONFIG_APPKEY_ADD,
        .params = add, .params_len = sizeof(add)
    };
    assert(ble_mesh_models_receive(&message) == 1);
    assert(mesh_network.state.has_app_key &&
           mesh_network.state.app_key_index == 0x234);
    assert(last_opcode == MESH_OP_CONFIG_APPKEY_STATUS && last_device_key &&
           last_params[0] == MESH_CONFIG_SUCCESS && last_dst == 0x1202);

    uint8_t bind[6] = {0x01, 0x12, 0x34, 0x02, 0x00, 0x10};
    message.opcode = MESH_OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(ble_mesh_models_receive(&message) == 1);
    assert(saved.onoff_server_bound &&
           last_opcode == MESH_OP_CONFIG_MODEL_APP_STATUS &&
           last_params[0] == MESH_CONFIG_SUCCESS);

    uint8_t on[2] = {1, 7};
    message.app_key_index = 0x234;
    message.opcode = MESH_OP_ONOFF_SET;
    message.params = on;
    message.params_len = sizeof(on);
    assert(ble_mesh_models_receive(&message) == 1);
    assert(applied_on == 1 && apply_count == 1 &&
           last_opcode == MESH_OP_ONOFF_STATUS && last_params[0] == 1 &&
           !last_device_key);
    assert(ble_mesh_models_receive(&message) == 1); // repeated TID
    assert(apply_count == 1);
    message.app_key_index = 0x235;
    assert(ble_mesh_models_receive(&message) == 0); // unbound AppKey
    message.app_key_index = 0xffff;
    assert(ble_mesh_models_receive(&message) == 0); // DevKey cannot set OnOff

    bind[4] = 0x01; // Generic OnOff Client
    message.opcode = MESH_OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(ble_mesh_models_receive(&message) == 1);
    assert(saved.onoff_client_bound);
    assert(ble_mesh_onoff_set(0x1202, 0, 1) == 1);
    assert(last_opcode == MESH_OP_ONOFF_SET && last_len == 2 &&
           last_params[0] == 0);

    uint8_t status = 1;
    message.app_key_index = 0x234;
    message.opcode = MESH_OP_ONOFF_STATUS;
    message.params = &status;
    message.params_len = 1;
    assert(ble_mesh_models_receive(&message) == 1);
    assert(report_count == 1 && reported_on == 1);

    bind[4] = 0x02;
    bind[5] = 0x00; // Health Server
    message.app_key_index = 0xffff;
    message.opcode = MESH_OP_CONFIG_MODEL_APP_BIND;
    message.params = bind;
    message.params_len = sizeof(bind);
    assert(ble_mesh_models_receive(&message) == 1);
    uint8_t seconds = 2;
    message.app_key_index = 0x234;
    message.opcode = MESH_OP_HEALTH_ATTENTION_SET;
    message.params = &seconds;
    message.params_len = 1;
    assert(ble_mesh_models_receive(&message) == 1);
    assert(attention_seconds == 2 &&
           last_opcode == MESH_OP_HEALTH_ATTENTION_STATUS);
    now_ms = 2000;
    ble_mesh_models_tick();
    assert(attention_seconds == 0);
    return 0;
}
