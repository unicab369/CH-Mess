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
static uint64_t current_seconds = 400000;

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
    *seconds = current_seconds;
    return 1;
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

    mesh_net_state source_state = node(0x1300);
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
    uint8_t saved_control_ack[31];
    size_t saved_control_ack_len = sent_len[3];
    memcpy(saved_control_ack, sent[3], saved_control_ack_len);

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
    assert(mesh_lpn_friend_address() == a.unicast_address &&
           transport_lpn.fsn == 1 &&
           transport_lpn.last_rx_ms == current_ms);

    // Established LPNs poll every third of PollTimeout and keep the same
    // FSN until a friendship response is received.
    mesh_net_state lpn_state = mesh_network.state;
    uint32_t first_poll_at = transport_lpn.last_tx_ms;
    current_ms = first_poll_at + transport_lpn.poll_timeout_ms / 3 - 1;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 2);
    current_ms++;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 3);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    mesh_net_message periodic_poll;
    assert(mesh_net_receive(sent[2] + 2, sent_len[2] - 2,
                            &periodic_poll) == 1);
    assert(periodic_poll.friendship &&
           periodic_poll.transport[0] == MESH_CONTROL_FRIEND_POLL &&
           periodic_poll.transport[1] == 1);

    assert(mesh_network_init(&lpn_state) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    // Without a response, the next Poll repeats the current FSN.
    current_ms = transport_lpn.last_tx_ms +
        transport_lpn.poll_timeout_ms / 3;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 4);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    assert(mesh_net_receive(sent[3] + 2, sent_len[3] - 2,
                            &periodic_poll) == 1);
    assert(periodic_poll.transport[0] == MESH_CONTROL_FRIEND_POLL &&
           periodic_poll.transport[1] == 1);

    assert(mesh_network_init(&lpn_state) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    assert(mesh_transport_receive(&update, &received) == 0);
    assert(transport_lpn.fsn == 0 &&
           transport_lpn.last_rx_ms == current_ms);
    current_ms = transport_lpn.last_tx_ms +
        transport_lpn.poll_timeout_ms / 3;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 5);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    assert(mesh_net_receive(sent[4] + 2, sent_len[4] - 2,
                            &periodic_poll) == 1);
    assert(periodic_poll.transport[0] == MESH_CONTROL_FRIEND_POLL &&
           periodic_poll.transport[1] == 0);

    assert(mesh_network_init(&lpn_state) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    const uint16_t subscription_addresses[2] = {0xc001, 0x8002};
    assert(mesh_lpn_subscription_update(
        MESH_CONTROL_FRIEND_SUBSCRIPTION_ADD, subscription_addresses, 2));
    assert(transport_lpn.subscription_pending &&
           transport_lpn.subscription_pending_transaction == 0 &&
           transport_lpn.subscription_transaction == 1 && sent_count == 6);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    mesh_net_message subscription_request;
    assert(mesh_net_receive(sent[5] + 2, sent_len[5] - 2,
                            &subscription_request) == 1);
    assert(subscription_request.friendship && subscription_request.ttl == 0 &&
           subscription_request.transport_len == 6 &&
           subscription_request.transport[0] ==
                MESH_CONTROL_FRIEND_SUBSCRIPTION_ADD &&
           subscription_request.transport[1] == 0 &&
           subscription_request.transport[2] == 0xc0 &&
           subscription_request.transport[3] == 0x01 &&
           subscription_request.transport[4] == 0x80 &&
           subscription_request.transport[5] == 0x02);
    assert(mesh_network_init(&lpn_state) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    current_ms = transport_lpn.last_tx_ms +
        transport_lpn.poll_timeout_ms / 3;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 8);
    assert(mesh_network_init(&a) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    assert(mesh_net_receive(sent[6] + 2, sent_len[6] - 2,
                            &subscription_request) == 1);
    assert(subscription_request.transport[0] ==
                MESH_CONTROL_FRIEND_SUBSCRIPTION_ADD &&
           subscription_request.transport[1] == 0);
    assert(mesh_network_init(&lpn_state) == 1);
    assert(mesh_friendship_add(lpn_state.net_key_index,
        lpn_state.unicast_address, a.unicast_address, 0, 7));
    mesh_net_message subscription_confirm = {
        .ctl = 1, .ttl = 0, .src = a.unicast_address,
        .dst = lpn_state.unicast_address,
        .net_key_index = lpn_state.net_key_index, .friendship = 1,
        .transport_len = 2,
        .transport = {MESH_CONTROL_FRIEND_SUBSCRIPTION_CONFIRM, 0}
    };
    assert(mesh_transport_receive(&subscription_confirm, &received) == 0 &&
           !transport_lpn.subscription_pending && transport_lpn.fsn == 1);
    assert(mesh_lpn_subscription_update(
        MESH_CONTROL_FRIEND_SUBSCRIPTION_REMOVE, subscription_addresses, 1));
    assert(transport_lpn.subscription_pending_transaction == 1 &&
           sent_count == 9);
    subscription_confirm.transport[1] = 1;
    assert(mesh_transport_receive(&subscription_confirm, &received) == 0 &&
           !transport_lpn.subscription_pending && transport_lpn.fsn == 0);

    current_ms = transport_lpn.last_rx_ms + transport_lpn.poll_timeout_ms;
    assert(mesh_transport_poll(&received) == 0 &&
           transport_lpn.state == MESH_LPN_IDLE &&
           mesh_lpn_friend_address() == 0 && mesh_lpn_next_counter() == 1);

    b = mesh_network.state;
    assert(mesh_network_init(&a) == 1);
    mesh_net_message control_ack;
    assert(mesh_net_receive(saved_control_ack + 2,
                            saved_control_ack_len - 2,
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
    assert(mesh_friend_enable(mesh_network.state.net_key_index, 10, 4, 0));
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
    mesh_net_message queued_for_lpn = {
        .ctl = 0, .ttl = 5, .seq = 0x12345,
        .iv_index = friend_state.iv_index,
        .src = 0x1300, .dst = b.unicast_address,
        .net_key_index = friend_request.net_key_index,
        .transport_len = 3, .transport = {0x00, 0xaa, 0x55}
    };
    assert(mesh_transport_receive(&queued_for_lpn, &received) == 0);
    assert(transport_friend_offers[0].queue_count == 1 &&
           transport_friend_offers[0].queue[0].message.ttl == 4);
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
    assert(friend_update.friendship && friend_update.src == queued_for_lpn.src &&
           friend_update.dst == queued_for_lpn.dst &&
           friend_update.seq == queued_for_lpn.seq && friend_update.ttl == 4 &&
           friend_update.transport_len == queued_for_lpn.transport_len &&
           memcmp(friend_update.transport, queued_for_lpn.transport,
                  queued_for_lpn.transport_len) == 0);

    // An unchanged FSN repeats the same queued PDU; a changed FSN consumes it.
    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friend_enable(friend_request.net_key_index, 10, 4, 0));
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    transport_friend_offers[0].used = 1;
    transport_friend_offers[0].net_key_index = friend_request.net_key_index;
    transport_friend_offers[0].lpn_address = b.unicast_address;
    transport_friend_offers[0].friend_counter = offered_counter;
    transport_friend_offers[0].poll_timeout_ms = 10000;
    transport_friend_offers[0].receive_delay_ms = 10;
    transport_friend_offers[0].expires_at_ms = current_ms + 10000;
    transport_friend_offers[0].offered = 1;
    transport_friend_offers[0].queue[0].message = queued_for_lpn;
    transport_friend_offers[0].queue[0].message.ttl--;
    transport_friend_offers[0].queue_count = 1;
    transport_friend_offers[0].has_poll_fsn = 1;
    transport_friend_offers[0].last_poll_fsn = 0;
    transport_friend_offers[0].last_response_queued = 1;
    sent_count = 0;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 1 && transport_friend_offers[0].queue_count == 1);
    uint8_t first_friend_response[31];
    size_t first_friend_response_len = sent_len[0];
    memcpy(first_friend_response, sent[0], first_friend_response_len);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    int cached_receive_result = mesh_net_receive(sent[0] + 2,
        sent_len[0] - 2, &friend_update);
    assert(cached_receive_result == 1);
    assert(friend_update.seq == queued_for_lpn.seq);
    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 2 && sent_len[1] == first_friend_response_len &&
           memcmp(sent[1], first_friend_response,
                  first_friend_response_len) == 0 &&
           transport_friend_offers[0].queue_count == 1);
    friend_poll_control.params[0] = 1;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 3 && transport_friend_offers[0].queue_count == 0);

    // Subscription List Add is idempotent for a repeated transaction and
    // enables Friend Queue storage for matching group/virtual destinations.
    mesh_transport_control_message subscription_add = {
        .src = b.unicast_address, .dst = friend_state.unicast_address,
        .net_key_index = friend_request.net_key_index, .ttl = 0,
        .opcode = MESH_CONTROL_FRIEND_SUBSCRIPTION_ADD,
        .friendship = 1, .len = 5,
        .params = {12, 0xc0, 0x01, 0x80, 0x02}
    };
    mesh_friend_subscription_receive(&subscription_add);
    assert(transport_friend_offers[0].subscription_count == 2 &&
           transport_friend_offers[0].subscriptions[0] == 0xc001 &&
           transport_friend_offers[0].subscriptions[1] == 0x8002 &&
           transport_friend_offers[0].subscription_confirm_pending &&
           sent_count == 3);
    mesh_friend_subscription_receive(&subscription_add);
    assert(transport_friend_offers[0].subscription_count == 2 &&
           sent_count == 3);
    mesh_net_message queued_group = {
        .ctl = 0, .ttl = 5, .seq = 0x12346,
        .iv_index = friend_state.iv_index,
        .src = 0x1300, .dst = 0xc001,
        .net_key_index = friend_request.net_key_index,
        .transport_len = 3, .transport = {0x00, 0xbb, 0x66}
    };
    assert(mesh_transport_receive(&queued_group, &received) == 0 &&
           transport_friend_offers[0].queue_count == 1 &&
           transport_friend_offers[0].queue[0].message.ttl == 4);
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 4);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    assert(mesh_net_receive(sent[3] + 2, sent_len[3] - 2,
                            &friend_update) == 1);
    assert(friend_update.friendship && friend_update.src == queued_group.src &&
           friend_update.dst == queued_group.dst && friend_update.ttl == 4 &&
           friend_update.seq == queued_group.seq);

    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    current_ms += 10;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 5 &&
           !transport_friend_offers[0].subscription_confirm_pending);
    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    mesh_transport_control_message subscription_remove = {
        .src = b.unicast_address, .dst = friend_state.unicast_address,
        .net_key_index = friend_request.net_key_index, .ttl = 0,
        .opcode = MESH_CONTROL_FRIEND_SUBSCRIPTION_REMOVE,
        .friendship = 1, .len = 3, .params = {13, 0xc0, 0x01}
    };
    mesh_friend_subscription_receive(&subscription_remove);
    assert(transport_friend_offers[0].subscription_count == 1 &&
           transport_friend_offers[0].subscriptions[0] == 0x8002 &&
           sent_count == 5 &&
           transport_friend_offers[0].subscription_confirm_pending);
    current_ms += 10;
    assert(mesh_transport_poll(&received) == 0 && sent_count == 6);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    assert(mesh_net_receive(sent[5] + 2, sent_len[5] - 2,
                            &friend_update) == 1);
    assert(friend_update.friendship &&
           friend_update.transport[0] ==
                MESH_CONTROL_FRIEND_SUBSCRIPTION_CONFIRM &&
           friend_update.transport[1] == 13);

    // A Friend retains a completed segmented message, acknowledges it OBO,
    // and delivers one original segment for each acknowledged Friend Poll.
    mesh_friend_offer *friend_offer_state = &transport_friend_offers[0];
    mesh_friend_queue_clear(friend_offer_state);
    friend_offer_state->queue_count = 0;
    friend_offer_state->used = 1;
    friend_offer_state->offered = 1;
    friend_offer_state->net_key_index = friend_request.net_key_index;
    friend_offer_state->lpn_address = b.unicast_address;
    friend_offer_state->num_elements = 1;
    friend_offer_state->expires_at_ms = current_ms + 10000;
    friend_offer_state->has_poll_fsn = 0;
    friend_offer_state->last_response_queued = 0;
    memset(transport_rx, 0, sizeof(transport_rx));
    sent_count = 0;
    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    mesh_net_message friend_segment = {
        .ctl = 0, .ttl = 5, .seq = 0x2345,
        .iv_index = friend_state.iv_index, .src = 0x1300,
        .dst = b.unicast_address,
        .net_key_index = friend_request.net_key_index,
        .transport_len = 16,
        .transport = {0x80, 0x0d, 0x14, 0x01,
                      0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b}
    };
    assert(mesh_transport_receive(&friend_segment, &received) == 0 &&
           friend_offer_state->queue_count == 0 && sent_count == 0);
    friend_segment.seq = 0x2346;
    friend_segment.transport_len = 9;
    friend_segment.transport[1] = 0x0d;
    friend_segment.transport[2] = 0x14;
    friend_segment.transport[3] = 0x21;
    friend_segment.transport[4] = 0x20;
    friend_segment.transport[5] = 0x21;
    friend_segment.transport[6] = 0x22;
    friend_segment.transport[7] = 0x23;
    friend_segment.transport[8] = 0x24;
    assert(mesh_transport_receive(&friend_segment, &received) == 0 &&
           friend_offer_state->queue_count == 1 && sent_count == 1);
    assert(mesh_network_init(&source_state) == 1);
    mesh_net_message friend_obo_ack;
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2,
                            &friend_obo_ack) == 1);
    assert(friend_obo_ack.ctl && friend_obo_ack.src == friend_state.unicast_address &&
           friend_obo_ack.dst == 0x1300 && friend_obo_ack.transport_len == 7 &&
           friend_obo_ack.transport[0] == 0x80 &&
           friend_obo_ack.transport[6] == 0x03);

    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    friend_poll_control.params[0] = 0;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 2 && friend_offer_state->queue_count == 1);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    mesh_net_message delivered_segment;
    assert(mesh_net_receive(sent[1] + 2, sent_len[1] - 2,
                            &delivered_segment) == 1);
    assert(delivered_segment.friendship && delivered_segment.src == 0x1300 &&
           delivered_segment.dst == b.unicast_address &&
           delivered_segment.ttl == 4 && delivered_segment.transport[3] == 0x01);

    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    friend_poll_control.params[0] = 1;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 3 && friend_offer_state->queue_count == 1);
    assert(mesh_network_init(&b) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    assert(mesh_net_receive(sent[2] + 2, sent_len[2] - 2,
                            &delivered_segment) == 1);
    assert(delivered_segment.transport[3] == 0x21 &&
           delivered_segment.transport_len == 9);

    assert(mesh_network_init(&friend_state) == 1);
    assert(mesh_friendship_add(friend_request.net_key_index, b.unicast_address,
        friend_state.unicast_address, 7, offered_counter));
    friend_poll_control.params[0] = 0;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == 4 && friend_offer_state->queue_count == 0);
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++)
        assert(!transport_rx[i].friend_queued);

    // OBO Segment Acknowledgments complete the sender's active SAR transaction.
    assert(mesh_network_init(&source_state) == 1);
    memset(&transport_tx, 0, sizeof(transport_tx));
    transport_tx.active = 1;
    transport_tx.net_idx = friend_request.net_key_index;
    transport_tx.src = 0x1300;
    transport_tx.dst = b.unicast_address;
    transport_tx.seq_zero = 0x0345;
    transport_tx.seg_n = 1;
    assert(mesh_net_receive(sent[0] + 2, sent_len[0] - 2,
                            &friend_obo_ack) == 1);
    assert(mesh_transport_receive(&friend_obo_ack, &received) == 0 &&
           !transport_tx.active);

    // Friend Updates advance and complete IV Update using the same timing
    // gates as authenticated Secure Network beacons.
    mesh_net_state iv_lpn_state = node(b.unicast_address);
    iv_lpn_state.iv_index = 10;
    iv_lpn_state.iv_time_valid = 1;
    iv_lpn_state.iv_state_start_time = 0;
    iv_lpn_state.next_seq = 7;
    assert(mesh_network_init(&iv_lpn_state) == 1);
    memset(&transport_lpn, 0, sizeof(transport_lpn));
    transport_lpn.net_key_index = iv_lpn_state.net_key_index;
    transport_lpn.friend_address = 0x1201;
    transport_lpn.num_elements = 1;
    transport_lpn.state = MESH_LPN_WAITING_FOR_UPDATE;
    mesh_transport_control_message friend_iv_update = {
        .src = 0x1201, .dst = b.unicast_address,
        .net_key_index = iv_lpn_state.net_key_index,
        .opcode = MESH_CONTROL_FRIEND_UPDATE, .friendship = 1,
        .len = 6, .params = {2, 0, 0, 0, 11, 0}
    };
    mesh_lpn_control_receive(&friend_iv_update);
    assert(transport_lpn.state == MESH_LPN_ESTABLISHED &&
           mesh_network.state.iv_index == 11 && mesh_network.state.iv_update);

    friend_iv_update.params[0] = 2;
    friend_iv_update.params[4] = 13;
    mesh_lpn_control_receive(&friend_iv_update);
    assert(mesh_network.state.iv_index == 11 && mesh_network.state.iv_update);

    friend_iv_update.params[0] = 0;
    friend_iv_update.params[4] = 11;
    mesh_lpn_control_receive(&friend_iv_update);
    assert(mesh_network.state.iv_update && mesh_network.state.next_seq == 7);
    current_seconds += MESH_NETWORK_IV_MIN_SECONDS;
    mesh_lpn_control_receive(&friend_iv_update);
    assert(!mesh_network.state.iv_update && mesh_network.state.next_seq == 0);

    // An LPN sends Friend Clear over friendship credentials and releases the
    // friendship only after the matching confirmation arrives.
    assert(mesh_friendship_add(mesh_network.state.net_key_index,
        mesh_network.state.unicast_address, 0x1201, 12, 3));
    transport_lpn.net_key_index = mesh_network.state.net_key_index;
    transport_lpn.lpn_counter = 12;
    transport_lpn.friend_address = 0x1201;
    transport_lpn.poll_timeout_ms = 10000;
    transport_lpn.state = MESH_LPN_ESTABLISHED;
    int clear_frame_index = sent_count;
    assert(mesh_lpn_clear() && transport_lpn.state == MESH_LPN_CLEARING);
    assert(sent_count == clear_frame_index + 1 && sent_len[clear_frame_index]);
    mesh_transport_control_message clear_confirm = {
        .src = 0x1201, .dst = mesh_network.state.unicast_address,
        .net_key_index = mesh_network.state.net_key_index,
        .opcode = MESH_CONTROL_FRIEND_CLEAR_CONFIRM, .friendship = 1,
        .len = 2,
        .params = {(uint8_t)(mesh_network.state.unicast_address >> 8),
                   (uint8_t)mesh_network.state.unicast_address}
    };
    mesh_lpn_control_receive(&clear_confirm);
    assert(transport_lpn.state == MESH_LPN_IDLE &&
           !mesh_network.friendships[0].used);

    // A Friend validates an LPN's clear, sends confirmation, then frees its
    // retained friendship queue and derived credentials.
    mesh_net_state clear_friend_state = node(0x1201);
    assert(mesh_network_init(&clear_friend_state) == 1);
    assert(mesh_friend_enable(mesh_network.state.net_key_index, 10, 4, 20));
    mesh_friend_offer *clear_offer = &transport_friend_offers[0];
    memset(clear_offer, 0, sizeof(*clear_offer));
    clear_offer->used = 1;
    clear_offer->net_key_index = mesh_network.state.net_key_index;
    clear_offer->lpn_address = 0x1202;
    clear_offer->lpn_counter = 31;
    clear_offer->friend_counter = 9;
    assert(mesh_friendship_add(clear_offer->net_key_index,
        clear_offer->lpn_address, mesh_network.state.unicast_address, 31, 9));
    mesh_transport_control_message lpn_clear_control = {
        .src = 0x1202, .dst = mesh_network.state.unicast_address,
        .net_key_index = clear_offer->net_key_index, .ttl = 0,
        .opcode = MESH_CONTROL_FRIEND_CLEAR, .friendship = 1,
        .len = 4, .params = {0x12, 0x02, 0, 31}
    };
    int clear_confirm_index = sent_count;
    mesh_friend_clear_receive(&lpn_clear_control);
    assert(!clear_offer->used && sent_count == clear_confirm_index + 1);

    // A replacement Friend tells the LPN's previous Friend after the new
    // friendship is established, retries with exponential delay, and stops
    // when that previous Friend confirms.
    assert(mesh_friend_enable(mesh_network.state.net_key_index, 10, 4, 21));
    clear_offer = &transport_friend_offers[0];
    memset(clear_offer, 0, sizeof(*clear_offer));
    clear_offer->used = clear_offer->offered = 1;
    clear_offer->net_key_index = mesh_network.state.net_key_index;
    clear_offer->lpn_address = 0x1202;
    clear_offer->lpn_counter = 32;
    clear_offer->friend_counter = 10;
    clear_offer->previous_friend = 0x1203;
    clear_offer->poll_timeout_ms = 5000;
    clear_offer->expires_at_ms = current_ms + clear_offer->poll_timeout_ms;
    assert(mesh_friendship_add(clear_offer->net_key_index, 0x1202,
        mesh_network.state.unicast_address, 32, 10));
    friend_poll_control.src = 0x1202;
    friend_poll_control.dst = mesh_network.state.unicast_address;
    friend_poll_control.net_key_index = clear_offer->net_key_index;
    friend_poll_control.params[0] = 0;
    int previous_clear_index = sent_count;
    mesh_friend_poll_receive(&friend_poll_control);
    assert(sent_count == previous_clear_index + 2);
    assert(transport_friend_offers[0].clear_pending);
    current_ms += 1000;
    assert(mesh_transport_poll(&received) == 0 &&
           sent_count == previous_clear_index + 3 &&
           clear_offer->clear_interval_ms == 2000);
    mesh_transport_control_message previous_clear_confirm = {
        .src = 0x1203, .dst = mesh_network.state.unicast_address,
        .net_key_index = clear_offer->net_key_index,
        .opcode = MESH_CONTROL_FRIEND_CLEAR_CONFIRM, .len = 2,
        .params = {0x12, 0x02}
    };
    previous_clear_confirm.params[0] = (uint8_t)(clear_offer->lpn_address >> 8);
    previous_clear_confirm.params[1] = (uint8_t)clear_offer->lpn_address;
    mesh_friend_clear_confirm_receive(&previous_clear_confirm);
    assert(!clear_offer->clear_pending && clear_offer->clear_done);

    // The previous Friend rejects an out-of-window LPNCounter delta, then
    // confirms and deletes the old friendship for a valid delta of 255.
    mesh_net_state previous_friend_state = node(0x1203);
    assert(mesh_network_init(&previous_friend_state) == 1);
    assert(mesh_friend_enable(mesh_network.state.net_key_index, 10, 4, 22));
    mesh_friend_offer *previous_offer = &transport_friend_offers[0];
    memset(previous_offer, 0, sizeof(*previous_offer));
    previous_offer->used = 1;
    previous_offer->net_key_index = mesh_network.state.net_key_index;
    previous_offer->lpn_address = 0x1202;
    previous_offer->lpn_counter = 31;
    previous_offer->friend_counter = 11;
    previous_offer->poll_timeout_ms = 5000;
    assert(mesh_friendship_add(previous_offer->net_key_index, 0x1202,
        mesh_network.state.unicast_address, 31, 11));
    mesh_transport_control_message replacement_clear = {
        .src = 0x1201, .dst = mesh_network.state.unicast_address,
        .net_key_index = previous_offer->net_key_index, .ttl = 0x20,
        .opcode = MESH_CONTROL_FRIEND_CLEAR, .len = 4,
        .params = {0x12, 0x02, 0x01, 0x2f}
    };
    int old_friend_confirm_index = sent_count;
    mesh_friend_clear_receive(&replacement_clear); // delta 0x012f is too large
    assert(previous_offer->used && sent_count == old_friend_confirm_index);
    replacement_clear.params[2] = 0x01;
    replacement_clear.params[3] = 0x1e; // delta 255
    mesh_friend_clear_receive(&replacement_clear);
    assert(previous_offer->used && previous_offer->clear_done &&
           previous_offer->clear_source == replacement_clear.src &&
           sent_count == old_friend_confirm_index + 1);
    mesh_friend_clear_receive(&replacement_clear);
    assert(sent_count == old_friend_confirm_index + 2);

    mesh_transport_control_message replacement_request = {
        .src = 0x1202, .dst = MESH_FRIENDS_ADDRESS,
        .net_key_index = mesh_network.state.net_key_index,
        .opcode = MESH_CONTROL_FRIEND_REQUEST, .len = 10,
        .params = {0x01, 10, 0, 0, 50, 0x12, 0x01, 1, 0, 33}
    };
    mesh_friend_request_receive(&replacement_request);
    assert(transport_friend_offers[0].used &&
           transport_friend_offers[0].previous_friend == 0x1201);
    mesh_friend_disable();
    return 0;
}
