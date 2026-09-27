// Mesh Model Identifiers 4.1:
// https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1706604193002

#ifndef ISLER_BLE_MESH_ACCESS_H
#define ISLER_BLE_MESH_ACCESS_H

#include "ble_mesh_2transport.h"

// TODO for a broadly usable Access layer:
// - Route messages to models on multiple elements and subscribed group addresses.
// - Check each receiving model's AppKey binding or Device Key permission.
// - Use configured publication address, AppKey, TTL, period, and retransmit settings.

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t app_key_index; // APP_KEY_INDEX_NONE means the Device Key was used
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint32_t opcode;        // opcode bytes in transmission order
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

// Queue an Access message using an opcode in transmission byte order.
// Returns 1 if accepted by transport, or 0 for invalid input/queue failure.
static inline int ble_mesh_access_queue(uint16_t dst, uint8_t ttl,
                                       uint16_t app_key_index, uint32_t opcode,
                                       const uint8_t *params, size_t params_len) {
    size_t opcode_len;
    if (opcode <= 0x7e) opcode_len = 1;
    else if (opcode >= 0x8000 && opcode <= 0xbfff) opcode_len = 2;
    else if (opcode >= 0xc00000 && opcode <= 0xffffff) opcode_len = 3;
    else return 0;

    if ((!params && params_len) ||
        params_len > MESH_TRANSPORT_MAX_ACCESS - opcode_len) return 0;

    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
    for (size_t i = 0; i < opcode_len; i++) {
        data[i] = (uint8_t)(opcode >> (8 * (opcode_len - 1 - i)));
    }

    if (params_len) memcpy(data + opcode_len, params, params_len);
    return ble_mesh_transport_queue(dst, ttl, app_key_index, NULL,
                                         data, opcode_len + params_len);
}

// Send an AppKey Access message to the virtual address derived from label.
static inline int ble_mesh_access_queue_virtual(
    const uint8_t label[16], uint8_t ttl, uint16_t app_key_index,
    uint32_t opcode,
    const uint8_t *params, size_t params_len
) {
    size_t opcode_len = opcode <= 0x7e ? 1 :
                        opcode >= 0x8000 && opcode <= 0xbfff ? 2 :
                        opcode >= 0xc00000 && opcode <= 0xffffff ? 3 : 0;
    if (!label || app_key_index == APP_KEY_INDEX_NONE || !opcode_len ||
        (!params && params_len) ||
        params_len > MESH_TRANSPORT_MAX_ACCESS - opcode_len) return 0;

    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
    for (size_t i = 0; i < opcode_len; i++) {
        data[i] = (uint8_t)(opcode >> (8 * (opcode_len - 1 - i)));
    }

    if (params_len) memcpy(data + opcode_len, params, params_len);
    return ble_mesh_transport_queue(ble_mesh_virtual_address(label), ttl,
                                     app_key_index, label, data,
                                     opcode_len + params_len);
}

// Poll transport and decode one Access message. The decoded params point into
// message, so keep it alive until the model handles the result.
// Returns 1 if decoded, 0 if none/malformed, or -1 on a transport error.
static inline int ble_mesh_access_poll(mesh_access_message *message,
                                       mesh_access_pdu *access) {
    if (!message || !access) return -1;
    int result = ble_mesh_transport_poll(message);
    if (result <= 0) return result;
    if (!message->len || message->len > sizeof(message->data)) return 0;

    uint8_t first = message->data[0];
    size_t opcode_len = (first & 0x80) == 0 ? 1 :
                        (first & 0xc0) == 0x80 ? 2 : 3;
    if (first == 0x7f || message->len < opcode_len) return 0;

    uint32_t opcode = first;
    for (size_t i = 1; i < opcode_len; i++)
        opcode = (opcode << 8) | message->data[i];

    *access = (mesh_access_pdu){
        .src = message->src,
        .dst = message->dst,
        .app_key_index = message->app_key_index,
        .ttl = message->ttl,
        .has_label = message->has_label,
        .opcode = opcode,
        .params = message->data + opcode_len,
        .params_len = message->len - opcode_len
    };
    if (message->has_label) memcpy(access->label, message->label, 16);
    return 1;
}

#endif
