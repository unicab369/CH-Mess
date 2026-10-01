// Mesh Model Identifiers 4.1:
// https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1706604193002

#ifndef ISLER_BLE_MESH_ACCESS_H
#define ISLER_BLE_MESH_ACCESS_H

#include "ble_mesh_2transport.h"

// TODO for broader Access support:
// - Defer Mesh 1.1 Opcode Aggregation; implement it later with the Aggregator
//   Client and Server model support.

#ifndef MESH_ACCESS_ACK_TIMEOUT_MS
#define MESH_ACCESS_ACK_TIMEOUT_MS 2000u
#endif
#ifndef MESH_ACCESS_ACK_RETRY_COUNT
#define MESH_ACCESS_ACK_RETRY_COUNT 2u
#endif

enum {
    MESH_ACCESS_ACK_IDLE,
    MESH_ACCESS_ACK_PENDING,
    MESH_ACCESS_ACK_COMPLETE,
    MESH_ACCESS_ACK_TIMED_OUT
};

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t app_key_index; // APP_KEY_INDEX_NONE means the Device Key was used
    uint16_t device_key_owner;
    uint16_t net_key_index;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint32_t opcode;        // opcode bytes in transmission order
    const uint8_t *params;
    size_t params_len;
} mesh_access_pdu;

