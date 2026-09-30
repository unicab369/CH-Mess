#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../ble_mesh_2transport.h"

// A deterministic block transform is sufficient to exercise framing and CCM
// round trips here; the firmware supplies the real AES implementation.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
    uint8_t block[16];
    for (int i = 0; i < 16; i++) block[i] = in[i] ^ key[i] ^ (uint8_t)(i * 13);
    memcpy(out, block, 16);
}

static uint8_t sent[32][31];
static size_t sent_len[32];
static int sent_count;
static mesh_transport_control_message control_event;
static int control_event_count;

static void receive_control_event(const mesh_transport_control_message *message) {
    control_event = *message;
    control_event_count++;
}

int BLE_MESH_QUEUE_TX(const uint8_t *ad, size_t len) {
    assert(sent_count < 32 && len <= sizeof(sent[0]));
    memcpy(sent[sent_count], ad, len);
    sent_len[sent_count++] = len;
    return 0;
}

int BLE_MESH_ADV_POLL(uint8_t *ad, size_t *len) {
    (void)ad;
    (void)len;
    return 0;
}

int BLE_MESH_NETWORK_LOAD_STATE(mesh_net_state *state) {
    (void)state;
    return 0;
}

int BLE_MESH_NETWORK_SAVE_STATE(const mesh_net_state *state) {
    (void)state;
    return 1;
}

int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq) {
    (void)next_seq;
    return 1;
}

int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds) {
    (void)seconds;
    return 0;
}

static uint32_t current_ms = 1000;
uint32_t GET_MILLIS(void) {
    return current_ms;
}

int BLE_MESH_TRANSPORT_GET_DEVICE_KEY(uint16_t address, uint8_t key[16]) {
    if (address != 0x1201 && address != 0x1202) return 0;
    memset(key, address == 0x1201 ? 0x11 : 0x22, 16);
    return 1;
}

static mesh_net_state node(uint16_t address) {
    mesh_net_state state = {0};
    memset(state.net_key, 0x42, 16);
    memset(state.app_keys[0].key, 0x73, 16);
    state.app_keys[0].used = 1;
    state.unicast_address = address;
    state.element_count = 1;
    return state;
}

static mesh_net_state node_with_secondary_subnet(uint16_t address) {
    mesh_net_state state = node(address);
    state.net_key_index = 0x100;
    state.additional_subnets[0].used = 1;
    state.additional_subnets[0].index = 0x222;
    memset(state.additional_subnets[0].key, 0x53, 16);
    state.app_keys[0].index = 0x031;
    state.app_keys[0].net_idx = 0x222;
    return state;
}

static int receive_frame(int index, mesh_access_message *access) {
    mesh_net_message network;
    assert(mesh_net_receive(sent[index] + 2, sent_len[index] - 2,
                                &network) == 1);
    return mesh_transport_receive(&network, access);
}

