#ifndef ISLER_BLE_MESH_MODELS_H
#define ISLER_BLE_MESH_MODELS_H

#include "ble_mesh_access.h"

#define MESH_MODEL_CONFIG_SERVER 0x0000
#define MESH_MODEL_HEALTH_SERVER 0x0002
#define MESH_MODEL_ONOFF_SERVER 0x1000
#define MESH_MODEL_ONOFF_CLIENT 0x1001
#define MESH_MODELS_DEFAULT_TTL 5

#define MESH_OP_CONFIG_APPKEY_ADD 0x00
#define MESH_OP_CONFIG_APPKEY_STATUS 0x8003
#define MESH_OP_CONFIG_MODEL_APP_BIND 0x803d
#define MESH_OP_CONFIG_MODEL_APP_STATUS 0x803e
#define MESH_OP_HEALTH_ATTENTION_GET 0x8004
#define MESH_OP_HEALTH_ATTENTION_SET 0x8005
#define MESH_OP_HEALTH_ATTENTION_SET_UNACK 0x8006
#define MESH_OP_HEALTH_ATTENTION_STATUS 0x8007
#define MESH_OP_ONOFF_GET 0x8201
#define MESH_OP_ONOFF_SET 0x8202
#define MESH_OP_ONOFF_SET_UNACK 0x8203
#define MESH_OP_ONOFF_STATUS 0x8204

#define MESH_CONFIG_SUCCESS 0x00
#define MESH_CONFIG_INVALID_ADDRESS 0x01
#define MESH_CONFIG_INVALID_MODEL 0x02
#define MESH_CONFIG_INVALID_APPKEY 0x03
#define MESH_CONFIG_INVALID_NETKEY 0x04
#define MESH_CONFIG_INSUFFICIENT_RESOURCES 0x05
#define MESH_CONFIG_KEY_ALREADY_STORED 0x06
#define MESH_CONFIG_STORAGE_FAILURE 0x09

// This implementation has one element and one AppKey. Bindings survive reboot.
typedef struct {
    uint8_t onoff_server_bound;
    uint8_t onoff_client_bound;
    uint8_t health_server_bound;
} mesh_models_state;

int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state);
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state);
void BLE_MESH_ONOFF_CHANGED(uint8_t on);
void BLE_MESH_ONOFF_STATUS(uint16_t src, uint8_t present);
void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len);
void BLE_MESH_HEALTH_ATTENTION(uint8_t seconds);

static struct {
    mesh_models_state state;
    uint8_t ready, onoff, attention, last_tid, has_tid, client_tid;
    uint16_t last_src, last_dst;
    uint32_t last_set_ms, attention_started_ms;
} mesh_models;

static inline int ble_mesh_models_init(void) {
    if (!mesh_network.ready ||
        BLE_MESH_MODELS_LOAD_STATE(&mesh_models.state) != 1 ||
        mesh_models.state.onoff_server_bound > 1 ||
        mesh_models.state.onoff_client_bound > 1 ||
        mesh_models.state.health_server_bound > 1) return 0;
    mesh_models.ready = 1;
    return 1;
}

static int mesh_model_key_matches(const mesh_access_pdu *message,
                                  uint8_t bound) {
    return bound && mesh_network.state.has_app_key &&
           message->app_key_index == mesh_network.state.app_key_index;
}

static int mesh_model_reply(const mesh_access_pdu *message, uint32_t opcode,
                            const uint8_t *params, size_t len,
                            uint8_t use_device_key) {
    return ble_mesh_access_send(message->src, MESH_MODELS_DEFAULT_TTL,
                                use_device_key,
                                opcode, params, len);
}

static uint8_t mesh_health_attention_remaining(void) {
    if (!mesh_models.attention) return 0;
    uint32_t elapsed = (uint32_t)(GET_MILLIS() - mesh_models.attention_started_ms);
    uint32_t total = (uint32_t)mesh_models.attention * 1000;
    if (elapsed >= total) {
        mesh_models.attention = 0;
        BLE_MESH_HEALTH_ATTENTION(0);
        return 0;
    }
    return (uint8_t)((total - elapsed + 999) / 1000);
}

