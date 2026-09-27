#ifndef ISLER_BLE_MESH_MODELS_H
#define ISLER_BLE_MESH_MODELS_H

#include "ble_mesh_3access.h"

#define MESH_MODEL_CONFIG_SERVER 0x0000
#define MESH_MODEL_HEALTH_SERVER 0x0002
#define MESH_MODEL_ONOFF_SERVER 0x1000
#define MESH_MODEL_ONOFF_CLIENT 0x1001
#define MODEL_TTL 5

#define OP_CONFIG_APPKEY_ADD 0x00
#define OP_CONFIG_APPKEY_STATUS 0x8003
#define OP_CONFIG_MODEL_APP_BIND 0x803d
#define OP_CONFIG_MODEL_APP_STATUS 0x803e
#define OP_CONFIG_MODEL_SUB_VIRTUAL_ADD 0x8020
#define OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE 0x8021
#define OP_CONFIG_MODEL_SUB_STATUS 0x801f
#define OP_HEALTH_ATTENTION_GET 0x8004
#define OP_HEALTH_ATTENTION_SET 0x8005
#define OP_HEALTH_ATTENTION_SET_UNACK 0x8006
#define OP_HEALTH_ATTENTION_STATUS 0x8007
#define OP_ONOFF_GET 0x8201
#define OP_ONOFF_SET 0x8202
#define OP_ONOFF_SET_UNACK 0x8203
#define OP_ONOFF_STATUS 0x8204

#define MESH_CONFIG_SUCCESS 0x00
#define MESH_CONFIG_INVALID_ADDRESS 0x01
#define MESH_CONFIG_INVALID_MODEL 0x02
#define MESH_CONFIG_INVALID_APPKEY 0x03
#define MESH_CONFIG_INVALID_NETKEY 0x04
#define MESH_CONFIG_INSUFFICIENT_RESOURCES 0x05
#define MESH_CONFIG_KEY_ALREADY_STORED 0x06
#define MESH_CONFIG_STORAGE_FAILURE 0x09
#define MESH_MODEL_VIRTUAL_SLOTS MESH_TRANSPORT_MAX_LABELS

// This implementation has one element and one AppKey. Bindings and virtual
// subscriptions survive reboot. A slot belongs to one SIG model.
typedef struct {
    uint16_t model;
    uint8_t label[16];
} mesh_model_label;

typedef struct {
    uint8_t onoff_server_bound;
    uint8_t onoff_client_bound;
    uint8_t health_server_bound;
    uint8_t virtual_count;
    mesh_model_label virtual[MESH_MODEL_VIRTUAL_SLOTS];
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
    uint8_t ready;
    struct mesh_onoff_server_state {
        uint8_t onoff, last_tid, has_tid;
        uint16_t last_src, last_dst;
        uint32_t last_set_ms;
    } onoff_server;
    struct {
        uint8_t tid;
    } onoff_client;
    struct {
        uint8_t attention;
        uint32_t attention_started_ms;
    } health_server;
} mesh_models;

static inline int ble_mesh_models_init(void) {
    if (!mesh_network.ready ||
        BLE_MESH_MODELS_LOAD_STATE(&mesh_models.state) != 1
    ) return 0;

    if (mesh_models.state.virtual_count > MESH_MODEL_VIRTUAL_SLOTS) return 0;
    ble_mesh_transport_clear_labels();

    for (uint8_t i = 0; i < mesh_models.state.virtual_count; i++) {
        if (!ble_mesh_label_add(mesh_models.state.virtual[i].label))
            return 0;
    }

    mesh_models.ready = 1;
    return 1;
}

static int mesh_virtual_model_valid(uint16_t model) {
    return model == MESH_MODEL_ONOFF_SERVER ||
           model == MESH_MODEL_ONOFF_CLIENT ||
           model == MESH_MODEL_HEALTH_SERVER;
}

