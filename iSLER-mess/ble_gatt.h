#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gap.h"

// TODO for complete BLE GATT support:
// - Call BLE_MESH_GATT_PROXY_OFFER after Network PDU authentication and
//   destination decoding so Proxy Filter rules are applied correctly.
// - Complete and harden ATT request validation, discovery, errors, and MTU
//   handling; add prepared writes and indications if needed.
// - Enforce attribute permissions and security through SMP and Link Layer
//   encryption before exposing protected attributes.
// - Add PB-GATT provisioning as a separate service if GATT provisioning is
//   required; it is not provided by the Mesh Proxy Service.
// - Add a GATT client only if this device must discover or use peer services.
// - Verify Proxy filtering, SAR, notifications, disconnect cleanup, and ATT
//   procedures against an independent BLE/GATT implementation and hardware.

// Single-link ATT server for the Bluetooth Mesh Proxy Service. The application
// must call mesh_gatt_poll() while servicing BLE connection events, register a
// receive callback, and offer received advertising-bearer PDUs for forwarding.
#ifndef MESH_GATT_ATT_MTU_MAX
#define MESH_GATT_ATT_MTU_MAX 247
#endif
#ifndef MESH_GATT_PROXY_PDU_MAX
#define MESH_GATT_PROXY_PDU_MAX 64
#endif
#ifndef MESH_GATT_PROXY_QUEUE_SIZE
#define MESH_GATT_PROXY_QUEUE_SIZE 4
#endif
#ifndef MESH_GATT_PROXY_FILTER_SIZE
#define MESH_GATT_PROXY_FILTER_SIZE 16
#endif
#ifndef MESH_GATT_PROXY_SAR_TIMEOUT_MS
#define MESH_GATT_PROXY_SAR_TIMEOUT_MS 20000u
#endif
#define MESH_GATT_PROXY_NETWORK_PDU_MIN 14
#define MESH_GATT_PROXY_NETWORK_PDU_MAX 29
#define MESH_GATT_PROXY_BEACON_PDU_LEN 22
#if MESH_GATT_ATT_MTU_MAX < 23 || MESH_GATT_ATT_MTU_MAX > 517
#error "MESH_GATT_ATT_MTU_MAX must be between 23 and 517"
#endif

#define MESH_GATT_ATT_CID 0x0004
#define MESH_GATT_PROXY_SERVICE_UUID 0x1828
#define MESH_GATT_PROXY_DATA_IN_UUID 0x2ADD
#define MESH_GATT_PROXY_DATA_OUT_UUID 0x2ADE

enum {
    MESH_GATT_PROXY_NETWORK = 0,
    MESH_GATT_PROXY_BEACON = 1,
    MESH_GATT_PROXY_CONFIGURATION = 2,
    MESH_GATT_PROXY_PROVISIONING = 3
};

enum {
    MESH_GATT_HANDLE_PROXY_SERVICE = 1,
    MESH_GATT_HANDLE_DATA_IN_DECL = 2,
    MESH_GATT_HANDLE_DATA_IN = 3,
    MESH_GATT_HANDLE_DATA_OUT_DECL = 4,
    MESH_GATT_HANDLE_DATA_OUT = 5,
    MESH_GATT_HANDLE_DATA_OUT_CCCD = 6
};

typedef int (*mesh_gatt_proxy_rx_fn)(uint8_t type, const uint8_t *pdu,
                                      size_t len, void *context);

static struct {
    uint8_t connected, cccd, mtu_exchanged, rx_active, rx_att_pending;
    uint8_t proxy_rx_active, proxy_rx_type, filter_type, filter_count;
    uint16_t mtu, filter[MESH_GATT_PROXY_FILTER_SIZE];
    uint16_t l2cap_expected, l2cap_used, att_rx_len;
    uint32_t proxy_rx_started_ms, proxy_tx_started_ms;
    uint8_t proxy_tx_sar_active, proxy_sar_disconnect_pending;
    uint8_t l2cap_rx[4 + MESH_GATT_ATT_MTU_MAX];
    uint8_t att_rx[MESH_GATT_ATT_MTU_MAX];
    uint8_t tx_active;
    uint16_t tx_len, tx_offset;
    uint8_t tx_l2cap[4 + MESH_GATT_ATT_MTU_MAX];
    uint8_t proxy_rx[MESH_GATT_PROXY_PDU_MAX];
    uint16_t proxy_rx_len;
    struct {
        uint8_t type, len, offset;
        uint16_t destination;
        uint8_t data[MESH_GATT_PROXY_PDU_MAX];
    } proxy_tx[MESH_GATT_PROXY_QUEUE_SIZE];
    uint8_t proxy_tx_head, proxy_tx_count;
    mesh_gatt_proxy_rx_fn proxy_rx_callback;
    void *proxy_rx_context;
} mesh_gatt;

