#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Exercise Access framing without a radio or cryptographic implementation.
#define ISLER_BLE_MESH_TRANSPORT_H
#define MESH_TRANSPORT_MAX_ACCESS 380
typedef struct {
    uint16_t src, dst, app_key_index, len;
    uint8_t ttl;
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_message;

static uint8_t sent[MESH_TRANSPORT_MAX_ACCESS];
static size_t sent_len;
static uint16_t sent_dst;
static uint8_t sent_ttl, sent_device_key;
static mesh_access_message incoming;
static int incoming_ready;

static int ble_mesh_transport_send(uint16_t dst, uint8_t ttl,
                                   uint8_t use_device_key,
                                   const uint8_t *data, size_t len) {
    memcpy(sent, data, len);
    sent_len = len;
    sent_dst = dst;
    sent_ttl = ttl;
    sent_device_key = use_device_key;
    return 1;
}

static int ble_mesh_transport_poll(mesh_access_message *out) {
    if (!incoming_ready) return 0;
    *out = incoming;
    incoming_ready = 0;
    return 1;
}

#include "../ble_mesh_access.h"

static uint32_t received_opcode;
static uint16_t received_src, received_dst, received_key;
static uint8_t received_ttl, received_params[8];
static size_t received_params_len;
static int handled;

int BLE_MESH_ACCESS_HANDLE(const mesh_access_pdu *message) {
    received_opcode = message->opcode;
    received_src = message->src;
    received_dst = message->dst;
    received_key = message->app_key_index;
    received_ttl = message->ttl;
    received_params_len = message->params_len;
    assert(message->params_len <= sizeof(received_params));
    memcpy(received_params, message->params, message->params_len);
    return handled;
}

int main(void) {
    const uint8_t params[] = {0x12, 0x34};
    assert(ble_mesh_access_send(0x1201, 5, 0, 0x01, params, 2) == 1);
    assert(sent_len == 3 && sent[0] == 0x01 &&
           memcmp(sent + 1, params, 2) == 0);
    assert(sent_dst == 0x1201 && sent_ttl == 5 && !sent_device_key);

    assert(ble_mesh_access_send(0x1202, 3, 1, 0x8202, NULL, 0) == 1);
    assert(sent_len == 2 && sent[0] == 0x82 && sent[1] == 0x02);
    assert(sent_device_key == 1);

    assert(ble_mesh_access_send(0x1203, 2, 0, 0xe33601, params, 2) == 1);
    assert(sent_len == 5 && sent[0] == 0xe3 && sent[1] == 0x36 &&
           sent[2] == 0x01 && memcmp(sent + 3, params, 2) == 0);

    assert(ble_mesh_access_send(1, 0, 0, 0x7f, NULL, 0) == 0);
    assert(ble_mesh_access_send(1, 0, 0, 0x1234, NULL, 0) == 0);
    assert(ble_mesh_access_send(1, 0, 0, 0x8201, NULL, 1) == 0);
    assert(ble_mesh_access_send(1, 0, 0, 0x8201, params,
                                MESH_TRANSPORT_MAX_ACCESS) == 0);

    mesh_access_message message = {0};
    message.src = 0x1201;
    message.dst = 0x1202;
    message.app_key_index = 0x0123;
    message.ttl = 4;
    message.len = 5;
    const uint8_t payload[] = {0xe3, 0x36, 0x01, 0x12, 0x34};
    memcpy(message.data, payload, sizeof(payload));
    handled = 1;
    assert(ble_mesh_access_receive(&message) == 1);
    assert(received_opcode == 0xe33601 && received_src == 0x1201 &&
           received_dst == 0x1202 && received_key == 0x0123 &&
           received_ttl == 4 && received_params_len == 2 &&
           memcmp(received_params, params, 2) == 0);

    handled = 0;
    assert(ble_mesh_access_receive(&message) == 0);
    handled = 1;
    message.len = 1;
    assert(ble_mesh_access_receive(&message) == 0); // truncated vendor opcode
    message.data[0] = 0x7f;
    assert(ble_mesh_access_receive(&message) == 0); // reserved opcode
    assert(ble_mesh_access_receive(NULL) == -1);

    incoming = message;
    incoming.data[0] = 0x01;
    incoming_ready = 1;
    assert(ble_mesh_access_poll() == 1);
    assert(received_opcode == 0x01 && received_params_len == 0);
    assert(ble_mesh_access_poll() == 0);
    return 0;
}
