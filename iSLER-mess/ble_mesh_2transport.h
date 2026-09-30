// akf = Application Key Flag
// aid = Application Key Identifier

#ifndef ISLER_BLE_MESH_TRANSPORT_H
#define ISLER_BLE_MESH_TRANSPORT_H

#include "ble_mesh_1network.h"

#define MESH_TRANSPORT_MAX_ACCESS 380
#define MESH_TRANSPORT_MAX_UPPER 384
#define MESH_TRANSPORT_SEGMENT_SIZE 12
#define MESH_TRANSPORT_CONTROL_SEGMENT_SIZE 8
#define MESH_TRANSPORT_SEGMENT_INTERVAL_STEP_DEFAULT 5
// 32 Segmented Control packets can carry 32 * 8 parameter bytes.
#define MESH_TRANSPORT_MAX_CONTROL 256
#define APP_KEY_INDEX_NONE 0xffff
// Outgoing Configuration Server replies use this node's Device Key.
#define DEVICE_KEY_LOCAL 0xfffe
#define MESH_TRANSPORT_MAX_LABELS 4
#define MESH_TRANSPORT_SEGMENTED_TX_QUEUE_MAX 4
#ifndef MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE
// Number of complete segmented Access messages that may wait behind active SAR.
#define MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE 1
#endif
#if MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE < 1 || \
    MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE > MESH_TRANSPORT_SEGMENTED_TX_QUEUE_MAX
#error MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE must be between 1 and MESH_TRANSPORT_SEGMENTED_TX_QUEUE_MAX
#endif
#ifndef MESH_TRANSPORT_RX_PACKET_SLOTS
// One slot stores a segment; the default 32 slots hold one maximum-size message.
#define MESH_TRANSPORT_RX_PACKET_SLOTS 32
#endif
#if MESH_TRANSPORT_RX_PACKET_SLOTS < 1
#error MESH_TRANSPORT_RX_PACKET_SLOTS must be at least 1
#endif

// TODO for broader Transport support:
// - Increase RX SAR capacity if more than one maximum-size message must be
//   reassembled concurrently; the default pool holds 32 segments total.
// - Verify SAR behavior against an independent Mesh implementation.
// - Implement the Friend Queue, recurring LPN polling, subscription updates,
//   Friend Update IV-state handling, RSSI-aware offer timing, and friendship
//   termination.
// - Skipped for now: support concurrent segmented TX contexts for different
//   destinations. One active context serializes segmented sends; the Mesh
//   Protocol only prohibits overlapping segmented sends to the same destination.

// Return 1 when a Device Key is known for this unicast address, or 0 otherwise.
int BLE_MESH_TRANSPORT_GET_DEVICE_KEY(uint16_t address, uint8_t key[16]);
uint32_t GET_MILLIS(void);

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t app_key_index; // APP_KEY_INDEX_NONE means the Device Key was used
    uint16_t device_key_owner; // 0 for AppKey; owner of the authenticating Device Key
    uint16_t net_key_index;
    uint16_t len;
    uint8_t ttl;
    uint8_t has_label;
    uint8_t label[16];
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_message;

typedef struct {
    uint16_t src, dst, net_key_index, len;
    uint8_t ttl, opcode, friendship;
    uint8_t params[MESH_TRANSPORT_MAX_CONTROL];
} mesh_transport_control_message;

#define MESH_CONTROL_FRIEND_POLL 0x01
#define MESH_CONTROL_FRIEND_UPDATE 0x02
#define MESH_CONTROL_FRIEND_REQUEST 0x03
#define MESH_CONTROL_FRIEND_OFFER 0x04
#define MESH_FRIENDS_ADDRESS 0xfffd
// Current offer capability; Friend Queue storage remains on the TODO above.
#define MESH_FRIEND_QUEUE_CAPACITY 2

enum {
    MESH_LPN_IDLE,
    MESH_LPN_REQUESTING,
    MESH_LPN_WAITING_FOR_UPDATE,
    MESH_LPN_ESTABLISHED
};

static struct {
    uint16_t net_key_index, lpn_counter, next_lpn_counter, previous_friend;
    uint16_t friend_counter, friend_address;
    uint32_t last_tx_ms;
    uint32_t poll_timeout_ms;
    uint8_t criteria, receive_delay, num_elements, state, fsn, poll_attempts;
} transport_lpn;

static struct {
    uint16_t net_key_index, next_counter;
    uint8_t enabled, receive_window, subscription_size;
} transport_friend;

static struct {
    uint16_t net_key_index, lpn_address, lpn_counter, friend_counter;
    uint32_t poll_timeout_ms, offer_at_ms, expires_at_ms;
    uint8_t used, offered;
} transport_friend_offers[MESH_NETWORK_MAX_FRIENDSHIPS];

typedef void (*mesh_transport_control_handler)(
    const mesh_transport_control_message *message);
static mesh_transport_control_handler transport_control_handler;

// Runtime copy of the persisted SAR Transmitter state used by transport.
static mesh_sar_tx_state transport_sar_tx = {
    5, 2, 2, 7, 1, 2, 9
};
static mesh_sar_rx_state transport_sar_rx = {3, 1, 1, 5, 0};

static inline mesh_sar_tx_state mesh_transport_get_sar_transmitter(void) {
    return transport_sar_tx;
}

// Register a handler for unsegmented and reassembled segmented Control PDUs.
static inline void mesh_transport_set_control_handler(
    mesh_transport_control_handler handler) {
    transport_control_handler = handler;
}

// Queue the next Friend Request using the caller-maintained LPN counter.
static inline int mesh_lpn_send_request(void) {
    uint16_t primary = mesh_network.state.unicast_address;
    uint16_t request_counter = transport_lpn.next_lpn_counter;
    uint8_t request[11] = {
        MESH_CONTROL_FRIEND_REQUEST, transport_lpn.criteria,
        transport_lpn.receive_delay,
        (uint8_t)(transport_lpn.poll_timeout_ms / 100u >> 16),
        (uint8_t)(transport_lpn.poll_timeout_ms / 100u >> 8),
        (uint8_t)(transport_lpn.poll_timeout_ms / 100u),
        (uint8_t)(transport_lpn.previous_friend >> 8),
        (uint8_t)transport_lpn.previous_friend,
        transport_lpn.num_elements,
        (uint8_t)(request_counter >> 8), (uint8_t)request_counter
    };
    if (!mesh_net_queue(transport_lpn.net_key_index, primary,
            MESH_FRIENDS_ADDRESS, 1, 0, request, sizeof(request))) return 0;
    transport_lpn.lpn_counter = request_counter;
    transport_lpn.next_lpn_counter++;
    transport_lpn.last_tx_ms = GET_MILLIS();
    transport_lpn.state = MESH_LPN_REQUESTING;
    return 1;
}