// Config Server: the messages needed to install one AppKey and bind it to one
// of the three models in this file. Other Config messages are not handled yet.
static int mesh_config_server_receive(const mesh_access_pdu *message) {
    const uint8_t *p = message->params;
    size_t len = message->params_len;
    if (message->opcode == MESH_OP_CONFIG_APPKEY_ADD) {
        if (len != 19) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)(p[1] & 0x0f) << 8);
        uint16_t app_idx = (p[1] >> 4) | ((uint16_t)p[2] << 4);
        uint8_t status = MESH_CONFIG_SUCCESS;

        if (net_idx != mesh_network.state.net_key_index)
            status = MESH_CONFIG_INVALID_NETKEY;
        else if (mesh_network.state.has_app_key) {
            if (mesh_network.state.app_key_index != app_idx)
                status = MESH_CONFIG_INSUFFICIENT_RESOURCES;
            else if (memcmp(mesh_network.state.app_key, p + 3, 16) != 0)
                status = MESH_CONFIG_KEY_ALREADY_STORED;
        } else {
            mesh_net_state next = mesh_network.state;
            next.app_key_index = app_idx;
            memcpy(next.app_key, p + 3, 16);
            next.has_app_key = 1;
            if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
        }

        uint8_t reply[4] = {status, p[0], p[1], p[2]};
        mesh_model_reply(message, MESH_OP_CONFIG_APPKEY_STATUS, reply, 4, 1);
        return 1;
    }

    if (message->opcode == MESH_OP_CONFIG_MODEL_APP_BIND) {
        if (len != 6) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t app_idx = p[2] | ((uint16_t)p[3] << 8);
        uint16_t model = p[4] | ((uint16_t)p[5] << 8);
        uint8_t status = MESH_CONFIG_SUCCESS;
        mesh_models_state next = mesh_models.state;

        if (element != mesh_network.state.unicast_address)
            status = MESH_CONFIG_INVALID_ADDRESS;
        else if (app_idx > 0x0fff || !mesh_network.state.has_app_key ||
                 app_idx != mesh_network.state.app_key_index)
            status = MESH_CONFIG_INVALID_APPKEY;
        else if (model == MESH_MODEL_ONOFF_SERVER)
            next.onoff_server_bound = 1;
        else if (model == MESH_MODEL_ONOFF_CLIENT)
            next.onoff_client_bound = 1;
        else if (model == MESH_MODEL_HEALTH_SERVER)
            next.health_server_bound = 1;
        else
            status = MESH_CONFIG_INVALID_MODEL;

        if (status == MESH_CONFIG_SUCCESS &&
            memcmp(&next, &mesh_models.state, sizeof(next)) != 0 &&
            BLE_MESH_MODELS_SAVE_STATE(&next) != 1)
            status = MESH_CONFIG_STORAGE_FAILURE;
        if (status == MESH_CONFIG_SUCCESS) mesh_models.state = next;

        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        mesh_model_reply(message, MESH_OP_CONFIG_MODEL_APP_STATUS, reply, 7, 1);
        return 1;
    }
    return 0;
}

// Config Client helpers for a provisioner configuring another node.
static inline int ble_mesh_config_add_app_key(uint16_t dst, uint16_t net_idx,
                                               uint16_t app_idx,
                                               const uint8_t key[16]) {
    if (!key || net_idx > 0x0fff || app_idx > 0x0fff) return 0;
    uint8_t params[19] = {
        (uint8_t)net_idx,
        (uint8_t)((net_idx >> 8) | (app_idx << 4)),
        (uint8_t)(app_idx >> 4)
    };
    memcpy(params + 3, key, 16);
    return ble_mesh_access_send(dst, MESH_MODELS_DEFAULT_TTL, 1,
                                MESH_OP_CONFIG_APPKEY_ADD,
                                params, sizeof(params));
}

static inline int ble_mesh_config_bind_model(uint16_t dst, uint16_t element,
                                              uint16_t app_idx, uint16_t model) {
    if (!element || element > 0x7fff || app_idx > 0x0fff) return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)app_idx, (uint8_t)(app_idx >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return ble_mesh_access_send(dst, MESH_MODELS_DEFAULT_TTL, 1,
                                MESH_OP_CONFIG_MODEL_APP_BIND,
                                params, sizeof(params));
}

// Health Server: attention support for a node with no reported faults.
static int mesh_health_server_receive(const mesh_access_pdu *message) {
    if (message->opcode == MESH_OP_HEALTH_ATTENTION_GET) {
        if (message->params_len != 0) return 0;
    } else if (message->opcode == MESH_OP_HEALTH_ATTENTION_SET ||
               message->opcode == MESH_OP_HEALTH_ATTENTION_SET_UNACK) {
        if (message->params_len != 1) return 0;
        mesh_models.attention = message->params[0];
        mesh_models.attention_started_ms = GET_MILLIS();
        BLE_MESH_HEALTH_ATTENTION(mesh_models.attention);
        if (message->opcode == MESH_OP_HEALTH_ATTENTION_SET_UNACK) return 1;
    } else return 0;

    uint8_t remaining = mesh_health_attention_remaining();
    return mesh_model_reply(message, MESH_OP_HEALTH_ATTENTION_STATUS,
                            &remaining, 1, 0);
}

