#ifndef ISLER_MESH_FOUNDATION_H
#define ISLER_MESH_FOUNDATION_H

#include "mesh_4state.h"

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

#define OP_CONFIG_BEACON_GET 0x8009
#define OP_CONFIG_BEACON_SET 0x800a
#define OP_CONFIG_BEACON_STATUS 0x800b
#define OP_CONFIG_FRIEND_GET 0x800f
#define OP_CONFIG_FRIEND_SET 0x8010
#define OP_CONFIG_FRIEND_STATUS 0x8011
#define OP_CONFIG_PROXY_GET 0x8012
#define OP_CONFIG_PROXY_SET 0x8013
#define OP_CONFIG_PROXY_STATUS 0x8014
#define OP_CONFIG_NET_TRANSMIT_GET 0x8023
#define OP_CONFIG_NET_TRANSMIT_SET 0x8024
#define OP_CONFIG_NET_TRANSMIT_STATUS 0x8025
#define OP_CONFIG_RELAY_GET 0x8026
#define OP_CONFIG_RELAY_SET 0x8027
#define OP_CONFIG_RELAY_STATUS 0x8028
#define OP_CONFIG_NODE_IDENTITY_GET 0x8046
#define OP_CONFIG_NODE_IDENTITY_SET 0x8047
#define OP_CONFIG_NODE_IDENTITY_STATUS 0x8048
#define OP_CONFIG_NODE_RESET 0x8049
#define OP_CONFIG_NODE_RESET_STATUS 0x804a
#define OP_CONFIG_SAR_TRANSMITTER_GET 0x806c
#define OP_CONFIG_SAR_TRANSMITTER_SET 0x806d
#define OP_CONFIG_SAR_TRANSMITTER_STATUS 0x806e
#define OP_CONFIG_SAR_RECEIVER_GET 0x806f
#define OP_CONFIG_SAR_RECEIVER_SET 0x8070
#define OP_CONFIG_SAR_RECEIVER_STATUS 0x8071
#define OP_CONFIG_HEARTBEAT_PUB_GET 0x8038
#define OP_CONFIG_HEARTBEAT_PUB_SET 0x8039
#define OP_CONFIG_HEARTBEAT_PUB_STATUS 0x06
#define OP_CONFIG_HEARTBEAT_SUB_GET 0x803a
#define OP_CONFIG_HEARTBEAT_SUB_SET 0x803b
#define OP_CONFIG_HEARTBEAT_SUB_STATUS 0x803c
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
#define OP_HEALTH_FAULT_STATUS 0x05
#define OP_HEALTH_FAULT_CLEAR 0x802f
#define OP_HEALTH_FAULT_CLEAR_UNACK 0x8030
#define OP_HEALTH_FAULT_GET 0x8031
#define OP_HEALTH_FAULT_TEST 0x8032
#define OP_HEALTH_FAULT_TEST_UNACK 0x8033
#define OP_HEALTH_PERIOD_GET 0x8034
#define OP_HEALTH_PERIOD_SET 0x8035
#define OP_HEALTH_PERIOD_SET_UNACK 0x8036
#define OP_HEALTH_PERIOD_STATUS 0x8037
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

void BLE_MESH_CONFIG_STATUS(
    uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len);
// Health Client status parameters are valid only during this callback.
void BLE_MESH_HEALTH_STATUS(
    uint16_t element, uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len);
void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds);
// Run a supported self-test and fill faults; *len is capacity on input/count on output.
// Return 1 for a completed test, 0 for an unsupported test or failure.
int BLE_MESH_HEALTH_TEST(uint16_t element, uint8_t test_id, uint8_t *faults, size_t *len);

// Queue to a unicast/group address, or to a Label UUID when label is set.
static int mesh_health_queue(
    uint16_t element, uint16_t dst,
                             const uint8_t *label, uint16_t app_idx,
                             uint32_t opcode, const uint8_t *params, size_t len
) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || mesh_models.reset_pending || index < 0) return 0;
    if (label)
        return app_key_allowed((uint8_t)index, MESH_MODEL_HEALTH_CLIENT, app_idx) &&
            mesh_access_queue_virtual(element, label, mesh_models.state.default_ttl,
                app_idx, opcode, params, len, 0);
    if (dst == 0) {
        mesh_publication *pub = &mesh_models.state.publications[index][3];
        if (!pub->address || pub->app_idx != app_idx) return 0;
        return mesh_publication_begin((uint8_t)index, MESH_MODEL_HEALTH_CLIENT,
            opcode, params, len);
    }
    if (!app_key_allowed((uint8_t)index, MESH_MODEL_HEALTH_CLIENT, app_idx)) return 0;
    uint32_t response_opcode =
        opcode == OP_HEALTH_FAULT_GET || opcode == OP_HEALTH_FAULT_CLEAR ||
        opcode == OP_HEALTH_FAULT_TEST ? OP_HEALTH_FAULT_STATUS :
        opcode == OP_HEALTH_PERIOD_GET || opcode == OP_HEALTH_PERIOD_SET ?
            OP_HEALTH_PERIOD_STATUS :
        opcode == OP_HEALTH_ATTENTION_GET || opcode == OP_HEALTH_ATTENTION_SET ?
            OP_HEALTH_ATTENTION_STATUS : 0;
    if (response_opcode && dst > 0 && dst <= 0x7fff)
        return mesh_access_queue_acknowledged(element, dst,
            mesh_models.state.default_ttl, app_idx, opcode, response_opcode,
            params, len, 0, MESH_ACCESS_ACK_TIMEOUT_MS,
            MESH_ACCESS_ACK_RETRY_COUNT);
    return mesh_access_queue(element, dst, mesh_models.state.default_ttl,
        app_idx, opcode, params, len, 0);
}

// Health Client requests use a bound AppKey; acknowledged sets request a status reply.
static inline int mesh_health_fault_get(
    uint16_t element, uint16_t dst, uint16_t app_idx, uint16_t company
) {
    uint8_t params[2] = {(uint8_t)company, (uint8_t)(company >> 8)};
    return mesh_health_queue(element, dst, NULL, app_idx, OP_HEALTH_FAULT_GET, params, sizeof(params));
}

static inline int mesh_health_fault_get_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx, uint16_t company
) {
    if (!label) return 0;
    uint8_t params[2] = {(uint8_t)company, (uint8_t)(company >> 8)};
    return mesh_health_queue(element, 0, label, app_idx, OP_HEALTH_FAULT_GET, params, sizeof(params));
}

static inline int mesh_health_fault_clear(
    uint16_t element, uint16_t dst, uint16_t app_idx, uint16_t company, uint8_t acknowledged
) {
    uint8_t params[2] = {(uint8_t)company, (uint8_t)(company >> 8)};
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, dst, NULL, app_idx,
        acknowledged ? OP_HEALTH_FAULT_CLEAR : OP_HEALTH_FAULT_CLEAR_UNACK,
        params, sizeof(params));
}

static inline int mesh_health_fault_clear_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx, uint16_t company, uint8_t acknowledged
) {
    if (!label) return 0;
    uint8_t params[2] = {(uint8_t)company, (uint8_t)(company >> 8)};
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, 0, label, app_idx,
        acknowledged ? OP_HEALTH_FAULT_CLEAR : OP_HEALTH_FAULT_CLEAR_UNACK,
        params, sizeof(params));
}