// Begin LPN friendship discovery. poll_timeout_ms must be 1,000–345,599,900 ms;
// next_lpn_counter is caller-owned persistent state initialized to zero.
static inline int mesh_lpn_start(uint16_t net_key_index, uint8_t criteria,
        uint8_t receive_delay, uint32_t poll_timeout_ms,
        uint16_t previous_friend, uint16_t next_lpn_counter) {
    uint8_t elements = mesh_network.state.element_count;
    if (!mesh_network.ready || transport_lpn.state != MESH_LPN_IDLE ||
        !elements || elements > MESH_MAX_ELEMENTS ||
        (criteria & 0x80) || !(criteria & 0x07) || receive_delay < 10 ||
        poll_timeout_ms < 1000 || poll_timeout_ms > 0x34bbffu * 100u ||
        poll_timeout_ms % 100u ||
        previous_friend > 0x7fff ||
        mesh_subnet_slot(&mesh_network.state, net_key_index) < 0) return 0;

    memset(&transport_lpn, 0, sizeof(transport_lpn));
    transport_lpn.net_key_index = net_key_index;
    transport_lpn.criteria = criteria;
    transport_lpn.receive_delay = receive_delay;
    transport_lpn.num_elements = elements;
    transport_lpn.poll_timeout_ms = poll_timeout_ms;
    transport_lpn.next_lpn_counter = next_lpn_counter;
    transport_lpn.previous_friend = previous_friend;
    if (mesh_lpn_send_request()) return 1;
    memset(&transport_lpn, 0, sizeof(transport_lpn));
    return 0;
}

static inline uint16_t mesh_lpn_friend_address(void) {
    return transport_lpn.state == MESH_LPN_ESTABLISHED ?
        transport_lpn.friend_address : 0;
}

// Read the next LPNCounter so the application can persist it between boots.
static inline uint16_t mesh_lpn_next_counter(void) {
    return transport_lpn.next_lpn_counter;
}

// Enable Friend responses on one subnet. The caller persists the counter.
static inline int mesh_friend_enable(uint16_t net_key_index,
        uint8_t receive_window, uint8_t subscription_size,
        uint16_t next_friend_counter) {
    if (!mesh_network.ready || !receive_window ||
        mesh_subnet_slot(&mesh_network.state, net_key_index) < 0) return 0;
    transport_friend.net_key_index = net_key_index;
    transport_friend.receive_window = receive_window;
    transport_friend.subscription_size = subscription_size;
    transport_friend.next_counter = next_friend_counter;
    transport_friend.enabled = 1;
    memset(transport_friend_offers, 0, sizeof(transport_friend_offers));
    return 1;
}

static inline void mesh_friend_disable(void) {
    transport_friend.enabled = 0;
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        if (!transport_friend_offers[i].used) continue;
        mesh_friendship_clear(transport_friend_offers[i].net_key_index,
            transport_friend_offers[i].lpn_address,
            mesh_network.state.unicast_address);
    }
    memset(transport_friend_offers, 0, sizeof(transport_friend_offers));
}

static inline uint16_t mesh_friend_next_counter(void) {
    return transport_friend.next_counter;
}

// Cache a qualifying request until its delayed Friend Offer can be sent.
static inline void mesh_friend_request_receive(
        const mesh_transport_control_message *message) {
    if (!transport_friend.enabled || !message ||
        message->opcode != MESH_CONTROL_FRIEND_REQUEST || message->friendship ||
        message->net_key_index != transport_friend.net_key_index ||
        message->dst != MESH_FRIENDS_ADDRESS || message->ttl != 0 ||
        message->len != 10 || !message->src || message->src > 0x7fff ||
        (message->params[0] & 0x80) || !(message->params[0] & 0x07) ||
        message->params[1] < 10) return;
    uint32_t poll_timeout = ((uint32_t)message->params[2] << 16) |
        ((uint32_t)message->params[3] << 8) | message->params[4];
    uint16_t previous_friend = (uint16_t)((message->params[5] << 8) |
                                           message->params[6]);
    uint8_t elements = message->params[7];
    if (poll_timeout < 10 || poll_timeout > 0x34bbff || !elements ||
        previous_friend > 0x7fff ||
        (uint32_t)message->src + elements - 1 > 0x7fff ||
        MESH_FRIEND_QUEUE_CAPACITY < (1u << (message->params[0] & 0x07))) return;

    uint16_t lpn_counter = (uint16_t)((message->params[8] << 8) |
                                       message->params[9]);
    uint8_t slot = 0;
    while (slot < MESH_NETWORK_MAX_FRIENDSHIPS &&
        (!transport_friend_offers[slot].used ||
         transport_friend_offers[slot].lpn_address != message->src ||
         transport_friend_offers[slot].net_key_index != message->net_key_index))
        slot++;
    if (slot == MESH_NETWORK_MAX_FRIENDSHIPS) {
        for (slot = 0; slot < MESH_NETWORK_MAX_FRIENDSHIPS; slot++)
            if (!transport_friend_offers[slot].used) break;
        if (slot == MESH_NETWORK_MAX_FRIENDSHIPS) return;
    } else if (transport_friend_offers[slot].lpn_counter == lpn_counter) {
        return;
    }

    uint16_t friend_counter = transport_friend.next_counter++;
    if (!mesh_friendship_add(message->net_key_index, message->src,
            mesh_network.state.unicast_address, lpn_counter, friend_counter)) return;
    transport_friend_offers[slot].used = 1;
    transport_friend_offers[slot].net_key_index = message->net_key_index;
    transport_friend_offers[slot].lpn_address = message->src;
    transport_friend_offers[slot].lpn_counter = lpn_counter;
    transport_friend_offers[slot].friend_counter = friend_counter;
    transport_friend_offers[slot].poll_timeout_ms = poll_timeout * 100u;
    transport_friend_offers[slot].offer_at_ms = GET_MILLIS() + 100u;
    transport_friend_offers[slot].expires_at_ms = GET_MILLIS() + poll_timeout * 100u;
}

// Reply to a friendship-key Friend Poll with the current Friend Update.
static inline void mesh_friend_poll_receive(
        const mesh_transport_control_message *message) {
    if (!transport_friend.enabled || !message ||
        message->opcode != MESH_CONTROL_FRIEND_POLL || !message->friendship ||
        message->net_key_index != transport_friend.net_key_index ||
        message->dst != mesh_network.state.unicast_address || message->ttl != 0 ||
        message->len != 1 ||
        (message->params[0] & 0xfe)) return;
    uint8_t slot = 0;
    while (slot < MESH_NETWORK_MAX_FRIENDSHIPS &&
        (!transport_friend_offers[slot].used ||
         transport_friend_offers[slot].net_key_index != message->net_key_index ||
         transport_friend_offers[slot].lpn_address != message->src ||
         !transport_friend_offers[slot].offered)) slot++;
    if (slot == MESH_NETWORK_MAX_FRIENDSHIPS) return;
    if ((int32_t)(GET_MILLIS() -
            transport_friend_offers[slot].expires_at_ms) >= 0) {
        mesh_friendship_clear(message->net_key_index, message->src,
            mesh_network.state.unicast_address);
        memset(&transport_friend_offers[slot], 0,
               sizeof(transport_friend_offers[slot]));
        return;
    }

    uint8_t phase = mesh_subnet_phase(&mesh_network.state, message->net_key_index);
    uint8_t update[7] = {
        MESH_CONTROL_FRIEND_UPDATE,
        (uint8_t)((phase == 2 ? 1u : 0u) |
                  (mesh_network.state.iv_update ? 2u : 0u)),
        (uint8_t)(mesh_network.state.iv_index >> 24),
        (uint8_t)(mesh_network.state.iv_index >> 16),
        (uint8_t)(mesh_network.state.iv_index >> 8),
        (uint8_t)mesh_network.state.iv_index,
        0
    };
    if (mesh_net_queue_friend(message->net_key_index,
        mesh_network.state.unicast_address, message->src, 1, 0,
        update, sizeof(update))) {
        transport_friend_offers[slot].offered = 1;
        transport_friend_offers[slot].expires_at_ms = GET_MILLIS() +
            transport_friend_offers[slot].poll_timeout_ms;
    }
}

