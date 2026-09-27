#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Exercise Access framing without a radio or cryptographic implementation.
#define ISLER_BLE_MESH_TRANSPORT_H
#define MESH_TRANSPORT_MAX_ACCESS 380
#define APP_KEY_INDEX_NONE 0xffff
typedef struct {
    uint16_t src, dst, app_key_index, len;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_message;

static uint8_t sent[MESH_TRANSPORT_MAX_ACCESS];
static size_t sent_len;
static uint16_t sent_dst;
static uint8_t sent_ttl;
static uint16_t sent_app_key_index;
static uint8_t sent_mic_64;
static mesh_access_message incoming;
static int incoming_ready;

static int ble_mesh_transport_queue(uint16_t dst, uint8_t ttl,
                                         uint16_t app_key_index,
                                         const uint8_t label[16],
                                         const uint8_t *data, size_t len,
                                         uint8_t mic_64) {
    (void)label;
    memcpy(sent, data, len);
    sent_len = len;
    sent_dst = dst;
    sent_ttl = ttl;
    sent_app_key_index = app_key_index;
    sent_mic_64 = mic_64;
    return 1;
}

static uint16_t ble_mesh_virtual_address(const uint8_t label[16]) {
    assert(label);
    return 0x8001;
}

static int ble_mesh_transport_poll(mesh_access_message *out) {
    if (!incoming_ready) return 0;
    *out = incoming;
    incoming_ready = 0;
    return 1;
}

#include "../ble_mesh_3access.h"

int main(void) {
    const uint8_t params[] = {0x12, 0x34};
    assert(ble_mesh_access_queue(0x1201, 5, 0, 0x01, params, 2, 0) == 1);
    assert(sent_len == 3 && sent[0] == 0x01 &&
           memcmp(sent + 1, params, 2) == 0);
    assert(sent_dst == 0x1201 && sent_ttl == 5 && sent_app_key_index == 0);
    assert(sent_mic_64 == 0);
    assert(ble_mesh_access_queue(0x1201, 5, 0, 0x01, params, 2, 1) == 1);
    assert(sent_mic_64 == 1);
    assert(ble_mesh_access_queue(0x1201, 5, 0, 0x01, params, 2, 2) == 0);

    assert(ble_mesh_access_queue(0x1202, 3, APP_KEY_INDEX_NONE,
                                 0x8202, NULL, 0, 0) == 1);
    assert(sent_len == 2 && sent[0] == 0x82 && sent[1] == 0x02);
    assert(sent_app_key_index == APP_KEY_INDEX_NONE);

    assert(ble_mesh_access_queue(0x1203, 2, 0, 0xe33601, params, 2, 0) == 1);
    assert(sent_len == 5 && sent[0] == 0xe3 && sent[1] == 0x36 &&
           sent[2] == 0x01 && memcmp(sent + 3, params, 2) == 0);

    assert(ble_mesh_access_queue(1, 0, 0, 0x7f, NULL, 0, 0) == 0);
    assert(ble_mesh_access_queue(1, 0, 0, 0x1234, NULL, 0, 0) == 0);
    assert(ble_mesh_access_queue(1, 0, 0, 0x8201, NULL, 1, 0) == 0);
    assert(ble_mesh_access_queue(1, 0, 0, 0x8201, params,
                                MESH_TRANSPORT_MAX_ACCESS, 0) == 0);
    uint8_t label[16] = {1};
    assert(ble_mesh_access_queue_virtual(label, 3, 0, 0x8201, NULL, 0, 0) == 1);
    assert(sent_dst == 0x8001 && sent_len == 2);
    assert(ble_mesh_access_queue_virtual(label, 3, 0, 0x8201, NULL, 0, 1) == 1);
    assert(sent_mic_64 == 1);

    mesh_access_message message = {0};
    message.src = 0x1201;
    message.dst = 0x1202;
    message.app_key_index = 0x0123;
    message.ttl = 4;
    message.has_label = 1;
    memcpy(message.label, label, 16);
    message.len = 5;
    const uint8_t payload[] = {0xe3, 0x36, 0x01, 0x12, 0x34};
    memcpy(message.data, payload, sizeof(payload));
    mesh_access_message raw;
    mesh_access_pdu access;
    incoming = message;
    incoming_ready = 1;
    assert(ble_mesh_access_poll(&raw, &access) == 1);
    assert(access.opcode == 0xe33601 && access.src == 0x1201 &&
           access.dst == 0x1202 && access.app_key_index == 0x0123 &&
           access.ttl == 4 && access.params_len == 2 &&
           memcmp(access.params, params, 2) == 0);
    assert(access.has_label && memcmp(access.label, label, 16) == 0);

    message.len = 1;
    incoming = message;
    incoming_ready = 1;
    assert(ble_mesh_access_poll(&raw, &access) == 0); // truncated vendor opcode
    message.data[0] = 0x7f;
    incoming = message;
    incoming_ready = 1;
    assert(ble_mesh_access_poll(&raw, &access) == 0); // reserved opcode

    incoming = message;
    incoming.data[0] = 0x01;
    incoming_ready = 1;
    assert(ble_mesh_access_poll(&raw, &access) == 1);
    assert(access.opcode == 0x01 && access.params_len == 0);
    assert(ble_mesh_access_poll(&raw, &access) == 0);
    assert(ble_mesh_access_poll(NULL, &access) == -1);
    return 0;
}
