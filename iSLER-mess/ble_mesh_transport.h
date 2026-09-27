// akf = Application Key Flag
// aid = Application Key Identifier

#ifndef ISLER_BLE_MESH_TRANSPORT_H
#define ISLER_BLE_MESH_TRANSPORT_H

#include "ble_mesh_network.h"

#define MESH_TRANSPORT_MAX_ACCESS 380
#define MESH_TRANSPORT_MAX_UPPER 384
#define MESH_TRANSPORT_SEGMENT_SIZE 12
#define MESH_TRANSPORT_RETRY_MS 1000
#define MESH_TRANSPORT_RX_TIMEOUT_MS 5000

// Return 1 when a Device Key is known for this unicast address, or 0 otherwise.
int BLE_MESH_TRANSPORT_GET_DEVICE_KEY(uint16_t address, uint8_t key[16]);
uint32_t GET_MILLIS(void);

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t app_key_index; // 0xffff means the Device Key was used
    uint16_t len;
    uint8_t ttl;
    uint8_t data[MESH_TRANSPORT_MAX_ACCESS];
} mesh_access_message;

static struct {
    uint8_t active;
    uint8_t akf, aid, ttl, seg_n, next_seg, retries;
    uint16_t src, dst, seq_zero, upper_len;
    uint32_t seq_auth, iv_index, acked, last_tx_ms;
    uint8_t upper[MESH_TRANSPORT_MAX_UPPER];
} transport_tx;

static struct {
    uint8_t active, complete, ack_pending;
    uint8_t akf, aid, ttl, seg_n, mic_64, last_len;
    uint16_t src, dst, seq_zero;
    uint32_t seq_auth, iv_index, received, started_ms, ack_at_ms;
    uint8_t upper[MESH_TRANSPORT_MAX_UPPER];
} transport_rx;

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
                             uint32_t seq, uint32_t iv_index, uint16_t src,
                             uint16_t dst, const uint8_t *upper, size_t len,
                             mesh_access_message *out) {
    size_t mic_len = mic_64 ? 8 : 4;
    if (len <= mic_len || len - mic_len > MESH_TRANSPORT_MAX_ACCESS ||
        (dst >= 0x8000 && dst < 0xc000)) return 0;

    uint8_t nonce[13], key[16];
    transport_nonce(nonce, !akf, mic_64, seq, src, dst, iv_index);
    out->app_key_index = 0xffff;

    if (akf) {
        const mesh_net_state *state = &mesh_network.state;
        if (!state->has_app_key) return 0;

        for (int i = 0; i < 2; i++) {
            if (i && !state->has_new_app_key) break;
            const uint8_t *app_key = i ? state->new_app_key : state->app_key;
            if (transport_app_aid(app_key) != aid) continue;

            if (ccm_auth_decrypt(app_key, nonce, 13, NULL, 0,
                                 upper, len - mic_len, upper + len - mic_len,
                                 mic_len, out->data) == CCM_OK
            ) {
                out->app_key_index = state->app_key_index;
                break;
            }
        }
        if (out->app_key_index == 0xffff) return 0;
    } else {
        if (aid != 0 || dst != mesh_network.state.unicast_address) return 0;
        int ok = 0;
        if (BLE_MESH_TRANSPORT_GET_DEVICE_KEY(dst, key) == 1 &&
            ccm_auth_decrypt(key, nonce, 13, NULL, 0, upper, len - mic_len,
                             upper + len - mic_len, mic_len, out->data) == CCM_OK)
            ok = 1;
        if (!ok && BLE_MESH_TRANSPORT_GET_DEVICE_KEY(src, key) == 1 &&
            ccm_auth_decrypt(key, nonce, 13, NULL, 0, upper, len - mic_len,
                             upper + len - mic_len, mic_len, out->data) == CCM_OK)
            ok = 1;
        if (!ok) return 0;
    }

    out->src = src;
    out->dst = dst;
    out->ttl = 0;
    out->len = (uint16_t)(len - mic_len);
    return 1;
}