// Complete the LPN side of Friend Offer -> friendship-key Friend Poll -> Update.
static inline void mesh_lpn_control_receive(
        const mesh_transport_control_message *message) {
    if (!message || message->net_key_index != transport_lpn.net_key_index ||
        !mesh_network.ready) return;

    if (transport_lpn.state == MESH_LPN_REQUESTING &&
        message->opcode == MESH_CONTROL_FRIEND_OFFER && !message->friendship &&
        message->len == 6 && message->dst == mesh_network.state.unicast_address &&
        message->src && message->src <= 0x7fff && message->params[0] &&
        message->params[1] >= (1u << (transport_lpn.criteria & 0x07))) {
        uint16_t friend_counter = (uint16_t)((message->params[4] << 8) |
                                              message->params[5]);
        if (!mesh_friendship_add(message->net_key_index,
                mesh_network.state.unicast_address, message->src,
                transport_lpn.lpn_counter, friend_counter)) return;

        uint8_t poll[2] = {MESH_CONTROL_FRIEND_POLL, 0};
        if (!mesh_net_queue_friend(message->net_key_index,
                mesh_network.state.unicast_address, message->src, 1, 0,
                poll, sizeof(poll))) {
            mesh_friendship_clear(message->net_key_index,
                mesh_network.state.unicast_address, message->src);
            return;
        }
        transport_lpn.friend_address = message->src;
        transport_lpn.friend_counter = friend_counter;
        transport_lpn.fsn = 0;
        transport_lpn.poll_attempts = 1;
        transport_lpn.last_tx_ms = GET_MILLIS();
        transport_lpn.state = MESH_LPN_WAITING_FOR_UPDATE;
        return;
    }

    if (transport_lpn.state == MESH_LPN_WAITING_FOR_UPDATE &&
        message->opcode == MESH_CONTROL_FRIEND_UPDATE && message->friendship &&
        message->len == 6 && message->src == transport_lpn.friend_address &&
        message->dst == mesh_network.state.unicast_address &&
        !(message->params[0] & 0xfc) && message->params[5] <= 1) {
        uint8_t phase = mesh_subnet_phase(&mesh_network.state,
                                          transport_lpn.net_key_index);
        if (message->params[0] & 1u) {
            if (phase == 1 && !mesh_key_refresh_transition(
                    transport_lpn.net_key_index, 2)) return;
            if (phase != 1 && phase != 2) return;
        } else if (phase == 2 && !mesh_key_refresh_transition(
                       transport_lpn.net_key_index, 3)) return;
        transport_lpn.state = MESH_LPN_ESTABLISHED;
    }
}

static struct {
    uint8_t label[16];
    uint16_t address;
} transport_labels[MESH_TRANSPORT_MAX_LABELS];
static uint8_t label_count;

// Bluetooth Mesh virtual address = 0x8000 | low 14 bits of
// AES-CMAC(s1("vtad"), Label UUID).
static inline uint16_t mesh_virtual_address(const uint8_t label[16]) {
    if (!label) return 0;
    const uint8_t zero[16] = {0};
    uint8_t salt[16], hash[16];
    aes_cmac(zero, (const uint8_t *)"vtad", 4, salt);
    aes_cmac(salt, label, 16, hash);
    return (uint16_t)(0x8000 | ((hash[14] & 0x3f) << 8) | hash[15]);
}

// Register receive labels. Colliding virtual addresses remain distinct.
static inline int mesh_label_add(const uint8_t label[16]) {
    if (!label) return 0;
    for (uint8_t i = 0; i < label_count; i++) {
        if (memcmp(transport_labels[i].label, label, 16) == 0) return 1;
    }

    if (label_count == MESH_TRANSPORT_MAX_LABELS) return 0;
    uint8_t i = label_count++;
    memcpy(transport_labels[i].label, label, 16);
    transport_labels[i].address = mesh_virtual_address(label);
    return 1;
}

static inline void mesh_transport_clear_labels(void) {
    label_count = 0;
}



static struct {
    uint8_t active;
    uint8_t akf, aid, ttl, seg_n, next_seg, retries, retries_without_progress, mic_64;
    uint16_t src, dst, seq_zero, upper_len, net_idx;
    uint32_t seq_auth, iv_index, acked, last_tx_ms;
    uint8_t upper[MESH_TRANSPORT_MAX_UPPER];
} transport_tx;

struct transport_tx_pending {
    uint16_t src, dst, net_idx, access_len;
    uint8_t akf, aid, ttl, mic_64, has_label;
    uint8_t key[16], label[16];
    uint8_t access[MESH_TRANSPORT_MAX_ACCESS];
};

static struct transport_tx_pending
    segmented_tx_queue[MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE];
static uint8_t segmented_tx_queue_head, segmented_tx_queue_count;

struct transport_rx {
    uint8_t active, ack_pending, ctl, delivered, ttl, transport_len;
    uint8_t ack_retrans_left, ack_sent;
    uint16_t src, dst, net_idx;
    uint32_t seq_auth, seq, iv_index, updated_ms, ack_at_ms, ack_sent_ms;
    uint8_t transport[16];
};

static struct transport_rx transport_rx[MESH_TRANSPORT_RX_PACKET_SLOTS];

static uint8_t transport_app_aid(const uint8_t app_key[16]) {
    const uint8_t zero[16] = {0};
    uint8_t salt[16], t[16], result[16];
    aes_cmac(zero, (const uint8_t *)"smk4", 4, salt);
    aes_cmac(salt, app_key, 16, t);
    aes_cmac(t, (const uint8_t *)"id6\x01", 4, result);
    return result[15] & 0x3f;
}

static void transport_nonce(uint8_t nonce[13], uint8_t device_key,
                            uint8_t mic_64, uint32_t seq, uint16_t src,
                            uint16_t dst, uint32_t iv_index) {
    nonce[0] = device_key ? 2 : 1;
    nonce[1] = mic_64 ? 0x80 : 0;
    nonce[2] = (uint8_t)(seq >> 16);
    nonce[3] = (uint8_t)(seq >> 8);
    nonce[4] = (uint8_t)seq;
    nonce[5] = (uint8_t)(src >> 8);
    nonce[6] = (uint8_t)src;
    nonce[7] = (uint8_t)(dst >> 8);
    nonce[8] = (uint8_t)dst;
    nonce[9] = (uint8_t)(iv_index >> 24);
    nonce[10] = (uint8_t)(iv_index >> 16);
    nonce[11] = (uint8_t)(iv_index >> 8);
    nonce[12] = (uint8_t)iv_index;
}

