#ifndef ISLER_BLE_MESH_FOUNDATION_H
#define ISLER_BLE_MESH_FOUNDATION_H

#include "ble_mesh_4state.h"

#define MESH_MODEL_CONFIG_SERVER 0x0000
#define MESH_MODEL_CONFIG_CLIENT 0x0001
// Override these with the product's assigned identifiers.
#ifndef MESH_COMPANY_ID
#define MESH_COMPANY_ID 0xffff
#endif
#ifndef MESH_PRODUCT_ID
#define MESH_PRODUCT_ID 0
#endif
#ifndef MESH_PRODUCT_VERSION
#define MESH_PRODUCT_VERSION 1
#endif

#define OP_CONFIG_COMPOSITION_GET 0x8008
#define OP_CONFIG_COMPOSITION_STATUS 0x02
#define OP_CONFIG_DEFAULT_TTL_GET 0x800c
#define OP_CONFIG_DEFAULT_TTL_SET 0x800d
#define OP_CONFIG_DEFAULT_TTL_STATUS 0x800e
#define OP_CONFIG_MODEL_PUB_GET 0x8018
#define OP_CONFIG_MODEL_PUB_SET 0x03
#define OP_CONFIG_MODEL_PUB_STATUS 0x8019
#define OP_CONFIG_MODEL_PUB_VIRTUAL_SET 0x801a
#define OP_CONFIG_MODEL_SUB_DELETE_ALL 0x801d
#define OP_CONFIG_MODEL_SUB_OVERWRITE 0x801e
#define OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE 0x8022
#define OP_CONFIG_SIG_SUB_GET 0x8029
#define OP_CONFIG_SIG_MODEL_SUB_LIST 0x802a
#define OP_HEALTH_CURRENT_STATUS 0x04
#define OP_CONFIG_NETKEY_ADD 0x8040
#define OP_CONFIG_NETKEY_DELETE 0x8041
#define OP_CONFIG_NETKEY_GET 0x8042
#define OP_CONFIG_NETKEY_LIST 0x8043
#define OP_CONFIG_NETKEY_STATUS 0x8044
#define OP_CONFIG_NETKEY_UPDATE 0x8045
#define OP_CONFIG_KEY_PHASE_GET 0x8015
#define OP_CONFIG_KEY_PHASE_SET 0x8016
#define OP_CONFIG_KEY_PHASE_STATUS 0x8017

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
#define MESH_APP_INDEX_BYTES ((MESH_MAX_APP_KEYS / 2) * 3 + \
                              (MESH_MAX_APP_KEYS % 2) * 2)

void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len);
void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds);

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

    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
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
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        add ? OP_CONFIG_MODEL_SUB_ADD : OP_CONFIG_MODEL_SUB_DELETE,
        params, sizeof(params), 0);
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
    uint16_t app_idx = mesh_network.state.app_keys[slot].index;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++)
        for (uint8_t j = 0; j < MESH_PUBLICATION_MODELS; j++)
            if (next.publications[i][j].address && next.publications[i][j].app_idx == app_idx)
                memset(&next.publications[i][j], 0, sizeof(next.publications[i][j]));
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