static uint16_t mesh_gatt_u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static void mesh_gatt_proxy_sar_cancel(void) {
    mesh_gatt.proxy_rx_active = 0;
    mesh_gatt.proxy_rx_len = 0;
    mesh_gatt.proxy_rx_started_ms = 0;
    mesh_gatt.proxy_tx_sar_active = 0;
    mesh_gatt.proxy_tx_started_ms = 0;
    mesh_gatt.proxy_sar_disconnect_pending = 0;
}

// Mesh Proxy SAR transfers have a fixed 20-second deadline. Expiry requires
// the Proxy Server to disconnect; reason 0x13 is Remote User Terminated.
static int mesh_gatt_proxy_sar_timeout_poll(void) {
    if (mesh_gatt.proxy_sar_disconnect_pending) {
        mesh_gap_disconnect(0x13);
        return 1;
    }
    uint32_t now = GET_MILLIS();
    if ((mesh_gatt.proxy_rx_active &&
         (uint32_t)(now - mesh_gatt.proxy_rx_started_ms) >=
             MESH_GATT_PROXY_SAR_TIMEOUT_MS) ||
        (mesh_gatt.proxy_tx_sar_active &&
         (uint32_t)(now - mesh_gatt.proxy_tx_started_ms) >=
             MESH_GATT_PROXY_SAR_TIMEOUT_MS)) {
        mesh_gatt_proxy_sar_cancel();
        mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
        mesh_gatt.proxy_sar_disconnect_pending = 1;
        mesh_gap_disconnect(0x13);
        return 1;
    }
    return 0;
}

void mesh_gatt_proxy_set_rx_callback(mesh_gatt_proxy_rx_fn callback,
                                      void *context) {
    mesh_gatt.proxy_rx_callback = callback;
    mesh_gatt.proxy_rx_context = context;
}

// Advertise the Mesh Proxy Service UUID as a connectable legacy Peripheral.
int mesh_gatt_proxy_advertising_start(uint16_t interval_ms) {
    static const uint8_t data[] = {
        2, 0x01, 0x06,             // General discoverable, BR/EDR not supported.
        3, 0x03, 0x28, 0x18         // Complete 16-bit service UUID list: 0x1828.
    };
    return mesh_gap_connectable_advertising_start(data, sizeof(data),
                                                   NULL, 0, interval_ms);
}

// Queue a Network/Beacon Proxy PDU for the subscribed client. Network PDUs
// pass the connection's whitelist or blacklist before they are queued.
int mesh_gatt_proxy_offer(uint8_t type, const uint8_t *pdu, size_t len,
                          uint16_t destination) {
    if (!pdu || !len || len > MESH_GATT_PROXY_PDU_MAX || type > 3 ||
        !mesh_gatt.connected || !mesh_gatt.cccd ||
        mesh_gatt.proxy_tx_count >= MESH_GATT_PROXY_QUEUE_SIZE) return 0;
    if ((type == MESH_GATT_PROXY_NETWORK &&
         (len < MESH_GATT_PROXY_NETWORK_PDU_MIN ||
          len > MESH_GATT_PROXY_NETWORK_PDU_MAX)) ||
        (type == MESH_GATT_PROXY_BEACON &&
         len != MESH_GATT_PROXY_BEACON_PDU_LEN)) return 0;
    if (type == MESH_GATT_PROXY_NETWORK) {
        uint8_t listed = 0;
        for (uint8_t i = 0; i < mesh_gatt.filter_count; i++)
            if (mesh_gatt.filter[i] == destination) listed = 1;
        if ((!mesh_gatt.filter_type && !listed) ||
            (mesh_gatt.filter_type && listed)) return 0;
    }
    uint8_t slot = (mesh_gatt.proxy_tx_head + mesh_gatt.proxy_tx_count) %
                   MESH_GATT_PROXY_QUEUE_SIZE;
    mesh_gatt.proxy_tx[slot].type = type;
    mesh_gatt.proxy_tx[slot].len = (uint8_t)len;
    mesh_gatt.proxy_tx[slot].offset = 0;
    mesh_gatt.proxy_tx[slot].destination = destination;
    memcpy(mesh_gatt.proxy_tx[slot].data, pdu, len);
    mesh_gatt.proxy_tx_count++;
    return 1;
}

