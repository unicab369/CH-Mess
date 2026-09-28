#ifndef ISLER_BLE_MESH_MODELS_H
#define ISLER_BLE_MESH_MODELS_H

#include "ble_mesh_3access.h"

#define MESH_MODEL_CONFIG_SERVER 0x0000
#define MESH_MODEL_HEALTH_SERVER 0x0002
#define MESH_MODEL_ONOFF_SERVER 0x1000
#define MESH_MODEL_ONOFF_CLIENT 0x1001
#define MODEL_TTL 5

#define OP_CONFIG_APPKEY_ADD 0x00
#define OP_CONFIG_APPKEY_UPDATE 0x01
#define OP_CONFIG_APPKEY_DELETE 0x8000
#define OP_CONFIG_APPKEY_GET 0x8001
#define OP_CONFIG_APPKEY_LIST 0x8002
#define OP_CONFIG_APPKEY_STATUS 0x8003
#define OP_CONFIG_MODEL_APP_BIND 0x803d
#define OP_CONFIG_MODEL_APP_STATUS 0x803e
#define OP_CONFIG_MODEL_APP_UNBIND 0x803f
#define OP_CONFIG_SIG_MODEL_APP_GET 0x804b
#define OP_CONFIG_SIG_MODEL_APP_LIST 0x804c
#define OP_CONFIG_MODEL_SUB_VIRTUAL_ADD 0x8020
#define OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE 0x8021
#define OP_CONFIG_MODEL_SUB_ADD 0x801b
#define OP_CONFIG_MODEL_SUB_DELETE 0x801c
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
#define MESH_CONFIG_CANNOT_UPDATE 0x0b
#define MESH_MODEL_VIRTUAL_SLOTS MESH_TRANSPORT_MAX_LABELS
#define MESH_MODEL_GROUP_SLOTS 8
#define MESH_APP_INDEX_BYTES ((MESH_MAX_APP_KEYS / 2) * 3 + \
                              (MESH_MAX_APP_KEYS % 2) * 2)

typedef struct {
    uint8_t element;
    uint16_t model;
    uint8_t label[16];
} mesh_model_label;

typedef struct {
    uint8_t element;
    uint16_t model;
    uint16_t address;
} mesh_model_group;

typedef struct {
    uint8_t onoff_server_bindings;
    uint8_t onoff_client_bindings;
    uint8_t health_server_bindings;
} mesh_element_bindings;

typedef struct {
    uint8_t onoff_server_bindings;
    uint8_t onoff_client_bindings;
    uint8_t health_server_bindings;
    uint8_t virtual_count;
    mesh_model_label virtual[MESH_MODEL_VIRTUAL_SLOTS];
    mesh_element_bindings other[MESH_MAX_ELEMENTS - 1];
    uint8_t group_count;
    mesh_model_group groups[MESH_MODEL_GROUP_SLOTS];
} mesh_models_state;

int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state);
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state);
void BLE_MESH_ONOFF_CHANGED(uint16_t element, uint8_t on);
void BLE_MESH_ONOFF_STATUS(uint16_t element, uint16_t src, uint8_t present);
void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len);
void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds);

static struct {
    mesh_models_state state;
    uint8_t ready;
    struct mesh_onoff_server_state {
        uint8_t onoff, last_tid, has_tid;
        uint16_t last_src, last_dst;
        uint32_t last_set_ms;
    } onoff_server[MESH_MAX_ELEMENTS];
    struct {
        uint8_t tid;
    } onoff_client[MESH_MAX_ELEMENTS];
    struct {
        uint8_t attention;
        uint32_t attention_started_ms;
    } health_server[MESH_MAX_ELEMENTS];
} mesh_models;

static int mesh_element_index(uint16_t address) {
    return mesh_local_element(address) ?
        (int)(address - mesh_network.state.unicast_address) : -1;
}