static inline int mesh_health_fault_test(
    uint16_t element, uint16_t dst, uint16_t app_idx, uint16_t company, uint8_t test_id, uint8_t acknowledged
) {
    uint8_t params[3] = {test_id, (uint8_t)company, (uint8_t)(company >> 8)};
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, dst, NULL, app_idx,
        acknowledged ? OP_HEALTH_FAULT_TEST : OP_HEALTH_FAULT_TEST_UNACK,
        params, sizeof(params));
}

static inline int mesh_health_fault_test_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx, uint16_t company, uint8_t test_id, uint8_t acknowledged
) {
    if (!label) return 0;
    uint8_t params[3] = {test_id, (uint8_t)company, (uint8_t)(company >> 8)};
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, 0, label, app_idx,
        acknowledged ? OP_HEALTH_FAULT_TEST : OP_HEALTH_FAULT_TEST_UNACK,
        params, sizeof(params));
}

static inline int mesh_health_period_get(
    uint16_t element, uint16_t dst, uint16_t app_idx
) {
    return mesh_health_queue(element, dst, NULL, app_idx, OP_HEALTH_PERIOD_GET, NULL, 0);
}

static inline int mesh_health_period_get_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx
) {
    if (!label) return 0;
    return mesh_health_queue(element, 0, label, app_idx, OP_HEALTH_PERIOD_GET, NULL, 0);
}

static inline int mesh_health_period_set(
    uint16_t element, uint16_t dst, uint16_t app_idx, uint8_t divisor, uint8_t acknowledged
) {
    if (divisor > 15 || acknowledged > 1) return 0;
    return mesh_health_queue(element, dst, NULL, app_idx,
        acknowledged ? OP_HEALTH_PERIOD_SET : OP_HEALTH_PERIOD_SET_UNACK,
        &divisor, 1);
}

static inline int mesh_health_period_set_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx, uint8_t divisor, uint8_t acknowledged
) {
    if (!label) return 0;
    if (divisor > 15 || acknowledged > 1) return 0;
    return mesh_health_queue(element, 0, label, app_idx,
        acknowledged ? OP_HEALTH_PERIOD_SET : OP_HEALTH_PERIOD_SET_UNACK,
        &divisor, 1);
}

static inline int mesh_health_attention_get(
    uint16_t element, uint16_t dst, uint16_t app_idx
) {
    return mesh_health_queue(element, dst, NULL, app_idx, OP_HEALTH_ATTENTION_GET, NULL, 0);
}

static inline int mesh_health_attention_get_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx
) {
    if (!label) return 0;
    return mesh_health_queue(element, 0, label, app_idx, OP_HEALTH_ATTENTION_GET, NULL, 0);
}

static inline int mesh_health_attention_set(
    uint16_t element, uint16_t dst, uint16_t app_idx, uint8_t seconds, uint8_t acknowledged
) {
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, dst, NULL, app_idx,
        acknowledged ? OP_HEALTH_ATTENTION_SET : OP_HEALTH_ATTENTION_SET_UNACK,
        &seconds, 1);
}

static inline int mesh_health_attention_set_virtual(
    uint16_t element, const uint8_t label[16], uint16_t app_idx, uint8_t seconds, uint8_t acknowledged
) {
    if (!label) return 0;
    if (acknowledged > 1) return 0;
    return mesh_health_queue(element, 0, label, app_idx,
        acknowledged ? OP_HEALTH_ATTENTION_SET : OP_HEALTH_ATTENTION_SET_UNACK,
        &seconds, 1);
}

static inline int mesh_config_virtual_sub(
    uint16_t dst, uint16_t element,
    uint16_t model, const uint8_t label[16], uint8_t add
) {
    if (!label || !element || element > 0x7fff ||
        !mesh_virtual_model_valid(model))
        return 0;

    uint8_t params[20] = {(uint8_t)element, (uint8_t)(element >> 8)};
    memcpy(params + 2, label, 16);
    params[18] = (uint8_t)model;
    params[19] = (uint8_t)(model >> 8);

    return mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        add ? OP_CONFIG_MODEL_SUB_VIRTUAL_ADD :
              OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE, params, sizeof(params), 0);
}

static inline int mesh_group_subscription(
    uint16_t dst, uint16_t element,
                                            uint16_t model, uint16_t group,
                                            uint8_t add
) {
    if (!element || element > 0x7fff || group < 0xc000 || group > 0xfeff ||
        !mesh_virtual_model_valid(model))
        return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)group, (uint8_t)(group >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        add ? OP_CONFIG_MODEL_SUB_ADD : OP_CONFIG_MODEL_SUB_DELETE,
        params, sizeof(params), 0);
}

static int mesh_unbind_slot(uint8_t slot) {
    mesh_models_state next = mesh_models.state;
    uint8_t bit = (uint8_t)~(1u << slot);
    next.onoff_server_bindings &= bit;
    next.onoff_client_bindings &= bit;
    next.health_server_bindings &= bit;
    next.health_client_bindings &= bit;
    for (uint8_t i = 1; i < mesh_network.state.element_count; i++) {
        next.other[i - 1].onoff_server_bindings &= bit;
        next.other[i - 1].onoff_client_bindings &= bit;
        next.other[i - 1].health_server_bindings &= bit;
        next.other[i - 1].health_client_bindings &= bit;
    }
    uint16_t app_idx = mesh_network.state.app_keys[slot].index;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++)
        for (uint8_t j = 0; j < MESH_PUBLICATION_MODELS; j++)
            if (next.publications[i][j].address && next.publications[i][j].app_idx == app_idx)
                memset(&next.publications[i][j], 0, sizeof(next.publications[i][j]));
    if (memcmp(&next, &mesh_models.state, sizeof(next)) != 0 &&
        BLE_MESH_MODELS_SAVE_STATE(&next) != 1)
        return 0;
    mesh_models.state = next;
    return 1;
}

// Pack 12-bit AppKey indexes as Bluetooth Mesh key-index pairs.
static size_t mesh_pack_app_indexes(
    uint8_t *out, const uint16_t *indexes,
                                    uint8_t count
) {
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
static uint8_t mesh_subscription_replace(
    uint16_t element, uint16_t model,
                                          uint16_t address, const uint8_t *label
) {
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
        BLE_MESH_MODELS_SAVE_STATE(&next) != 1)
        return MESH_CONFIG_STORAGE_FAILURE;
    mesh_models.state = next;
    mesh_transport_clear_labels();
    for (uint8_t i = 0; i < next.virtual_count; i++)
        mesh_label_add(next.virtual[i].label);
    return MESH_CONFIG_SUCCESS;
}

// Queue Node Reset Status, then clear the node after it finishes advertising.
// Return 1 on success, 0 if reset cannot be scheduled.
int BLE_MESH_NODE_RESET(uint16_t dst);

// Publication counts round up; subscription counts and periods round down.
static uint8_t mesh_heartbeat_log(uint32_t value, uint8_t round_up) {
    if (!value) return 0;
    uint8_t log = 1;
    if (round_up) {
        value--;
        while (value) { log++; value >>= 1; }
        return log;
    }
    while (value >>= 1) log++;
    return log;
}

// Config Server: persisted node and model configuration.