// Replace or clear all subscriptions of one model in a single saved update.
static uint8_t mesh_subscription_replace(uint16_t element, uint16_t model,
                                          uint16_t address, const uint8_t *label) {
    int index = mesh_element_index(element);
    if (index < 0 || (address && (address < 0xc000 || address > 0xfeff)))
        return MESH_CONFIG_INVALID_ADDRESS;
    if (!mesh_virtual_model_valid(model)) return MESH_CONFIG_INVALID_MODEL;
    mesh_models_state next = mesh_models.state;
    uint8_t count = 0;
    for (uint8_t i = 0; i < next.group_count; i++)
        if (next.groups[i].element != index || next.groups[i].model != model)
            next.groups[count++] = next.groups[i];
    memset(next.groups + count, 0, (next.group_count - count) * sizeof(next.groups[0]));
    next.group_count = count;
    count = 0;
    for (uint8_t i = 0; i < next.virtual_count; i++)
        if (next.virtual[i].element != index || next.virtual[i].model != model)
            next.virtual[count++] = next.virtual[i];
    memset(next.virtual + count, 0, (next.virtual_count - count) * sizeof(next.virtual[0]));
    next.virtual_count = count;
    if (address) {
        if (next.group_count == MESH_MODEL_GROUP_SLOTS)
            return MESH_CONFIG_INSUFFICIENT_RESOURCES;
        next.groups[next.group_count++] = (mesh_model_group){(uint8_t)index, model, address};
    }
    if (label) {
        if (next.virtual_count == MESH_MODEL_VIRTUAL_SLOTS)
            return MESH_CONFIG_INSUFFICIENT_RESOURCES;
        mesh_model_label *entry = &next.virtual[next.virtual_count++];
        entry->element = (uint8_t)index;
        entry->model = model;
        memcpy(entry->label, label, 16);
    }
    if (memcmp(&next, &mesh_models.state, sizeof(next)) &&
        BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return MESH_CONFIG_STORAGE_FAILURE;
    mesh_models.state = next;
    ble_mesh_transport_clear_labels();
    for (uint8_t i = 0; i < next.virtual_count; i++)
        ble_mesh_label_add(next.virtual[i].label);
    return MESH_CONFIG_SUCCESS;
}

// Config Server: persisted node and model configuration.

static int server_config_receive(const mesh_access_pdu *message) {
    const mesh_net_state *state = &mesh_network.state;
    const uint8_t *p = message->params;
    size_t len = message->params_len;

    if (message->opcode == OP_CONFIG_NETKEY_GET) {
        if (len != 0) return 0;
        uint8_t reply[2] = {(uint8_t)state->net_key_index,
                            (uint8_t)(state->net_key_index >> 8)};
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_NETKEY_LIST, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_NETKEY_ADD ||
        message->opcode == OP_CONFIG_NETKEY_UPDATE ||
        message->opcode == OP_CONFIG_NETKEY_DELETE) {
        uint8_t remove = message->opcode == OP_CONFIG_NETKEY_DELETE;
        if (len != (remove ? 2u : 18u) || (p[1] & 0xf0)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t status = MESH_CONFIG_SUCCESS;
        if (message->opcode == OP_CONFIG_NETKEY_ADD) {
            // This node has one subnet, installed during provisioning.
            if (net_idx != state->net_key_index)
                status = MESH_CONFIG_INSUFFICIENT_RESOURCES;
            else if (memcmp(state->net_key, p + 2, 16))
                status = MESH_CONFIG_KEY_ALREADY_STORED;
        } else if (remove) {
            // Deleting an absent key is redundant; the last key cannot be removed.
            if (net_idx == state->net_key_index) status = MESH_CONFIG_CANNOT_REMOVE;
        } else if (net_idx != state->net_key_index)
            status = MESH_CONFIG_INVALID_NETKEY;
        else if (!state->phase2_provisioned && state->key_refresh_phase == 0 &&
                 !memcmp(state->net_key, p + 2, 16))
            status = MESH_CONFIG_KEY_ALREADY_STORED;
        else if (state->phase2_provisioned || state->key_refresh_phase == 2 ||
                 (state->key_refresh_phase == 1 && memcmp(state->new_net_key, p + 2, 16)))
            status = MESH_CONFIG_CANNOT_UPDATE;
        else if (!ble_mesh_stage_net_key(p + 2))
            status = MESH_CONFIG_STORAGE_FAILURE;

        uint8_t reply[3] = {status, p[0], p[1]};
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_NETKEY_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_KEY_PHASE_GET ||
        message->opcode == OP_CONFIG_KEY_PHASE_SET) {
        uint8_t set = message->opcode == OP_CONFIG_KEY_PHASE_SET;
        if (len != (set ? 3u : 2u) || (p[1] & 0xf0) ||
            (set && p[2] != 2 && p[2] != 3)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t phase = state->phase2_provisioned ? 2 : state->key_refresh_phase;
        uint8_t status = net_idx != state->net_key_index ?
            MESH_CONFIG_INVALID_NETKEY : MESH_CONFIG_SUCCESS;
        if (set && status == MESH_CONFIG_SUCCESS) {
            if (p[2] == 2 && phase == 0) status = MESH_CONFIG_CANNOT_UPDATE;
            else if (!ble_mesh_key_refresh_transition(p[2]))
                status = MESH_CONFIG_STORAGE_FAILURE;
            phase = state->phase2_provisioned ? 2 : state->key_refresh_phase;
        }
        uint8_t reply[4] = {status, p[0], p[1],
                            net_idx == state->net_key_index ? phase : 0};
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_KEY_PHASE_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_COMPOSITION_GET) {
        if (len != 1) return 0;
        // Page 0 is the highest supported page, including for unknown requests.
        uint8_t reply[11 + 14 + (MESH_MAX_ELEMENTS - 1) * 10] = {
            0, (uint8_t)MESH_COMPANY_ID, (uint8_t)(MESH_COMPANY_ID >> 8),
            (uint8_t)MESH_PRODUCT_ID, (uint8_t)(MESH_PRODUCT_ID >> 8),
            (uint8_t)MESH_PRODUCT_VERSION, (uint8_t)(MESH_PRODUCT_VERSION >> 8),
            (uint8_t)MESH_NETWORK_REPLAY_SLOTS, (uint8_t)(MESH_NETWORK_REPLAY_SLOTS >> 8),
            0, 0 // No Relay, Proxy, Friend, or Low Power feature.
        };
        size_t size = 11;
        for (uint8_t i = 0; i < state->element_count; i++) {
            reply[size++] = 0; reply[size++] = 0; // Unknown element location.
            reply[size++] = i ? 3 : 5;
            reply[size++] = 0; // No vendor models.
            const uint16_t models[] = {MESH_MODEL_CONFIG_SERVER, MESH_MODEL_CONFIG_CLIENT,
                MESH_MODEL_HEALTH_SERVER, MESH_MODEL_ONOFF_SERVER, MESH_MODEL_ONOFF_CLIENT};
            for (uint8_t j = i ? 2 : 0; j < 5; j++) {
                reply[size++] = (uint8_t)models[j];
                reply[size++] = (uint8_t)(models[j] >> 8);
            }
        }
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_COMPOSITION_STATUS, reply, size, 0);
    }

    if (message->opcode == OP_CONFIG_DEFAULT_TTL_GET ||
        message->opcode == OP_CONFIG_DEFAULT_TTL_SET) {
        if (message->opcode == OP_CONFIG_DEFAULT_TTL_SET) {
            if (len != 1 || p[0] == 1 || p[0] > 0x7f) return 0;
            mesh_models_state next = mesh_models.state;
            next.default_ttl = p[0];
            if (next.default_ttl != mesh_models.state.default_ttl &&
                BLE_MESH_MODELS_SAVE_STATE(&next) != 1) return 0;
            mesh_models.state = next;
        } else if (len != 0) return 0;
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_DEFAULT_TTL_STATUS, &mesh_models.state.default_ttl, 1, 0);
    }

    if (message->opcode == OP_CONFIG_MODEL_PUB_GET ||
        message->opcode == OP_CONFIG_MODEL_PUB_SET ||
        message->opcode == OP_CONFIG_MODEL_PUB_VIRTUAL_SET) {
        uint8_t get = message->opcode == OP_CONFIG_MODEL_PUB_GET;
        uint8_t virtual = message->opcode == OP_CONFIG_MODEL_PUB_VIRTUAL_SET;
        if (len != (get ? 4u : virtual ? 25u : 11u)) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[len - 2] | ((uint16_t)p[len - 1] << 8);
        int index = mesh_element_index(element), slot = mesh_publication_slot(model);
        uint8_t status = index < 0 ? MESH_CONFIG_INVALID_ADDRESS :
            slot < 0 ? (index == 0 && (model == MESH_MODEL_CONFIG_SERVER ||
                         model == MESH_MODEL_CONFIG_CLIENT) ?
                         MESH_CONFIG_INVALID_PUBLICATION : MESH_CONFIG_INVALID_MODEL) :
                         MESH_CONFIG_SUCCESS;
        mesh_publication pub = {0};
        if (status == MESH_CONFIG_SUCCESS) pub = mesh_models.state.publications[index][slot];
        if (!get && status == MESH_CONFIG_SUCCESS) {
            mesh_publication next = {0};
            next.address = virtual ? ble_mesh_virtual_address(p + 2) :
                p[2] | ((uint16_t)p[3] << 8);
            size_t offset = virtual ? 18 : 4;
            uint16_t flags = p[offset] | ((uint16_t)p[offset + 1] << 8);
            if (flags & 0xe000) return 0; // Reserved bits.
            next.app_idx = flags & 0x0fff;
            next.ttl = p[offset + 2];
            next.period = p[offset + 3];
            next.retransmit = p[offset + 4];
            next.has_label = virtual;
            if (virtual) memcpy(next.label, p + 2, 16);
            if (!next.address) memset(&next, 0, sizeof(next));
            else if (!virtual && ((next.address >= 0x8000 && next.address < 0xc000) ||
                                  (next.address >= 0xff00 && next.address < 0xfffc)))
                status = MESH_CONFIG_INVALID_ADDRESS;
            else if (next.ttl > 0x7f && next.ttl != 0xff)
                status = MESH_CONFIG_INVALID_PUBLICATION;
            else if (flags & 0x1000)
                status = MESH_CONFIG_FEATURE_NOT_SUPPORTED;
            else if (mesh_app_key_slot(state, next.app_idx) < 0)
                status = MESH_CONFIG_INVALID_APPKEY;
            else if (!app_key_allowed((uint8_t)index, model, next.app_idx))
                status = MESH_CONFIG_INVALID_BINDING;
            if (status == MESH_CONFIG_SUCCESS) {
                mesh_models_state saved = mesh_models.state;
                saved.publications[index][slot] = next;
                if (memcmp(&saved, &mesh_models.state, sizeof(saved)) &&
                    BLE_MESH_MODELS_SAVE_STATE(&saved) != 1)
                    status = MESH_CONFIG_STORAGE_FAILURE;
                else {
                    mesh_models.state = saved;
                    pub = next;
                    memset(&mesh_models.publications[index][slot], 0,
                           sizeof(mesh_models.publications[index][slot]));
                    mesh_models.publications[index][slot].period_at_ms =
                        GET_MILLIS() + mesh_publication_period(next.period);
                }
            }
        }
        uint8_t reply[12] = {status, p[0], p[1],
            (uint8_t)pub.address, (uint8_t)(pub.address >> 8),
            (uint8_t)pub.app_idx, (uint8_t)(pub.app_idx >> 8),
            pub.ttl, pub.period, pub.retransmit,
            (uint8_t)model, (uint8_t)(model >> 8)};
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_MODEL_PUB_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_SIG_SUB_GET) {
        if (len != 4) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[2] | ((uint16_t)p[3] << 8);
        int index = mesh_element_index(element);
        uint8_t reply[5 + 2 * (MESH_MODEL_GROUP_SLOTS + MESH_MODEL_VIRTUAL_SLOTS)] = {
            index < 0 ? MESH_CONFIG_INVALID_ADDRESS :
            !mesh_virtual_model_valid(model) ? MESH_CONFIG_INVALID_MODEL : MESH_CONFIG_SUCCESS,
            p[0], p[1], p[2], p[3]};
        size_t size = 5;
        if (reply[0] == MESH_CONFIG_SUCCESS) {
            for (uint8_t i = 0; i < mesh_models.state.group_count; i++) {
                mesh_model_group *entry = &mesh_models.state.groups[i];
                if (entry->element != index || entry->model != model) continue;
                reply[size++] = (uint8_t)entry->address;
                reply[size++] = (uint8_t)(entry->address >> 8);
            }
            for (uint8_t i = 0; i < mesh_models.state.virtual_count; i++) {
                mesh_model_label *entry = &mesh_models.state.virtual[i];
                if (entry->element != index || entry->model != model) continue;
                uint16_t address = ble_mesh_virtual_address(entry->label);
                size_t j = 5;
                while (j < size && (reply[j] | ((uint16_t)reply[j + 1] << 8)) != address) j += 2;
                if (j < size) continue; // Colliding labels share one listed address.
                reply[size++] = (uint8_t)address;
                reply[size++] = (uint8_t)(address >> 8);
            }
        }
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_SIG_MODEL_SUB_LIST, reply, size, 0);
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_DELETE_ALL) {
        if (len != 4) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[2] | ((uint16_t)p[3] << 8);
        uint8_t reply[7] = {mesh_subscription_replace(element, model, 0, NULL),
            p[0], p[1], 0, 0, p[2], p[3]};
        return ble_mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
    }

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
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
            const mesh_app_key *app = &state->app_keys[slot];
            if (state->key_refresh_phase != 1 ||
                (app->has_new_key && memcmp(app->new_key, p + 3, 16)) ||
                (!app->has_new_key && !memcmp(app->key, p + 3, 16)))
                status = MESH_CONFIG_CANNOT_UPDATE;
            else if (!ble_mesh_stage_app_key(app_idx, p + 3))
                status = MESH_CONFIG_STORAGE_FAILURE;
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
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
            else {
                *bindings &= (uint8_t)~(1u << slot);
                int pub_slot = mesh_publication_slot(model);
                if (pub_slot >= 0 && next.publications[index][pub_slot].app_idx == app_idx)
                    memset(&next.publications[index][pub_slot], 0,
                           sizeof(next.publications[index][pub_slot]));
            }
        }

        if (status == MESH_CONFIG_SUCCESS &&
            memcmp(&next, &mesh_models.state, sizeof(next)) != 0 &&
            BLE_MESH_MODELS_SAVE_STATE(&next) != 1
        )
            status = MESH_CONFIG_STORAGE_FAILURE;
        if (status == MESH_CONFIG_SUCCESS)
            mesh_models.state = next;

        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_APP_STATUS, reply, 7, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE) {
        if (len != 20) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[18] | ((uint16_t)p[19] << 8);
        uint16_t address = ble_mesh_virtual_address(p + 2);
        uint8_t status = mesh_element_index(element) < 0 ?
            MESH_CONFIG_INVALID_ADDRESS :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE ?
            mesh_subscription_replace(element, model, 0, p + 2) :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ?
            ble_mesh_model_label_add(element, model, p + 2) :
            ble_mesh_model_label_remove(element, model, p + 2);
        uint8_t reply[7] = {
            status, p[0], p[1], (uint8_t)address, (uint8_t)(address >> 8),
            p[18], p[19]
        };
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_DELETE ||
        message->opcode == OP_CONFIG_MODEL_SUB_OVERWRITE) {
        if (len != 6) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t address = p[2] | ((uint16_t)p[3] << 8);
        uint16_t model = p[4] | ((uint16_t)p[5] << 8);
        uint8_t status = message->opcode == OP_CONFIG_MODEL_SUB_OVERWRITE ?
            (address < 0xc000 || address > 0xfeff ? MESH_CONFIG_INVALID_ADDRESS :
             mesh_subscription_replace(element, model, address, NULL)) :
            mesh_model_group_change(element, model, address,
                                   message->opcode == OP_CONFIG_MODEL_SUB_ADD);
        uint8_t reply[7] = {status, p[0], p[1], p[2], p[3], p[4], p[5]};
        ble_mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
        return 1;
    }

    return 0;
}