// One acknowledged unicast transaction is active at a time. Its original
// Access payload is retained so retries keep the same model transaction ID.
static struct {
    uint16_t src, dst, app_key_index, reply_net_idx;
    uint8_t ttl, mic_64, retries_sent, retry_limit, state;
    uint32_t response_opcode, timeout_ms, retry_at_ms;
    size_t len;
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_ack;

// Queue an Access message using an opcode in transmission byte order.
// Use APP_KEY_INDEX_NONE for remote Device Key requests, DEVICE_KEY_LOCAL for replies.
// Returns 1 if accepted by transport, or 0 for invalid input/queue failure.
// Set mic_64 to 1 for a segmented message with an 8-byte TransMIC.
static inline int mesh_access_queue(
    uint16_t src, uint16_t dst,
    uint8_t ttl,
    uint16_t app_key_index, uint32_t opcode,
    const uint8_t *params, size_t params_len,
    uint8_t mic_64
) {
    size_t opcode_len;
    if (opcode <= 0x7e) opcode_len = 1;
    else if (opcode >= 0x8000 && opcode <= 0xbfff) opcode_len = 2;
    else if (opcode >= 0xc00000 && opcode <= 0xffffff) opcode_len = 3;
    else return 0;

    if ((!params && params_len) || mic_64 > 1 ||
        params_len > MESH_TRANSPORT_MAX_ACCESS - (mic_64 ? 4u : 0u) -
                     opcode_len) return 0;

    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
    for (size_t i = 0; i < opcode_len; i++) {
        data[i] = (uint8_t)(opcode >> (8 * (opcode_len - 1 - i)));
    }

    if (params_len) memcpy(data + opcode_len, params, params_len);
    return mesh_transport_queue(src, dst, ttl, app_key_index, NULL,
                                         data, opcode_len + params_len, mic_64);
}

// Queue an acknowledged unicast request and retain it for response matching
// and bounded retries. The response is still returned by mesh_access_poll.
static inline int mesh_access_queue_acknowledged(
    uint16_t src, uint16_t dst, uint8_t ttl, uint16_t app_key_index,
    uint32_t request_opcode, uint32_t response_opcode,
    const uint8_t *params, size_t params_len, uint8_t mic_64,
    uint32_t timeout_ms, uint8_t retry_count
) {
    size_t opcode_len = request_opcode <= 0x7e ? 1 :
        request_opcode <= 0xbfff && request_opcode >= 0x8000 ? 2 :
        request_opcode >= 0xc00000 && request_opcode <= 0xffffff ? 3 : 0;
    size_t response_len = response_opcode <= 0x7e ? 1 :
        response_opcode <= 0xbfff && response_opcode >= 0x8000 ? 2 :
        response_opcode >= 0xc00000 && response_opcode <= 0xffffff ? 3 : 0;
    if (mesh_access_ack.state == MESH_ACCESS_ACK_PENDING || !opcode_len ||
        !response_len || !src || src > 0x7fff || !dst || dst > 0x7fff ||
        (!params && params_len) || mic_64 > 1 || !timeout_ms ||
        timeout_ms > 0x7fffffffu ||
        params_len > MESH_TRANSPORT_MAX_ACCESS - (mic_64 ? 4u : 0u) - opcode_len)
        return 0;

    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
    for (size_t i = 0; i < opcode_len; i++)
        data[i] = (uint8_t)(request_opcode >> (8 * (opcode_len - 1 - i)));
    if (params_len) memcpy(data + opcode_len, params, params_len);
    size_t len = opcode_len + params_len;
    if (!mesh_transport_queue(src, dst, ttl, app_key_index, NULL,
            data, len, mic_64)) return 0;

    mesh_access_ack.src = src;
    mesh_access_ack.dst = dst;
    mesh_access_ack.app_key_index = app_key_index;
    mesh_access_ack.reply_net_idx = mesh_network.reply_net_idx;
    mesh_access_ack.ttl = ttl;
    mesh_access_ack.mic_64 = mic_64;
    mesh_access_ack.retries_sent = 0;
    mesh_access_ack.retry_limit = retry_count;
    mesh_access_ack.response_opcode = response_opcode;
    mesh_access_ack.timeout_ms = timeout_ms;
    mesh_access_ack.retry_at_ms = GET_MILLIS() + timeout_ms;
    mesh_access_ack.len = len;
    memcpy(mesh_access_ack.data, data, len);
    mesh_access_ack.state = MESH_ACCESS_ACK_PENDING;
    return 1;
}

// Read the most recent acknowledged transaction result.
static inline uint8_t mesh_access_ack_status(void) {
    return mesh_access_ack.state;
}

// Service the one outstanding transaction after checking for a reply.
static inline void mesh_access_ack_poll(void) {
    if (mesh_access_ack.state != MESH_ACCESS_ACK_PENDING ||
        (int32_t)(GET_MILLIS() - mesh_access_ack.retry_at_ms) < 0) return;
    if (mesh_access_ack.retries_sent >= mesh_access_ack.retry_limit) {
        mesh_access_ack.state = MESH_ACCESS_ACK_TIMED_OUT;
        return;
    }
    uint16_t previous_net_idx = mesh_network.reply_net_idx;
    mesh_network.reply_net_idx = mesh_access_ack.reply_net_idx;
    int queued = mesh_transport_queue(mesh_access_ack.src, mesh_access_ack.dst,
            mesh_access_ack.ttl, mesh_access_ack.app_key_index, NULL,
            mesh_access_ack.data, mesh_access_ack.len,
            mesh_access_ack.mic_64);
    mesh_network.reply_net_idx = previous_net_idx;
    if (queued) {
        mesh_access_ack.retries_sent++;
        mesh_access_ack.retry_at_ms = GET_MILLIS() + mesh_access_ack.timeout_ms;
    } else {
        // A full transport queue is not a lost response; try the same retry
        // again shortly without consuming one of the bounded attempts.
        mesh_access_ack.retry_at_ms = GET_MILLIS() + 100u;
    }
}

// Queue a Device Key configuration message over the specified subnet.
static inline int mesh_access_queue_on_net(
    uint16_t net_idx,
    uint16_t dst, uint8_t ttl, uint32_t opcode,
    const uint8_t *params, size_t params_len
) {
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
    uint16_t previous_net_idx = mesh_network.reply_net_idx;
    mesh_network.reply_net_idx = net_idx;

    int result = mesh_transport_queue(mesh_network.state.unicast_address,
        dst, ttl, APP_KEY_INDEX_NONE, NULL, data, opcode_len + params_len, 0);
    mesh_network.reply_net_idx = previous_net_idx;
    return result;
}

// Send an AppKey Access message to the virtual address derived from label.
static inline int mesh_access_queue_virtual(
    uint16_t src, const uint8_t label[16], uint8_t ttl, uint16_t app_key_index,
    uint32_t opcode,
    const uint8_t *params, size_t params_len, uint8_t mic_64
) {
    size_t opcode_len = opcode <= 0x7e ? 1 :
                        opcode >= 0x8000 && opcode <= 0xbfff ? 2 :
                        opcode >= 0xc00000 && opcode <= 0xffffff ? 3 : 0;

    if (!label || app_key_index == APP_KEY_INDEX_NONE ||
        app_key_index == DEVICE_KEY_LOCAL || !opcode_len ||
        mic_64 > 1 || (!params && params_len) ||
        params_len > MESH_TRANSPORT_MAX_ACCESS - (mic_64 ? 4u : 0u) -
                     opcode_len) return 0;

    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
    for (size_t i = 0; i < opcode_len; i++) {
        data[i] = (uint8_t)(opcode >> (8 * (opcode_len - 1 - i)));
    }

    if (params_len) memcpy(data + opcode_len, params, params_len);
    return mesh_transport_queue(src, mesh_virtual_address(label), ttl,
                                     app_key_index, label, data,
                                     opcode_len + params_len, mic_64);
}

// Poll transport and decode one Access message. The decoded params point into
// message, so keep it alive until the model handles the result.
// Returns 1 if decoded, 0 if none/malformed, or -1 on a transport error.
static inline int mesh_access_poll(mesh_access_message *message,
                                       mesh_access_pdu *access) {
    if (!message || !access) return -1;
    int result = mesh_transport_poll(message);
    if (result < 0) return result;
    if (!result) {
        mesh_access_ack_poll();
        return 0;
    }
    if (!message->len || message->len > sizeof(message->data)) {
        mesh_access_ack_poll();
        return 0;
    }

    uint8_t first = message->data[0];
    size_t opcode_len = (first & 0x80) == 0 ? 1 :
                        (first & 0xc0) == 0x80 ? 2 : 3;
    if (first == 0x7f || message->len < opcode_len) {
        mesh_access_ack_poll();
        return 0;
    }

    uint32_t opcode = first;
    for (size_t i = 1; i < opcode_len; i++)
        opcode = (opcode << 8) | message->data[i];

    *access = (mesh_access_pdu){
        .src = message->src,
        .dst = message->dst,
        .app_key_index = message->app_key_index,
        .device_key_owner = message->device_key_owner,
        .net_key_index = message->net_key_index,
        .ttl = message->ttl,
        .has_label = message->has_label,
        .opcode = opcode,
        .params = message->data + opcode_len,
        .params_len = message->len - opcode_len
    };
    if (message->has_label) memcpy(access->label, message->label, 16);
    if (mesh_access_ack.state == MESH_ACCESS_ACK_PENDING &&
        access->src == mesh_access_ack.dst && access->dst == mesh_access_ack.src &&
        access->app_key_index == mesh_access_ack.app_key_index &&
        access->opcode == mesh_access_ack.response_opcode)
        mesh_access_ack.state = MESH_ACCESS_ACK_COMPLETE;
    mesh_access_ack_poll();
    return 1;
}

#endif