static int server_config_receive(const mesh_access_pdu *message) {
    const mesh_net_state *state = &mesh_network.state;
    mesh_network.reply_net_idx = message->net_key_index;
    const uint8_t *p = message->params;
    size_t len = message->params_len;

    if (message->opcode == OP_CONFIG_NODE_RESET) {
        if (len) return 0;
        mesh_models.reset_pending = BLE_MESH_NODE_RESET(message->src) == 1;
        return mesh_models.reset_pending;
    }

    if (message->opcode == OP_CONFIG_SAR_TRANSMITTER_GET ||
        message->opcode == OP_CONFIG_SAR_TRANSMITTER_SET) {
        uint8_t set = message->opcode == OP_CONFIG_SAR_TRANSMITTER_SET;
        if (len != (set ? 4u : 0u) || (set && (p[3] & 0xf0))) return 0;
        uint8_t status = MESH_CONFIG_SUCCESS;
        if (set) {
            mesh_sar_tx_state next = {
                p[0] & 0x0f, p[0] >> 4,
                p[1] & 0x0f, p[1] >> 4,
                p[2] & 0x0f, p[2] >> 4,
                p[3] & 0x0f
            };
            mesh_models_state saved = mesh_models.state;
            saved.sar_transmitter = next;
            if (memcmp(&saved, &mesh_models.state, sizeof(saved)) &&
                BLE_MESH_MODELS_SAVE_STATE(&saved) != 1)
                status = MESH_CONFIG_STORAGE_FAILURE;
            else {
                mesh_models.state = saved;
                if (mesh_sar_tx_valid(&next)) transport_sar_tx = next;
            }
        }
        const mesh_sar_tx_state *sar = &mesh_models.state.sar_transmitter;
        uint8_t reply[5] = {
            status,
            (uint8_t)(sar->segment_interval_step |
                      (sar->unicast_retrans_count << 4)),
            (uint8_t)(sar->unicast_retrans_wo_progress_count |
                      (sar->unicast_retrans_interval_step << 4)),
            (uint8_t)(sar->unicast_retrans_interval_increment |
                      (sar->multicast_retrans_count << 4)),
            sar->multicast_retrans_interval_step
        };
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_SAR_TRANSMITTER_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_SAR_RECEIVER_GET ||
        message->opcode == OP_CONFIG_SAR_RECEIVER_SET) {
        uint8_t set = message->opcode == OP_CONFIG_SAR_RECEIVER_SET;
        if (len != (set ? 3u : 0u) || (set && (p[2] & 0xfc))) return 0;
        uint8_t status = MESH_CONFIG_SUCCESS;
        if (set) {
            mesh_sar_rx_state next = {
                p[0] & 0x1f, p[0] >> 5,
                p[1] & 0x0f, p[1] >> 4,
                p[2] & 0x03
            };
            mesh_models_state saved = mesh_models.state;
            saved.sar_receiver = next;
            if (memcmp(&saved, &mesh_models.state, sizeof(saved)) &&
                BLE_MESH_MODELS_SAVE_STATE(&saved) != 1)
                status = MESH_CONFIG_STORAGE_FAILURE;
            else {
                mesh_models.state = saved;
                if (mesh_sar_rx_valid(&next)) transport_sar_rx = next;
            }
        }
        const mesh_sar_rx_state *sar = &mesh_models.state.sar_receiver;
        uint8_t reply[4] = {
            status,
            (uint8_t)(sar->segments_threshold |
                      (sar->ack_delay_increment << 5)),
            (uint8_t)(sar->discard_timeout |
                      (sar->segment_interval_step << 4)),
            sar->ack_retrans_count
        };
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_SAR_RECEIVER_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_HEARTBEAT_PUB_GET ||
        message->opcode == OP_CONFIG_HEARTBEAT_PUB_SET) {
        uint8_t set = message->opcode == OP_CONFIG_HEARTBEAT_PUB_SET;
        if (len != (set ? 9u : 0u)) return 0;
        mesh_heartbeat_publication pub = state->heartbeat;
        uint8_t status = MESH_CONFIG_SUCCESS;
        uint16_t count = mesh_network.heartbeat.remaining;
        if (set) {
            pub = (mesh_heartbeat_publication){
                .dst = p[0] | (uint16_t)p[1] << 8,
                .count_log = p[2], .period_log = p[3], .ttl = p[4],
                .features = p[5] | (uint16_t)p[6] << 8,
                .net_idx = p[7] | (uint16_t)p[8] << 8};
            if (pub.ttl > 0x7f || pub.net_idx > 0x0fff) return 0;
            if ((pub.dst >= 0x8000 && pub.dst < 0xc000) ||
                (pub.dst >= 0xff00 && pub.dst < 0xfffc))
                status = MESH_CONFIG_INVALID_ADDRESS;
            else if ((pub.count_log > 0x11 && pub.count_log != 0xff) || pub.period_log > 0x11)
                status = MESH_CONFIG_CANNOT_SET;
            else if (pub.dst && mesh_subnet_slot(state, pub.net_idx) < 0)
                status = MESH_CONFIG_INVALID_NETKEY;
            if (status == MESH_CONFIG_SUCCESS) {
                if (!pub.dst) memset(&pub, 0, sizeof(pub));
                pub.features = 0; // Feature-change Heartbeat triggers are not tracked.
                mesh_net_state next = *state;
                next.heartbeat = pub;
                if (memcmp(&next, state, sizeof(next)) && !mesh_commit(&next))
                    status = MESH_CONFIG_STORAGE_FAILURE;
                else {
                    count = pub.count_log == 0xff ? 0xffff :
                            pub.count_log == 0x11 ? 0xfffe :
                            pub.count_log ? (uint16_t)(1u << (pub.count_log - 1)) : 0;
                    mesh_network.heartbeat.remaining = count;
                    mesh_network.heartbeat.publish_at_ms = GET_MILLIS();
                }
            }
            if (status != MESH_CONFIG_SUCCESS) {
                count = pub.count_log == 0xff ? 0xffff :
                        pub.count_log <= 0x11 && pub.count_log ?
                        (uint16_t)(pub.count_log == 0x11 ? 0xfffe : 1u << (pub.count_log - 1)) : 0;
            }
        }
        uint8_t reply[10] = {status, (uint8_t)pub.dst, (uint8_t)(pub.dst >> 8)};
        if (pub.dst) {
            reply[3] = count == 0xffff ? 0xff : mesh_heartbeat_log(count, 1);
            if (set && (pub.count_log > 0x11 && pub.count_log != 0xff)) reply[3] = pub.count_log;
            reply[4] = pub.period_log;
            reply[5] = pub.ttl;
            reply[6] = (uint8_t)pub.features; reply[7] = (uint8_t)(pub.features >> 8);
            reply[8] = (uint8_t)pub.net_idx; reply[9] = (uint8_t)(pub.net_idx >> 8);
        }
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_HEARTBEAT_PUB_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_HEARTBEAT_SUB_GET ||
        message->opcode == OP_CONFIG_HEARTBEAT_SUB_SET) {
        uint8_t set = message->opcode == OP_CONFIG_HEARTBEAT_SUB_SET;
        if (len != (set ? 5u : 0u)) return 0;
        uint8_t stopped = 0;
        if (set) {
            uint16_t src = p[0] | (uint16_t)p[1] << 8;
            uint16_t dst = p[2] | (uint16_t)p[3] << 8;
            if (src > 0x7fff || p[4] > 0x11 ||
                (dst && dst != state->unicast_address && dst < 0xc000) ||
                (dst >= 0xff00 && dst < 0xfffc))
                return 0;
            if (!src || !dst) {
                mesh_network.heartbeat.src = mesh_network.heartbeat.dst = 0;
                mesh_network.heartbeat.count = 0;
                mesh_network.heartbeat.min_hops = mesh_network.heartbeat.max_hops = 0;
                mesh_network.heartbeat.expires_at_ms = GET_MILLIS();
                mesh_network.heartbeat.subscribed = 0;
            } else if (p[4]) {
                mesh_network.heartbeat.src = src; mesh_network.heartbeat.dst = dst;
                mesh_network.heartbeat.count = 0;
                mesh_network.heartbeat.min_hops = 0x7f; mesh_network.heartbeat.max_hops = 0;
                mesh_network.heartbeat.expires_at_ms = GET_MILLIS() + (1u << (p[4] - 1)) * 1000u;
                mesh_network.heartbeat.subscribed = 1;
            } else {
                mesh_network.heartbeat.expires_at_ms = GET_MILLIS();
                mesh_network.heartbeat.subscribed = 0;
                stopped = 1;
            }
        }
        int32_t ms = (int32_t)(mesh_network.heartbeat.expires_at_ms - GET_MILLIS());
        if (ms <= 0) mesh_network.heartbeat.subscribed = 0;
        uint32_t seconds = mesh_network.heartbeat.subscribed ? (uint32_t)ms / 1000u : 0;
        uint16_t src = mesh_network.heartbeat.src, dst = mesh_network.heartbeat.dst;
        uint16_t count = mesh_network.heartbeat.count;
        uint8_t reply[] = {MESH_CONFIG_SUCCESS, (uint8_t)src, (uint8_t)(src >> 8),
            (uint8_t)dst, (uint8_t)(dst >> 8), mesh_heartbeat_log(seconds, 0),
            count == 0xffff ? 0xff : mesh_heartbeat_log(count, 0),
            set && (!src || stopped) ? 0x7f : mesh_network.heartbeat.min_hops,
            mesh_network.heartbeat.max_hops};
        int result = mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_HEARTBEAT_SUB_STATUS, reply, sizeof(reply), 0);
        if (stopped) mesh_network.heartbeat.count = 0;
        return result;
    }

    if (message->opcode == OP_CONFIG_BEACON_GET ||
        message->opcode == OP_CONFIG_BEACON_SET ||
        message->opcode == OP_CONFIG_NET_TRANSMIT_GET ||
        message->opcode == OP_CONFIG_NET_TRANSMIT_SET) {
        uint8_t beacon = message->opcode == OP_CONFIG_BEACON_GET ||
                         message->opcode == OP_CONFIG_BEACON_SET;
        uint8_t set = message->opcode == OP_CONFIG_BEACON_SET ||
                      message->opcode == OP_CONFIG_NET_TRANSMIT_SET;
        if (len != (set ? 1u : 0u) || (set && beacon && p[0] > 1)) return 0;
        if (set) {
            mesh_net_state next = *state;
            if (beacon) next.beacon = p[0];
            else next.network_transmit = p[0];
            if (memcmp(&next, state, sizeof(next)) && !mesh_commit(&next)) return 0;
        }
        uint8_t reply = beacon ? state->beacon : state->network_transmit;
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            beacon ? OP_CONFIG_BEACON_STATUS : OP_CONFIG_NET_TRANSMIT_STATUS,
            &reply, 1, 0);
    }

    if (message->opcode == OP_CONFIG_RELAY_GET ||
        message->opcode == OP_CONFIG_RELAY_SET) {
        uint8_t set = message->opcode == OP_CONFIG_RELAY_SET;
        if (len != (set ? 2u : 0u) || (set && p[0] > 1)) return 0;
        if (set) {
            mesh_net_state next = *state;
            next.relay = p[0];
            next.relay_retransmit = p[1];
            if (memcmp(&next, state, sizeof(next)) && !mesh_commit(&next)) return 0;
        }
        uint8_t reply[2] = {state->relay, state->relay_retransmit};
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_RELAY_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_PROXY_GET ||
        message->opcode == OP_CONFIG_PROXY_SET ||
        message->opcode == OP_CONFIG_FRIEND_GET ||
        message->opcode == OP_CONFIG_FRIEND_SET) {
        uint8_t proxy = message->opcode == OP_CONFIG_PROXY_GET ||
                        message->opcode == OP_CONFIG_PROXY_SET;
        uint8_t set = message->opcode == OP_CONFIG_PROXY_SET ||
                      message->opcode == OP_CONFIG_FRIEND_SET;
        if (len != (set ? 1u : 0u) || (set && p[0] > 1)) return 0;
        uint8_t reply = 2; // Proxy and Friend are not supported.
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            proxy ? OP_CONFIG_PROXY_STATUS : OP_CONFIG_FRIEND_STATUS,
            &reply, 1, 0);
    }

    if (message->opcode == OP_CONFIG_NODE_IDENTITY_GET ||
        message->opcode == OP_CONFIG_NODE_IDENTITY_SET) {
        uint8_t set = message->opcode == OP_CONFIG_NODE_IDENTITY_SET;
        if (len != (set ? 3u : 2u) || (p[1] & 0xf0) || (set && p[2] > 1)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t known = mesh_subnet_slot(state, net_idx) >= 0;
        uint8_t reply[4] = {known ? set ? MESH_CONFIG_FEATURE_NOT_SUPPORTED : MESH_CONFIG_SUCCESS :
                            MESH_CONFIG_INVALID_NETKEY, p[0], p[1],
                            known ? 2 : set ? p[2] : 0};
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_NODE_IDENTITY_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_NETKEY_GET) {
        if (len != 0) return 0;
        uint8_t reply[2 + 2 * (MESH_MAX_SUBNETS - 1)] = {0};
        uint16_t indexes[MESH_MAX_SUBNETS];
        uint8_t count = 0;
        indexes[count++] = state->net_key_index;
        for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++)
            if (state->additional_subnets[i].used)
                indexes[count++] = state->additional_subnets[i].index;
        size_t reply_len = mesh_pack_app_indexes(reply, indexes, count);
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_NETKEY_LIST, reply, reply_len, 0);
    }

    if (message->opcode == OP_CONFIG_NETKEY_ADD ||
        message->opcode == OP_CONFIG_NETKEY_UPDATE ||
        message->opcode == OP_CONFIG_NETKEY_DELETE) {
        uint8_t remove = message->opcode == OP_CONFIG_NETKEY_DELETE;
        if (len != (remove ? 2u : 18u) || (p[1] & 0xf0)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t status = MESH_CONFIG_SUCCESS;
        if (message->opcode == OP_CONFIG_NETKEY_ADD) {
            int subnet = mesh_subnet_slot(state, net_idx);
            if (subnet >= 0) {
                uint8_t current[16];
                mesh_subnet_key(state, net_idx, current, 0);
                if (memcmp(current, p + 2, 16)) status = MESH_CONFIG_KEY_ALREADY_STORED;
            } else {
                int free_slot = -1;
                for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++)
                    if (!state->additional_subnets[i].used) { free_slot = i; break; }
                if (free_slot < 0) status = MESH_CONFIG_INSUFFICIENT_RESOURCES;
                else {
                    mesh_net_state next = *state;
                    mesh_additional_subnet *sub = &next.additional_subnets[free_slot];
                    memset(sub, 0, sizeof(*sub));
                    sub->used = 1;
                    sub->index = net_idx;
                    memcpy(sub->key, p + 2, 16);
                    if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
                }
            }
        } else if (remove) {
            int subnet = mesh_subnet_slot(state, net_idx);
            if (subnet == 0 || subnet > 0) {
                uint8_t subnet_count = 1;
                for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++)
                    subnet_count += state->additional_subnets[i].used != 0;
                if (subnet_count == 1) status = MESH_CONFIG_CANNOT_REMOVE;
            }
            if (status == MESH_CONFIG_SUCCESS && subnet > 0) {
                for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
                    const mesh_app_key *app = &state->app_keys[i];
                    if (app->used && mesh_app_net_idx(state, app) == net_idx &&
                        !mesh_unbind_slot(i)) { status = MESH_CONFIG_STORAGE_FAILURE; break; }
                }
                if (status == MESH_CONFIG_SUCCESS) {
                    mesh_net_state next = *state;
                    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
                        if (next.app_keys[i].used && next.app_keys[i].net_idx == net_idx)
                            memset(&next.app_keys[i], 0, sizeof(next.app_keys[i]));
                    if (next.heartbeat.dst && next.heartbeat.net_idx == net_idx)
                        memset(&next.heartbeat, 0, sizeof(next.heartbeat));
                    memset(&next.additional_subnets[subnet - 1], 0,
                           sizeof(next.additional_subnets[subnet - 1]));
                    if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
                }
            } else if (status == MESH_CONFIG_SUCCESS && subnet == 0) {
                int promote = -1;
                for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++)
                    if (state->additional_subnets[i].used) { promote = i; break; }
                if (promote < 0) status = MESH_CONFIG_CANNOT_REMOVE;
                for (uint8_t i = 0; status == MESH_CONFIG_SUCCESS && i < MESH_MAX_APP_KEYS; i++) {
                    const mesh_app_key *app = &state->app_keys[i];
                    if (app->used && mesh_app_net_idx(state, app) == net_idx &&
                        !mesh_unbind_slot(i)) status = MESH_CONFIG_STORAGE_FAILURE;
                }
                if (status == MESH_CONFIG_SUCCESS) {
                    mesh_net_state next = *state;
                    mesh_additional_subnet *sub = &next.additional_subnets[promote];
                    memcpy(next.net_key, sub->key, 16);
                    memcpy(next.new_net_key, sub->new_key, 16);
                    next.net_key_index = sub->index;
                    next.has_new_key = sub->has_new_key;
                    next.key_refresh_phase = sub->key_refresh_phase;
                    next.phase2_provisioned = sub->phase2_provisioned;
                    memset(sub, 0, sizeof(*sub));
                    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
                        if (next.app_keys[i].used && next.app_keys[i].net_idx == net_idx)
                            memset(&next.app_keys[i], 0, sizeof(next.app_keys[i]));
                    if (next.heartbeat.dst && next.heartbeat.net_idx == net_idx)
                        memset(&next.heartbeat, 0, sizeof(next.heartbeat));
                    if (!mesh_commit(&next)) status = MESH_CONFIG_STORAGE_FAILURE;
                }
            }
        } else {
            uint8_t current_key[16], staged_key[16];
            uint8_t phase = mesh_subnet_phase(state, net_idx);
            if (phase == 0xff) status = MESH_CONFIG_INVALID_NETKEY;
            else {
                mesh_subnet_key(state, net_idx, current_key, 0);
                if (phase == 0 && !memcmp(current_key, p + 2, 16))
                    status = MESH_CONFIG_KEY_ALREADY_STORED;
                else if (phase == 2 ||
                         (phase == 1 &&
                          (!mesh_subnet_key(state, net_idx, staged_key, 1) ||
                           memcmp(staged_key, p + 2, 16))))
                    status = MESH_CONFIG_CANNOT_UPDATE;
                else if (!mesh_stage_net_key(net_idx, p + 2))
                    status = MESH_CONFIG_STORAGE_FAILURE;
            }
        }

        uint8_t reply[3] = {status, p[0], p[1]};
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_NETKEY_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_KEY_PHASE_GET ||
        message->opcode == OP_CONFIG_KEY_PHASE_SET) {
        uint8_t set = message->opcode == OP_CONFIG_KEY_PHASE_SET;
        if (len != (set ? 3u : 2u) || (p[1] & 0xf0) ||
            (set && p[2] != 2 && p[2] != 3))
            return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t phase = mesh_subnet_phase(state, net_idx);
        uint8_t status = phase == 0xff ?
            MESH_CONFIG_INVALID_NETKEY : MESH_CONFIG_SUCCESS;
        if (set && status == MESH_CONFIG_SUCCESS) {
            if (p[2] == 2 && phase == 0) status = MESH_CONFIG_CANNOT_UPDATE;
            else {
                mesh_net_state next = mesh_network.state;
                if (!mesh_key_refresh_transition_apply(&next, net_idx, p[2]))
                    status = MESH_CONFIG_STORAGE_FAILURE;
                else if (memcmp(&next, &mesh_network.state, sizeof(next)) != 0 &&
                         !mesh_commit(&next))
                    status = MESH_CONFIG_STORAGE_FAILURE;
            }
            phase = mesh_subnet_phase(&mesh_network.state, net_idx);
        }
        uint8_t reply[4] = {status, p[0], p[1],
                            phase != 0xff ? phase : 0};
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_KEY_PHASE_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_COMPOSITION_GET) {
        if (len != 1) return 0;
        // Page 0 is the highest supported page, including for unknown requests.
        uint8_t reply[11 + 18 + (MESH_MAX_ELEMENTS - 1) * 12] = {
            0, (uint8_t)MESH_COMPANY_ID, (uint8_t)(MESH_COMPANY_ID >> 8),
            (uint8_t)MESH_PRODUCT_ID, (uint8_t)(MESH_PRODUCT_ID >> 8),
            (uint8_t)MESH_PRODUCT_VERSION, (uint8_t)(MESH_PRODUCT_VERSION >> 8),
            (uint8_t)MESH_NETWORK_REPLAY_SLOTS, (uint8_t)(MESH_NETWORK_REPLAY_SLOTS >> 8),
            1, 0 // Relay is supported; Proxy, Friend, and Low Power are not.
        };
        size_t size = 11;
        for (uint8_t i = 0; i < state->element_count; i++) {
            reply[size++] = 0; reply[size++] = 0; // Unknown element location.
            reply[size++] = i ? 4 : 7;
            reply[size++] = 0; // No vendor models.
            const uint16_t models[] = {MESH_MODEL_CONFIG_SERVER, MESH_MODEL_CONFIG_CLIENT,
                MESH_MODEL_SAR_CONFIG_SERVER, MESH_MODEL_HEALTH_SERVER,
                MESH_MODEL_HEALTH_CLIENT, MESH_MODEL_ONOFF_SERVER, MESH_MODEL_ONOFF_CLIENT};
            for (uint8_t j = i ? 3 : 0; j < 7; j++) {
                reply[size++] = (uint8_t)models[j];
                reply[size++] = (uint8_t)(models[j] >> 8);
            }
        }
        return mesh_access_queue(state->unicast_address, message->src,
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
                BLE_MESH_MODELS_SAVE_STATE(&next) != 1)
                return 0;
            mesh_models.state = next;
        } else if (len != 0) return 0;
        return mesh_access_queue(state->unicast_address, message->src,
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
            next.address = virtual ? mesh_virtual_address(p + 2) :
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
                        GET_MILLIS() + (model == MESH_MODEL_HEALTH_SERVER ?
                        mesh_health_period((uint8_t)index) : mesh_publication_period(next.period));
                }
            }
        }
        uint8_t reply[12] = {status, p[0], p[1],
            (uint8_t)pub.address, (uint8_t)(pub.address >> 8),
            (uint8_t)pub.app_idx, (uint8_t)(pub.app_idx >> 8),
            pub.ttl, pub.period, pub.retransmit,
            (uint8_t)model, (uint8_t)(model >> 8)};
        return mesh_access_queue(state->unicast_address, message->src,
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
                uint16_t address = mesh_virtual_address(entry->label);
                size_t j = 5;
                while (j < size && (reply[j] | ((uint16_t)reply[j + 1] << 8)) != address) j += 2;
                if (j < size) continue; // Colliding labels share one listed address.
                reply[size++] = (uint8_t)address;
                reply[size++] = (uint8_t)(address >> 8);
            }
        }
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_SIG_MODEL_SUB_LIST, reply, size, 0);
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_DELETE_ALL) {
        if (len != 4) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[2] | ((uint16_t)p[3] << 8);
        uint8_t reply[7] = {mesh_subscription_replace(element, model, 0, NULL),
            p[0], p[1], 0, 0, p[2], p[3]};
        return mesh_access_queue(state->unicast_address, message->src,
            mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
            OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
    }

    if (message->opcode == OP_CONFIG_APPKEY_GET) {
        if (len != 2 || (p[1] & 0xf0)) return 0;
        uint16_t net_idx = p[0] | ((uint16_t)p[1] << 8);
        uint8_t reply[3 + MESH_APP_INDEX_BYTES] = {
            mesh_subnet_slot(state, net_idx) >= 0 ? MESH_CONFIG_SUCCESS :
                                               MESH_CONFIG_INVALID_NETKEY,
            p[0], p[1]
        };
        uint16_t indexes[MESH_MAX_APP_KEYS];
        uint8_t count = 0;
        if (reply[0] == MESH_CONFIG_SUCCESS) {
            for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++)
                if (state->app_keys[i].used &&
                    mesh_app_net_idx(state, &state->app_keys[i]) == net_idx)
                    indexes[count++] = state->app_keys[i].index;
        }
        size_t reply_len = 3 + mesh_pack_app_indexes(reply + 3, indexes, count);
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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

        if (mesh_subnet_slot(state, net_idx) < 0)
            status = MESH_CONFIG_INVALID_NETKEY;
        else if (message->opcode == OP_CONFIG_APPKEY_ADD) {
            if (slot >= 0) {
                if (mesh_app_net_idx(state, &state->app_keys[slot]) != net_idx)
                    status = MESH_CONFIG_INVALID_APPKEY;
                else if (memcmp(state->app_keys[slot].key, p + 3, 16) != 0)
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
                    app->net_idx = net_idx;
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
            if (mesh_app_net_idx(state, app) != net_idx)
                status = MESH_CONFIG_INVALID_APPKEY;
            else if (mesh_subnet_phase(state, net_idx) != 1 ||
                (app->has_new_key && memcmp(app->new_key, p + 3, 16)) ||
                (!app->has_new_key && !memcmp(app->key, p + 3, 16)))
                status = MESH_CONFIG_CANNOT_UPDATE;
            else if (!mesh_stage_app_key_for(net_idx, app_idx, p + 3))
                status = MESH_CONFIG_STORAGE_FAILURE;
        } else {
            if (mesh_app_net_idx(state, &state->app_keys[slot]) != net_idx) {
                status = MESH_CONFIG_INVALID_APPKEY;
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
        }

        uint8_t reply[4] = {status, p[0], p[1], p[2]};
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_APP_STATUS, reply, 7, 0);
        return 1;
    }

    if (message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE ||
        message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE) {
        if (len != 20) return 0;
        uint16_t element = p[0] | ((uint16_t)p[1] << 8);
        uint16_t model = p[18] | ((uint16_t)p[19] << 8);
        uint16_t address = mesh_virtual_address(p + 2);
        uint8_t status = mesh_element_index(element) < 0 ?
            MESH_CONFIG_INVALID_ADDRESS :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE ?
            mesh_subscription_replace(element, model, 0, p + 2) :
            message->opcode == OP_CONFIG_MODEL_SUB_VIRTUAL_ADD ?
            mesh_model_label_add(element, model, p + 2) :
            mesh_model_label_remove(element, model, p + 2);
        uint8_t reply[7] = {
            status, p[0], p[1], (uint8_t)address, (uint8_t)(address >> 8),
            p[18], p[19]
        };
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
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
        mesh_access_queue(mesh_network.state.unicast_address, message->src, mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
                              OP_CONFIG_MODEL_SUB_STATUS, reply, sizeof(reply), 0);
        return 1;
    }

    return 0;
}