static int mesh_gatt_tx_att(const uint8_t *att, size_t len) {
    if (!att || !len || len > mesh_gatt.mtu || mesh_gatt.tx_active) return 0;
    mesh_gatt.tx_l2cap[0] = (uint8_t)len;
    mesh_gatt.tx_l2cap[1] = (uint8_t)(len >> 8);
    mesh_gatt.tx_l2cap[2] = MESH_GATT_ATT_CID;
    mesh_gatt.tx_l2cap[3] = 0;
    memcpy(mesh_gatt.tx_l2cap + 4, att, len);
    mesh_gatt.tx_len = (uint16_t)(len + 4);
    mesh_gatt.tx_offset = 0;
    mesh_gatt.tx_active = 1;
    return 1;
}

static int mesh_gatt_error(uint8_t request, uint16_t handle, uint8_t error) {
    uint8_t rsp[5] = {0x01, request, (uint8_t)handle,
                      (uint8_t)(handle >> 8), error};
    return mesh_gatt_tx_att(rsp, sizeof(rsp));
}

// Return a readable attribute's complete value; distinguish missing handles
// from known attributes that are not readable.
static uint8_t mesh_gatt_read_value(uint16_t handle, uint8_t *value,
                                    size_t *len) {
    static const uint8_t service[] = {0x28, 0x18};
    static const uint8_t data_in[] = {4, 3, 0, 0xdd, 0x2a};
    static const uint8_t data_out[] = {0x10, 5, 0, 0xde, 0x2a};
    if (!handle || handle > 6) return 1;
    const uint8_t *src;
    size_t size;
    if (handle == 1) { src = service; size = sizeof(service); }
    else if (handle == 2) { src = data_in; size = sizeof(data_in); }
    else if (handle == 4) { src = data_out; size = sizeof(data_out); }
    else if (handle == 6) {
        value[0] = mesh_gatt.cccd;
        value[1] = 0;
        *len = 2;
        return 0;
    } else return 2;
    memcpy(value, src, size);
    *len = size;
    return 0;
}

static int mesh_gatt_proxy_queue(uint8_t type, const uint8_t *data,
                                 size_t len) {
    if (!data || !len || len > MESH_GATT_PROXY_PDU_MAX ||
        mesh_gatt.proxy_tx_count >= MESH_GATT_PROXY_QUEUE_SIZE) return 0;
    uint8_t slot = (mesh_gatt.proxy_tx_head + mesh_gatt.proxy_tx_count) %
                   MESH_GATT_PROXY_QUEUE_SIZE;
    mesh_gatt.proxy_tx[slot].type = type;
    mesh_gatt.proxy_tx[slot].len = (uint8_t)len;
    mesh_gatt.proxy_tx[slot].offset = 0;
    mesh_gatt.proxy_tx[slot].destination = 0;
    memcpy(mesh_gatt.proxy_tx[slot].data, data, len);
    mesh_gatt.proxy_tx_count++;
    return 1;
}