// Config Client helpers for a provisioner configuring another node.
// Set update to 1 to start Key Refresh, or 0 to add a subnet key.
static inline int ble_mesh_add_or_update_net_key(uint16_t dst, uint16_t net_idx,
    const uint8_t key[16], uint8_t update) {
    if (!key || net_idx > 0x0fff || update > 1) return 0;
    uint8_t params[18] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    memcpy(params + 2, key, 16);
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        update ? OP_CONFIG_NETKEY_UPDATE : OP_CONFIG_NETKEY_ADD,
        params, sizeof(params), 0);
}

static inline int ble_mesh_get_net_keys(uint16_t dst) {
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_NETKEY_GET, NULL, 0, 0);
}

static inline int ble_mesh_delete_net_key(uint16_t dst, uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_NETKEY_DELETE, params, sizeof(params), 0);
}

static inline int ble_mesh_get_key_phase(uint16_t dst, uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_KEY_PHASE_GET, params, sizeof(params), 0);
}

// Transition 2 starts sending with new keys; 3 revokes old keys and returns to 0.
static inline int ble_mesh_set_key_phase(uint16_t dst, uint16_t net_idx,
                                         uint8_t transition) {
    if (net_idx > 0x0fff || (transition != 2 && transition != 3)) return 0;
    uint8_t params[3] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8), transition};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_KEY_PHASE_SET, params, sizeof(params), 0);
}

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
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
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
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_APPKEY_DELETE, params, sizeof(params), 0);
}

