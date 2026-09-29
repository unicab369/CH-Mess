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
    assert(ble_mesh_net_receive(sent[index] + 2, sent_len[index] - 2,
                                &network) == 1);
    return ble_mesh_transport_receive(&network, access);
}

int main(void) {
    mesh_net_state a = node(0x1201), b = node(0x1202);
    mesh_access_message received;
    const uint8_t short_access[] = {0x82, 0x01, 0x01};

    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 1);
    assert(sent_count == 1);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.len == sizeof(short_access));
    assert(memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(received.app_key_index == 0);
    assert(received.device_key_owner == 0);

    uint8_t long_access[40];
    for (int i = 0; i < 40; i++) long_access[i] = (uint8_t)i;
    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;

    assert(ble_mesh_network_init(&b) == 1);
    for (int i = 0; i < 3; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(3, &received) == 1);
    assert(received.len == sizeof(long_access));
    assert(memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(received.app_key_index == APP_KEY_INDEX_NONE);
    assert(received.device_key_owner == b.unicast_address);
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);

    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.active == 0);

    // A partial acknowledgment causes only the missing segment to be resent.
    sent_count = 0;
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;

    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(receive_frame(1, &received) == 0);
    assert(receive_frame(3, &received) == 0);
    current_ms += 200;
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);
    b = mesh_network.state;

    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.acked == 0x0b);
    assert(transport_segment_queue() == 1);
    assert(sent_count == 6);
    a = mesh_network.state;

    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(5, &received) == 1);
    assert(memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(sent_count == 7);

    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(6, &received) == 0);
    assert(transport_tx.active == 0);

    // A virtual destination needs the matching Label UUID as CCM AAD.
    uint8_t label[16] = {1, 2, 3, 4};
    uint8_t wrong[16] = {5, 6, 7, 8};
    uint16_t virtual_dst = ble_mesh_virtual_address(label);
    assert(virtual_dst >= 0x8000 && virtual_dst <= 0xbfff);
    sent_count = 0;
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 0);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                         short_access, sizeof(short_access), 0) == 1);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    ble_mesh_transport_clear_labels();
    assert(receive_frame(0, &received) == 0);
    assert(ble_mesh_network_init(&b) == 1);
    assert(ble_mesh_label_add(wrong) == 1);
    assert(receive_frame(0, &received) == 0);
    assert(ble_mesh_network_init(&b) == 1);
    assert(ble_mesh_label_add(label) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.dst == virtual_dst && received.has_label &&
           memcmp(received.label, label, 16) == 0 &&
           memcmp(received.data, short_access, sizeof(short_access)) == 0);

    // A matching 16-bit hash alone is insufficient: authenticate the UUID.
    ble_mesh_transport_clear_labels();
    assert(ble_mesh_label_add(wrong) == 1);
    transport_labels[0].address = virtual_dst;
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 0);

    // Segmented virtual messages use the same label and need no segment ACK.
    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                         long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    ble_mesh_transport_clear_labels();
    assert(ble_mesh_label_add(label) == 1);
    for (int i = 0; i < 3; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(3, &received) == 1);
    assert(received.has_label && received.len == sizeof(long_access) &&
           memcmp(received.data, long_access, sizeof(long_access)) == 0);
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(transport_tx.active == 0 && sent_count == 4);

    // A second AppKey is selected by index and identified after decryption.
    sent_count = 0;
    a.app_keys[1].used = b.app_keys[1].used = 1;
    a.app_keys[1].index = b.app_keys[1].index = 0x235;
    memset(a.app_keys[1].key, 0x99, 16);
    memset(b.app_keys[1].key, 0x99, 16);
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.app_key_index == 0x235);
    b.app_keys[1].used = 0;
    assert(ble_mesh_network_init(&b) == 1);
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
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.app_key_index == 0x235);

    // An 8-byte TransMIC uses segmented transport even for a short message.
    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, b.unicast_address, 5, 0, NULL,
                                    short_access, sizeof(short_access), 1) == 1);
    assert(sent_count == 1 && transport_tx.active &&
           transport_tx.seg_n == 0);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    mesh_net_message net;
    assert(ble_mesh_net_receive(sent[0] + 2, sent_len[0] - 2, &net) == 1);
    assert((net.transport[0] & 0x80) && (net.transport[1] & 0x80));
    assert(ble_mesh_transport_receive(&net, &received) == 1);
    assert(received.len == sizeof(short_access) &&
           memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(sent_count == 2);
    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(1, &received) == 0 && !transport_tx.active);

    // 376 bytes plus an 8-byte TransMIC fills all 32 segments.
    uint8_t max_access[376];
    for (size_t i = 0; i < sizeof(max_access); i++) max_access[i] = (uint8_t)i;
    sent_count = 0;
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                    max_access, sizeof(max_access) + 1, 1) == 0);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, virtual_dst, 5, 0, label,
                                    max_access, sizeof(max_access), 1) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 32 && transport_tx.seg_n == 31);
    assert(ble_mesh_network_init(&b) == 1);
    ble_mesh_transport_clear_labels();
    assert(ble_mesh_label_add(label) == 1);
    for (int i = 0; i < 31; i++) assert(receive_frame(i, &received) == 0);
    assert(receive_frame(31, &received) == 1);
    assert(received.len == sizeof(max_access) &&
           memcmp(received.data, max_access, sizeof(max_access)) == 0);

    // An AppKey message can target the second element, and it can reply from it.
    mesh_net_state c = node(0x1300);
    a.element_count = 2;
    sent_count = 0;
    transport_tx.active = 0;
    assert(ble_mesh_network_init(&c) == 1);
    assert(ble_mesh_transport_queue(mesh_network.state.unicast_address, 0x1202, 5, 0, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(0, &received) == 1 && received.dst == 0x1202);
    sent_count = 0;
    assert(ble_mesh_transport_queue(0x1202, 0x1300, 5, 0, NULL,
                                         short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&c) == 1);
    assert(receive_frame(0, &received) == 1 && received.src == 0x1202);

    // Requests use the target's key; server replies use the server's own key.
    a = node(0x1201);
    b = node(0x1202);
    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1 &&
           received.device_key_owner == received.dst);
    sent_count = 0;
    assert(ble_mesh_transport_queue(0x1202, 0x1201, 5, DEVICE_KEY_LOCAL, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(0, &received) == 1 &&
           received.device_key_owner == received.src);
    // AppKey traffic uses its owning subnet; Device Key traffic can select one.
    a = node_with_secondary_subnet(0x1201);
    b = node_with_secondary_subnet(0x1202);
    sent_count = 0;
    transport_tx.active = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(0x1201, 0x1202, 5, 0x031, NULL,
                                    short_access, sizeof(short_access), 0) == 1);
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.net_key_index == 0x222 && received.app_key_index == 0x031);

    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    mesh_network.reply_net_idx = 0x222;
    assert(ble_mesh_transport_queue(0x1201, 0x1202, 5, APP_KEY_INDEX_NONE,
        NULL, short_access, sizeof(short_access), 0) == 1);
    mesh_network.reply_net_idx = mesh_network.state.net_key_index;
    assert(ble_mesh_network_init(&b) == 1);
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
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(0x1201, 0x1202, 5, 0, NULL,
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
    assert(ble_mesh_network_init(&c) == 1);
    assert(ble_mesh_transport_queue(0x1300, 0x1202, 5, 0, NULL,
        long_access, sizeof(long_access), 0) == 1);
    while (transport_tx.next_seg <= transport_tx.seg_n)
        assert(transport_segment_queue() == 1);
    assert(sent_count == 4);
    for (int i = 0; i < 4; i++) {
        memcpy(stream_c[i], sent[i], sent_len[i]);
        stream_c_len[i] = sent_len[i];
    }
    assert(ble_mesh_network_init(&b) == 1);
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
    return 0;
}