static uint8_t *mesh_model_bindings(mesh_models_state *state,
                                    uint8_t element, uint16_t model) {
    if (element >= mesh_network.state.element_count) return NULL;
    if (element == 0) {
        if (model == MESH_MODEL_ONOFF_SERVER) return &state->onoff_server_bindings;
        if (model == MESH_MODEL_ONOFF_CLIENT) return &state->onoff_client_bindings;
        if (model == MESH_MODEL_HEALTH_SERVER) return &state->health_server_bindings;
    } else {
        mesh_element_bindings *bindings = &state->other[element - 1];
        if (model == MESH_MODEL_ONOFF_SERVER) return &bindings->onoff_server_bindings;
        if (model == MESH_MODEL_ONOFF_CLIENT) return &bindings->onoff_client_bindings;
        if (model == MESH_MODEL_HEALTH_SERVER) return &bindings->health_server_bindings;
    }
    return NULL;
}

static int mesh_virtual_model_valid(uint16_t model) {
    return model == MESH_MODEL_ONOFF_SERVER ||
           model == MESH_MODEL_ONOFF_CLIENT ||
           model == MESH_MODEL_HEALTH_SERVER;
}

static inline int ble_mesh_models_init(void) {
    if (!mesh_network.ready ||
        BLE_MESH_MODELS_LOAD_STATE(&mesh_models.state) != 1
    ) return 0;

    if (mesh_models.state.virtual_count > MESH_MODEL_VIRTUAL_SLOTS ||
        mesh_models.state.group_count > MESH_MODEL_GROUP_SLOTS) return 0;
    uint8_t valid_mask = (1u << MESH_MAX_APP_KEYS) - 1u;
    if ((mesh_models.state.onoff_server_bindings & ~valid_mask) ||
        (mesh_models.state.onoff_client_bindings & ~valid_mask) ||
        (mesh_models.state.health_server_bindings & ~valid_mask)) return 0;
    for (uint8_t i = 1; i < mesh_network.state.element_count; i++) {
        mesh_element_bindings *b = &mesh_models.state.other[i - 1];
        if ((b->onoff_server_bindings & ~valid_mask) ||
            (b->onoff_client_bindings & ~valid_mask) ||
            (b->health_server_bindings & ~valid_mask)) return 0;
    }
    ble_mesh_transport_clear_labels();

    for (uint8_t i = 0; i < mesh_models.state.virtual_count; i++) {
        if (mesh_models.state.virtual[i].element >=
            mesh_network.state.element_count) return 0;
        if (!ble_mesh_label_add(mesh_models.state.virtual[i].label))
            return 0;
    }
    for (uint8_t i = 0; i < mesh_models.state.group_count; i++) {
        mesh_model_group *group = &mesh_models.state.groups[i];
        if (group->element >= mesh_network.state.element_count ||
            group->address < 0xc000 || group->address > 0xfeff ||
            !mesh_virtual_model_valid(group->model)) return 0;
    }

    mesh_models.ready = 1;
    return 1;
}


// Local subscription interface; returns a Mesh Configuration status code.
static inline uint8_t ble_mesh_model_label_add(
    uint16_t element, uint16_t model, const uint8_t label[16]
) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || !label || index < 0) return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;

    mesh_models_state next = mesh_models.state;
    for (uint8_t i = 0; i < next.virtual_count; i++) {
        if (next.virtual[i].element == index && next.virtual[i].model == model &&
            memcmp(next.virtual[i].label, label, 16) == 0) return MESH_CONFIG_SUCCESS;
    }
    if (next.virtual_count == MESH_MODEL_VIRTUAL_SLOTS)
        return MESH_CONFIG_INSUFFICIENT_RESOURCES;

    uint8_t i = next.virtual_count++;
    next.virtual[i].element = (uint8_t)index;
    next.virtual[i].model = model;
    memcpy(next.virtual[i].label, label, 16);
    if (BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return MESH_CONFIG_STORAGE_FAILURE;

    mesh_models.state = next;
    ble_mesh_label_add(label);
    return MESH_CONFIG_SUCCESS;
}

