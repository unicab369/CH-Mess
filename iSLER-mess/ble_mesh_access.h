// Mesh Model Identifiers 4.1:
// https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1706604193002

#ifndef ISLER_BLE_MESH_ACCESS_H
#define ISLER_BLE_MESH_ACCESS_H

#include "ble_mesh_transport.h"

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t app_key_index; // 0xffff means the Device Key was used
    uint8_t ttl;
    uint32_t opcode;        // opcode bytes in transmission order
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

// Deliver to a model only if it supports the opcode, accepts the destination,
// and is bound to the AppKey (or uses the Device Key). Return 1 if handled.
int BLE_MESH_ACCESS_HANDLE(const mesh_access_pdu *message);

// Queue an Access message using an opcode in transmission byte order.
// Returns 1 if accepted by transport, or 0 for invalid input/send failure.
static inline int ble_mesh_access_send(uint16_t dst, uint8_t ttl,
                                       uint8_t use_device_key, uint32_t opcode,
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
    return ble_mesh_transport_queue(dst, ttl, use_device_key,
                                   data, opcode_len + params_len);
}

// Poll transport and dispatch one complete Access message, if available.
// Returns 1 if handled, 0 if none/ignored, or -1 on a transport error.
static inline int ble_mesh_access_poll(void) {
    mesh_access_message message;
    int result = ble_mesh_transport_poll(&message);
    if (result <= 0) return result;
    if (!message.len || message.len > sizeof(message.data)) return 0;

    uint8_t first = message.data[0];
    size_t opcode_len = (first & 0x80) == 0 ? 1 :
                        (first & 0xc0) == 0x80 ? 2 : 3;
    if (first == 0x7f || message.len < opcode_len) return 0;

    uint32_t opcode = first;
    for (size_t i = 1; i < opcode_len; i++)
        opcode = (opcode << 8) | message.data[i];

    mesh_access_pdu access = {
        .src = message.src,
        .dst = message.dst,
        .app_key_index = message.app_key_index,
        .ttl = message.ttl,
        .opcode = opcode,
        .params = message.data + opcode_len,
        .params_len = message.len - opcode_len
    };
    return BLE_MESH_ACCESS_HANDLE(&access) == 1;
}

#endif