static void mesh_gatt_proxy_configuration(const uint8_t *p, size_t len) {
    if (!len) return;
    uint8_t opcode = p[0];
    if (opcode == 0 && len == 2 && p[1] <= 1) {
        mesh_gatt.filter_type = p[1];
        mesh_gatt.filter_count = 0;
    } else if ((opcode == 1 || opcode == 2) && len >= 3 && !((len - 1) & 1)) {
        for (size_t off = 1; off + 1 < len; off += 2) {
            uint16_t address = mesh_gatt_u16(p + off);
            uint8_t i = 0;
            while (i < mesh_gatt.filter_count && mesh_gatt.filter[i] != address) i++;
            if (opcode == 1 && i == mesh_gatt.filter_count &&
                i < MESH_GATT_PROXY_FILTER_SIZE)
                mesh_gatt.filter[mesh_gatt.filter_count++] = address;
            if (opcode == 2 && i < mesh_gatt.filter_count)
                mesh_gatt.filter[i] = mesh_gatt.filter[--mesh_gatt.filter_count];
        }
    } else return;
    uint8_t status[4] = {3, mesh_gatt.filter_type,
        (uint8_t)mesh_gatt.filter_count, (uint8_t)(mesh_gatt.filter_count >> 8)};
    mesh_gatt_proxy_queue(MESH_GATT_PROXY_CONFIGURATION, status, sizeof(status));
}

static void mesh_gatt_proxy_deliver(uint8_t type, const uint8_t *pdu, size_t len) {
    if (type == MESH_GATT_PROXY_CONFIGURATION) {
        mesh_gatt_proxy_configuration(pdu, len);
    } else if (type != MESH_GATT_PROXY_PROVISIONING &&
               mesh_gatt.proxy_rx_callback) {
        mesh_gatt.proxy_rx_callback(type, pdu, len, mesh_gatt.proxy_rx_context);
    }
}

static void mesh_gatt_proxy_input(const uint8_t *p, size_t len) {
    if (!len) return;
    if (mesh_gatt_proxy_sar_timeout_poll()) return;
    uint8_t header = p[0], sar = header >> 6, type = header & 0x0f;
    if ((header & 0x30) || type > 3) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        return;
    }
    p++; len--;
    if (!sar) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        if (len) mesh_gatt_proxy_deliver(type, p, len);
    } else if (sar == 1) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        if (!len || len > sizeof(mesh_gatt.proxy_rx)) return;
        memcpy(mesh_gatt.proxy_rx, p, len);
        mesh_gatt.proxy_rx_len = (uint16_t)len;
        mesh_gatt.proxy_rx_type = type;
        mesh_gatt.proxy_rx_active = 1;
        mesh_gatt.proxy_rx_started_ms = GET_MILLIS();
    } else if (!len || !mesh_gatt.proxy_rx_active || type != mesh_gatt.proxy_rx_type ||
               len > sizeof(mesh_gatt.proxy_rx) - mesh_gatt.proxy_rx_len) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
    } else {
        memcpy(mesh_gatt.proxy_rx + mesh_gatt.proxy_rx_len, p, len);
        mesh_gatt.proxy_rx_len += (uint16_t)len;
        if (sar == 3) {
            mesh_gatt_proxy_deliver(type, mesh_gatt.proxy_rx, mesh_gatt.proxy_rx_len);
            mesh_gatt.proxy_rx_active = 0;
            mesh_gatt.proxy_rx_len = 0;
            mesh_gatt.proxy_rx_started_ms = 0;
        }
    }
}