static inline uint8_t ble_mesh_model_label_remove(uint16_t element, uint16_t model,
                                                         const uint8_t label[16]) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || !label || index < 0) return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;

    mesh_models_state next = mesh_models.state;
    for (uint8_t i = 0; i < next.virtual_count; i++) {
        if (next.virtual[i].element != index || next.virtual[i].model != model ||
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

// Add or remove a model's group subscription on one element and save it.
// Returns a Mesh Configuration status code.
static inline uint8_t mesh_model_group_change(uint16_t element, uint16_t model,
                                              uint16_t address, uint8_t add) {
    int index = mesh_element_index(element);
    if (index < 0 || address < 0xc000 || address > 0xfeff)
        return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;
    mesh_models_state next = mesh_models.state;
    uint8_t i = 0;
    while (i < next.group_count &&
           !(next.groups[i].element == index && next.groups[i].model == model &&
             next.groups[i].address == address)) i++;
    if (add && i == next.group_count) {
        if (i == MESH_MODEL_GROUP_SLOTS) return MESH_CONFIG_INSUFFICIENT_RESOURCES;
        next.groups[next.group_count++] = (mesh_model_group){(uint8_t)index, model, address};
    } else if (!add && i < next.group_count) {
        for (uint8_t j = i + 1; j < next.group_count; j++)
            next.groups[j - 1] = next.groups[j];
        memset(&next.groups[--next.group_count], 0, sizeof(next.groups[0]));
    } else return MESH_CONFIG_SUCCESS;
    if (BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return MESH_CONFIG_STORAGE_FAILURE;
    mesh_models.state = next;
    return MESH_CONFIG_SUCCESS;
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

    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
        add ? OP_CONFIG_MODEL_SUB_VIRTUAL_ADD :
              OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE, params, sizeof(params), 0);
}

static inline int ble_mesh_group_subscription(uint16_t dst, uint16_t element,
                                            uint16_t model, uint16_t group,
                                            uint8_t add) {
    if (!element || element > 0x7fff || group < 0xc000 || group > 0xfeff ||
        !mesh_virtual_model_valid(model)) return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)group, (uint8_t)(group >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
        add ? OP_CONFIG_MODEL_SUB_ADD : OP_CONFIG_MODEL_SUB_DELETE,
        params, sizeof(params), 0);
}

static int app_key_allowed(uint8_t element, uint16_t model, uint16_t app_idx) {
    int slot = mesh_app_key_slot(&mesh_network.state, app_idx);
    if (slot < 0) return 0;
    uint8_t *bindings = mesh_model_bindings(&mesh_models.state, element, model);
    return bindings && (*bindings & (1u << slot));
}

static int mesh_unbind_slot(uint8_t slot) {
    mesh_models_state next = mesh_models.state;
    uint8_t bit = (uint8_t)~(1u << slot);
    next.onoff_server_bindings &= bit;
    next.onoff_client_bindings &= bit;
    next.health_server_bindings &= bit;
    for (uint8_t i = 1; i < mesh_network.state.element_count; i++) {
        next.other[i - 1].onoff_server_bindings &= bit;
        next.other[i - 1].onoff_client_bindings &= bit;
        next.other[i - 1].health_server_bindings &= bit;
    }
    if (memcmp(&next, &mesh_models.state, sizeof(next)) != 0 &&
        BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return 0;
    mesh_models.state = next;
    return 1;
}

// Pack 12-bit AppKey indexes as Bluetooth Mesh key-index pairs.
static size_t mesh_pack_app_indexes(uint8_t *out, const uint16_t *indexes,
                                    uint8_t count) {
    size_t len = 0;
    for (uint8_t i = 0; i < count; i += 2) {
        out[len++] = (uint8_t)indexes[i];
        out[len++] = (uint8_t)(indexes[i] >> 8);
        if (i + 1 < count) {
            out[len - 1] |= (uint8_t)(indexes[i + 1] << 4);
            out[len++] = (uint8_t)(indexes[i + 1] >> 4);
        }
    }
    return len;
}

static uint8_t mesh_health_attention_remaining(uint8_t element) {
    if (!mesh_models.health_server[element].attention) return 0;

    uint32_t elapsed = (uint32_t)(GET_MILLIS() - mesh_models.health_server[element].attention_started_ms);
    uint32_t total = (uint32_t)mesh_models.health_server[element].attention * 1000;
    if (elapsed >= total) {
        mesh_models.health_server[element].attention = 0;
        BLE_MESH_HEALTH_ATTENTION(mesh_network.state.unicast_address + element, 0);
        return 0;
    }
    return (uint8_t)((total - elapsed + 999) / 1000);
}

// Config Server: AppKey management and bindings for the three local models.
static int server_config_receive(const mesh_access_pdu *message) {
    const mesh_net_state *state = &mesh_network.state;
    const uint8_t *p = message->params;
    size_t len = message->params_len;

    if (message->opcode == OP_CONFIG_APPKEY_GET) {
        if (len != 2 || (p[1] & 0xf0)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t reply[3 + MESH_APP_INDEX_BYTES] = {
            net_idx == state->net_key_index ? MESH_CONFIG_SUCCESS :
                                               MESH_CONFIG_INVALID_NETKEY,
            p[0], p[1]
        };
        uint16_t indexes[MESH_MAX_APP_KEYS];
        uint8_t count = 0;
        if (reply[0] == MESH_CONFIG_SUCCESS) {
            for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
                if (state->app_keys[i].used)
                    indexes[count++] = state->app_keys[i].index;
        }
        size_t reply_len = 3 + mesh_pack_app_indexes(reply + 3, indexes, count);
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_APPKEY_LIST, reply, reply_len, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_SIG_MODEL_APP_GET) {
        if (len != 4) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[2] | ((uint16_t)p[3] << 8);
        int index = mesh_element_index(element);
        uint8_t *bindings = index < 0 ? NULL :
            mesh_model_bindings(&mesh_models.state, (uint8_t)index, model);
        uint8_t reply[5 + MESH_APP_INDEX_BYTES] = {
            index < 0 ? MESH_CONFIG_INVALID_ADDRESS :
            !bindings ? MESH_CONFIG_INVALID_MODEL : MESH_CONFIG_SUCCESS,
            p[0], p[1], p[2], p[3]
        };
        uint16_t indexes[MESH_MAX_APP_KEYS];
        uint8_t count = 0;
        if (reply[0] == MESH_CONFIG_SUCCESS) {
            for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
                if (state->app_keys[i].used && (*bindings & (1u << i)))
                    indexes[count++] = state->app_keys[i].index;
        }
        size_t reply_len = 5 + mesh_pack_app_indexes(reply + 5, indexes, count);
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_SIG_MODEL_APP_LIST, reply, reply_len, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_APPKEY_ADD ||
        message->opcode == OP_CONFIG_APPKEY_UPDATE ||
        message->opcode == OP_CONFIG_APPKEY_DELETE) {
        if (len != (message->opcode == OP_CONFIG_APPKEY_DELETE ? 3u : 19u))
            return 0;
        uint16_t net_idx = p[0] | ((uint16_t)(p[1] & 0x0f) << 8);
        uint16_t app_idx = (p[1] >> 4) | ((uint16_t)p[2] << 4);
        uint8_t status = MESH_CONFIG_SUCCESS;
        int slot = mesh_app_key_slot(state, app_idx);

        if (net_idx != state->net_key_index)
            status = MESH_CONFIG_INVALID_NETKEY;
        else if (message->opcode == OP_CONFIG_APPKEY_ADD) {
            if (slot >= 0) {
                if (memcmp(state->app_keys[slot].key, p + 3, 16) != 0)
                    status = MESH_CONFIG_KEY_ALREADY_STORED;
            } else {
                for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
                    if (!state->app_keys[i].used) { slot = i; break; }
                }
                if (slot < 0) status = MESH_CONFIG_INSUFFICIENT_RESOURCES;
                else {
                    mesh_net_state next = *state;
                    mesh_app_key *app = &next.app_keys[slot];
                    memset(app, 0, sizeof(*app));
                    app->index = app_idx;
                    app->used = 1;
                    memcpy(app->key, p + 3, 16);
                    if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
                }
            }
        } else if (slot < 0 && message->opcode == OP_CONFIG_APPKEY_UPDATE)
            status = MESH_CONFIG_INVALID_APPKEY;
        else if (slot < 0) status = MESH_CONFIG_SUCCESS;
        else if (message->opcode == OP_CONFIG_APPKEY_UPDATE) {
            if (!ble_mesh_stage_app_key(app_idx, p + 3))
                status = MESH_CONFIG_CANNOT_UPDATE;
        } else {
            // Remove bindings first so a reused slot cannot inherit permissions.
            if (!mesh_unbind_slot((uint8_t)slot))
                status = MESH_CONFIG_STORAGE_FAILURE;
            else {
                mesh_net_state next = *state;
                memset(&next.app_keys[slot], 0, sizeof(next.app_keys[slot]));
                if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
            }
        }

        uint8_t reply[4] = {status, p[0], p[1], p[2]};
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_APPKEY_STATUS, reply, 4, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_APP_BIND ||
        message->opcode == OP_CONFIG_MODEL_APP_UNBIND) {
        if (len != 6) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t app_idx = p[2] | ((uint16_t)p[3] << 8);
        uint16_t model = p[4] | ((uint16_t)p[5] << 8);
        uint8_t status = MESH_CONFIG_SUCCESS;
        mesh_models_state next = mesh_models.state;
        int slot = mesh_app_key_slot(state, app_idx);

        int index = mesh_element_index(element);
        if (index < 0)
            status = MESH_CONFIG_INVALID_ADDRESS;
        else if (slot < 0)
            status = MESH_CONFIG_INVALID_APPKEY;
        else {
            uint8_t *bindings = mesh_model_bindings(&next, (uint8_t)index, model);
            if (!bindings) status = MESH_CONFIG_INVALID_MODEL;
            else if (message->opcode == OP_CONFIG_MODEL_APP_BIND)
                *bindings |= (uint8_t)(1u << slot);
            else
                *bindings &= (uint8_t)~(1u << slot);
        }

        if (status == MESH_CONFIG_SUCCESS &&
            memcmp(&next, &mesh_models.state, sizeof(next)) != 0 &&
            BLE_MESH_MODELS_SAVE_STATE(&next) != 1
        )
            status = MESH_CONFIG_STORAGE_FAILURE;
        if (status == MESH_CONFIG_SUCCESS)
            mesh_models.state = next;

        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_APP_STATUS, reply, 7, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE) {
        if (len != 20) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[18] | ((uint16_t)p[19] << 8);
        uint16_t address = ble_mesh_virtual_address(p + 2);
        uint8_t status = mesh_element_index(element) < 0 ?
            MESH_CONFIG_INVALID_ADDRESS :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ?
            ble_mesh_model_label_add(element, model, p + 2) :
            ble_mesh_model_label_remove(element, model, p + 2);
        uint8_t reply[7] = {
            status, p[0], p[1], (uint8_t)address, (uint8_t)(address >> 8),
            p[18], p[19]
        };
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_DELETE) {
        if (len != 6) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t address = p[2] | ((uint16_t)p[3] << 8);
        uint16_t model = p[4] | ((uint16_t)p[5] << 8);
        uint8_t status = mesh_model_group_change(element, model, address,
            message->opcode == OP_CONFIG_MODEL_SUB_ADD);
        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, MODEL_TTL, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
        return 1;
    }

    return 0;
}

// Config Client helpers for a provisioner configuring another node.
// Set update to 0 to add a key, or 1 to stage a replacement during Key Refresh.
static inline int ble_mesh_add_or_update_app_key(
    uint16_t dst, uint16_t net_idx,
    uint16_t app_idx, const uint8_t key[16], uint8_t update
) {
    if (!key || net_idx > 0x0fff || app_idx > 0x0fff || update > 1) return 0;
    uint8_t params[19] = {
        (uint8_t)net_idx,
        (uint8_t)((net_idx >> 8) | (app_idx << 4)),
        (uint8_t)(app_idx >> 4)
    };
    memcpy(params + 3, key, 16);
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
                                 update ? OP_CONFIG_APPKEY_UPDATE : OP_CONFIG_APPKEY_ADD,
                                 params, sizeof(params), 0);
}

static inline int ble_mesh_delete_app_key(
    uint16_t dst, uint16_t net_idx, uint16_t app_idx
) {
    if (net_idx > 0x0fff || app_idx > 0x0fff) return 0;
    uint8_t params[3] = {
        (uint8_t)net_idx,
        (uint8_t)((net_idx >> 8) | (app_idx << 4)),
        (uint8_t)(app_idx >> 4)
    };
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_APPKEY_DELETE, params, sizeof(params), 0);
}