// Local subscription interface; returns a Mesh Configuration status code.
static inline uint8_t ble_mesh_model_label_add(
    uint16_t model, const uint8_t label[16]
) {
    if (!mesh_models.ready || !label) return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;

    mesh_models_state next = mesh_models.state;
    for (uint8_t i = 0; i < next.virtual_count; i++) {
        if (next.virtual[i].model == model &&
            memcmp(next.virtual[i].label, label, 16) == 0) return MESH_CONFIG_SUCCESS;
    }
    if (next.virtual_count == MESH_MODEL_VIRTUAL_SLOTS)
        return MESH_CONFIG_INSUFFICIENT_RESOURCES;

    uint8_t i = next.virtual_count++;
    next.virtual[i].model = model;
    memcpy(next.virtual[i].label, label, 16);
    if (BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return MESH_CONFIG_STORAGE_FAILURE;

    mesh_models.state = next;
    ble_mesh_label_add(label);
    return MESH_CONFIG_SUCCESS;
}

static inline uint8_t ble_mesh_model_label_remove(uint16_t model,
                                                         const uint8_t label[16]) {
    if (!mesh_models.ready || !label) return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;

    mesh_models_state next = mesh_models.state;
    for (uint8_t i = 0; i < next.virtual_count; i++) {
        if (next.virtual[i].model != model ||
            memcmp(next.virtual[i].label, label, 16) != 0) continue;

        for (uint8_t j = i + 1; j < next.virtual_count; j++) {
            next.virtual[j - 1] = next.virtual[j];
        }

        memset(&next.virtual[--next.virtual_count], 0, sizeof(next.virtual[0]));
        if (BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return MESH_CONFIG_STORAGE_FAILURE;

        mesh_models.state = next;
        ble_mesh_transport_clear_labels();

        for (uint8_t j = 0; j < next.virtual_count; j++) {
            ble_mesh_label_add(next.virtual[j].label);
        }

        break;
    }
    return MESH_CONFIG_SUCCESS;
}

static int mesh_model_has_label(uint16_t model, const uint8_t label[16]) {
    for (uint8_t i = 0; i < mesh_models.state.virtual_count; i++) {
        if (mesh_models.state.virtual[i].model == model &&
            memcmp(mesh_models.state.virtual[i].label, label, 16) == 0) return 1;
    }

    return 0;
}

static inline int ble_mesh_config_virtual_sub(
    uint16_t dst, uint16_t element,
    uint16_t model, const uint8_t label[16], uint8_t add
) {
    if (!label || !element || element > 0x7fff ||
        !mesh_virtual_model_valid(model)) return 0;

    uint8_t params[20] = {(uint8_t)element, (uint8_t)(element >> 8)};
    memcpy(params + 2, label, 16);
    params[18] = (uint8_t)model;
    params[19] = (uint8_t)(model >> 8);

    return ble_mesh_access_queue(dst, MODEL_TTL, 1,
        add ? OP_CONFIG_MODEL_SUB_VIRTUAL_ADD :
              OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE, params, sizeof(params));
}

static int app_key_allowed(
    const mesh_access_pdu *message, uint8_t bound
) {
    return bound && mesh_network.state.has_app_key &&
           message->app_key_index == mesh_network.state.app_key_index;
}

static uint8_t mesh_health_attention_remaining(void) {
    if (!mesh_models.health_server.attention) return 0;

    uint32_t elapsed = (uint32_t)(GET_MILLIS() - mesh_models.health_server.attention_started_ms);
    uint32_t total = (uint32_t)mesh_models.health_server.attention * 1000;
    if (elapsed >= total) {
        mesh_models.health_server.attention = 0;
        BLE_MESH_HEALTH_ATTENTION(0);
        return 0;
    }
    return (uint8_t)((total - elapsed + 999) / 1000);
}

// Config Server: the messages needed to install one AppKey and bind it to one
// of the three models in this file. Other Config messages are not handled yet.
static int server_config_receive(const mesh_access_pdu *message) {
    const mesh_net_state *state = &mesh_network.state;
    const uint8_t *p = message->params;
    size_t len = message->params_len;

    if (message->opcode == OP_CONFIG_APPKEY_ADD) {
        if (len != 19) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)(p[1] & 0x0f) << 8);
        uint16_t app_idx = (p[1] >> 4) | ((uint16_t)p[2] << 4);
        uint8_t status = MESH_CONFIG_SUCCESS;

        if (net_idx != state->net_key_index)
            status = MESH_CONFIG_INVALID_NETKEY;
        else if (state->has_app_key) {
            if (state->app_key_index != app_idx)
                status = MESH_CONFIG_INSUFFICIENT_RESOURCES;
            else if (memcmp(state->app_key, p + 3, 16) != 0)
                status = MESH_CONFIG_KEY_ALREADY_STORED;
        } else {
            mesh_net_state next = *state;
            next.app_key_index = app_idx;
            memcpy(next.app_key, p + 3, 16);
            next.has_app_key = 1;
            if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
        }

        uint8_t reply[4] = {status, p[0], p[1], p[2]};
        ble_mesh_access_queue(message->src, MODEL_TTL, 1,
                              OP_CONFIG_APPKEY_STATUS, reply, 4);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_APP_BIND) {
        if (len != 6) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t app_idx = p[2] | ((uint16_t)p[3] << 8);
        uint16_t model = p[4] | ((uint16_t)p[5] << 8);
        uint8_t status = MESH_CONFIG_SUCCESS;
        mesh_models_state next = mesh_models.state;

        if (element != state->unicast_address)
            status = MESH_CONFIG_INVALID_ADDRESS;
        else if (app_idx > 0x0fff || !state->has_app_key ||
                 app_idx != state->app_key_index)
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
            BLE_MESH_MODELS_SAVE_STATE(&next) != 1
        )
            status = MESH_CONFIG_STORAGE_FAILURE;
        if (status == MESH_CONFIG_SUCCESS)
            mesh_models.state = next;

        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        ble_mesh_access_queue(message->src, MODEL_TTL, 1,
                              OP_CONFIG_MODEL_APP_STATUS, reply, 7);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE) {
        if (len != 20) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[18] | ((uint16_t)p[19] << 8);
        uint16_t address = ble_mesh_virtual_address(p + 2);
        uint8_t status = element != state->unicast_address ?
            MESH_CONFIG_INVALID_ADDRESS :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ?
            ble_mesh_model_label_add(model, p + 2) :
            ble_mesh_model_label_remove(model, p + 2);
        uint8_t reply[7] = {
            status, p[0], p[1], (uint8_t)address, (uint8_t)(address >> 8),
            p[18], p[19]
        };
        ble_mesh_access_queue(message->src, MODEL_TTL, 1,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply));
        return 1;
    }

    return 0;
}