static int transport_decrypt(uint8_t akf, uint8_t aid, uint8_t mic_64,
                             uint32_t seq, uint32_t iv_index, uint16_t net_idx,
                             uint16_t src,
                             uint16_t dst, const uint8_t *upper, size_t len,
                             mesh_access_message *out) {
    size_t mic_len = mic_64 ? 8 : 4;
    if (len <= mic_len || len - mic_len > MESH_TRANSPORT_MAX_ACCESS) return 0;

    uint8_t nonce[13], key[16];
    transport_nonce(nonce, !akf, mic_64, seq, src, dst, iv_index);
    out->app_key_index = APP_KEY_INDEX_NONE;
    out->device_key_owner = 0;
    out->has_label = 0;

    if (akf) {
        const mesh_net_state *state = &mesh_network.state;

        for (uint8_t slot = 0; slot < MESH_MAX_APP_KEYS; slot++) {
            const mesh_app_key *app = &state->app_keys[slot];
            if (!app->used || mesh_app_net_idx(state, app) != net_idx) continue;
            for (uint8_t version = 0; version < 2; version++) {
                if (version && !app->has_new_key) break;
                const uint8_t *key = version ? app->new_key : app->key;
                if (transport_app_aid(key) != aid) continue;

                // 0x8000-0xBFFF is the Bluetooth Mesh virtual address range.
                if (dst >= 0x8000 && dst < 0xc000) {
                    for (uint8_t j = 0; j < label_count; j++) {
                        if (transport_labels[j].address != dst) continue;
                        if (ccm_auth_decrypt(key, nonce, 13,
                                             transport_labels[j].label, 16,
                                             upper, len - mic_len,
                                             upper + len - mic_len, mic_len,
                                             out->data) == CCM_OK) {
                            out->app_key_index = app->index;
                            out->has_label = 1;
                            memcpy(out->label, transport_labels[j].label, 16);
                            break;
                        }
                    }
                } else if (ccm_auth_decrypt(key, nonce, 13, NULL, 0,
                                            upper, len - mic_len,
                                            upper + len - mic_len, mic_len,
                                            out->data) == CCM_OK) {
                    out->app_key_index = app->index;
                }
                if (out->app_key_index != APP_KEY_INDEX_NONE) break;
            }
            if (out->app_key_index != APP_KEY_INDEX_NONE) break;
        }
        if (out->app_key_index == APP_KEY_INDEX_NONE) return 0;
    } else {
        if (aid != 0 || !mesh_local_element(dst)) return 0;
        int ok = 0;
        if (BLE_MESH_TRANSPORT_GET_DEVICE_KEY(dst, key) == 1 &&
            ccm_auth_decrypt(key, nonce, 13, NULL, 0, upper, len - mic_len,
                             upper + len - mic_len, mic_len, out->data) == CCM_OK) {
            ok = 1;
            out->device_key_owner = dst;
        }
        if (!ok && BLE_MESH_TRANSPORT_GET_DEVICE_KEY(src, key) == 1 &&
            ccm_auth_decrypt(key, nonce, 13, NULL, 0, upper, len - mic_len,
                             upper + len - mic_len, mic_len, out->data) == CCM_OK) {
            ok = 1;
            out->device_key_owner = src;
        }
        if (!ok) return 0;
    }

    out->src = src;
    out->dst = dst;
    out->net_key_index = net_idx;
    out->ttl = 0;
    out->len = (uint16_t)(len - mic_len);
    return 1;
}

static int transport_segment_queue(void) {
    if (!transport_tx.active) return 0;

    while (transport_tx.next_seg <= transport_tx.seg_n &&
           (transport_tx.acked & ((uint32_t)1 << transport_tx.next_seg))
    ) { transport_tx.next_seg++; }

    if (transport_tx.next_seg > transport_tx.seg_n) return 1;

    uint32_t iv = mesh_network.state.iv_index - (mesh_network.state.iv_update ? 1 : 0);
    if (iv != transport_tx.iv_index ||
        !mesh_local_element(transport_tx.src) ||
        mesh_network.state.next_seq < transport_tx.seq_auth ||
        mesh_network.state.next_seq > 0xffffff ||
        mesh_network.state.next_seq - transport_tx.seq_auth >= 8192
    ) {
        transport_tx.active = 0;
        return -1;
    }

    uint8_t seg_o = transport_tx.next_seg;
    size_t offset = (size_t)seg_o * MESH_TRANSPORT_SEGMENT_SIZE;
    size_t count = transport_tx.upper_len - offset;
    uint8_t lower[16];

    lower[0] = 0x80 | (transport_tx.akf << 6) | transport_tx.aid;
    lower[1] = (uint8_t)((transport_tx.mic_64 << 7) |
                         (transport_tx.seq_zero >> 6));
    lower[2] = (uint8_t)(((transport_tx.seq_zero & 0x3f) << 2) | (seg_o >> 3));
    lower[3] = (uint8_t)((seg_o << 5) | transport_tx.seg_n);

    if (count > MESH_TRANSPORT_SEGMENT_SIZE) count = MESH_TRANSPORT_SEGMENT_SIZE;
    memcpy(lower + 4, transport_tx.upper + offset, count);

    if (!mesh_net_queue(transport_tx.net_idx, transport_tx.src, transport_tx.dst,
                           0, transport_tx.ttl,
                           lower, count + 4)) return 0;
    transport_tx.next_seg++;
    transport_tx.last_tx_ms = GET_MILLIS();
    return 1;
}

// Start a queued Access message only when it owns the active SAR context.
static int transport_tx_start(const struct transport_tx_pending *pending) {
    if (!pending || transport_tx.active) return 0;
    const mesh_net_state *state = &mesh_network.state;
    size_t mic_len = pending->mic_64 ? 8u : 4u;
    size_t upper_len = pending->access_len + mic_len;
    uint8_t upper[MESH_TRANSPORT_MAX_UPPER], nonce[13];
    uint32_t seq = state->next_seq;
    uint32_t iv = state->iv_index - (state->iv_update ? 1u : 0u);
    uint8_t *label = pending->has_label ? (uint8_t *)pending->label : NULL;

    if (seq > 0xffffff || seq + (upper_len - 1) / MESH_TRANSPORT_SEGMENT_SIZE >
            0xffffff || !mesh_local_element(pending->src)) return -1;
    transport_nonce(nonce, !pending->akf, pending->mic_64, seq,
                    pending->src, pending->dst, iv);
    if (ccm_encrypt_and_tag(pending->key, nonce, 13, label,
            pending->has_label ? 16u : 0u, pending->access,
            pending->access_len, upper, upper + pending->access_len,
            mic_len) != CCM_OK) return -1;

    memset(&transport_tx, 0, sizeof(transport_tx));
    transport_tx.active = 1;
    transport_tx.akf = pending->akf;
    transport_tx.aid = pending->aid;
    transport_tx.mic_64 = pending->mic_64;
    transport_tx.ttl = pending->ttl;
    transport_tx.src = pending->src;
    transport_tx.net_idx = pending->net_idx;
    transport_tx.dst = pending->dst;
    transport_tx.seq_zero = seq & 0x1fff;
    transport_tx.seq_auth = seq;
    transport_tx.iv_index = iv;
    transport_tx.upper_len = (uint16_t)upper_len;
    transport_tx.seg_n = (uint8_t)((upper_len - 1) / MESH_TRANSPORT_SEGMENT_SIZE);
    memcpy(transport_tx.upper, upper, upper_len);
    return transport_segment_queue();
}

static int transport_rx_matches(const struct transport_rx *rx, uint8_t ctl,
                                uint16_t net_idx, uint16_t src, uint16_t dst,
                                uint32_t seq_auth, uint32_t iv_index) {
    return rx->active && rx->ctl == ctl && rx->net_idx == net_idx && rx->src == src &&
        rx->dst == dst && rx->seq_auth == seq_auth &&
        rx->iv_index == iv_index;
}