static int transport_send_segment(void) {
    if (!transport_tx.active) return 0;

    while (transport_tx.next_seg <= transport_tx.seg_n &&
           (transport_tx.acked & ((uint32_t)1 << transport_tx.next_seg))
    ) { transport_tx.next_seg++; }

    if (transport_tx.next_seg > transport_tx.seg_n) return 1;

    uint32_t iv = mesh_network.state.iv_index - (mesh_network.state.iv_update ? 1 : 0);
    if (iv != transport_tx.iv_index ||
        mesh_network.state.unicast_address != transport_tx.src ||
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
    lower[1] = (uint8_t)(transport_tx.seq_zero >> 6);
    lower[2] = (uint8_t)(((transport_tx.seq_zero & 0x3f) << 2) | (seg_o >> 3));
    lower[3] = (uint8_t)((seg_o << 5) | transport_tx.seg_n);

    if (count > MESH_TRANSPORT_SEGMENT_SIZE) count = MESH_TRANSPORT_SEGMENT_SIZE;
    memcpy(lower + 4, transport_tx.upper + offset, count);

    if (!ble_mesh_net_send(transport_tx.dst, 0, transport_tx.ttl,
                           lower, count + 4)) return 0;
    transport_tx.next_seg++;
    transport_tx.last_tx_ms = GET_MILLIS();
    return 1;
}

// Queue an encrypted access message. Returns 1 if accepted, 0 on failure.
// Sends a 32-bit TransMIC; virtual addresses are not supported yet.
// Only one segmented outgoing access message may be active at a time.
static inline int ble_mesh_transport_send(uint16_t dst, uint8_t ttl,
                                           uint8_t use_device_key,
                                           const uint8_t *access, size_t len) {
    if (!mesh_network.ready || !access || len == 0 ||
        len > MESH_TRANSPORT_MAX_ACCESS || ttl > 0x7f || dst == 0 ||
        (dst >= 0x8000 && dst < 0xc000) || transport_tx.active
    ) return 0;

    const mesh_net_state *state = &mesh_network.state;
    uint8_t key[16], akf = !use_device_key, aid = 0;

    if (use_device_key) {
        if (dst > 0x7fff || BLE_MESH_TRANSPORT_GET_DEVICE_KEY(dst, key) != 1)
            return 0;
    } else {
        if (!state->has_app_key ||
            (state->key_refresh_phase == 2 && !state->has_new_app_key)) return 0;
        const uint8_t *app_key = state->key_refresh_phase == 2 &&
                                 state->has_new_app_key ?
                                 state->new_app_key : state->app_key;
        memcpy(key, app_key, 16);
        aid = transport_app_aid(key);
    }

    size_t upper_len = len + 4;
    uint8_t seg_n = upper_len > 15 ? (uint8_t)((upper_len - 1) / 12) : 0;
    uint32_t seq = state->next_seq;
    if (seq > 0xffffff || seq + seg_n > 0xffffff) return 0;

    uint8_t nonce[13];
    uint8_t upper[MESH_TRANSPORT_MAX_UPPER];
    uint32_t iv = state->iv_index - (state->iv_update ? 1 : 0);

    transport_nonce(nonce, use_device_key, 0, seq, state->unicast_address, dst, iv);
    if (ccm_encrypt_and_tag(key, nonce, 13, NULL, 0, access, len,
                            upper, upper + len, 4) != CCM_OK) return 0;

    if (upper_len <= 15) {
        uint8_t lower[16];
        lower[0] = (akf << 6) | aid;
        memcpy(lower + 1, upper, upper_len);
        return ble_mesh_net_send(dst, 0, ttl, lower, upper_len + 1);
    }

    transport_tx.active = 1;
    transport_tx.akf = akf;
    transport_tx.aid = aid;
    transport_tx.ttl = ttl;
    transport_tx.src = state->unicast_address;
    transport_tx.dst = dst;
    transport_tx.seq_zero = seq & 0x1fff;
    transport_tx.seq_auth = seq;
    transport_tx.iv_index = iv;
    transport_tx.upper_len = (uint16_t)upper_len;
    transport_tx.seg_n = seg_n;
    transport_tx.next_seg = 0;
    transport_tx.retries = 0;
    transport_tx.acked = 0;
    memcpy(transport_tx.upper, upper, upper_len);

    if (transport_send_segment() != 1) {
        transport_tx.active = 0;
        return 0;
    }
    return 1;
}

static int transport_send_ack(void) {
    if (!transport_rx.active || !transport_rx.ack_pending ||
        transport_rx.dst != mesh_network.state.unicast_address) return 0;

    uint16_t seq_zero = transport_rx.seq_zero;
    uint32_t mask = transport_rx.received;
    uint8_t pdu[7] = {
        0,
        (uint8_t)(seq_zero >> 6),
        (uint8_t)((seq_zero & 0x3f) << 2),
        (uint8_t)(mask >> 24), (uint8_t)(mask >> 16),
        (uint8_t)(mask >> 8), (uint8_t)mask
    };
    if (!ble_mesh_net_send(transport_rx.src, 1, transport_rx.ttl,
                           pdu, sizeof(pdu))) return -1;
    transport_rx.ack_pending = 0;
    return 1;
}

// Consume one authenticated Network message. Returns 1 for an Access message,
// 0 for an incomplete/ignored message, or -1 for bad arguments.
static inline int ble_mesh_transport_receive(const mesh_net_message *net,
                                              mesh_access_message *out) {
    if (!net || !out) return -1;
    if (!net->transport_len || net->transport_len > sizeof(net->transport)) return 0;
    const uint8_t *pdu = net->transport;

    if (net->ctl) {
        if (net->transport_len != 7 || pdu[0] != 0 || (pdu[1] & 0x80) ||
            (pdu[2] & 3) || !transport_tx.active ||
            net->src != transport_tx.dst ||
            net->dst != mesh_network.state.unicast_address
        ) return 0;

        uint16_t seq_zero = (uint16_t)(((pdu[1] & 0x7f) << 6) | (pdu[2] >> 2));
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
        transport_tx.acked |= acked & segment_mask;

        if (transport_tx.acked == segment_mask)
            transport_tx.active = 0;
        else if (transport_tx.next_seg > transport_tx.seg_n)
            transport_tx.next_seg = 0;
        return 0;
    }

    if (net->dst <= 0x7fff && net->dst != mesh_network.state.unicast_address) return 0;

    uint8_t akf = (pdu[0] >> 6) & 1;
    uint8_t aid = pdu[0] & 0x3f;

    if (!(pdu[0] & 0x80)) {
        if (net->transport_len < 6) return 0;
        int result = transport_decrypt(akf, aid, 0, net->seq, net->iv_index,
                                       net->src, net->dst, pdu + 1,
                                       net->transport_len - 1, out);
        if (result) out->ttl = net->ttl;
        return result;
    }

    if (net->transport_len < 5) return 0;
    uint8_t mic_64 = pdu[1] >> 7;
    uint16_t seq_zero = (uint16_t)(((pdu[1] & 0x7f) << 6) | ((pdu[2] >> 2) & 0x3f));
    uint8_t seg_o = (uint8_t)(((pdu[2] & 3) << 3) | (pdu[3] >> 5));
    uint8_t seg_n = pdu[3] & 0x1f;
    size_t segment_len = net->transport_len - 4;

    if (seg_o > seg_n || segment_len > 12 || (seg_o != seg_n && segment_len != 12)) return 0;

    uint32_t seq_auth = (net->seq & ~0x1fff) | seq_zero;
    if (seq_auth > net->seq) {
        if (seq_auth < 0x2000) return 0;
        seq_auth -= 0x2000;
    }

    uint32_t now = GET_MILLIS();
    if (transport_rx.active &&
        (transport_rx.src != net->src ||
        transport_rx.dst != net->dst ||
        transport_rx.seq_auth != seq_auth ||
        transport_rx.iv_index != net->iv_index)
    ) {
        if (!transport_rx.complete && (uint32_t)(now - transport_rx.started_ms) <
            MESH_TRANSPORT_RX_TIMEOUT_MS
        ) return 0;
        transport_rx.active = 0;
    }
    if (!transport_rx.active) {
        memset(&transport_rx, 0, sizeof(transport_rx));
        transport_rx.active = 1;
        transport_rx.akf = akf;
        transport_rx.aid = aid;
        transport_rx.ttl = net->ttl;
        transport_rx.seg_n = seg_n;
        transport_rx.mic_64 = mic_64;
        transport_rx.src = net->src;
        transport_rx.dst = net->dst;
        transport_rx.seq_zero = seq_zero;
        transport_rx.seq_auth = seq_auth;
        transport_rx.iv_index = net->iv_index;
        transport_rx.started_ms = now;
    }
    if (transport_rx.akf != akf || transport_rx.aid != aid ||
        transport_rx.seg_n != seg_n || transport_rx.mic_64 != mic_64) return 0;
    if (transport_rx.complete) {
        transport_rx.ack_pending = net->dst == mesh_network.state.unicast_address;
        transport_rx.ack_at_ms = now;
        return 0;
    }

    uint32_t bit = (uint32_t)1 << seg_o;
    if (!(transport_rx.received & bit)) {
        memcpy(transport_rx.upper + (size_t)seg_o * 12, pdu + 4, segment_len);
        transport_rx.received |= bit;
        transport_rx.started_ms = now;
        if (seg_o == seg_n) transport_rx.last_len = (uint8_t)segment_len;
    }
    transport_rx.ack_pending = net->dst == mesh_network.state.unicast_address;
    transport_rx.ack_at_ms = now + 200;
    uint32_t segment_mask = (seg_n == 31) ? UINT32_MAX : ((uint32_t)1 << (seg_n + 1)) - 1;
    if (transport_rx.received != segment_mask) return 0;

    transport_rx.complete = 1;
    transport_rx.ack_at_ms = now;
    size_t upper_len = (size_t)seg_n * 12 + transport_rx.last_len;
    int result = transport_decrypt(akf, aid, mic_64, seq_auth, net->iv_index,
                                   net->src, net->dst, transport_rx.upper,
                                   upper_len, out);
    if (result) out->ttl = net->ttl;
    return result;
}

// Poll the network, reassemble received access messages, and service SAR.
// Returns 1 with an Access message, 0 if none, or -1 for a send/radio error.
static inline int ble_mesh_transport_poll(mesh_access_message *out) {
    if (!out) return -1;
    mesh_net_message net;
    int received = ble_mesh_net_poll(&net);
    if (received < 0) return -1;
    int result = received ? ble_mesh_transport_receive(&net, out) : 0;
    if (result < 0) return -1;

    uint32_t now = GET_MILLIS();
    if (transport_rx.active &&
        (uint32_t)(now - transport_rx.started_ms) >= MESH_TRANSPORT_RX_TIMEOUT_MS) {
        transport_rx.active = 0;
        transport_rx.ack_pending = 0;
    }
    if (transport_rx.ack_pending &&
        (int32_t)(now - transport_rx.ack_at_ms) >= 0 &&
        transport_send_ack() < 0) return -1;

    if (transport_tx.active) {
        if (transport_tx.next_seg <= transport_tx.seg_n) {
            if (transport_send_segment() < 0) return -1;
        }
        else if (transport_tx.dst >= 0xc000) {
            // Group destinations do not send Segment Acknowledgments.
            transport_tx.active = 0;
        }
        else if ((uint32_t)(now - transport_tx.last_tx_ms) >= MESH_TRANSPORT_RETRY_MS) {
            if (transport_tx.retries++ >= 3) {
                transport_tx.active = 0;
                return -1;
            }
            transport_tx.next_seg = 0;
        }
    }
    return result;
}

#endif