// Config Client helpers for a provisioner configuring another node.
static inline int ble_mesh_config_add_app_key(
    uint16_t dst, uint16_t net_idx,
    uint16_t app_idx, const uint8_t key[16]
) {
    if (!key || net_idx > 0x0fff || app_idx > 0x0fff) return 0;
    uint8_t params[19] = {
        (uint8_t)net_idx,
        (uint8_t)((net_idx >> 8) | (app_idx << 4)),
        (uint8_t)(app_idx >> 4)
    };
    memcpy(params + 3, key, 16);
    return ble_mesh_access_queue(dst, MODEL_TTL, 1,
                                OP_CONFIG_APPKEY_ADD,
                                params, sizeof(params));
}

static inline int ble_mesh_config_bind_model(
    uint16_t dst, uint16_t element,
    uint16_t app_idx, uint16_t model
) {
    if (!element || element > 0x7fff || app_idx > 0x0fff) return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)app_idx, (uint8_t)(app_idx >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return ble_mesh_access_queue(dst, MODEL_TTL, 1,
                                OP_CONFIG_MODEL_APP_BIND,
                                params, sizeof(params));
}

// Health Server: attention support for a node with no reported faults.
static int server_health_receive(const mesh_access_pdu *message) {
    if (message->opcode == OP_HEALTH_ATTENTION_GET) {
        if (message->params_len != 0) return 0;
    }
    else if (
        message->opcode == OP_HEALTH_ATTENTION_SET ||
        message->opcode == OP_HEALTH_ATTENTION_SET_UNACK
    ) {
        if (message->params_len != 1) return 0;

        mesh_models.health_server.attention = message->params[0];
        mesh_models.health_server.attention_started_ms = GET_MILLIS();
        BLE_MESH_HEALTH_ATTENTION(mesh_models.health_server.attention);
        if (message->opcode == OP_HEALTH_ATTENTION_SET_UNACK) return 1;
    } else return 0;

    uint8_t remaining = mesh_health_attention_remaining();
    return ble_mesh_access_queue(message->src, MODEL_TTL, 0,
                                 OP_HEALTH_ATTENTION_STATUS, &remaining, 1);
}

static inline int ble_mesh_onoff_get(uint16_t dst) {
    if (!mesh_models.ready || !mesh_models.state.onoff_client_bound) return 0;
    return ble_mesh_access_queue(dst, MODEL_TTL, 0, OP_ONOFF_GET, NULL, 0);
}

static inline int ble_mesh_onoff_get_virtual(const uint8_t label[16]) {
    if (!mesh_models.ready || !mesh_models.state.onoff_client_bound) return 0;
    return ble_mesh_access_queue_virtual(label, MODEL_TTL,
                                         OP_ONOFF_GET, NULL, 0);
}

static inline int ble_mesh_onoff_set(
    uint16_t dst, uint8_t on, uint8_t acknowledged
) {
    if (!mesh_models.ready || on > 1 ||
        !mesh_models.state.onoff_client_bound
    ) return 0;

    uint8_t params[2] = {on, mesh_models.onoff_client.tid++};
    uint32_t opcode = acknowledged ? OP_ONOFF_SET : OP_ONOFF_SET_UNACK;
    return ble_mesh_access_queue(dst, MODEL_TTL, 0, opcode, params, sizeof(params));
}

