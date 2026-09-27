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
    assert(ble_mesh_transport_queue(b.unicast_address, 5, 0, NULL,
                                         short_access, sizeof(short_access)) == 1);
    assert(sent_count == 1);
    a = mesh_network.state;
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.len == sizeof(short_access));
    assert(memcmp(received.data, short_access, sizeof(short_access)) == 0);
    assert(received.app_key_index == 0);

    uint8_t long_access[40];
    for (int i = 0; i < 40; i++) long_access[i] = (uint8_t)i;
    sent_count = 0;
    assert(ble_mesh_network_init(&a) == 1);
    assert(ble_mesh_transport_queue(b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access)) == 1);
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
    assert(ble_mesh_transport_poll(&received) == 0);
    assert(sent_count == 5);

    assert(ble_mesh_network_init(&a) == 1);
    assert(receive_frame(4, &received) == 0);
    assert(transport_tx.active == 0);

    // A partial acknowledgment causes only the missing segment to be resent.
    sent_count = 0;
    assert(ble_mesh_transport_queue(b.unicast_address, 5, APP_KEY_INDEX_NONE, NULL,
                                         long_access, sizeof(long_access)) == 1);
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
    assert(ble_mesh_transport_queue(virtual_dst, 5, 0, NULL,
                                         short_access, sizeof(short_access)) == 0);
    assert(ble_mesh_transport_queue(virtual_dst, 5, 0, label,
                                         short_access, sizeof(short_access)) == 1);
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
    assert(ble_mesh_transport_queue(virtual_dst, 5, 0, label,
                                         long_access, sizeof(long_access)) == 1);
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
    assert(ble_mesh_transport_queue(b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access)) == 1);
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
    assert(ble_mesh_transport_queue(b.unicast_address, 5, 0x235, NULL,
                                    short_access, sizeof(short_access)) == 1);
    assert(ble_mesh_network_init(&b) == 1);
    assert(receive_frame(0, &received) == 1);
    assert(received.app_key_index == 0x235);
    return 0;
}