static inline int ble_mesh_get_app_keys(uint16_t dst,
                                                uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
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
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
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
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_SIG_MODEL_APP_GET,
                                 params, sizeof(params), 0);
}

static inline int ble_mesh_get_composition(uint16_t dst, uint8_t page) {
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_COMPOSITION_GET, &page, 1, 0);
}

static inline int ble_mesh_get_default_ttl(uint16_t dst) {
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_DEFAULT_TTL_GET, NULL, 0, 0);
}

static inline int ble_mesh_set_default_ttl(uint16_t dst, uint8_t ttl) {
    if (ttl == 1 || ttl > 0x7f) return 0;
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_DEFAULT_TTL_SET, &ttl, 1, 0);
}

static inline int ble_mesh_get_publication(uint16_t dst, uint16_t element,
                                           uint16_t model) {
    if (!element || element > 0x7fff || mesh_publication_slot(model) < 0) return 0;
    uint8_t params[] = {(uint8_t)element, (uint8_t)(element >> 8),
                        (uint8_t)model, (uint8_t)(model >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_MODEL_PUB_GET, params, sizeof(params), 0);
}

// address 0 disables publication; has_label selects a Label UUID destination.
static inline int ble_mesh_set_publication(uint16_t dst, uint16_t element,
                                           uint16_t model, const mesh_publication *pub) {
    if (!pub || !element || element > 0x7fff || mesh_publication_slot(model) < 0 ||
        pub->has_label > 1 || ((pub->address || pub->has_label) &&
        (pub->app_idx > 0x0fff || (pub->ttl > 0x7f && pub->ttl != 0xff)))) return 0;
    if (!pub->has_label && ((pub->address >= 0x8000 && pub->address < 0xc000) ||
                           (pub->address >= 0xff00 && pub->address < 0xfffc))) return 0;
    uint8_t params[25] = {(uint8_t)element, (uint8_t)(element >> 8)};
    size_t offset = 4;
    if (pub->has_label) {
        memcpy(params + 2, pub->label, 16);
        offset = 18;
    } else {
        params[2] = (uint8_t)pub->address;
        params[3] = (uint8_t)(pub->address >> 8);
    }
    uint8_t enabled = pub->address || pub->has_label;
    params[offset++] = enabled ? (uint8_t)pub->app_idx : 0;
    params[offset++] = enabled ? (uint8_t)(pub->app_idx >> 8) : 0;
    params[offset++] = enabled ? pub->ttl : 0;
    params[offset++] = enabled ? pub->period : 0;
    params[offset++] = enabled ? pub->retransmit : 0;
    params[offset++] = (uint8_t)model;
    params[offset++] = (uint8_t)(model >> 8);
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        pub->has_label ? OP_CONFIG_MODEL_PUB_VIRTUAL_SET : OP_CONFIG_MODEL_PUB_SET,
        params, offset, 0);
}