static uint32_t transport_rx_received(uint8_t ctl, uint16_t net_idx, uint16_t src,
                                      uint16_t dst, uint32_t seq_auth,
                                      uint32_t iv_index) {
    uint32_t received = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
        const struct transport_rx *rx = &transport_rx[i];
        if (!transport_rx_matches(rx, ctl, net_idx, src, dst, seq_auth, iv_index))
            continue;
        uint8_t seg_o = (uint8_t)(((rx->transport[2] & 3) << 3) |
                                  (rx->transport[3] >> 5));
        received |= (uint32_t)1 << seg_o;
    }
    return received;
}

static void transport_rx_ack(uint8_t ctl, uint16_t net_idx, uint16_t src, uint16_t dst,
                             uint32_t seq_auth, uint32_t iv_index,
                             uint8_t pending, uint32_t ack_at_ms,
                             uint32_t updated_ms, uint8_t retrans_left) {
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
        struct transport_rx *rx = &transport_rx[i];
        if (!transport_rx_matches(rx, ctl, net_idx, src, dst, seq_auth, iv_index))
            continue;
        rx->ack_pending = pending;
        rx->ack_at_ms = ack_at_ms;
        rx->updated_ms = updated_ms;
        rx->ack_retrans_left = retrans_left;
    }
}

// For an incomplete message, wait before ACKing the segments received so far;
// this gives more segments time to arrive and avoids frequent partial ACKs.
// The final missing segment is ACKed immediately.
static uint32_t transport_sar_rx_ack_delay_ms(uint8_t seg_n) {
    uint32_t by_length_half_steps = (uint32_t)seg_n * 2 + 1;
    uint32_t by_state_half_steps =
        (uint32_t)transport_sar_rx.ack_delay_increment * 2 + 3;
    uint32_t half_steps = by_length_half_steps < by_state_half_steps ?
        by_length_half_steps : by_state_half_steps;
    uint32_t interval_ms =
        ((uint32_t)transport_sar_rx.segment_interval_step + 1) * 10;
    return half_steps * interval_ms / 2;
}

// Queue an encrypted Access message. Returns 1 if accepted, 0 on failure.
// APP_KEY_INDEX_NONE uses the destination's Device Key; DEVICE_KEY_LOCAL uses ours.
// Set mic_64 to 1 for an 8-byte TransMIC and segmented transport.
static inline int mesh_transport_queue(uint16_t src,
                                                uint16_t dst, uint8_t ttl,
                                                uint16_t app_key_index,
                                                const uint8_t label[16],
                                                const uint8_t *access, size_t len,
                                                uint8_t mic_64) {
    // A destination in the virtual address range requires its Label UUID.
    if (!mesh_network.ready || !mesh_local_element(src) ||
        !access || len == 0 || mic_64 > 1 ||
        len > MESH_TRANSPORT_MAX_UPPER - (mic_64 ? 8u : 4u) ||
        ttl > 0x7f || dst == 0 ||
        ((dst >= 0x8000 && dst < 0xc000) != (label != NULL)) ||
        (label && (app_key_index == APP_KEY_INDEX_NONE ||
                   app_key_index == DEVICE_KEY_LOCAL))
    ) return 0;

    const mesh_net_state *state = &mesh_network.state;
    uint8_t key[16], akf = app_key_index != APP_KEY_INDEX_NONE &&
                           app_key_index != DEVICE_KEY_LOCAL, aid = 0;
    uint16_t net_idx = !akf ? mesh_network.reply_net_idx : state->net_key_index;
    if (!akf && mesh_subnet_slot(state, net_idx) < 0)
        net_idx = state->net_key_index;

    if (!akf) {
        uint16_t owner = app_key_index == DEVICE_KEY_LOCAL ? src : dst;
        if (dst > 0x7fff || BLE_MESH_TRANSPORT_GET_DEVICE_KEY(owner, key) != 1)
            return 0;
    } else {
        int slot = mesh_app_key_slot(state, app_key_index);
        if (slot < 0) return 0;
        const mesh_app_key *app = &state->app_keys[slot];
        net_idx = mesh_app_net_idx(state, app);
        memcpy(key, mesh_subnet_phase(state, net_idx) == 2 && app->has_new_key ?
                    app->new_key : app->key, 16);
        aid = transport_app_aid(key);
    }

    size_t mic_len = mic_64 ? 8u : 4u;
    size_t upper_len = len + mic_len;
    if (!mic_64 && upper_len <= 15) {
        uint32_t seq = state->next_seq;
        if (seq > 0xffffff) return 0;
        uint32_t iv = state->iv_index - (state->iv_update ? 1u : 0u);
        uint8_t nonce[13], upper[MESH_TRANSPORT_MAX_UPPER];
        transport_nonce(nonce, !akf, mic_64, seq, src, dst, iv);
        if (ccm_encrypt_and_tag(key, nonce, 13, label, label ? 16u : 0u,
                access, len, upper, upper + len, mic_len) != CCM_OK) return 0;
        uint8_t lower[16];
        lower[0] = (akf << 6) | aid;
        memcpy(lower + 1, upper, upper_len);
        return mesh_net_queue(net_idx, src, dst, 0, ttl, lower, upper_len + 1);
    }

    struct transport_tx_pending pending = {
        .src = src,
        .dst = dst,
        .net_idx = net_idx,
        .access_len = (uint16_t)len,
        .akf = akf,
        .aid = aid,
        .ttl = ttl,
        .mic_64 = mic_64,
        .has_label = label != NULL
    };
    memcpy(pending.key, key, sizeof(pending.key));
    if (label) memcpy(pending.label, label, sizeof(pending.label));
    memcpy(pending.access, access, len);

    if (transport_tx.active || segmented_tx_queue_count) {
        if (segmented_tx_queue_count == MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE)
            return 0;
        uint8_t tail = (uint8_t)((segmented_tx_queue_head +
                                  segmented_tx_queue_count) %
                                 MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE);
        segmented_tx_queue[tail] = pending;
        segmented_tx_queue_count++;
        return 1;
    }

    int result = transport_tx_start(&pending);
    if (result != 1) memset(&transport_tx, 0, sizeof(transport_tx));
    return result == 1;
}