int main(void) {
    mesh_net_state a = node(0x1201), b = node(0x1202);
    mesh_access_message received;
    const uint8_t short_access[] = {0x82, 0x01, 0x01};

    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 1);
    assert(sent_count == 1);
    a = mesh_network.state;
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.len == sizeof(short_access));
    assert(memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(received.app_key_index == 0);
    assert(received.device_key_owner == 0);

    uint8_t long_access[40];
    for (int i = 0; i < 40; i++) long_access[i] = (uint8_t)i;
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;

    assert(mesh_network_init(&b) == 1);
    for (int i = 0; i < 3; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(3, &received) == 1);
    assert(received.len == sizeof(long_access));
    assert(memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(received.app_key_index == APP_KEY_INDEX_NONE);
    assert(received.device_key_owner == b.unicast_address);
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);

    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.active == 0);

    // Different sources can have segmented messages reassembling concurrently.
    uint8_t concurrent_access[sizeof(long_access)];
    for (size_t i = 0; i < sizeof(concurrent_access); i++)
        concurrent_access[i] = (uint8_t)(255 - i);
    mesh_net_state sender_c = node(0x1203);
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    transport_tx.active = 0;
    assert(sent_count == 4);
    assert(mesh_network_init(&sender_c) == 1);
    assert(mesh_transport_queue(0x1203, 0x1202, 5, 0, NULL,
        concurrent_access, sizeof(concurrent_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    transport_tx.active = 0;
    assert(sent_count == 8);

    memset(transport_rx, 0, sizeof(transport_rx));
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(receive_frame(4, &received) == 0);
    assert(receive_frame(2, &received) == 0);
    assert(receive_frame(6, &received) == 0);
    assert(receive_frame(1, &received) == 0);
    assert(receive_frame(5, &received) == 0);
    size_t active_rx = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++)
        active_rx += transport_rx[i].active != 0;
    assert(active_rx == 6);
    assert(receive_frame(7, &received) == 1 &&
           received.src == sender_c.unicast_address &&
           received.len == sizeof(concurrent_access) &&
           memcmp(received.data, concurrent_access,
                  sizeof(concurrent_access)) == 0);
    assert(receive_frame(3, &received) == 1 &&
           received.src == a.unicast_address &&
           received.len == sizeof(long_access) &&
           memcmp(received.data, long_access, sizeof(long_access)) == 0);

    // Unsegmented traffic bypasses the active SAR transfer, while another
    // segmented message waits in the bounded transport queue.
    memset(transport_rx, 0, sizeof(transport_rx));
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE,
        NULL, long_access, sizeof(long_access), 0) == 1);
    assert(transport_tx.active && sent_count == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, 0, NULL,
        short_access, sizeof(short_access), 0) == 1);
    assert(sent_count == 2 && transport_tx.active);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE,
        NULL, long_access, sizeof(long_access), 0) == 1);
    assert(segmented_tx_queue_count == 1 && sent_count == 2);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE,
        NULL, long_access, sizeof(long_access), 0) == 0);
    assert(segmented_tx_queue_count == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 5);
    a = mesh_network.state;

    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(receive_frame(1, &received) == 1 &&
           received.len == sizeof(short_access) &&
           memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(receive_frame(2, &received) == 0);
    assert(receive_frame(3, &received) == 0);
    assert(receive_frame(4, &received) == 1 &&
           received.len == sizeof(long_access) &&
           memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(mesh_transport_poll(&received) == 0 && sent_count == 6);
    b = mesh_network.state;

    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(5, &received) == 0 && !transport_tx.active);
    assert(segmented_tx_queue_count == 1);
    assert(mesh_transport_poll(&received) == 0);
    assert(transport_tx.active && segmented_tx_queue_count == 0 &&
           sent_count == 7);
    transport_tx.active = 0;
    a = mesh_network.state;

    // Reordered segments are stored and reassembled; replay protection checks
    // the completed message using the final segment's SEQ.
    sent_count = 0;
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;

    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(1, &received) == 0);
    assert(mesh_network.replay_count == 0);
    assert(receive_frame(0, &received) == 0);
    mesh_net_message reordered;
    assert(mesh_net_receive(sent[3] + 2, sent_len[3] - 2,
                                &reordered) == 1);
    uint32_t final_segment_seq = reordered.seq;
    assert(mesh_transport_receive(&reordered, &received) == 0);
    assert(mesh_network.replay_count == 0);
    assert(receive_frame(2, &received) == 1);
    assert(received.len == sizeof(long_access));
    assert(memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(mesh_network.replay_count == 1 &&
           mesh_network.replay[0].seq == final_segment_seq);
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);
    b = mesh_network.state;

    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.active == 0);
    a = mesh_network.state;

    // Missing segments are reported by a partial ACK and retransmitted.
    sent_count = 0;
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;

    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(receive_frame(1, &received) == 0);
    assert(receive_frame(3, &received) == 0);
    current_ms += 200;
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);
    b = mesh_network.state;

    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.acked == 0x0b);
    assert(transport_segment_queue() == 1);
    assert(sent_count == 6);
    a = mesh_network.state;

    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(5, &received) == 1);
    assert(memcmp(received.data, long_access, sizeof(long_access)) == 0);
    current_ms += 150;
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 7);

    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(6, &received) == 0);
    assert(transport_tx.active == 0);

    // Segmented Control messages use 8-byte chunks and can arrive out of order.
    const uint8_t control_params[17] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
    };
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    uint16_t control_seq_zero = (uint16_t)(mesh_network.state.next_seq & 0x1fff);
    for (uint8_t seg_o = 0; seg_o < 3; seg_o++) {
        uint8_t lower[12];
        size_t offset = (size_t)seg_o * MESH_TRANSPORT_CONTROL_SEGMENT_SIZE;
        size_t count = sizeof(control_params) - offset;
        if (count > MESH_TRANSPORT_CONTROL_SEGMENT_SIZE)
            count = MESH_TRANSPORT_CONTROL_SEGMENT_SIZE;
        lower[0] = 0x80 | 0x02; // Segmented Control, opcode 0x02.
        lower[1] = (uint8_t)(control_seq_zero >> 6);
        lower[2] = (uint8_t)((control_seq_zero & 0x3f) << 2);
        lower[3] = (uint8_t)((seg_o << 5) | 2);
        memcpy(lower + 4, control_params + offset, count);
        assert(mesh_net_queue(mesh_network.state.net_key_index,
            mesh_network.state.unicast_address, b.unicast_address, 1, 5,
            lower, count + 4) == 1);
    }
    assert(sent_count == 3);
    a = mesh_network.state;
    control_event_count = 0;
    mesh_transport_set_control_handler(receive_control_event);
    transport_sar_rx = (mesh_sar_rx_state){1, 1, 1, 0, 1};
    assert(mesh_network_init(&b) == 1);
    mesh_net_message friend_poll = {
        .ctl = 1,
        .ttl = 0,
        .src = a.unicast_address,
        .dst = b.unicast_address,
        .net_key_index = mesh_network.state.net_key_index,
        .friendship = 1,
        .transport_len = 2,
        .transport = {0x01, 0x01}
    };
    assert(mesh_transport_receive(&friend_poll, &received) == 0);
    assert(control_event_count == 1 && control_event.opcode == 0x01 &&
           control_event.len == 1 && control_event.params[0] == 1 &&
           control_event.friendship &&
           control_event.src == a.unicast_address &&
           control_event.dst == b.unicast_address);
    control_event_count = 0;
    assert(receive_frame(1, &received) == 0);
    assert(receive_frame(0, &received) == 0);
    assert(receive_frame(2, &received) == 0);
    assert(mesh_transport_poll(&received) == 0);
    assert(control_event_count == 1);
    assert(control_event.opcode == 0x02 &&
           control_event.len == sizeof(control_params));
    assert(control_event.src == a.unicast_address &&
           control_event.dst == b.unicast_address);
    assert(memcmp(control_event.params, control_params,
                  sizeof(control_params)) == 0);
    mesh_transport_set_control_handler(NULL);
    assert(mesh_transport_poll(&received) == 0 && sent_count == 4);
    current_ms += 9;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 4);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 5);
    current_ms += 10;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 5);
    transport_sar_rx = MESH_SAR_RX_DEFAULT;
    assert(sent_count == 5); // Initial Segment ACK plus one configured repeat.

    // Exercise the LPN-side Friend Request -> Offer -> Poll -> Update path.
    memset(&transport_lpn, 0, sizeof(transport_lpn));
    sent_count = 0;
    assert(mesh_network_init(&b) == 1);
    assert(mesh_lpn_start(mesh_network.state.net_key_index, 0x01, 10,
                          10000, 0, 0) == 1);
    assert(transport_lpn.state == MESH_LPN_REQUESTING && sent_count == 1);
    mesh_net_message request;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2, &request) == 1);
    assert(request.ctl && request.dst == MESH_FRIENDS_ADDRESS &&
           request.ttl == 0 && request.transport_len == 11 &&
           request.transport[0] == MESH_CONTROL_FRIEND_REQUEST &&
           request.transport[1] == 0x01 && request.transport[2] == 10 &&
           request.transport[8] == 1);
    assert(mesh_network_init(&b) == 1);
    mesh_net_message offer = {
        .ctl = 1,
        .ttl = 0,
        .src = a.unicast_address,
        .dst = b.unicast_address,
        .net_key_index = mesh_network.state.net_key_index,
        .transport_len = 7,
        .transport = {MESH_CONTROL_FRIEND_OFFER, 20, 2, 4, 0, 0, 7}
    };
    assert(mesh_transport_receive(&offer, &received) == 0);
    assert(transport_lpn.state == MESH_LPN_WAITING_FOR_UPDATE &&
           transport_lpn.friend_address == a.unicast_address && sent_count == 2);
    mesh_net_message update = {
        .ctl = 1,
        .ttl = 0,
        .src = a.unicast_address,
        .dst = b.unicast_address,
        .net_key_index = mesh_network.state.net_key_index,
        .friendship = 1,
        .transport_len = 7,
        .transport = {MESH_CONTROL_FRIEND_UPDATE, 0, 0, 0, 0, 0, 0}
    };
    assert(mesh_transport_receive(&update, &received) == 0);
    assert(mesh_lpn_friend_address() == a.unicast_address);

    b = mesh_network.state;
    assert(mesh_network_init(&a) == 1);
    mesh_net_message control_ack;
    assert(mesh_net_receive(sent[3] + 2, sent_len[3] - 2,
                                &control_ack) == 1);
    assert(control_ack.ctl && control_ack.transport_len == 7 &&
           control_ack.transport[0] == 0 && control_ack.transport[6] == 0x07);

    // A virtual destination needs the matching Label UUID as CCM AAD.
    uint8_t label[16] = {1, 2, 3, 4};
    uint8_t wrong[16] = {5, 6, 7, 8};
    uint16_t virtual_dst = mesh_virtual_address(label);
    assert(virtual_dst >= 0x8000 && virtual_dst <= 0xbfff);
    sent_count = 0;
    assert(mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 0);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                         short_access, sizeof(short_access), 0) == 1);
    a = mesh_network.state;
    assert(mesh_network_init(&b) == 1);
    mesh_transport_clear_labels();
    assert(receive_frame(0, &received) == 0);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_label_add(wrong) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_label_add(label) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.dst == virtual_dst && received.has_label &&
           memcmp(received.label, label, 16) == 0 &&
           memcmp(received.data, short_access, sizeof(short_access)) == 0);

    // A matching 16-bit hash alone is insufficient: authenticate the UUID.
    mesh_transport_clear_labels();
    assert(mesh_label_add(wrong) == 1);
    transport_labels[0].address = virtual_dst;
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);

    // Segmented virtual messages use the same label and need no segment ACK.
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;
    assert(mesh_network_init(&b) == 1);
    mesh_transport_clear_labels();
    assert(mesh_label_add(label) == 1);
    for (int i = 0; i < 3; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(3, &received) == 1);
    assert(received.has_label && received.len == sizeof(long_access) &&
           memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(mesh_transport_poll(&received) == 0);
    assert(transport_tx.active && sent_count == 4);
    transport_tx.active = 0; // The separate group-retry test covers retransmission.

    // A second AppKey is selected by index and identified after decryption.
    sent_count = 0;
    a.app_keys[1].used = b.app_keys[1].used = 1;
    a.app_keys[1].index = b.app_keys[1].index = 0x235;
    memset(a.app_keys[1].key, 0x99, 16);
    memset(b.app_keys[1].key, 0x99, 16);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    a = mesh_network.state;
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.app_key_index == 0x235);
    b.app_keys[1].used = 0;
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);

    // AID is only six bits: a collision still has to try both full keys.
    uint8_t candidate[16] = {0};
    uint8_t target_aid = transport_app_aid(a.app_keys[0].key);
    int collision_found = 0;
    for (int value = 0; value < 256; value++) {
        candidate[15] = (uint8_t)value;
        if (memcmp(candidate, a.app_keys[0].key, 16) != 0 &&
            transport_app_aid(candidate) == target_aid) {
            collision_found = 1;
            break;
        }
    }
    assert(collision_found);
    memcpy(a.app_keys[1].key, candidate, 16);
    memcpy(b.app_keys[1].key, candidate, 16);
    b.app_keys[1].used = 1;
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.app_key_index == 0x235);

    // An 8-byte TransMIC uses segmented transport even for a short message.
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0, NULL,
                                    short_access, sizeof(short_access), 1) == 1);
    assert(sent_count == 1 && transport_tx.active &&
           transport_tx.seg_n == 0);
    a = mesh_network.state;
    assert(mesh_network_init(&b) == 1);
    mesh_net_message net;
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2, &net) == 1);
    assert((net.transport[0] & 0x80) && (net.transport[1] & 0x80));
    assert(mesh_transport_receive(&net, &received) == 1);
    assert(received.len == sizeof(short_access) &&
           memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 2);
    assert(mesh_transport_receive(&net, &received) == 0); // Completed duplicate.
    assert(mesh_transport_poll(&received) == 0 && sent_count == 2);
    current_ms += 149;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 2);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 3);
    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(1, &received) == 0 && !transport_tx.active);

    // 376 bytes plus an 8-byte TransMIC fills all 32 segments.
    uint8_t max_access[376];
    for (size_t i = 0; i < sizeof(max_access); i++) max_access[i] = (uint8_t)i;
    sent_count = 0;
    assert(mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                    max_access, sizeof(max_access) + 1, 1) == 0);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                    max_access, sizeof(max_access), 1) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 32 && transport_tx.seg_n == 31);
    assert(mesh_network_init(&b) == 1);
    mesh_transport_clear_labels();
    assert(mesh_label_add(label) == 1);
    for (int i = 0; i < 31; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(31, &received) == 1);
    assert(received.len == sizeof(max_access) &&
           memcmp(received.data, max_access, sizeof(max_access)) == 0);

    // An AppKey message can target the second element, and it can reply from it.
    mesh_net_state c = node(0x1300);
    a.element_count = 2;
    sent_count = 0;
    transport_tx.active = 0;
    assert(mesh_network_init(&c) == 1);
    assert(mesh_transport_queue(mesh_network.state.unicast_address, 0x1202, 5, 0, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(0, &received) == 1 && received.dst == 0x1202);
    sent_count = 0;
    assert(mesh_transport_queue(0x1202, 0x1300, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&c) == 1);
    assert(receive_frame(0, &received) == 1 && received.src == 0x1202);

    // Requests use the target's key; server replies use the server's own key.
    a = node(0x1201);
    b = node(0x1202);
    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1 &&
           received.device_key_owner == received.dst);
    sent_count = 0;
    assert(mesh_transport_queue(0x1202, 0x1201, 5, DEVICE_KEY_LOCAL, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&a) == 1);
    assert(receive_frame(0, &received) == 1 &&
           received.device_key_owner == received.src);
    // AppKey traffic uses its owning subnet; Device Key traffic can select one.
    a = node_with_secondary_subnet(0x1201);
    b = node_with_secondary_subnet(0x1202);
    sent_count = 0;
    transport_tx.active = 0;
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, 0x031, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.net_key_index == 0x222 && received.app_key_index == 0x031);

    sent_count = 0;
    assert(mesh_network_init(&a) == 1);
    mesh_network.reply_net_idx = 0x222;
    assert(mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE,
        NULL, short_access, sizeof(short_access), 0) == 1);
    mesh_network.reply_net_idx = mesh_network.state.net_key_index;
    assert(mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.net_key_index == 0x222 &&
           received.device_key_owner == received.dst);

    // Two segmented messages from different sources can be reassembled at once.
    uint8_t stream_a[4][31], stream_c[4][31];
    size_t stream_a_len[4], stream_c_len[4];
    sent_count = 0;
    a = node(0x1201);
    b = node(0x1202);
    c = node(0x1300);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    for (int i = 0; i < 4; i++) {
        memcpy(stream_a[i], sent[i], sent_len[i]);
        stream_a_len[i] = sent_len[i];
    }
    transport_tx.active = 0;
    sent_count = 0;
    assert(mesh_network_init(&c) == 1);
    assert(mesh_transport_queue(0x1300, 0x1202, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    for (int i = 0; i < 4; i++) {
        memcpy(stream_c[i], sent[i], sent_len[i]);
        stream_c_len[i] = sent_len[i];
    }
    assert(mesh_network_init(&b) == 1);
    memset(transport_rx, 0, sizeof(transport_rx));
    for (int i = 0; i < 4; i++) {
        memcpy(sent[0], stream_a[i], stream_a_len[i]);
        sent_len[0] = stream_a_len[i];
        int result_a = receive_frame(0, &received);
        uint16_t src_a = received.src;
        uint8_t data_a[sizeof(long_access)];
        memcpy(data_a, received.data, sizeof(data_a));
        memcpy(sent[0], stream_c[i], stream_c_len[i]);
        sent_len[0] = stream_c_len[i];
        int result_c = receive_frame(0, &received);
        if (i < 3) assert(result_a == 0 && result_c == 0);
        else {
            assert(result_a == 1 && src_a == 0x1201 &&
                   memcmp(data_a, long_access, sizeof(long_access)) == 0);
            assert(result_c == 1 && received.src == 0x1300 &&
                   memcmp(received.data, long_access, sizeof(long_access)) == 0);
        }
    }

    // Group segmented sends retransmit the whole message twice without ACKs.
    sent_count = 0;
    transport_tx.active = 0;
    a = node(0x1201);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0xc001, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4 && transport_tx.active);

    current_ms += 249;
    assert(mesh_transport_poll(&received) == 0);
    assert(sent_count == 4);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0);
    for (int i = 0; i < 4; i++) {
        current_ms += 60;
        assert(mesh_transport_poll(&received) == 0);
    }
    assert(sent_count == 8 && transport_tx.retries == 1);

    current_ms += 250;
    assert(mesh_transport_poll(&received) == 0);
    for (int i = 0; i < 4; i++) {
        current_ms += 60;
        assert(mesh_transport_poll(&received) == 0);
    }
    assert(sent_count == 12 && transport_tx.retries == 2);

    current_ms += 250;
    assert(mesh_transport_poll(&received) == 0);
    assert(!transport_tx.active && sent_count == 12);

    // Segments are paced by the configurable SAR interval (default 60 ms).
    mesh_sar_tx_state sar = mesh_transport_get_sar_transmitter();
    assert(sar.segment_interval_step == 5);
    sar.segment_interval_step = 16;
    assert(!mesh_sar_tx_valid(&sar));
    sar.segment_interval_step = 5;
    assert(mesh_sar_tx_valid(&sar));
    transport_sar_tx = sar;
    sent_count = 0;
    a = node(0x1201);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0xc001, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    assert(sent_count == 1 && transport_tx.next_seg == 1);
    assert(mesh_transport_poll(&received) == 0 && sent_count == 1);
    current_ms += 59;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 1);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 2);
    while (transport_tx.next_seg <= transport_tx.seg_n) {
        current_ms += 60;
        assert(mesh_transport_poll(&received) == 0);
    }
    assert(sent_count == 4);
    transport_tx.active = 0;

    // SAR Discard Timeout controls how long partial RX state is retained.
    sent_count = 0;
    a = node(0x1201);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_transport_queue(0x1201, 0x1202, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    transport_tx.active = 0;
    sent_count = 0;
    b = node(0x1202);
    assert(mesh_network_init(&b) == 1);
    memset(transport_rx, 0, sizeof(transport_rx));
    transport_sar_rx = (mesh_sar_rx_state){31, 1, 0, 5, 0};
    assert(receive_frame(0, &received) == 0 && transport_rx[0].active);
    assert(mesh_transport_poll(&received) == 0 && sent_count == 0);
    current_ms += 149;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 0);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 1);
    current_ms += 4849;
    assert(mesh_transport_poll(&received) == 0 && transport_rx[0].active);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && !transport_rx[0].active);

    // A full pool of incomplete reassemblies rejects a new unicast transfer
    // with an empty Segment ACK instead of overwriting stored segments.
    b = node(0x1202);
    a = node(0x1201);
    sent_count = 0;
    memset(transport_rx, 0, sizeof(transport_rx));
    assert(mesh_network_init(&b) == 1);
    for (uint32_t seq = 0; seq < MESH_TRANSPORT_RX_PACKET_SLOTS; seq++) {
        mesh_net_message net = {
            .ctl = 0,
            .ttl = 5,
            .seq = seq,
            .iv_index = mesh_network.state.iv_index,
            .src = a.unicast_address,
            .dst = b.unicast_address,
            .net_key_index = mesh_network.state.net_key_index,
            .transport_len = 16,
            .transport = {0x80, 0, 0, 31}
        };
        uint16_t seq_zero = (uint16_t)(seq & 0x1fff);
        net.transport[1] = (uint8_t)(seq_zero >> 6);
        net.transport[2] = (uint8_t)((seq_zero & 0x3f) << 2);
        assert(mesh_transport_receive(&net, &received) == 0);
    }
    active_rx = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++)
        active_rx += transport_rx[i].active != 0;
    assert(active_rx == MESH_TRANSPORT_RX_PACKET_SLOTS);

    mesh_net_message overflow = {
        .ctl = 0,
        .ttl = 5,
        .seq = MESH_TRANSPORT_RX_PACKET_SLOTS,
        .iv_index = mesh_network.state.iv_index,
        .src = a.unicast_address,
        .dst = b.unicast_address,
        .net_key_index = mesh_network.state.net_key_index,
        .transport_len = 16,
        .transport = {0x80, 0, 0, 31}
    };
    uint16_t overflow_seq_zero =
        (uint16_t)(overflow.seq & 0x1fff);
    overflow.transport[1] = (uint8_t)(overflow_seq_zero >> 6);
    overflow.transport[2] = (uint8_t)((overflow_seq_zero & 0x3f) << 2);
    assert(mesh_transport_receive(&overflow, &received) == 0);
    active_rx = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++)
        active_rx += transport_rx[i].active != 0;
    assert(active_rx == MESH_TRANSPORT_RX_PACKET_SLOTS && sent_count == 1);

    assert(mesh_network_init(&a) == 1);
    mesh_net_message rejected_ack;
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2,
                                &rejected_ack) == 1);
    assert(rejected_ack.ctl && rejected_ack.transport_len == 7 &&
           rejected_ack.transport[0] == 0 &&
           rejected_ack.transport[3] == 0 &&
           rejected_ack.transport[4] == 0 &&
           rejected_ack.transport[5] == 0 &&
           rejected_ack.transport[6] == 0);

    // Exercise the Friend side of Request -> Offer -> Poll -> Update.
    memset(transport_rx, 0, sizeof(transport_rx));
    memset(&transport_lpn, 0, sizeof(transport_lpn));
    sent_count = 0;
    assert(mesh_friend_enable(mesh_network.state.net_key_index, 10, 0, 0));
    mesh_transport_control_message friend_request = {
        .src = b.unicast_address,
        .dst = MESH_FRIENDS_ADDRESS,
        .net_key_index = mesh_network.state.net_key_index,
        .ttl = 0,
        .opcode = MESH_CONTROL_FRIEND_REQUEST,
        .len = 10,
        .params = {0x01, 10, 0, 0, 100, 0, 0, 1, 0, 7}
    };
    mesh_friend_request_receive(&friend_request);
    assert(transport_friend_offers[0].used &&
           mesh_friend_next_counter() == 1);
    uint16_t offered_counter = transport_friend_offers[0].friend_counter;
    mesh_net_state friend_state = mesh_network.state;
    current_ms += 100;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 1);
    assert(mesh_network_init(&b) == 1);
    mesh_net_message friend_offer;
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2,
                            &friend_offer) == 1);
    assert(friend_offer.transport[0] == MESH_CONTROL_FRIEND_OFFER &&
           friend_offer.dst == b.unicast_address &&
           friend_offer.transport[2] == MESH_FRIEND_QUEUE_CAPACITY &&
           friend_offer.transport[5] == 0);

    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7,
        offered_counter));
    mesh_transport_control_message friend_poll_control = {
        .src = b.unicast_address,
        .dst = friend_state.unicast_address,
        .net_key_index = friend_request.net_key_index,
        .ttl = 0,
        .opcode = MESH_CONTROL_FRIEND_POLL,
        .friendship = 1,
        .len = 1,
        .params = {0}
    };
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 2);
    mesh_friend_disable();
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index,
        b.unicast_address, friend_state.unicast_address, 7,
        offered_counter));
    mesh_net_message friend_update;
    assert(mesh_net_receive(sent[1] + 2, sent_len[1] - 2,
                            &friend_update) == 1);
    assert(friend_update.friendship &&
           friend_update.transport[0] == MESH_CONTROL_FRIEND_UPDATE &&
           friend_update.transport[6] == 0);
    return 0;
}