static inline int ble_mesh_get_app_keys(uint16_t dst,
                                                uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_APPKEY_GET, params, sizeof(params), 0);
}

// Set bind to 1 to bind the AppKey to the model, or 0 to unbind it.
static inline int ble_mesh_model_binding(
    uint16_t dst, uint16_t element,
    uint16_t app_idx, uint16_t model, uint8_t bind
) {
    if (!element || element > 0x7fff || app_idx > 0x0fff || bind > 1) return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)app_idx, (uint8_t)(app_idx >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
                                 bind ? OP_CONFIG_MODEL_APP_BIND : OP_CONFIG_MODEL_APP_UNBIND,
                                 params, sizeof(params), 0);
}

static inline int ble_mesh_get_bindings(uint16_t dst,
                                                 uint16_t element,
                                                 uint16_t model) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model)) return 0;
    uint8_t params[4] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, MODEL_TTL, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_SIG_MODEL_APP_GET,
                                 params, sizeof(params), 0);
}

// Health Server: attention support for a node with no reported faults.
static int server_health_receive(const mesh_access_pdu *message, uint8_t element) {
    if (message->opcode == OP_HEALTH_ATTENTION_GET) {
        if (message->params_len != 0) return 0;
    }
    else if (
        message->opcode == OP_HEALTH_ATTENTION_SET ||
        message->opcode == OP_HEALTH_ATTENTION_SET_UNACK
    ) {
        if (message->params_len != 1) return 0;

        mesh_models.health_server[element].attention = message->params[0];
        mesh_models.health_server[element].attention_started_ms = GET_MILLIS();
        BLE_MESH_HEALTH_ATTENTION(mesh_network.state.unicast_address + element,
                                  mesh_models.health_server[element].attention);
        if (message->opcode == OP_HEALTH_ATTENTION_SET_UNACK) return 1;
    } else return 0;

    uint8_t remaining = mesh_health_attention_remaining(element);
    return ble_mesh_access_queue(mesh_network.state.unicast_address + element,
                                 message->src, MODEL_TTL,
                                 message->app_key_index,
                                 OP_HEALTH_ATTENTION_STATUS, &remaining, 1, 0);
}