static int mesh_gatt_att_request(const uint8_t *p, size_t len) {
    if (!len) return 0;
    uint8_t op = p[0], out[MESH_GATT_ATT_MTU_MAX];
    size_t n = 0;
    if (op == 0x02) { // Exchange MTU
        if (len != 3 || mesh_gatt_u16(p + 1) < 23)
            return mesh_gatt_error(op, 0, 0x04);
        if (mesh_gatt.mtu_exchanged)
            return mesh_gatt_error(op, 0, 0x06);
        uint16_t peer = mesh_gatt_u16(p + 1);
        mesh_gatt.mtu = peer < MESH_GATT_ATT_MTU_MAX ? peer : MESH_GATT_ATT_MTU_MAX;
        mesh_gatt.mtu_exchanged = 1;
        out[0] = 0x03; out[1] = MESH_GATT_ATT_MTU_MAX;
        out[2] = MESH_GATT_ATT_MTU_MAX >> 8;
        return mesh_gatt_tx_att(out, 3);
    }
    if (op == 0x10) { // Read By Group Type: Primary Service only.
        if (len != 7) return mesh_gatt_error(op, 0, 0x04);
        uint16_t first = mesh_gatt_u16(p + 1), last = mesh_gatt_u16(p + 3);
        if (!first || first > last) return mesh_gatt_error(op, first, 0x01);
        if (mesh_gatt_u16(p + 5) != 0x2800 || first > 1 || last < 1)
            return mesh_gatt_error(op, first, 0x0a);
        const uint8_t rsp[8] = {0x11, 6, 1, 0, 6, 0, 0x28, 0x18};
        return mesh_gatt_tx_att(rsp, sizeof(rsp));
    }
    if (op == 0x06) { // Find By Type Value: Mesh Proxy primary service.
        if (len != 9) return mesh_gatt_error(op, 0, 0x04);
        uint16_t first = mesh_gatt_u16(p + 1), last = mesh_gatt_u16(p + 3);
        if (!first || first > last) return mesh_gatt_error(op, first, 0x01);
        if (mesh_gatt_u16(p + 5) != 0x2800 ||
            mesh_gatt_u16(p + 7) != MESH_GATT_PROXY_SERVICE_UUID ||
            first > 1 || last < 1)
            return mesh_gatt_error(op, first, 0x0a);
        const uint8_t rsp[5] = {0x07, 1, 0, 6, 0};
        return mesh_gatt_tx_att(rsp, sizeof(rsp));
    }
    if (op == 0x08) { // Read By Type: Characteristic declarations only.
        if (len != 7) return mesh_gatt_error(op, 0, 0x04);
        uint16_t first = mesh_gatt_u16(p + 1), last = mesh_gatt_u16(p + 3);
        if (!first || first > last) return mesh_gatt_error(op, first, 0x01);
        out[0] = 0x09; out[1] = 7; n = 2;
        if (mesh_gatt_u16(p + 5) == 0x2803 && first <= 2 && last >= 2) {
            const uint8_t e[7] = {2,0,4,3,0,0xdd,0x2a};
            memcpy(out + n, e, sizeof(e)); n += sizeof(e);
        }
        if (mesh_gatt_u16(p + 5) == 0x2803 && first <= 4 && last >= 4) {
            const uint8_t e[7] = {4,0,0x10,5,0,0xde,0x2a};
            memcpy(out + n, e, sizeof(e)); n += sizeof(e);
        }
        if (n == 2) return mesh_gatt_error(op, first, 0x0a);
        return mesh_gatt_tx_att(out, n);
    }
    if (op == 0x04) { // Find Information
        if (len != 5) return mesh_gatt_error(op, 0, 0x04);
        uint16_t first = mesh_gatt_u16(p + 1), last = mesh_gatt_u16(p + 3);
        if (!first || first > last) return mesh_gatt_error(op, first, 0x01);
        if (first > 6) return mesh_gatt_error(op, first, 0x0a);
        out[0] = 0x05; out[1] = 1; n = 2;
        size_t max_entries = (mesh_gatt.mtu - 2) / 4;
        for (uint16_t h = first; h <= last && h <= 6 && n + 4 <= mesh_gatt.mtu &&
             (n - 2) / 4 < max_entries; h++) {
            uint16_t uuid = h == 1 ? 0x2800 : (h == 2 || h == 4) ? 0x2803 :
                h == 3 ? MESH_GATT_PROXY_DATA_IN_UUID :
                h == 5 ? MESH_GATT_PROXY_DATA_OUT_UUID : 0x2902;
            out[n++] = (uint8_t)h; out[n++] = 0;
            out[n++] = (uint8_t)uuid; out[n++] = (uint8_t)(uuid >> 8);
        }
        if (n == 2) return mesh_gatt_error(op, first, 0x0a);
        return mesh_gatt_tx_att(out, n);
    }
    if (op == 0x0a || op == 0x0c) { // Read / Read Blob.
        size_t request_len = op == 0x0a ? 3 : 5;
        if (len != request_len) return mesh_gatt_error(op, 0, 0x04);
        uint16_t h = mesh_gatt_u16(p + 1);
        uint8_t value[MESH_GATT_ATT_MTU_MAX];
        size_t value_len = 0;
        uint8_t status = mesh_gatt_read_value(h, value, &value_len);
        if (status) return mesh_gatt_error(op, h, status);
        size_t offset = op == 0x0c ? mesh_gatt_u16(p + 3) : 0;
        if (offset > value_len) return mesh_gatt_error(op, h, 0x07);
        out[0] = op == 0x0a ? 0x0b : 0x0d;
        size_t remaining = value_len - offset;
        if (remaining > mesh_gatt.mtu - 1) remaining = mesh_gatt.mtu - 1;
        if (remaining) memcpy(out + 1, value + offset, remaining);
        n = remaining + 1;
        return mesh_gatt_tx_att(out, n);
    }
    if (op == 0x12) { // Write Request: CCCD only.
        if (len < 3) return mesh_gatt_error(op, 0, 0x04);
        uint16_t h = mesh_gatt_u16(p + 1);
        if (h != 6)
            return mesh_gatt_error(op, h, h >= 1 && h <= 5 ? 3 : 1);
        if (len != 5)
            return mesh_gatt_error(op, h, 0x0d);
        if (mesh_gatt_u16(p + 3) & ~1u)
            return mesh_gatt_error(op, h, 0x13);
        mesh_gatt.cccd = p[3] & 1;
        out[0] = 0x13;
        return mesh_gatt_tx_att(out, 1);
    }
    if (op == 0x52) { // Write Command: Mesh Proxy Data In
        if (len >= 3 && mesh_gatt_u16(p + 1) == 3)
            mesh_gatt_proxy_input(p + 3, len - 3);
        return 1;
    }
    if (op & 0x40) return 1;
    return mesh_gatt_error(op, len >= 3 ? mesh_gatt_u16(p + 1) : 0, 6);
}