// Config Client helpers for a provisioner configuring another node.
static inline int mesh_reset_node(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_NODE_RESET, NULL, 0, 0);
}

static inline int mesh_get_heartbeat_pub(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_HEARTBEAT_PUB_GET, NULL, 0, 0);
}

static inline int mesh_set_heartbeat_pub(uint16_t dst, const mesh_heartbeat_publication *pub) {
    if (!pub || pub->ttl > 0x7f || pub->net_idx > 0x0fff ||
        pub->period_log > 0x11 || (pub->count_log > 0x11 && pub->count_log != 0xff) ||
        (pub->dst >= 0x8000 && pub->dst < 0xc000) ||
        (pub->dst >= 0xff00 && pub->dst < 0xfffc))
        return 0;
    uint8_t params[] = {(uint8_t)pub->dst, (uint8_t)(pub->dst >> 8), pub->count_log,
        pub->period_log, pub->ttl, (uint8_t)pub->features, (uint8_t)(pub->features >> 8),
        (uint8_t)pub->net_idx, (uint8_t)(pub->net_idx >> 8)};
    return mesh_access_queue_on_net(pub->net_idx,
        dst, mesh_models.state.default_ttl,
        OP_CONFIG_HEARTBEAT_PUB_SET, params, sizeof(params));
}