// Consume one authenticated Network message. Returns 1 for an Access message,
// 0 for an incomplete/ignored message, or -1 for bad arguments. Completed
// segmented Control messages are dispatched during polling when a handler is
// registered.
static inline int mesh_transport_receive(const mesh_net_message *net,
                                              mesh_access_message *out) {
    if (!net || !out) return -1;
    if (!net->transport_len || net->transport_len > sizeof(net->transport)) return 0;
    const uint8_t *pdu = net->transport;

    uint8_t segmented = pdu[0] & 0x80;
    if (net->ctl && !segmented) {
        if (!net->transport_len) return 0;
        if (pdu[0] == 0) {
            if (net->transport_len != 7 || (pdu[1] & 0x80) ||
                (pdu[2] & 3) || !transport_tx.active ||
                net->net_key_index != transport_tx.net_idx ||
                net->src != transport_tx.dst ||
                net->dst != transport_tx.src
            ) return 0;

            uint16_t seq_zero = (uint16_t)(((pdu[1] & 0x7f) << 6) |
                                            (pdu[2] >> 2));
            if (seq_zero != transport_tx.seq_zero) return 0;
            uint32_t acked = ((uint32_t)pdu[3] << 24) |
                            ((uint32_t)pdu[4] << 16) |
                            ((uint32_t)pdu[5] << 8) | pdu[6];
            if (!acked) {
                transport_tx.active = 0;
                return 0;
            }
            uint32_t segment_mask = transport_tx.seg_n == 31 ? UINT32_MAX :
                                    ((uint32_t)1 << (transport_tx.seg_n + 1)) - 1;
            uint32_t new_acked = (acked & segment_mask) & ~transport_tx.acked;
            transport_tx.acked |= acked & segment_mask;

            if (transport_tx.acked == segment_mask)
                transport_tx.active = 0;
            else if (transport_tx.next_seg > transport_tx.seg_n) {
                if (transport_tx.retries >= transport_sar_tx.unicast_retrans_count ||
                    (!new_acked && transport_tx.retries_without_progress >=
                                       transport_sar_tx.unicast_retrans_wo_progress_count)) {
                    transport_tx.active = 0;
                    return 0;
                }
                transport_tx.retries++;
                if (new_acked) transport_tx.retries_without_progress = 0;
                else transport_tx.retries_without_progress++;
                transport_tx.next_seg = 0;
            }
            return 0;
        }

        // Friendship control messages are unsegmented; deliver them to the
        // control handler just like reassembled segmented Control messages.
        if ((net->dst > 0x7fff || mesh_local_element(net->dst)) &&
            (transport_control_handler || net->transport_len > 1)) {
            mesh_transport_control_message control = {0};
            control.src = net->src;
            control.dst = net->dst;
            control.net_key_index = net->net_key_index;
            control.ttl = net->ttl;
            control.opcode = pdu[0];
            control.friendship = net->friendship;
            control.len = (uint16_t)(net->transport_len - 1);
            if (control.len)
                memcpy(control.params, pdu + 1, control.len);
            mesh_lpn_control_receive(&control);
            mesh_friend_request_receive(&control);
            mesh_friend_poll_receive(&control);
            if (transport_control_handler)
                transport_control_handler(&control);
        }
        return 0;
    }

    if (net->dst <= 0x7fff && !mesh_local_element(net->dst)) return 0;

    if (!segmented) {
        if (net->transport_len < 6) return 0;
        uint8_t akf = (pdu[0] >> 6) & 1;
        uint8_t aid = pdu[0] & 0x3f;
        int result = transport_decrypt(akf, aid, 0, net->seq, net->iv_index,
                                       net->net_key_index, net->src, net->dst, pdu + 1,
                                       net->transport_len - 1, out);
        if (result) out->ttl = net->ttl;
        return result;
    }

    if (net->transport_len < 5) return 0;
    uint8_t mic_64 = net->ctl ? 0 : pdu[1] >> 7;
    if (net->ctl && (!(pdu[0] & 0x7f) || (pdu[1] & 0x80))) return 0;
    uint8_t akf = (pdu[0] >> 6) & 1;
    uint8_t aid = pdu[0] & 0x3f;
    uint16_t seq_zero = (uint16_t)(((pdu[1] & 0x7f) << 6) | ((pdu[2] >> 2) & 0x3f));
    uint8_t seg_o = (uint8_t)(((pdu[2] & 3) << 3) | (pdu[3] >> 5));
    uint8_t seg_n = pdu[3] & 0x1f;
    size_t segment_len = net->transport_len - 4;
    size_t segment_size = net->ctl ? MESH_TRANSPORT_CONTROL_SEGMENT_SIZE :
                                     MESH_TRANSPORT_SEGMENT_SIZE;

    if (seg_o > seg_n || !segment_len || segment_len > segment_size ||
        (seg_o != seg_n && segment_len != segment_size)) return 0;

    uint32_t seq_auth = (net->seq & ~0x1fff) | seq_zero;
    if (seq_auth > net->seq) {
        if (seq_auth < 0x2000) return 0;
        seq_auth -= 0x2000;
    }

    uint32_t now = GET_MILLIS();
    struct transport_rx *free_rx = NULL;
    uint32_t context_updated_ms = now;
    uint32_t mask;
    struct transport_rx *matched_rx = NULL;
    uint32_t segment_mask = seg_n == 31 ? UINT32_MAX :
        ((uint32_t)1 << (seg_n + 1)) - 1;
    uint8_t duplicate = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
        struct transport_rx *rx = &transport_rx[i];
        if (rx->active && (uint32_t)(now - rx->updated_ms) >=
            ((uint32_t)transport_sar_rx.discard_timeout + 1) * 5000)
            rx->active = rx->ack_pending = 0;
        if (!rx->active) {
            if (!free_rx) free_rx = rx;
            continue;
        }
        if (!transport_rx_matches(rx, net->ctl, net->net_key_index, net->src, net->dst,
                                  seq_auth, net->iv_index)) continue;
        matched_rx = rx;
        context_updated_ms = rx->updated_ms;
        uint16_t old_seq_zero = (uint16_t)(((rx->transport[1] & 0x7f) << 6) |
                                            ((rx->transport[2] >> 2) & 0x3f));
        uint8_t old_seg_n = rx->transport[3] & 0x1f;
        uint8_t old_seg_o = (uint8_t)(((rx->transport[2] & 3) << 3) |
                                      (rx->transport[3] >> 5));
        if (rx->transport[0] != pdu[0] || old_seq_zero != seq_zero ||
            old_seg_n != seg_n || (rx->transport[1] >> 7) != mic_64)
            return 0;
        if (old_seg_o == seg_o) duplicate = 1;
    }
    mask = transport_rx_received(net->ctl, net->net_key_index, net->src, net->dst,
                                  seq_auth, net->iv_index);

    if (duplicate) {
        uint8_t ack_pending = mesh_local_element(net->dst);
        uint32_t ack_at_ms = now;
        if (mask == segment_mask) {
            // A repeated segment from a completed message gets an ACK, subject
            // to the SAR acknowledgment retransmission interval.
            if (matched_rx && matched_rx->ack_sent) {
                // Bound duplicate ACKs by (Ack Delay Increment + 1.5) times
                // the receiver segment interval.
                uint32_t ack_delay_half_steps =
                    (uint32_t)transport_sar_rx.ack_delay_increment * 2 + 3;
                uint32_t segment_interval_ms =
                    ((uint32_t)transport_sar_rx.segment_interval_step + 1) * 10;
                uint32_t next_ack = matched_rx->ack_sent_ms +
                    ack_delay_half_steps * segment_interval_ms / 2;
                if ((int32_t)(next_ack - now) > 0) ack_at_ms = next_ack;
            }
        } else {
            ack_at_ms += transport_sar_rx_ack_delay_ms(seg_n);
        }
        transport_rx_ack(net->ctl, net->net_key_index, net->src, net->dst, seq_auth,
            net->iv_index, ack_pending, ack_at_ms, context_updated_ms,
            transport_sar_rx.ack_retrans_count);
        return 0;
    } else {
        if (!free_rx) {
            // Completed packets remain briefly so duplicate segments can be ACKed.
            // Reclaim the oldest completed transaction when packet storage is full.
            struct transport_rx *oldest = NULL;
            for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
                struct transport_rx *candidate = &transport_rx[i];
                uint8_t candidate_seg_n = candidate->transport[3] & 0x1f;
                uint32_t candidate_mask = candidate_seg_n == 31 ? UINT32_MAX :
                    ((uint32_t)1 << (candidate_seg_n + 1)) - 1;
                if (!candidate->active ||
                    transport_rx_received(candidate->ctl, candidate->net_idx, candidate->src,
                        candidate->dst, candidate->seq_auth, candidate->iv_index) !=
                            candidate_mask) continue;
                if (!oldest || (uint32_t)(now - candidate->updated_ms) >
                    (uint32_t)(now - oldest->updated_ms)) oldest = candidate;
            }
            if (!oldest) {
                // Reject a unicast transfer with an empty BlockAck when no
                // segment slot can be reclaimed, as required by SAR behavior.
                if (mesh_local_element(net->dst)) {
                    uint16_t rejected_seq_zero = (uint16_t)(seq_auth & 0x1fff);
                    uint8_t ack[7] = {
                        0,
                        (uint8_t)(rejected_seq_zero >> 6),
                        (uint8_t)((rejected_seq_zero & 0x3f) << 2),
                        0, 0, 0, 0
                    };
                    mesh_net_queue(net->net_key_index, net->dst, net->src,
                        1, net->ttl, ack, sizeof(ack));
                }
                return 0;
            }
            // Free every stored segment belonging to the completed transaction.
            for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
                struct transport_rx *rx = &transport_rx[i];
                if (transport_rx_matches(rx, oldest->ctl, oldest->net_idx, oldest->src,
                        oldest->dst, oldest->seq_auth, oldest->iv_index))
                    rx->active = rx->ack_pending = 0;
            }
            free_rx = oldest;
        }
        memset(free_rx, 0, sizeof(*free_rx));
        free_rx->active = 1;
        free_rx->ctl = net->ctl;
        free_rx->ttl = net->ttl;
        free_rx->src = net->src;
        free_rx->dst = net->dst;
        free_rx->net_idx = net->net_key_index;
        free_rx->seq_auth = seq_auth;
        free_rx->seq = net->seq;
        free_rx->iv_index = net->iv_index;
        free_rx->transport_len = (uint8_t)net->transport_len;
        memcpy(free_rx->transport, pdu, net->transport_len);
        transport_rx_ack(net->ctl, net->net_key_index, net->src, net->dst, seq_auth,
            net->iv_index, mesh_local_element(net->dst),
            now + transport_sar_rx_ack_delay_ms(seg_n), now,
            transport_sar_rx.ack_retrans_count);
        mask = transport_rx_received(net->ctl, net->net_key_index, net->src, net->dst,
                                      seq_auth, net->iv_index);
        if (mask == segment_mask) {
            // Last Segment completes reassembly and is acknowledged immediately.
            transport_rx_ack(net->ctl, net->net_key_index, net->src, net->dst,
                seq_auth, net->iv_index, mesh_local_element(net->dst), now, now,
                transport_sar_rx.ack_retrans_count);
        }
    }

    if (mask != segment_mask) return 0;
    if (net->ctl) {
        uint32_t last_seq = 0;
        for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
            const struct transport_rx *rx = &transport_rx[i];
            if (!transport_rx_matches(rx, 1, net->net_key_index, net->src,
                    net->dst, seq_auth, net->iv_index)) continue;
            uint8_t part = (uint8_t)(((rx->transport[2] & 3) << 3) |
                                     (rx->transport[3] >> 5));
            if (part == seg_n) last_seq = rx->seq;
        }
        if (!mesh_net_replay_update(net->src, net->net_key_index,
                                        net->iv_index, last_seq)) {
            for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
                struct transport_rx *rx = &transport_rx[i];
                if (transport_rx_matches(rx, 1, net->net_key_index, net->src,
                        net->dst, seq_auth, net->iv_index)) rx->delivered = 1;
            }
        }
        return 0;
    }

    uint8_t upper[MESH_TRANSPORT_MAX_UPPER];
    uint8_t last_len = 0;
    uint32_t last_seq = 0;
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
        const struct transport_rx *rx = &transport_rx[i];
        if (!transport_rx_matches(rx, net->ctl, net->net_key_index, net->src, net->dst,
                                  seq_auth, net->iv_index)) continue;
        uint8_t part = (uint8_t)(((rx->transport[2] & 3) << 3) |
                                 (rx->transport[3] >> 5));
        size_t part_len = rx->transport_len - 4;
        memcpy(upper + (size_t)part * segment_size,
               rx->transport + 4, part_len);
        if (part == seg_n) {
            last_len = (uint8_t)part_len;
            last_seq = rx->seq;
        }
    }
    size_t upper_len = (size_t)seg_n * segment_size + last_len;
    int result = transport_decrypt(akf, aid, mic_64, seq_auth, net->iv_index,
                                   net->net_key_index, net->src, net->dst, upper,
                                   upper_len, out);
    if (result && !mesh_net_replay_update(net->src, net->net_key_index,
                                               net->iv_index, last_seq))
        return 0;
    if (result) out->ttl = net->ttl;
    return result;
}

