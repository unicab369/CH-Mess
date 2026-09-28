#ifndef ISLER_BLE_MESH_MODEL_STATE_H
#define ISLER_BLE_MESH_MODEL_STATE_H

#include "ble_mesh_3access.h"

// Shared model identifiers, runtime state, bindings, and subscriptions.
#define MESH_MODEL_HEALTH_SERVER 0x0002
#define MESH_MODEL_ONOFF_SERVER 0x1000
#define MESH_MODEL_ONOFF_CLIENT 0x1001
#define MODEL_TTL 5

#define MESH_CONFIG_SUCCESS 0x00
#define MESH_CONFIG_INVALID_ADDRESS 0x01
#define MESH_CONFIG_INVALID_MODEL 0x02
#define MESH_CONFIG_INVALID_APPKEY 0x03
#define MESH_CONFIG_INVALID_NETKEY 0x04
#define MESH_CONFIG_INSUFFICIENT_RESOURCES 0x05
#define MESH_CONFIG_KEY_ALREADY_STORED 0x06
#define MESH_CONFIG_INVALID_PUBLICATION 0x07
#define MESH_CONFIG_STORAGE_FAILURE 0x09
#define MESH_CONFIG_FEATURE_NOT_SUPPORTED 0x0a
#define MESH_CONFIG_CANNOT_UPDATE 0x0b
#define MESH_CONFIG_INVALID_BINDING 0x11
#define MESH_MODEL_VIRTUAL_SLOTS MESH_TRANSPORT_MAX_LABELS
#define MESH_MODEL_GROUP_SLOTS 8
#define MESH_PUBLICATION_MODELS 3
#define MESH_PUBLICATION_MAX_PARAMS 8

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
    uint16_t address;
    uint16_t app_idx;
    uint8_t ttl, period, retransmit, has_label;
    uint8_t label[16];
} mesh_publication;

typedef struct {
    uint8_t onoff_server_bindings;
    uint8_t onoff_client_bindings;
    uint8_t health_server_bindings;
    uint8_t virtual_count;
    mesh_model_label virtual[MESH_MODEL_VIRTUAL_SLOTS];
    mesh_element_bindings other[MESH_MAX_ELEMENTS - 1];
    uint8_t group_count;
    mesh_model_group groups[MESH_MODEL_GROUP_SLOTS];
    uint8_t default_ttl;
    mesh_publication publications[MESH_MAX_ELEMENTS][MESH_PUBLICATION_MODELS];
} mesh_models_state;

int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state);
int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state);

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
    struct {
        uint32_t period_at_ms, retransmit_at_ms, opcode;
        uint8_t remaining, len;
        uint8_t params[MESH_PUBLICATION_MAX_PARAMS];
    } publications[MESH_MAX_ELEMENTS][MESH_PUBLICATION_MODELS];
    struct {
        uint8_t pending;
        mesh_access_pdu access;
        uint8_t params[MESH_PUBLICATION_MAX_PARAMS];
    } local;
} mesh_models;

static int mesh_publication_slot(uint16_t model) {
    return model == MESH_MODEL_ONOFF_SERVER ? 0 :
           model == MESH_MODEL_ONOFF_CLIENT ? 1 :
           model == MESH_MODEL_HEALTH_SERVER ? 2 : -1;
}