static inline int mesh_get_heartbeat_sub(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_HEARTBEAT_SUB_GET, NULL, 0, 0);
}

static inline int mesh_set_heartbeat_sub(
    uint16_t dst, uint16_t src,
                                            uint16_t address, uint8_t period_log
) {
    if (src > 0x7fff || period_log > 0x11 ||
        (address && address != dst && address < 0xc000) ||
        (address >= 0xff00 && address < 0xfffc))
        return 0;
    uint8_t params[] = {(uint8_t)src, (uint8_t)(src >> 8),
        (uint8_t)address, (uint8_t)(address >> 8), period_log};
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_HEARTBEAT_SUB_SET, params, sizeof(params), 0);
}

static inline int mesh_get_beacon(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_BEACON_GET, NULL, 0, 0);
}

static inline int mesh_set_beacon(uint16_t dst, uint8_t enabled) {
    if (enabled > 1) return 0;
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_BEACON_SET, &enabled, 1, 0);
}

static inline int mesh_get_net_transmit(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_NET_TRANSMIT_GET, NULL, 0, 0);
}

// count is 0..7 extra sends; interval_steps is 0..31 in units of 10 ms.
static inline int mesh_set_net_transmit(
    uint16_t dst, uint8_t count,
                                           uint8_t interval_steps
) {
    if (count > 7 || interval_steps > 31) return 0;
    uint8_t params = count | (interval_steps << 3);
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_NET_TRANSMIT_SET, &params, 1, 0);
}