static inline int ble_mesh_onoff_set_virtual(
    const uint8_t label[16], uint8_t on, uint8_t acknowledged
) {
    if (!mesh_models.ready || !label || on > 1 ||
        !mesh_models.state.onoff_client_bound) return 0;

    uint8_t params[2] = {on, mesh_models.onoff_client.tid++};
    return ble_mesh_access_queue_virtual(label, MODEL_TTL,
        acknowledged ? OP_ONOFF_SET : OP_ONOFF_SET_UNACK,
        params, sizeof(params));
}

static int server_onoff_receive(const mesh_access_pdu *message) {
    struct mesh_onoff_server_state *server = &mesh_models.onoff_server;
    uint32_t opcode = message->opcode;
    if (opcode == OP_ONOFF_GET) {
        if (message->params_len != 0) return 0;
    }
    else if (
        opcode == OP_ONOFF_SET ||
        opcode == OP_ONOFF_SET_UNACK
    ) {
        if (message->params_len != 2 || message->params[0] > 1) return 0;
        uint32_t now = GET_MILLIS();

        if (!server->has_tid ||
            server->last_src != message->src ||
            server->last_dst != message->dst ||
            server->last_tid != message->params[1] ||
            (uint32_t)(now - server->last_set_ms) >= 6000
        ) {
            server->onoff = message->params[0];
            BLE_MESH_ONOFF_CHANGED(server->onoff);
            server->last_src = message->src;
            server->last_dst = message->dst;
            server->last_tid = message->params[1];
            server->last_set_ms = now;
            server->has_tid = 1;
        }
        if (opcode == OP_ONOFF_SET_UNACK) return 1;
    } else return 0;

    uint8_t present = server->onoff;
    return ble_mesh_access_queue(message->src, MODEL_TTL, 0,
                                 OP_ONOFF_STATUS, &present, 1);
}

static inline int ble_mesh_models_poll(void) {
    if (mesh_models.ready) mesh_health_attention_remaining();
    if (!mesh_models.ready && !ble_mesh_models_init()) return 0;
    mesh_access_message raw;
    mesh_access_pdu access;
    int result = ble_mesh_access_poll(&raw, &access);
    if (result <= 0) return result;

    // Dispatch by opcode and the receiving model's virtual subscription.
    const mesh_access_pdu *message = &access;
    uint32_t opcode = message->opcode;
    if (message->dst != mesh_network.state.unicast_address &&
        !(message->has_label && message->dst ==
          ble_mesh_virtual_address(message->label))) return 0;

    if (message->app_key_index == APP_KEY_INDEX_NONE) {
        if (message->has_label) return 0;
        if (server_config_receive(message)) return 1;

        if (opcode == OP_CONFIG_APPKEY_STATUS ||
            opcode == OP_CONFIG_MODEL_APP_STATUS ||
            opcode == OP_CONFIG_MODEL_SUB_STATUS
        ) {
            BLE_MESH_CONFIG_STATUS(message->src, opcode,
                                    message->params, message->params_len);
            return 1;
        }
        return 0;
    }

    if (opcode == OP_ONOFF_GET ||
        opcode == OP_ONOFF_SET ||
        opcode == OP_ONOFF_SET_UNACK
    ) {
        if (message->has_label &&
            !mesh_model_has_label(MESH_MODEL_ONOFF_SERVER, message->label)) return 0;
        if (!app_key_allowed(message, mesh_models.state.onoff_server_bound)) return 0;
        return server_onoff_receive(message);
    }

    if (opcode == OP_ONOFF_STATUS) {
        if (message->has_label &&
            !mesh_model_has_label(MESH_MODEL_ONOFF_CLIENT, message->label)) return 0;
        if (!app_key_allowed(message, mesh_models.state.onoff_client_bound) ||
            message->params_len != 1 || message->params[0] > 1
        ) return 0;

        BLE_MESH_ONOFF_STATUS(message->src, message->params[0]);
        return 1;
    }

    if (opcode == OP_HEALTH_ATTENTION_GET ||
        opcode == OP_HEALTH_ATTENTION_SET ||
        opcode == OP_HEALTH_ATTENTION_SET_UNACK
    ) {
        if (message->has_label &&
            !mesh_model_has_label(MESH_MODEL_HEALTH_SERVER, message->label)) return 0;
        if (!app_key_allowed(message, mesh_models.state.health_server_bound)
    ) return 0;
        return server_health_receive(message);
    }
    return 0;
}

#endif