static inline int ble_mesh_onoff_get(uint16_t element, uint16_t dst,
                                          uint16_t app_idx) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)) return 0;
    return ble_mesh_access_queue(element, dst, MODEL_TTL, app_idx,
                                      OP_ONOFF_GET, NULL, 0, 0);
}

static inline int ble_mesh_onoff_get_virtual(uint16_t element,
                                                  const uint8_t label[16],
                                                  uint16_t app_idx) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)) return 0;
    return ble_mesh_access_queue_virtual(element, label, MODEL_TTL,
        app_idx, OP_ONOFF_GET, NULL, 0, 0);
}

static inline int ble_mesh_onoff_set(
    uint16_t element, uint16_t dst, uint16_t app_idx,
    uint8_t on, uint8_t acknowledged
) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 || on > 1 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)
    ) return 0;

    uint8_t params[2] = {on, mesh_models.onoff_client[index].tid++};
    uint32_t opcode = acknowledged ? OP_ONOFF_SET : OP_ONOFF_SET_UNACK;
    return ble_mesh_access_queue(element, dst, MODEL_TTL, app_idx,
                                      opcode, params, sizeof(params), 0);
}

static inline int ble_mesh_onoff_set_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx,
    uint8_t on, uint8_t acknowledged
) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 || !label || on > 1 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)) return 0;

    uint8_t params[2] = {on, mesh_models.onoff_client[index].tid++};
    return ble_mesh_access_queue_virtual(element, label, MODEL_TTL, app_idx,
        acknowledged ? OP_ONOFF_SET : OP_ONOFF_SET_UNACK,
        params, sizeof(params), 0);
}