static inline int mesh_get_sar_transmitter(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SAR_TRANSMITTER_GET, NULL, 0, 0);
}

static inline int mesh_set_sar_transmitter(
    uint16_t dst, const mesh_sar_tx_state *sar
) {
    if (!mesh_sar_tx_valid(sar)) return 0;
    uint8_t params[4] = {
        (uint8_t)(sar->segment_interval_step | (sar->unicast_retrans_count << 4)),
        (uint8_t)(sar->unicast_retrans_wo_progress_count |
                  (sar->unicast_retrans_interval_step << 4)),
        (uint8_t)(sar->unicast_retrans_interval_increment |
                  (sar->multicast_retrans_count << 4)),
        sar->multicast_retrans_interval_step
    };
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SAR_TRANSMITTER_SET, params, sizeof(params), 0);
}

static inline int mesh_get_sar_receiver(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SAR_RECEIVER_GET, NULL, 0, 0);
}

static inline int mesh_set_sar_receiver(
    uint16_t dst, const mesh_sar_rx_state *sar
) {
    if (!mesh_sar_rx_valid(sar)) return 0;
    uint8_t params[3] = {
        (uint8_t)(sar->segments_threshold | (sar->ack_delay_increment << 5)),
        (uint8_t)(sar->discard_timeout | (sar->segment_interval_step << 4)),
        sar->ack_retrans_count
    };
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SAR_RECEIVER_SET, params, sizeof(params), 0);
}