static inline int ble_mesh_get_subscriptions(uint16_t dst, uint16_t element,
                                             uint16_t model) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model)) return 0;
    uint8_t params[] = {(uint8_t)element, (uint8_t)(element >> 8),
                        (uint8_t)model, (uint8_t)(model >> 8)};
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SIG_SUB_GET, params, sizeof(params), 0);
}

// A label replaces all subscriptions with that label; address 0 clears them.
static inline int ble_mesh_replace_subscription(uint16_t dst, uint16_t element,
    uint16_t model, uint16_t address, const uint8_t *label) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model) ||
        (!label && address && (address < 0xc000 || address > 0xfeff))) return 0;
    uint8_t params[20] = {(uint8_t)element, (uint8_t)(element >> 8)};
    uint32_t opcode = OP_CONFIG_MODEL_SUB_DELETE_ALL;
    size_t offset = 2;
    if (label) {
        memcpy(params + offset, label, 16);
        offset += 16;
        opcode = OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE;
    } else if (address) {
        params[offset++] = (uint8_t)address;
        params[offset++] = (uint8_t)(address >> 8);
        opcode = OP_CONFIG_MODEL_SUB_OVERWRITE;
    }
    params[offset++] = (uint8_t)model;
    params[offset++] = (uint8_t)(model >> 8);
    return ble_mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, opcode, params, offset, 0);
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
                                 message->src, mesh_models.state.default_ttl,
                                 message->app_key_index,
                                 OP_HEALTH_ATTENTION_STATUS, &remaining, 1, 0);
}

#endif