static int server_onoff_receive(const mesh_access_pdu *message, uint8_t element) {
    struct mesh_onoff_server_state *server = &mesh_models.onoff_server[element];
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
            BLE_MESH_ONOFF_CHANGED(mesh_network.state.unicast_address + element,
                                   server->onoff);
            server->last_src = message->src;
            server->last_dst = message->dst;
            server->last_tid = message->params[1];
            server->last_set_ms = now;
            server->has_tid = 1;
        }
        if (opcode == OP_ONOFF_SET_UNACK) return 1;
    } else return 0;

    uint8_t present = server->onoff;
    return ble_mesh_access_queue(mesh_network.state.unicast_address + element,
                                 message->src, MODEL_TTL,
                                 message->app_key_index,
                                 OP_ONOFF_STATUS, &present, 1, 0);
}

static inline int ble_mesh_models_poll(void) {
    if (!mesh_models.ready && !ble_mesh_models_init()) return 0;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++)
        mesh_health_attention_remaining(i);
    mesh_access_message raw;
    mesh_access_pdu access;
    int result = ble_mesh_access_poll(&raw, &access);
    if (result <= 0) return result;

    // Decode once, then deliver to each subscribed model instance.
    const mesh_access_pdu *message = &access;
    uint32_t opcode = message->opcode;
    int unicast = mesh_element_index(message->dst);
    uint8_t virtual = message->has_label &&
        message->dst == ble_mesh_virtual_address(message->label);
    uint8_t group = message->dst >= 0xc000 && message->dst <= 0xfeff;
    if (unicast < 0 && !virtual && !group) return 0;

    if (message->app_key_index == APP_KEY_INDEX_NONE) {
        if (unicast != 0 || message->has_label) return 0;
        // Server requests use our key; client replies use the sending node's key.
        if (message->device_key_owner == message->dst &&
            server_config_receive(message)) return 1;
        if (message->device_key_owner != message->src) return 0;

        if (opcode == OP_CONFIG_APPKEY_STATUS ||
            opcode == OP_CONFIG_APPKEY_LIST ||
            opcode == OP_CONFIG_MODEL_APP_STATUS ||
            opcode == OP_CONFIG_SIG_MODEL_APP_LIST ||
            opcode == OP_CONFIG_MODEL_SUB_STATUS
        ) {
            BLE_MESH_CONFIG_STATUS(message->src, opcode,
                                    message->params, message->params_len);
            return 1;
        }
        return 0;
    }

    int handled = 0;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++) {
        if (unicast >= 0 && unicast != i) continue;
        uint16_t model = 0;
        if (opcode == OP_ONOFF_GET || opcode == OP_ONOFF_SET ||
            opcode == OP_ONOFF_SET_UNACK) model = MESH_MODEL_ONOFF_SERVER;
        else if (opcode == OP_ONOFF_STATUS) model = MESH_MODEL_ONOFF_CLIENT;
        else if (opcode == OP_HEALTH_ATTENTION_GET ||
                 opcode == OP_HEALTH_ATTENTION_SET ||
                 opcode == OP_HEALTH_ATTENTION_SET_UNACK)
            model = MESH_MODEL_HEALTH_SERVER;
        else continue;

        if (virtual) {
            // Deliver a virtual message only to models subscribed to its label.
            uint8_t subscribed = 0;
            for (uint8_t j = 0; j < mesh_models.state.virtual_count; j++) {
                mesh_model_label *entry = &mesh_models.state.virtual[j];
                if (entry->element == i && entry->model == model &&
                    memcmp(entry->label, message->label, 16) == 0) {
                    subscribed = 1;
                    break;
                }
            }
            if (!subscribed) continue;
        }
        if (group) {
            // Deliver a group message only to subscribed model instances.
            uint8_t subscribed = 0;
            for (uint8_t j = 0; j < mesh_models.state.group_count; j++) {
                mesh_model_group *entry = &mesh_models.state.groups[j];
                if (entry->element == i && entry->model == model &&
                    entry->address == message->dst) {
                    subscribed = 1;
                    break;
                }
            }
            if (!subscribed) continue;
        }
        if (!app_key_allowed(i, model, message->app_key_index)) continue;

        if (model == MESH_MODEL_ONOFF_SERVER)
            handled |= server_onoff_receive(message, i);
        else if (model == MESH_MODEL_ONOFF_CLIENT) {
            if (message->params_len != 1 || message->params[0] > 1) continue;
            BLE_MESH_ONOFF_STATUS(mesh_network.state.unicast_address + i,
                                  message->src, message->params[0]);
            handled = 1;
        } else handled |= server_health_receive(message, i);
    }
    return handled;
}

#endif