static uint32_t mesh_publication_period(uint8_t period) {
    const uint32_t resolution[] = {100, 1000, 10000, 600000};
    return (period & 0x3f) * resolution[period >> 6];
}

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
        mesh_models.state.group_count > MESH_MODEL_GROUP_SLOTS ||
        mesh_models.state.default_ttl == 1 ||
        mesh_models.state.default_ttl > 0x7f) return 0;
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
            mesh_network.state.element_count ||
            !mesh_virtual_model_valid(mesh_models.state.virtual[i].model)) return 0;
        if (!ble_mesh_label_add(mesh_models.state.virtual[i].label))
            return 0;
    }
    for (uint8_t i = 0; i < mesh_models.state.group_count; i++) {
        mesh_model_group *group = &mesh_models.state.groups[i];
        if (group->element >= mesh_network.state.element_count ||
            group->address < 0xc000 || group->address > 0xfeff ||
            !mesh_virtual_model_valid(group->model)) return 0;
    }

    uint32_t now = GET_MILLIS();
    memset(mesh_models.publications, 0, sizeof(mesh_models.publications));
    mesh_models.local.pending = 0;
    for (uint8_t i = 0; i < mesh_network.state.element_count; i++) {
        for (uint8_t j = 0; j < MESH_PUBLICATION_MODELS; j++) {
            mesh_publication *pub = &mesh_models.state.publications[i][j];
            if ((pub->ttl > 0x7f && pub->ttl != 0xff) || pub->has_label > 1 ||
                (pub->address && pub->app_idx > 0x0fff) ||
                ((pub->address >= 0x8000 && pub->address < 0xc000) !=
                 (pub->has_label != 0)) ||
                (pub->has_label && pub->address != ble_mesh_virtual_address(pub->label)))
                return 0;
            mesh_models.publications[i][j].period_at_ms =
                now + mesh_publication_period(pub->period);
        }
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

static int app_key_allowed(uint8_t element, uint16_t model, uint16_t app_idx) {
    int slot = mesh_app_key_slot(&mesh_network.state, app_idx);
    if (slot < 0) return 0;
    uint8_t *bindings = mesh_model_bindings(&mesh_models.state, element, model);
    return bindings && (*bindings & (1u << slot));
}

// Start a publication; another publication cancels the old retransmissions.
static int mesh_publication_begin(uint8_t element, uint16_t model,
                                   uint32_t opcode, const uint8_t *params,
                                   size_t len) {
    int slot = mesh_publication_slot(model);
    if (!mesh_models.ready || element >= mesh_network.state.element_count ||
        slot < 0 || len > MESH_PUBLICATION_MAX_PARAMS || (!params && len)) return 0;
    mesh_publication *pub = &mesh_models.state.publications[element][slot];
    if (!pub->address || !app_key_allowed(element, model, pub->app_idx)) return 0;
    mesh_models.publications[element][slot].opcode = opcode;
    mesh_models.publications[element][slot].len = (uint8_t)len;
    if (len) memcpy(mesh_models.publications[element][slot].params, params, len);
    mesh_models.publications[element][slot].remaining = (pub->retransmit & 7) + 1;
    mesh_models.publications[element][slot].retransmit_at_ms = GET_MILLIS();
    return 1;
}

static int mesh_publication_send(uint8_t element, uint8_t slot) {
    mesh_publication *pub = &mesh_models.state.publications[element][slot];
    uint16_t src = mesh_network.state.unicast_address + element;
    uint8_t ttl = pub->ttl == 0xff ? mesh_models.state.default_ttl : pub->ttl;
    uint32_t opcode = mesh_models.publications[element][slot].opcode;
    const uint8_t *params = mesh_models.publications[element][slot].params;
    size_t len = mesh_models.publications[element][slot].len;
    // TTL 1 publications stay on this node and use the same model dispatcher.
    if (ttl == 1) {
        if (mesh_models.local.pending) return 0;
        mesh_models.local.access = (mesh_access_pdu){
            .src = src, .dst = pub->address, .app_key_index = pub->app_idx,
            .ttl = ttl, .has_label = pub->has_label, .opcode = opcode,
            .params = mesh_models.local.params, .params_len = len
        };
        if (len) memcpy(mesh_models.local.params, params, len);
        if (pub->has_label) memcpy(mesh_models.local.access.label, pub->label, 16);
        mesh_models.local.pending = 1;
        return 1;
    }
    return pub->has_label ?
        ble_mesh_access_queue_virtual(src, pub->label, ttl, pub->app_idx,
                                       opcode, params, len, 0) :
        ble_mesh_access_queue(src, pub->address, ttl, pub->app_idx,
                              opcode, params, len, 0);
}

#endif