static inline int mesh_get_relay(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_RELAY_GET, NULL, 0, 0);
}

static inline int mesh_set_relay(
    uint16_t dst, uint8_t enabled,
        uint8_t retransmit_count, uint8_t retransmit_interval_steps
) {
    if (enabled > 1 || retransmit_count > 7 ||
        retransmit_interval_steps > 31)
        return 0;
    uint8_t params[2] = {enabled,
        (uint8_t)((retransmit_interval_steps << 3) | retransmit_count)};
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_RELAY_SET, params, sizeof(params), 0);
}

static inline int mesh_get_proxy(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_PROXY_GET, NULL, 0, 0);
}

static inline int mesh_get_friend(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, OP_CONFIG_FRIEND_GET, NULL, 0, 0);
}

static inline int mesh_get_node_identity(uint16_t dst, uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_NODE_IDENTITY_GET,
        params, sizeof(params));
}

//! Net key
// Set update to 1 to start Key Refresh, or 0 to add a subnet key.
static inline int mesh_netkey_add_or_update(
    uint16_t dst, uint16_t net_idx,
    const uint8_t key[16], uint8_t update
) {
    if (!key || net_idx > 0x0fff || update > 1) return 0;
    uint8_t params[18] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    memcpy(params + 2, key, 16);
    if (update)
        return mesh_access_queue_on_net(net_idx,
            dst, mesh_models.state.default_ttl,
            OP_CONFIG_NETKEY_UPDATE, params, sizeof(params));
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_NETKEY_ADD, params, sizeof(params), 0);
}

static inline int mesh_netkey_get(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_NETKEY_GET, NULL, 0, 0);
}

static inline int mesh_netkey_delete(uint16_t dst, uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_NETKEY_DELETE, params, sizeof(params));
}

static inline int mesh_netkey_get_phase(uint16_t dst, uint16_t net_idx) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_KEY_PHASE_GET,
        params, sizeof(params));
}

// Transition 2 starts sending with new keys; 3 revokes old keys and returns to 0.
static inline int mesh_netkey_set_phase(
    uint16_t dst, uint16_t net_idx,
                                         uint8_t transition
) {
    if (net_idx > 0x0fff || (transition != 2 && transition != 3)) return 0;
    uint8_t params[3] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8), transition};
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_KEY_PHASE_SET,
        params, sizeof(params));
}

//! App Key
// Set update to 0 to add a key, or 1 to stage a replacement during Key Refresh.
static inline int mesh_add_or_update_app_key(
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
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        update ? OP_CONFIG_APPKEY_UPDATE : OP_CONFIG_APPKEY_ADD,
        params, sizeof(params));
}

static inline int mesh_delete_app_key(
    uint16_t dst, uint16_t net_idx, uint16_t app_idx
) {
    if (net_idx > 0x0fff || app_idx > 0x0fff) return 0;
    uint8_t params[3] = {
        (uint8_t)net_idx,
        (uint8_t)((net_idx >> 8) | (app_idx << 4)),
        (uint8_t)(app_idx >> 4)
    };
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_APPKEY_DELETE,
        params, sizeof(params));
}

static inline int mesh_get_app_keys(
    uint16_t dst,
                                                uint16_t net_idx
) {
    if (net_idx > 0x0fff) return 0;
    uint8_t params[2] = {(uint8_t)net_idx, (uint8_t)(net_idx >> 8)};
    return mesh_access_queue_on_net(net_idx, dst, mesh_models.state.default_ttl,
        OP_CONFIG_APPKEY_GET,
        params, sizeof(params));
}

// Set bind to 1 to bind the AppKey to the model, or 0 to unbind it.
static inline int mesh_model_binding(
    uint16_t dst, uint16_t element,
    uint16_t app_idx, uint16_t model, uint8_t bind
) {
    if (!element || element > 0x7fff || app_idx > 0x0fff || bind > 1) return 0;
    uint8_t params[6] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)app_idx, (uint8_t)(app_idx >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
                                 bind ? OP_CONFIG_MODEL_APP_BIND : OP_CONFIG_MODEL_APP_UNBIND,
                                 params, sizeof(params), 0);
}

static inline int mesh_get_bindings(
    uint16_t dst,
                                                 uint16_t element,
                                                 uint16_t model
) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model)) return 0;
    uint8_t params[4] = {
        (uint8_t)element, (uint8_t)(element >> 8),
        (uint8_t)model, (uint8_t)(model >> 8)
    };
    return mesh_access_queue(mesh_network.state.unicast_address, dst, mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
                                 OP_CONFIG_SIG_MODEL_APP_GET,
                                 params, sizeof(params), 0);
}

static inline int mesh_get_composition(uint16_t dst, uint8_t page) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_COMPOSITION_GET, &page, 1, 0);
}

static inline int mesh_get_default_ttl(uint16_t dst) {
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_DEFAULT_TTL_GET, NULL, 0, 0);
}

static inline int mesh_set_default_ttl(uint16_t dst, uint8_t ttl) {
    if (ttl == 1 || ttl > 0x7f) return 0;
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_DEFAULT_TTL_SET, &ttl, 1, 0);
}

static inline int mesh_get_publication(
    uint16_t dst, uint16_t element,
                                           uint16_t model
) {
    if (!element || element > 0x7fff || mesh_publication_slot(model) < 0) return 0;
    uint8_t params[] = {(uint8_t)element, (uint8_t)(element >> 8),
                        (uint8_t)model, (uint8_t)(model >> 8)};
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_MODEL_PUB_GET, params, sizeof(params), 0);
}