static void mesh_gatt_receive_fragment(uint8_t llid, const uint8_t *p, size_t n) {
    if (llid == 2) {
        mesh_gatt.rx_active = 0;
        mesh_gatt.l2cap_used = 0;
        if (n < 4 || n > sizeof(mesh_gatt.l2cap_rx)) return;
        memcpy(mesh_gatt.l2cap_rx, p, n);
        mesh_gatt.l2cap_used = (uint16_t)n;
        mesh_gatt.l2cap_expected = (uint16_t)(4 + mesh_gatt_u16(p));
        if (mesh_gatt.l2cap_expected > sizeof(mesh_gatt.l2cap_rx) ||
            mesh_gatt.l2cap_expected < 5 || n > mesh_gatt.l2cap_expected) return;
        mesh_gatt.rx_active = n < mesh_gatt.l2cap_expected;
    } else if (llid == 1 && mesh_gatt.rx_active) {
        if (n > mesh_gatt.l2cap_expected - mesh_gatt.l2cap_used) {
            mesh_gatt.rx_active = 0; return;
        }
        memcpy(mesh_gatt.l2cap_rx + mesh_gatt.l2cap_used, p, n);
        mesh_gatt.l2cap_used += (uint16_t)n;
        mesh_gatt.rx_active = mesh_gatt.l2cap_used < mesh_gatt.l2cap_expected;
    } else return;
    if (mesh_gatt.rx_active || mesh_gatt.l2cap_used != mesh_gatt.l2cap_expected) return;
    if (mesh_gatt_u16(mesh_gatt.l2cap_rx + 2) != MESH_GATT_ATT_CID ||
        mesh_gatt.l2cap_expected - 4 > mesh_gatt.mtu) return;
    mesh_gatt.att_rx_len = mesh_gatt.l2cap_expected - 4;
    memcpy(mesh_gatt.att_rx, mesh_gatt.l2cap_rx + 4, mesh_gatt.att_rx_len);
    mesh_gatt.rx_att_pending = 1;
}

