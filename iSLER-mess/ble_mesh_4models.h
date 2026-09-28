#ifndef ISLER_BLE_MESH_MODELS_H
#define ISLER_BLE_MESH_MODELS_H

#include "ble_mesh_4foundation.h"

#define OP_ONOFF_GET 0x8201
#define OP_ONOFF_SET 0x8202
#define OP_ONOFF_SET_UNACK 0x8203
#define OP_ONOFF_STATUS 0x8204

void BLE_MESH_ONOFF_CHANGED(uint16_t element, uint8_t on);
void BLE_MESH_ONOFF_STATUS(uint16_t element, uint16_t src, uint8_t present);

static inline int ble_mesh_onoff_get(uint16_t element, uint16_t dst,
                                          uint16_t app_idx) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)) return 0;
    return ble_mesh_access_queue(element, dst, mesh_models.state.default_ttl, app_idx,
                                      OP_ONOFF_GET, NULL, 0, 0);
}

static inline int ble_mesh_onoff_get_virtual(uint16_t element,
                                                  const uint8_t label[16],
                                                  uint16_t app_idx) {
    int index = mesh_element_index(element);
    if (!mesh_models.ready || index < 0 ||
        !app_key_allowed((uint8_t)index, MESH_MODEL_ONOFF_CLIENT, app_idx)) return 0;
    return ble_mesh_access_queue_virtual(element, label, mesh_models.state.default_ttl,
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
    return ble_mesh_access_queue(element, dst, mesh_models.state.default_ttl, app_idx,
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
    return ble_mesh_access_queue_virtual(element, label, mesh_models.state.default_ttl, app_idx,
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
            uint8_t changed = server->onoff != message->params[0];
            server->onoff = message->params[0];
            if (changed) mesh_publication_begin(element, MESH_MODEL_ONOFF_SERVER,
                OP_ONOFF_STATUS, &server->onoff, 1);
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
                                 message->src, mesh_models.state.default_ttl,
                                 message->app_key_index,
                                 OP_ONOFF_STATUS, &present, 1, 0);
}

// Publish through the OnOff Client's configured address and AppKey.
static inline int ble_mesh_onoff_publish(uint16_t element, uint8_t on,
                                         uint8_t acknowledged) {
    int index = mesh_element_index(element);
    if (index < 0 || on > 1 || acknowledged > 1) return 0;
    uint8_t params[2] = {on, mesh_models.onoff_client[index].tid};
    int result = mesh_publication_begin((uint8_t)index, MESH_MODEL_ONOFF_CLIENT,
        acknowledged ? OP_ONOFF_SET : OP_ONOFF_SET_UNACK, params, sizeof(params));
    if (result) mesh_models.onoff_client[index].tid++;
    return result;
}

// Periodic publications and retransmissions share the existing access TX path.
static void mesh_publications_poll(void) {
    uint32_t now = GET_MILLIS();
    const uint16_t models[] = {MESH_MODEL_ONOFF_SERVER, MESH_MODEL_ONOFF_CLIENT,
                              MESH_MODEL_HEALTH_SERVER, MESH_MODEL_HEALTH_CLIENT};
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++) {
        for (uint8_t j = 0; j < MESH_PUBLICATION_MODELS; j++) {
            mesh_publication *pub = &mesh_models.state.publications[i][j];
            if (!pub->address || !app_key_allowed(i, models[j], pub->app_idx)) {
                mesh_models.publications[i][j].remaining = 0;
                continue;
            }
            uint32_t period = j == 2 ? mesh_health_period(i) :
                j == 3 ? 0 : mesh_publication_period(pub->period);
            if ((j == 2 && mesh_models.health_server[i].publish_pending) ||
                (period && (int32_t)(now - mesh_models.publications[i][j].period_at_ms) >= 0)) {
                mesh_models.publications[i][j].period_at_ms = now + period;
                if (j == 0) mesh_publication_begin(i, models[j], OP_ONOFF_STATUS,
                    &mesh_models.onoff_server[i].onoff, 1);
                else if (j == 2) {
                    uint8_t params[3 + MESH_HEALTH_MAX_FAULTS] = {
                        mesh_models.health_server[i].test_id, (uint8_t)MESH_COMPANY_ID,
                        (uint8_t)(MESH_COMPANY_ID >> 8)};
                    uint8_t count = mesh_models.health_server[i].current_count;
                    memcpy(params + 3, mesh_models.health_server[i].current, count);
                    mesh_publication_begin(i, models[j], OP_HEALTH_CURRENT_STATUS,
                                           params, 3u + count);
                    mesh_models.health_server[i].publish_pending = 0;
                } else if (mesh_models.publications[i][j].opcode) {
                    // Each periodic Set is a new transaction; retries keep its TID.
                    mesh_models.publications[i][j].params[1] = mesh_models.onoff_client[i].tid++;
                    mesh_publication_begin(i, models[j], mesh_models.publications[i][j].opcode,
                        mesh_models.publications[i][j].params, mesh_models.publications[i][j].len);
                }
            }
            if (mesh_models.publications[i][j].remaining &&
                (int32_t)(now - mesh_models.publications[i][j].retransmit_at_ms) >= 0 &&
                mesh_publication_send(i, j)) {
                mesh_models.publications[i][j].remaining--;
                mesh_models.publications[i][j].retransmit_at_ms =
                    now + ((pub->retransmit >> 3) + 1u) * 50u;
            }
        }
    }
}

static inline int ble_mesh_models_poll(void) {
    if (mesh_models.reset_pending) {
        uint8_t ad[31]; size_t len = sizeof(ad);
        BLE_MESH_ADV_POLL(ad, &len);
        return 0;
    }
    if (!mesh_models.ready && !ble_mesh_models_init()) return 0;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++)
        mesh_health_attention_remaining(i);
    mesh_access_message raw;
    mesh_access_pdu access;
    int result;
    if (mesh_models.local.pending) {
        access = mesh_models.local.access;
        mesh_models.local.pending = 0;
        result = 1;
    } else result = ble_mesh_access_poll(&raw, &access);
    // Handle received requests first so publications do not occupy their reply slot.
    if (result <= 0) {
        mesh_publications_poll();
        if (mesh_models.local.pending) {
            access = mesh_models.local.access;
            mesh_models.local.pending = 0;
            result = 1;
        }
    }
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

        if (opcode == OP_CONFIG_BEACON_STATUS ||
            opcode == OP_CONFIG_NODE_RESET_STATUS ||
            opcode == OP_CONFIG_HEARTBEAT_PUB_STATUS ||
            opcode == OP_CONFIG_HEARTBEAT_SUB_STATUS ||
            opcode == OP_CONFIG_NET_TRANSMIT_STATUS ||
            opcode == OP_CONFIG_RELAY_STATUS ||
            opcode == OP_CONFIG_PROXY_STATUS ||
            opcode == OP_CONFIG_FRIEND_STATUS ||
            opcode == OP_CONFIG_NODE_IDENTITY_STATUS ||
            opcode == OP_CONFIG_NETKEY_STATUS ||
            opcode == OP_CONFIG_NETKEY_LIST ||
            opcode == OP_CONFIG_KEY_PHASE_STATUS ||
            opcode == OP_CONFIG_APPKEY_STATUS ||
            opcode == OP_CONFIG_APPKEY_LIST ||
            opcode == OP_CONFIG_MODEL_APP_STATUS ||
            opcode == OP_CONFIG_SIG_MODEL_APP_LIST ||
            opcode == OP_CONFIG_COMPOSITION_STATUS ||
            opcode == OP_CONFIG_DEFAULT_TTL_STATUS ||
            opcode == OP_CONFIG_MODEL_PUB_STATUS ||
            opcode == OP_CONFIG_SIG_MODEL_SUB_LIST ||
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
                 opcode == OP_HEALTH_ATTENTION_SET_UNACK ||
                 opcode == OP_HEALTH_FAULT_GET || opcode == OP_HEALTH_FAULT_CLEAR ||
                 opcode == OP_HEALTH_FAULT_CLEAR_UNACK || opcode == OP_HEALTH_FAULT_TEST ||
                 opcode == OP_HEALTH_FAULT_TEST_UNACK || opcode == OP_HEALTH_PERIOD_GET ||
                 opcode == OP_HEALTH_PERIOD_SET || opcode == OP_HEALTH_PERIOD_SET_UNACK)
            model = MESH_MODEL_HEALTH_SERVER;
        else if (opcode == OP_HEALTH_CURRENT_STATUS || opcode == OP_HEALTH_FAULT_STATUS ||
                 opcode == OP_HEALTH_PERIOD_STATUS || opcode == OP_HEALTH_ATTENTION_STATUS)
            model = MESH_MODEL_HEALTH_CLIENT;
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
        } else if (model == MESH_MODEL_HEALTH_CLIENT) {
            // Validate Health statuses before delivering replies or subscribed fault reports.
            size_t len = message->params_len;
            if (opcode == OP_HEALTH_CURRENT_STATUS || opcode == OP_HEALTH_FAULT_STATUS) {
                if (len < 3) continue; // Test ID, Company ID, then zero or more faults.
            } else if (len != 1 ||
                       (opcode == OP_HEALTH_PERIOD_STATUS && message->params[0] > 15)) continue;
            BLE_MESH_HEALTH_STATUS(mesh_network.state.unicast_address + i,
                                   message->src, opcode, message->params, len);
            handled = 1;
        } else handled |= server_health_receive(message, i);
    }
    return handled;
}

#endif