// address 0 disables publication; has_label selects a Label UUID destination.
static inline int mesh_set_publication(
    uint16_t dst, uint16_t element,
                                           uint16_t model, const mesh_publication *pub
) {
    if (!pub || !element || element > 0x7fff || mesh_publication_slot(model) < 0 ||
        pub->has_label > 1 || ((pub->address || pub->has_label) &&
        (pub->app_idx > 0x0fff || (pub->ttl > 0x7f && pub->ttl != 0xff))))
        return 0;
    if (!pub->has_label && ((pub->address >= 0x8000 && pub->address < 0xc000) ||
                           (pub->address >= 0xff00 && pub->address < 0xfffc)))
        return 0;
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
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        pub->has_label ? OP_CONFIG_MODEL_PUB_VIRTUAL_SET : OP_CONFIG_MODEL_PUB_SET,
        params, offset, 0);
}

static inline int mesh_get_subscriptions(
    uint16_t dst, uint16_t element,
                                             uint16_t model
) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model)) return 0;
    uint8_t params[] = {(uint8_t)element, (uint8_t)(element >> 8),
                        (uint8_t)model, (uint8_t)(model >> 8)};
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE,
        OP_CONFIG_SIG_SUB_GET, params, sizeof(params), 0);
}

// A label replaces all subscriptions with that label; address 0 clears them.
static inline int mesh_replace_subscription(
    uint16_t dst, uint16_t element,
    uint16_t model, uint16_t address, const uint8_t *label
) {
    if (!element || element > 0x7fff || !mesh_virtual_model_valid(model) ||
        (!label && address && (address < 0xc000 || address > 0xfeff)))
        return 0;
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
    return mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, APP_KEY_INDEX_NONE, opcode, params, offset, 0);
}

// Replace active faults and retain their history until Fault Clear or reboot.
static inline int mesh_health_faults(
    uint16_t element, uint8_t test_id,
                                        const uint8_t *faults, size_t len
) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || mesh_models.reset_pending || index < 0 ||
        len > MESH_HEALTH_MAX_FAULTS || (!faults && len))
        return 0;
    uint8_t current[MESH_HEALTH_MAX_FAULTS] = {0}, registered[MESH_HEALTH_MAX_FAULTS];
    uint8_t current_count = 0, registered_count = mesh_models.health_server[index].registered_count;
    memcpy(registered, mesh_models.health_server[index].registered, sizeof(registered));
    for (size_t i = 0; i < len; i++) {
        if (!faults[i]) return 0; // An empty list means no faults.
        uint8_t found = 0;
        for (uint8_t j = 0; j < current_count; j++) if (current[j] == faults[i]) found = 1;
        if (!found) current[current_count++] = faults[i];
        found = 0;
        for (uint8_t j = 0; j < registered_count; j++) if (registered[j] == faults[i]) found = 1;
        if (!found) {
            if (registered_count == MESH_HEALTH_MAX_FAULTS) return 0;
            registered[registered_count++] = faults[i];
        }
    }
    uint8_t changed = mesh_models.health_server[index].test_id != test_id ||
        mesh_models.health_server[index].current_count != current_count ||
        memcmp(mesh_models.health_server[index].current, current, sizeof(current));
    mesh_models.health_server[index].test_id = test_id;
    mesh_models.health_server[index].current_count = current_count;
    mesh_models.health_server[index].registered_count = registered_count;
    memcpy(mesh_models.health_server[index].current, current, sizeof(current));
    memcpy(mesh_models.health_server[index].registered, registered, sizeof(registered));
    if (changed) {
        mesh_models.health_server[index].publish_pending = 1;
        mesh_models.publications[index][2].period_at_ms = GET_MILLIS() + mesh_health_period((uint8_t)index);
    }
    return 1;
}

// Health Server: AppKey-protected diagnostics, publication cadence, and attention.
static int server_health_receive(const mesh_access_pdu *message, uint8_t element) {
    const uint8_t *p = message->params;
    size_t len = message->params_len;
    uint32_t opcode = message->opcode;
    if (opcode == OP_HEALTH_PERIOD_GET || opcode == OP_HEALTH_PERIOD_SET ||
        opcode == OP_HEALTH_PERIOD_SET_UNACK) {
        uint8_t set = opcode != OP_HEALTH_PERIOD_GET;
        if (len != (set ? 1u : 0u) || (set && p[0] > 15)) return 0;
        if (set) {
            mesh_models_state next = mesh_models.state;
            next.health_period[element] = p[0];
            if (memcmp(&next, &mesh_models.state, sizeof(next)) &&
                BLE_MESH_MODELS_SAVE_STATE(&next) != 1)
                return 0;
            mesh_models.state = next;
            mesh_models.publications[element][2].period_at_ms = GET_MILLIS() + mesh_health_period(element);
            if (opcode == OP_HEALTH_PERIOD_SET_UNACK) return 1;
        }
        return mesh_access_queue(mesh_network.state.unicast_address + element,
            message->src, mesh_models.state.default_ttl, message->app_key_index,
            OP_HEALTH_PERIOD_STATUS, &mesh_models.state.health_period[element], 1, 0);
    }
    if (opcode == OP_HEALTH_FAULT_GET || opcode == OP_HEALTH_FAULT_CLEAR ||
        opcode == OP_HEALTH_FAULT_CLEAR_UNACK || opcode == OP_HEALTH_FAULT_TEST ||
        opcode == OP_HEALTH_FAULT_TEST_UNACK) {
        uint8_t test = opcode == OP_HEALTH_FAULT_TEST || opcode == OP_HEALTH_FAULT_TEST_UNACK;
        if (len != (test ? 3u : 2u) ||
            (uint16_t)(p[test] | (uint16_t)p[test + 1] << 8) != MESH_COMPANY_ID)
            return 0;
        if (test) {
            uint8_t faults[MESH_HEALTH_MAX_FAULTS];
            size_t count = sizeof(faults);
            if (BLE_MESH_HEALTH_TEST(mesh_network.state.unicast_address + element,
                                    p[0], faults, &count) != 1 ||
                !mesh_health_faults(mesh_network.state.unicast_address + element,
                                        p[0], faults, count))
                return 0;
        } else if (opcode != OP_HEALTH_FAULT_GET) {
            mesh_models.health_server[element].registered_count = 0;
            memset(mesh_models.health_server[element].registered, 0,
                   sizeof(mesh_models.health_server[element].registered));
        }
        if (opcode == OP_HEALTH_FAULT_CLEAR_UNACK || opcode == OP_HEALTH_FAULT_TEST_UNACK) return 1;
        uint8_t reply[3 + MESH_HEALTH_MAX_FAULTS] = {
            mesh_models.health_server[element].test_id,
            (uint8_t)MESH_COMPANY_ID, (uint8_t)(MESH_COMPANY_ID >> 8)};
        uint8_t count = mesh_models.health_server[element].registered_count;
        memcpy(reply + 3, mesh_models.health_server[element].registered, count);
        return mesh_access_queue(mesh_network.state.unicast_address + element,
            message->src, mesh_models.state.default_ttl, message->app_key_index,
            OP_HEALTH_FAULT_STATUS, reply, 3u + count, 0);
    }
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
    return mesh_access_queue(mesh_network.state.unicast_address + element,
                                 message->src, mesh_models.state.default_ttl,
                                 message->app_key_index,
                                 OP_HEALTH_ATTENTION_STATUS, &remaining, 1, 0);
}

#endif