static void mesh_gatt_notify_poll(void) {
    if (!mesh_gatt.cccd || !mesh_gatt.proxy_tx_count || mesh_gatt.tx_active) return;
    uint8_t slot = mesh_gatt.proxy_tx_head;
    uint8_t att[MESH_GATT_ATT_MTU_MAX];
    size_t remaining = mesh_gatt.proxy_tx[slot].len - mesh_gatt.proxy_tx[slot].offset;
    size_t max_payload = mesh_gatt.mtu - 3, chunk;
    uint8_t sar;
    if (!mesh_gatt.proxy_tx[slot].offset && remaining + 1 <= max_payload) {
        sar = 0; chunk = remaining;
    } else if (!mesh_gatt.proxy_tx[slot].offset) {
        sar = 1; chunk = max_payload - 1;
    } else if (remaining + 1 <= max_payload) {
        sar = 3; chunk = remaining;
    } else {
        sar = 2; chunk = max_payload - 1;
    }
    att[0] = 0x1b; att[1] = 5; att[2] = 0;
    att[3] = (uint8_t)((sar << 6) | mesh_gatt.proxy_tx[slot].type);
    memcpy(att + 4, mesh_gatt.proxy_tx[slot].data + mesh_gatt.proxy_tx[slot].offset, chunk);
    if (!mesh_gatt_tx_att(att, chunk + 4)) return;
    if (sar == 1) {
        mesh_gatt.proxy_tx_sar_active = 1;
        mesh_gatt.proxy_tx_started_ms = GET_MILLIS();
    }
    mesh_gatt.proxy_tx[slot].offset += (uint8_t)chunk;
    if (!sar || sar == 3) {
        mesh_gatt.proxy_tx_sar_active = 0;
        mesh_gatt.proxy_tx_started_ms = 0;
        mesh_gatt.proxy_tx_head = (slot + 1) % MESH_GATT_PROXY_QUEUE_SIZE;
        mesh_gatt.proxy_tx_count--;
    }
}

// Call once per application poll. It consumes/reassembles ATT L2CAP data and
// queues ATT responses or one Proxy Data Out notification for the GAP link.
static void mesh_gatt_link_reset(void) {
    mesh_gatt.connected = 0;
    mesh_gatt.cccd = 0;
    mesh_gatt.mtu = 23;
    mesh_gatt.mtu_exchanged = 0;
    mesh_gatt.rx_active = 0;
    mesh_gatt.rx_att_pending = 0;
    mesh_gatt_proxy_sar_cancel();
    mesh_gatt.l2cap_expected = mesh_gatt.l2cap_used = 0;
    mesh_gatt.att_rx_len = 0;
    mesh_gatt.tx_active = 0;
    mesh_gatt.tx_len = mesh_gatt.tx_offset = 0;
    mesh_gatt.filter_type = mesh_gatt.filter_count = 0;
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
}

void mesh_gatt_poll(void) {
    if (!mesh_gap_connected()) {
        if (mesh_gatt.connected) mesh_gatt_link_reset();
        return;
    }
    if (!mesh_gatt.connected) {
        mesh_gatt_link_reset();
        mesh_gatt.connected = 1;
    }
    if (mesh_gatt_proxy_sar_timeout_poll()) return;
    if (mesh_gatt.rx_att_pending && !mesh_gatt.tx_active) {
        mesh_gatt_att_request(mesh_gatt.att_rx, mesh_gatt.att_rx_len);
        mesh_gatt.rx_att_pending = 0;
    }
    if (!mesh_gatt.rx_att_pending) {
        uint8_t llid, fragment[MESH_GAP_CONN_DATA_MAX];
        size_t len = sizeof(fragment);
        if (mesh_gap_receive_data(&llid, fragment, &len) == 1)
            mesh_gatt_receive_fragment(llid, fragment, len);
    }
    if (mesh_gatt.tx_active) {
        mesh_gap_data_length length = mesh_gap_data_length_get();
        size_t chunk = length.tx_octets < 4 ? 27 : length.tx_octets;
        size_t remaining = mesh_gatt.tx_len - mesh_gatt.tx_offset;
        if (chunk > remaining) chunk = remaining;
        if (mesh_gap_send_data(mesh_gatt.tx_offset ? 1 : 2,
                mesh_gatt.tx_l2cap + mesh_gatt.tx_offset, chunk)) {
            mesh_gatt.tx_offset += (uint16_t)chunk;
            if (mesh_gatt.tx_offset == mesh_gatt.tx_len) mesh_gatt.tx_active = 0;
        }
    }
    mesh_gatt_notify_poll();
}

#endif