// Poll the network, reassemble Access/Control messages, and service SAR.
// Returns 1 with an Access message, 0 if none, or -1 for a send/radio error.
static inline int mesh_transport_poll(mesh_access_message *out) {
    if (!out) return -1;
    mesh_net_message net;
    int received = mesh_net_poll(&net);
    if (received < 0) return -1;
    int result = received ? mesh_transport_receive(&net, out) : 0;
    if (result < 0) return -1;
    // Send delayed Friend Offers and expire LPNs that did not establish or
    // maintain their friendship before the negotiated Poll Timeout.
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        if (!transport_friend_offers[i].used) continue;
        if ((int32_t)(GET_MILLIS() -
                transport_friend_offers[i].expires_at_ms) >= 0) {
            mesh_friendship_clear(transport_friend_offers[i].net_key_index,
                transport_friend_offers[i].lpn_address,
                mesh_network.state.unicast_address);
            memset(&transport_friend_offers[i], 0,
                   sizeof(transport_friend_offers[i]));
            continue;
        }
        if (!transport_friend.enabled || transport_friend_offers[i].offered ||
            (int32_t)(GET_MILLIS() -
                transport_friend_offers[i].offer_at_ms) < 0) continue;
        uint8_t offer[7] = {
            MESH_CONTROL_FRIEND_OFFER,
            transport_friend.receive_window,
            MESH_FRIEND_QUEUE_CAPACITY,
            transport_friend.subscription_size,
            0, // The advertising bearer API currently does not report received RSSI.
            (uint8_t)(transport_friend_offers[i].friend_counter >> 8),
            (uint8_t)transport_friend_offers[i].friend_counter
        };
        if (mesh_net_queue(transport_friend_offers[i].net_key_index,
                mesh_network.state.unicast_address,
                transport_friend_offers[i].lpn_address, 1, 0,
                offer, sizeof(offer)))
            transport_friend_offers[i].offered = 1;
    }
    // Retry discovery every 1.1 seconds and resend Friend Poll while awaiting
    // the initial Friend Update; discard a friendship after six unanswered polls.
    if (transport_lpn.state == MESH_LPN_REQUESTING &&
        (uint32_t)(GET_MILLIS() - transport_lpn.last_tx_ms) >= 1100u) {
        mesh_lpn_send_request();
    } else if (transport_lpn.state == MESH_LPN_WAITING_FOR_UPDATE &&
        (uint32_t)(GET_MILLIS() - transport_lpn.last_tx_ms) >= 1000u) {
        if (transport_lpn.poll_attempts >= 6) {
            mesh_friendship_clear(transport_lpn.net_key_index,
                mesh_network.state.unicast_address, transport_lpn.friend_address);
            transport_lpn.friend_address = 0;
            transport_lpn.state = MESH_LPN_IDLE;
        } else {
            uint8_t poll[2] = {MESH_CONTROL_FRIEND_POLL, transport_lpn.fsn};
            if (mesh_net_queue_friend(transport_lpn.net_key_index,
                    mesh_network.state.unicast_address,
                    transport_lpn.friend_address, 1, 0, poll, sizeof(poll))) {
                transport_lpn.poll_attempts++;
                transport_lpn.last_tx_ms = GET_MILLIS();
            }
        }
    }

    if (transport_control_handler) {
        // Deliver each complete Segmented Control message to the registered handler.
        for (;;) {
            struct transport_rx *first = NULL;
            for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
                struct transport_rx *candidate = &transport_rx[i];
                if (!candidate->active || !candidate->ctl || candidate->delivered)
                    continue;
                uint8_t seg_n = candidate->transport[3] & 0x1f;
                uint32_t segment_mask = seg_n == 31 ? UINT32_MAX :
                    ((uint32_t)1 << (seg_n + 1)) - 1;
                if (transport_rx_received(1, candidate->net_idx, candidate->src,
                        candidate->dst, candidate->seq_auth,
                        candidate->iv_index) == segment_mask) {
                    first = candidate;
                    break;
                }
            }
            if (!first) break;

            mesh_transport_control_message control = {0};
            uint8_t seg_n = first->transport[3] & 0x1f;
            uint8_t last_len = 0;
            control.src = first->src;
            control.dst = first->dst;
            control.net_key_index = first->net_idx;
            control.ttl = first->ttl;
            control.opcode = first->transport[0] & 0x7f;
            for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
                struct transport_rx *rx = &transport_rx[i];
                if (!transport_rx_matches(rx, 1, first->net_idx, first->src,
                        first->dst, first->seq_auth, first->iv_index)) continue;
                uint8_t part = (uint8_t)(((rx->transport[2] & 3) << 3) |
                                         (rx->transport[3] >> 5));
                size_t part_len = rx->transport_len - 4;
                memcpy(control.params + (size_t)part *
                       MESH_TRANSPORT_CONTROL_SEGMENT_SIZE,
                       rx->transport + 4, part_len);
                if (part == seg_n) last_len = (uint8_t)part_len;
                rx->delivered = 1;
            }
            control.len = (uint16_t)((size_t)seg_n *
                MESH_TRANSPORT_CONTROL_SEGMENT_SIZE + last_len);
            transport_control_handler(&control);
        }
    }

    uint32_t now = GET_MILLIS();
    // Expire stored segments and send one due Segment Acknowledgment.
    for (size_t i = 0; i < MESH_TRANSPORT_RX_PACKET_SLOTS; i++) {
        struct transport_rx *rx = &transport_rx[i];
        if (rx->active &&
            (uint32_t)(now - rx->updated_ms) >=
                ((uint32_t)transport_sar_rx.discard_timeout + 1) * 5000) {
            rx->active = rx->ack_pending = 0;
        }
        if (!rx->active || !rx->ack_pending || !mesh_local_element(rx->dst) ||
            (int32_t)(now - rx->ack_at_ms) < 0) continue;
        uint16_t seq_zero = (uint16_t)(((rx->transport[1] & 0x7f) << 6) |
                                        ((rx->transport[2] >> 2) & 0x3f));
        uint32_t mask = transport_rx_received(rx->ctl, rx->net_idx, rx->src, rx->dst,
                                              rx->seq_auth, rx->iv_index);
        uint8_t pdu[7] = {
            0,
            (uint8_t)(seq_zero >> 6),
            (uint8_t)((seq_zero & 0x3f) << 2),
            (uint8_t)(mask >> 24), (uint8_t)(mask >> 16),
            (uint8_t)(mask >> 8), (uint8_t)mask
        };
        if (!mesh_net_queue(rx->net_idx, rx->dst, rx->src,
                                1, rx->ttl,
                                pdu, sizeof(pdu))) return -1;
        for (size_t j = 0; j < MESH_TRANSPORT_RX_PACKET_SLOTS; j++) {
            struct transport_rx *sent_rx = &transport_rx[j];
            if (!transport_rx_matches(sent_rx, rx->ctl, rx->net_idx, rx->src,
                    rx->dst, rx->seq_auth, rx->iv_index)) continue;
            sent_rx->ack_sent = 1;
            sent_rx->ack_sent_ms = now;
        }
        uint8_t seg_n = rx->transport[3] & 0x1f;
        uint8_t retrans_left = rx->ack_retrans_left;
        uint8_t retransmit = seg_n > transport_sar_rx.segments_threshold &&
                             retrans_left != 0;
        if (retransmit) retrans_left--;
        transport_rx_ack(rx->ctl, rx->net_idx, rx->src, rx->dst, rx->seq_auth,
            rx->iv_index, retransmit,
            retransmit ? now + ((uint32_t)transport_sar_rx.segment_interval_step + 1) * 10 : 0,
            rx->updated_ms, retrans_left);
        break;
    }

    // Start the oldest queued segmented message after active SAR completes.
    if (!transport_tx.active && segmented_tx_queue_count) {
        struct transport_tx_pending *pending =
            &segmented_tx_queue[segmented_tx_queue_head];
        int started = transport_tx_start(pending);
        if (started == 1) {
            memset(pending, 0, sizeof(*pending));
            segmented_tx_queue_head = (uint8_t)((segmented_tx_queue_head + 1) %
                                                 MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE);
            segmented_tx_queue_count--;
        } else {
            memset(&transport_tx, 0, sizeof(transport_tx));
            if (started < 0) {
                // Discard a saved message that cannot use the current sequence state.
                memset(pending, 0, sizeof(*pending));
                segmented_tx_queue_head = (uint8_t)((segmented_tx_queue_head + 1) %
                                                     MESH_TRANSPORT_SEGMENTED_TX_QUEUE_SIZE);
                segmented_tx_queue_count--;
                return -1;
            }
        }
    }

    if (transport_tx.active) {
        if (transport_tx.next_seg <= transport_tx.seg_n) {
            uint32_t segment_interval_ms =
                ((uint32_t)transport_sar_tx.segment_interval_step + 1) * 10;
            if ((uint32_t)(now - transport_tx.last_tx_ms) >= segment_interval_ms &&
                transport_segment_queue() < 0) return -1;
        }
        else if (transport_tx.dst >= 0x8000 &&
                 (uint32_t)(now - transport_tx.last_tx_ms) >=
                     ((uint32_t)transport_sar_tx.multicast_retrans_interval_step + 1) * 25) {
            // Multicast has no Segment ACK, so repeat the full segment set.
            if (transport_tx.retries >= transport_sar_tx.multicast_retrans_count)
                transport_tx.active = 0;
            else {
                transport_tx.retries++;
                transport_tx.next_seg = 0;
            }
        }
        else if ((uint32_t)(now - transport_tx.last_tx_ms) >=
                 (((uint32_t)transport_sar_tx.unicast_retrans_interval_step + 1) * 25) +
                 (((uint32_t)transport_sar_tx.unicast_retrans_interval_increment + 1) * 25) *
                     (transport_tx.ttl ? transport_tx.ttl - 1 : 0)) {
            if (transport_tx.retries >= transport_sar_tx.unicast_retrans_count ||
                transport_tx.retries_without_progress >=
                    transport_sar_tx.unicast_retrans_wo_progress_count) {
                transport_tx.active = 0;
                return -1;
            }
            transport_tx.retries++;
            transport_tx.retries_without_progress++;
            transport_tx.next_seg = 0;
        }
    }
    return result;
}

#endif