static inline int ble_mesh_onoff_get(uint16_t dst) {
    if (!mesh_models.ready || !mesh_models.state.onoff_client_bound) return 0;
    return ble_mesh_access_send(dst, MESH_MODELS_DEFAULT_TTL, 0,
                                MESH_OP_ONOFF_GET, NULL, 0);
}

static inline int ble_mesh_onoff_set(uint16_t dst, uint8_t on,
                                     uint8_t acknowledged) {
    if (!mesh_models.ready || !mesh_models.state.onoff_client_bound || on > 1)
        return 0;
    uint8_t params[2] = {on, mesh_models.client_tid++};
    return ble_mesh_access_send(dst, MESH_MODELS_DEFAULT_TTL, 0,
                                acknowledged ? MESH_OP_ONOFF_SET :
                                               MESH_OP_ONOFF_SET_UNACK,
                                params, sizeof(params));
}

static int mesh_onoff_server_receive(const mesh_access_pdu *message) {
    if (message->opcode == MESH_OP_ONOFF_GET) {
        if (message->params_len != 0) return 0;
    } else if (message->opcode == MESH_OP_ONOFF_SET ||
               message->opcode == MESH_OP_ONOFF_SET_UNACK) {
        if (message->params_len != 2 || message->params[0] > 1) return 0;
        uint32_t now = GET_MILLIS();
        if (!mesh_models.has_tid ||
            mesh_models.last_src != message->src ||
            mesh_models.last_dst != message->dst ||
            mesh_models.last_tid != message->params[1] ||
            (uint32_t)(now - mesh_models.last_set_ms) >= 6000) {
            mesh_models.onoff = message->params[0];
            BLE_MESH_ONOFF_CHANGED(mesh_models.onoff);
            mesh_models.last_src = message->src;
            mesh_models.last_dst = message->dst;
            mesh_models.last_tid = message->params[1];
            mesh_models.last_set_ms = now;
            mesh_models.has_tid = 1;
        }
        if (message->opcode == MESH_OP_ONOFF_SET_UNACK) return 1;
    } else return 0;

    uint8_t present = mesh_models.onoff;
    return mesh_model_reply(message, MESH_OP_ONOFF_STATUS, &present, 1, 0);
}

// Dispatch by opcode. This one-element version accepts only local unicast
// destinations. AppKey messages must be bound to the receiving model.
static inline int ble_mesh_models_receive(const mesh_access_pdu *message) {
    if (!message || !mesh_models.ready ||
        message->dst != mesh_network.state.unicast_address) return 0;

    if (message->app_key_index == 0xffff) {
        if (mesh_config_server_receive(message)) return 1;
        if (message->opcode == MESH_OP_CONFIG_APPKEY_STATUS ||
            message->opcode == MESH_OP_CONFIG_MODEL_APP_STATUS) {
            BLE_MESH_CONFIG_STATUS(message->src, message->opcode,
                                   message->params, message->params_len);
            return 1;
        }
        return 0;
    }

    if (message->opcode == MESH_OP_ONOFF_GET ||
        message->opcode == MESH_OP_ONOFF_SET ||
        message->opcode == MESH_OP_ONOFF_SET_UNACK) {
        if (!mesh_model_key_matches(message,
                                    mesh_models.state.onoff_server_bound)) return 0;
        return mesh_onoff_server_receive(message);
    }
    if (message->opcode == MESH_OP_ONOFF_STATUS) {
        if (!mesh_model_key_matches(message,
                                    mesh_models.state.onoff_client_bound) ||
            message->params_len != 1 || message->params[0] > 1) return 0;
        BLE_MESH_ONOFF_STATUS(message->src, message->params[0]);
        return 1;
    }
    if (message->opcode == MESH_OP_HEALTH_ATTENTION_GET ||
        message->opcode == MESH_OP_HEALTH_ATTENTION_SET ||
        message->opcode == MESH_OP_HEALTH_ATTENTION_SET_UNACK) {
        if (!mesh_model_key_matches(message,
                                    mesh_models.state.health_server_bound)) return 0;
        return mesh_health_server_receive(message);
    }
    return 0;
}

static inline void ble_mesh_models_tick(void) {
    if (mesh_models.ready) mesh_health_attention_remaining();
}

static inline int ble_mesh_models_poll(void) {
    ble_mesh_models_tick();
    return ble_mesh_access_poll();
}

#endif
