#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Exercise Access framing without a radio or cryptographic implementation.
#define ISLER_MESH_TRANSPORT_H
#define MESH_TRANSPORT_MAX_ACCESS 380
#define APP_KEY_INDEX_NONE 0xffff
#define DEVICE_KEY_LOCAL 0xfffe
typedef struct {
    uint16_t src, dst, app_key_index, len;
    uint16_t device_key_owner;
    uint16_t net_key_index;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_message;

static struct {
    struct { uint16_t unicast_address; } state;
    uint16_t reply_net_idx;
} mesh_network;

static uint8_t sent[MESH_TRANSPORT_MAX_ACCESS];
static size_t sent_len;
static uint16_t sent_dst;
static uint8_t sent_ttl;
static uint16_t sent_app_key_index;
static uint16_t sent_net_key_index;
static uint8_t sent_mic_64;
static int send_count;
static mesh_access_message incoming;
static int incoming_ready;
static uint32_t now_ms;

uint32_t GET_MILLIS(void) { return now_ms; }

static int mesh_transport_queue(uint16_t src, uint16_t dst, uint8_t ttl,
                                         uint16_t app_key_index,
                                         const uint8_t label[16],
                                         const uint8_t *data, size_t len,
                                         uint8_t mic_64) {
    (void)src;
    (void)label;
    sent_net_key_index = mesh_network.reply_net_idx;
    memcpy(sent, data, len);
    sent_len = len;
    sent_dst = dst;
    sent_ttl = ttl;
    sent_app_key_index = app_key_index;
    sent_mic_64 = mic_64;
    send_count++;
    return 1;
}

static uint16_t mesh_virtual_address(const uint8_t label[16]) {
    assert(label);
    return 0x8001;
}

static int mesh_transport_poll(mesh_access_message *out) {
    if (!incoming_ready) return 0;
    *out = incoming;
    incoming_ready = 0;
    return 1;
}

#include "../ble_mesh/mesh_3access.h"

int main(void) {
    const uint8_t params[] = {0x12, 0x34};
    assert(mesh_access_queue(0x1200, 0x1201, 5, 0, 0x01, params, 2, 0) == 1);
    assert(sent_len == 3 && sent[0] == 0x01 &&
           memcmp(sent + 1, params, 2) == 0);
    assert(sent_dst == 0x1201 && sent_ttl == 5 && sent_app_key_index == 0);
    assert(sent_mic_64 == 0);
    assert(mesh_access_queue(0x1200, 0x1201, 5, 0, 0x01, params, 2, 1) == 1);
    assert(sent_mic_64 == 1);
    assert(mesh_access_queue(0x1200, 0x1201, 5, 0, 0x01, params, 2, 2) == 0);

    assert(mesh_access_queue(0x1200, 0x1202, 3, APP_KEY_INDEX_NONE,
                                 0x8202, NULL, 0, 0) == 1);
    assert(sent_len == 2 && sent[0] == 0x82 && sent[1] == 0x02);
    assert(sent_app_key_index == APP_KEY_INDEX_NONE);

    mesh_network.reply_net_idx = 0x123;
    mesh_network.state.unicast_address = 0x1200;
    assert(mesh_access_queue_on_net(0x234, 0x1202, 3,
                                        0x8202, NULL, 0));
    assert(sent_net_key_index == 0x234 && mesh_network.reply_net_idx == 0x123);

    assert(mesh_access_queue(0x1200, 0x1203, 2, 0, 0xe33601, params, 2, 0) == 1);
    assert(sent_len == 5 && sent[0] == 0xe3 && sent[1] == 0x36 &&
           sent[2] == 0x01 && memcmp(sent + 3, params, 2) == 0);

    assert(mesh_access_queue(0x1200, 1, 0, 0, 0x7f, NULL, 0, 0) == 0);
    assert(mesh_access_queue(0x1200, 1, 0, 0, 0x1234, NULL, 0, 0) == 0);
    assert(mesh_access_queue(0x1200, 1, 0, 0, 0x8201, NULL, 1, 0) == 0);
    assert(mesh_access_queue(0x1200, 1, 0, 0, 0x8201, params,
                                MESH_TRANSPORT_MAX_ACCESS, 0) == 0);
    uint8_t label[16] = {1};
    assert(mesh_access_queue_virtual(0x1200, label, 3, 0,
                                              0x8201, NULL, 0, 0) == 1);
    assert(sent_dst == 0x8001 && sent_len == 2);
    assert(mesh_access_queue_virtual(0x1200, label, 3, 0,
                                              0x8201, NULL, 0, 1) == 1);
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
    assert(mesh_access_poll(&raw, &access) == 1);
    assert(access.opcode == 0xe33601 && access.src == 0x1201 &&
           access.dst == 0x1202 && access.app_key_index == 0x0123 &&
           access.ttl == 4 && access.params_len == 2 &&
           memcmp(access.params, params, 2) == 0);
    assert(access.has_label && memcmp(access.label, label, 16) == 0);

    message.app_key_index = APP_KEY_INDEX_NONE;
    message.device_key_owner = message.src;
    message.has_label = 0;
    incoming = message;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 1);
    assert(access.device_key_owner == message.src);

    message.len = 1;
    incoming = message;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 0); // truncated vendor opcode
    message.data[0] = 0x7f;
    incoming = message;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 0); // reserved opcode

    incoming = message;
    incoming.data[0] = 0x01;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 1);
    assert(access.opcode == 0x01 && access.params_len == 0);
    assert(mesh_access_poll(&raw, &access) == 0);
    assert(mesh_access_poll(NULL, &access) == -1);

    // Acknowledged requests retain their TID-bearing payload, retry on timeout,
    // and stop retrying when the expected response matches the peer and key.
    now_ms = 1000;
    send_count = 0;
    mesh_network.state.unicast_address = 0x1200;
    assert(mesh_access_queue_acknowledged(0x1200, 0x1201, 5, 0x0123,
        0x8202, 0x8204, params, sizeof(params), 0, 100, 2));
    assert(mesh_access_ack_status() == MESH_ACCESS_ACK_PENDING &&
           send_count == 1 && sent[0] == 0x82 && sent[1] == 0x02);
    now_ms += 100;
    assert(mesh_access_poll(&raw, &access) == 0 && send_count == 2);
    now_ms += 100;
    assert(mesh_access_poll(&raw, &access) == 0 && send_count == 3);
    now_ms += 100;
    assert(mesh_access_poll(&raw, &access) == 0 &&
           mesh_access_ack_status() == MESH_ACCESS_ACK_TIMED_OUT &&
           send_count == 3);

    assert(mesh_access_queue_acknowledged(0x1200, 0x1201, 5, 0x0123,
        0x8201, 0x8204, NULL, 0, 0, 100, 1));
    incoming = (mesh_access_message){
        .src = 0x1202, .dst = 0x1200, .app_key_index = 0x0123,
        .net_key_index = 0, .ttl = 4, .len = 3,
        .data = {0x82, 0x04, 1}
    };
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 1 &&
           mesh_access_ack_status() == MESH_ACCESS_ACK_PENDING);
    incoming.src = 0x1201;
    incoming.app_key_index = 0x0124;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 1 &&
           mesh_access_ack_status() == MESH_ACCESS_ACK_PENDING);
    incoming.app_key_index = 0x0123;
    incoming_ready = 1;
    assert(mesh_access_poll(&raw, &access) == 1 &&
           access.opcode == 0x8204 &&
           mesh_access_ack_status() == MESH_ACCESS_ACK_COMPLETE);
    return 0;
}
