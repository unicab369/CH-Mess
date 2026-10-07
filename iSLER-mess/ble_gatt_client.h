#ifndef BLE_GATT_CLIENT_H
#define BLE_GATT_CLIENT_H

// Transport-independent ATT client transaction core. The application supplies
// complete ATT PDUs through ble_gatt_client_receive() and a send callback.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef BLE_GATT_ATT_VALUE_MAX
#define BLE_GATT_ATT_VALUE_MAX 512u
#endif
#ifndef BLE_GATT_CLIENT_MTU_MAX
#define BLE_GATT_CLIENT_MTU_MAX 517
#endif
#ifndef BLE_GATT_CLIENT_TIMEOUT_MS
#define BLE_GATT_CLIENT_TIMEOUT_MS 30000u
#endif
#ifndef BLE_GATT_CLIENT_VALUE_MAX
#define BLE_GATT_CLIENT_VALUE_MAX 512
#endif
#if BLE_GATT_CLIENT_VALUE_MAX > BLE_GATT_ATT_VALUE_MAX
#error "BLE_GATT_CLIENT_VALUE_MAX cannot exceed the ATT attribute value limit"
#endif

typedef int (*ble_gatt_client_send_fn)(void *context,
                                       const uint8_t *pdu, uint16_t len);
typedef int (*ble_gatt_client_sign_fn)(void *context,
    const uint8_t *signed_pdu, uint16_t signed_len, uint8_t signature[12]);
typedef void (*ble_gatt_client_result_fn)(void *context, uint8_t status,
                                          const uint8_t *pdu, uint16_t len);
typedef void (*ble_gatt_client_event_fn)(void *context, uint16_t handle,
                                         const uint8_t *value, uint16_t len);

enum {
    BLE_GATT_CLIENT_IDLE = 0,
    BLE_GATT_CLIENT_PENDING = 1,
    BLE_GATT_CLIENT_TIMEOUT = 2,
    BLE_GATT_CLIENT_REMOTE_ERROR = 3,
    BLE_GATT_CLIENT_PROTOCOL_ERROR = 4,
    BLE_GATT_CLIENT_DISCONNECTED = 5
};

typedef struct {
    uint16_t mtu, local_mtu;
    uint16_t request_len;
    uint8_t request_pdu[BLE_GATT_CLIENT_MTU_MAX];
    uint8_t operation, operation_error[5];
    uint16_t operation_handle, operation_offset, operation_total;
    uint16_t operation_chunk, long_value_len;
    uint32_t now_ms;
    uint8_t long_value[BLE_GATT_CLIENT_VALUE_MAX];
    uint8_t pending, request_opcode, expected_opcode, mtu_exchanged;
    uint8_t bearer_failed;
    uint32_t deadline_ms;
    ble_gatt_client_send_fn send;
    ble_gatt_client_sign_fn sign;
    ble_gatt_client_result_fn result;
    ble_gatt_client_event_fn notification, indication;
    void *context;
} ble_gatt_client;

static inline void ble_gatt_client_init(ble_gatt_client *client,
    ble_gatt_client_send_fn send, ble_gatt_client_result_fn result,
    ble_gatt_client_event_fn notification,
    ble_gatt_client_event_fn indication, void *context) {
    if (!client) return;
    memset(client, 0, sizeof(*client));
    client->mtu = client->local_mtu = 23;
    client->send = send;
    client->result = result;
    client->notification = notification;
    client->indication = indication;
    client->context = context;
}

static inline void ble_gatt_client_set_signer(ble_gatt_client *client,
    ble_gatt_client_sign_fn sign) {
    if (client) client->sign = sign;
}

static inline void ble_gatt_client_reset(ble_gatt_client *client) {
    if (!client) return;
    uint8_t cancelled = client->pending;
    client->mtu = client->local_mtu = 23;
    client->pending = 0;
    client->bearer_failed = 0;
    client->request_opcode = client->expected_opcode = 0;
    client->mtu_exchanged = 0;
    client->request_len = 0;
    client->deadline_ms = 0;
    client->operation = 0;
    client->operation_handle = client->operation_offset = 0;
    client->operation_total = client->operation_chunk = 0;
    client->long_value_len = 0;
    if (cancelled && client->result)
        client->result(client->context, BLE_GATT_CLIENT_DISCONNECTED,
                       NULL, 0);
}

// Begin one ATT request. Responses are delivered intact to `result`; helpers
// for the standard GATT procedures can build on this single-bearer primitive.
static inline int ble_gatt_client_request(ble_gatt_client *client,
    const uint8_t *pdu, uint16_t len, uint8_t expected_opcode,
    uint32_t now_ms) {
    if (!client || client->bearer_failed || !client->send ||
        !client->result || !pdu || !len ||
        len > client->mtu || !expected_opcode || client->pending) return 0;
    if (pdu[0] == 0x02 && client->mtu_exchanged) return 0;
    switch (pdu[0]) {
    case 0x02: if (expected_opcode != 0x03) return 0; break;
    case 0x04: if (expected_opcode != 0x05) return 0; break;
    case 0x06: if (expected_opcode != 0x07) return 0; break;
    case 0x08: if (expected_opcode != 0x09) return 0; break;
    case 0x0a: if (expected_opcode != 0x0b) return 0; break;
    case 0x0c: if (expected_opcode != 0x0d) return 0; break;
    case 0x0e: if (expected_opcode != 0x0f) return 0; break;
    case 0x10: if (expected_opcode != 0x11) return 0; break;
    case 0x12: if (expected_opcode != 0x13) return 0; break;
    case 0x16: if (expected_opcode != 0x17) return 0; break;
    case 0x18: if (expected_opcode != 0x19) return 0; break;
    case 0x20: if (expected_opcode != 0x21) return 0; break;
    default: return 0;
    }
    if (!client->send(client->context, pdu, len)) return 0;
    if (pdu[0] == 0x02) client->mtu_exchanged = 1;
    memcpy(client->request_pdu, pdu, len);
    client->request_len = len;
    client->request_opcode = pdu[0];
    if (pdu[0] == 0x02 && len == 3) {
        uint16_t requested = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;
        client->local_mtu = requested < 23 ? 23 :
            (requested > BLE_GATT_CLIENT_MTU_MAX ?
             BLE_GATT_CLIENT_MTU_MAX : requested);
    }
    client->expected_opcode = expected_opcode;
    client->deadline_ms = now_ms + BLE_GATT_CLIENT_TIMEOUT_MS;
    client->now_ms = now_ms;
    client->pending = 1;
    return 1;
}

static inline uint16_t ble_gatt_client_get_u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static inline uint16_t ble_gatt_client_uuid_assigned16(const uint8_t *uuid,
                                                       uint8_t uuid_len) {
    static const uint8_t base_prefix[14] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00,
        0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00
    };
    if (!uuid) return 0;
    if (uuid_len == 2) return ble_gatt_client_get_u16(uuid);
    if (uuid_len == 16 && !memcmp(uuid, base_prefix, sizeof(base_prefix)))
        return ble_gatt_client_get_u16(uuid + 14);
    return 0;
}

static inline int ble_gatt_client_response_valid(const ble_gatt_client *client,
    const uint8_t *pdu, uint16_t len) {
    if (!client || !pdu || !len || len > client->mtu) return 0;
    switch (pdu[0]) {
    case 0x03:
        return len == 3 && ((uint16_t)pdu[1] | (uint16_t)pdu[2] << 8) >= 23;
    case 0x05:
        if (client->request_len != 5 || client->request_pdu[0] != 0x04 ||
            len < 6 || (pdu[1] != 1 && pdu[1] != 2) ||
            (pdu[1] == 1 ? (len - 2) % 4 != 0 : (len - 2) % 18 != 0))
            return 0;
        {
            uint16_t first = ble_gatt_client_get_u16(client->request_pdu + 1);
            uint16_t last = ble_gatt_client_get_u16(client->request_pdu + 3);
            if (!first || first > last) return 0;
            uint16_t previous = 0;
            uint8_t entry_len = pdu[1] == 1 ? 4 : 18;
            for (uint16_t offset = 2; offset < len; offset += entry_len) {
                uint16_t handle = ble_gatt_client_get_u16(pdu + offset);
                if (!handle || handle < first || handle > last ||
                    handle <= previous) return 0;
                previous = handle;
            }
            return 1;
        }
    case 0x07: {
        if (client->request_len < 7 || client->request_pdu[0] != 0x06 ||
            len < 5 || (len - 1) % 4) return 0;
        uint16_t first = ble_gatt_client_get_u16(client->request_pdu + 1);
        uint16_t last = ble_gatt_client_get_u16(client->request_pdu + 3);
        if (!first || first > last) return 0;
        uint16_t type = ble_gatt_client_get_u16(client->request_pdu + 5);
        uint16_t previous_end = 0;
        for (uint16_t offset = 1; offset < len; offset += 4) {
            uint16_t found = ble_gatt_client_get_u16(pdu + offset);
            uint16_t group_end = ble_gatt_client_get_u16(pdu + offset + 2);
            if (!found || found < first || found > last ||
                group_end < found || found <= previous_end ||
                ((type != 0x2800 && type != 0x2801) && group_end != found))
                return 0;
            previous_end = group_end;
        }
        return 1;
    }
    case 0x09: {
        if ((client->request_len != 7 && client->request_len != 21) ||
            client->request_pdu[0] != 0x08 ||
            len < 4 || pdu[1] < 2 || (len - 2) % pdu[1]) return 0;
        uint16_t first = ble_gatt_client_get_u16(client->request_pdu + 1);
        uint16_t last = ble_gatt_client_get_u16(client->request_pdu + 3);
        uint16_t type = ble_gatt_client_uuid_assigned16(
            client->request_pdu + 5, (uint8_t)(client->request_len - 5));
        if (!first || first > last) return 0;
        // Read By Type's entry size includes the two-byte attribute handle.
        // Characteristic declarations have 5- or 19-byte values; included
        // service declarations carry 4 bytes plus an optional 16-bit UUID.
        if ((type == 0x2803 && pdu[1] != 7 && pdu[1] != 21) ||
            (type == 0x2802 && pdu[1] != 6 && pdu[1] != 8)) return 0;
        uint16_t previous = 0;
        for (uint16_t offset = 2; offset < len; offset += pdu[1]) {
            uint16_t handle = ble_gatt_client_get_u16(pdu + offset);
            if (!handle || handle < first || handle > last ||
                handle <= previous) return 0;
            if (type == 0x2803) {
                uint16_t value_handle = ble_gatt_client_get_u16(pdu + offset + 3);
                // The declaration handle must be in the requested range, but
                // its value handle may follow the range's end when callers
                // query a partial range ending at the declaration itself.
                if (!value_handle || value_handle <= handle)
                    return 0;
            } else if (type == 0x2802) {
                uint16_t service_start = ble_gatt_client_get_u16(pdu + offset + 2);
                uint16_t service_end = ble_gatt_client_get_u16(pdu + offset + 4);
                if (!service_start || service_start > service_end) return 0;
            }
            previous = handle;
        }
        return 1;
    }
    case 0x0b:
        return len - 1 <= BLE_GATT_ATT_VALUE_MAX;
    case 0x0d: {
        if (len - 1 > BLE_GATT_ATT_VALUE_MAX || client->request_len != 5 ||
            client->request_pdu[0] != 0x0c) return 0;
        uint16_t offset = ble_gatt_client_get_u16(client->request_pdu + 3);
        return offset <= BLE_GATT_ATT_VALUE_MAX &&
               (uint32_t)offset + len - 1 <= BLE_GATT_ATT_VALUE_MAX;
    }
    case 0x0f:
        return 1;
    case 0x11: {
        if ((client->request_len != 7 && client->request_len != 21) ||
            client->request_pdu[0] != 0x10 ||
            len < 8 || (pdu[1] != 6 && pdu[1] != 20) ||
            (len - 2) % pdu[1]) return 0;
        uint16_t first = ble_gatt_client_get_u16(client->request_pdu + 1);
        uint16_t last = ble_gatt_client_get_u16(client->request_pdu + 3);
        if (!first || first > last) return 0;
        uint16_t previous_end = 0;
        for (uint16_t offset = 2; offset < len; offset += pdu[1]) {
            uint16_t start = ble_gatt_client_get_u16(pdu + offset);
            uint16_t end = ble_gatt_client_get_u16(pdu + offset + 2);
            if (!start || start < first || start > last || end < start ||
                start <= previous_end) return 0;
            previous_end = end;
        }
        return 1;
    }
    case 0x13: case 0x19:
        return len == 1;
    case 0x17:
        return client->request_len == len && len >= 5 &&
               !memcmp(client->request_pdu + 1, pdu + 1, len - 1);
    case 0x21: {
        uint16_t offset = 1;
        uint16_t tuples = 0;
        if (!client->request_len || client->request_pdu[0] != 0x20 ||
            client->request_len < 5 ||
            ((client->request_len - 1) & 1)) return 0;
        uint16_t requested = (client->request_len - 1) / 2;
        if (len < 3) return 0;
        while (offset < len) {
            if (len - offset < 2) return 0;
            uint16_t value_len = (uint16_t)pdu[offset] |
                                 (uint16_t)pdu[offset + 1] << 8;
            offset += 2;
            if (value_len > BLE_GATT_ATT_VALUE_MAX) return 0;
            if (++tuples > requested) return 0;
            if (value_len > len - offset)
                return len == client->mtu;
            offset += value_len;
        }
        return offset == len && tuples != 0;
    }
    default:
        return 0;
    }
}

static inline int ble_gatt_client_error_handle_valid(
    const ble_gatt_client *client, const uint8_t *pdu) {
    uint16_t error_handle = ble_gatt_client_get_u16(pdu + 2);
    uint8_t request = client->request_opcode;
    if (!error_handle) {
        if (pdu[4] == 0x04 || pdu[4] == 0x06 || request == 0x02 ||
            request == 0x18) return 1;
        switch (request) {
        case 0x0a: case 0x12:
            return client->request_len >= 3 &&
                !ble_gatt_client_get_u16(client->request_pdu + 1);
        case 0x0c: case 0x16:
            return client->request_len >= 5 &&
                !ble_gatt_client_get_u16(client->request_pdu + 1);
        case 0x04: case 0x06: case 0x08: case 0x10:
            return client->request_len >= 5 &&
                !ble_gatt_client_get_u16(client->request_pdu + 1);
        case 0x0e: case 0x20:
            if (client->request_len < 5 ||
                ((client->request_len - 1) & 1)) return 0;
            for (uint16_t offset = 1; offset < client->request_len;
                 offset += 2)
                if (!ble_gatt_client_get_u16(client->request_pdu + offset))
                    return 1;
            return 0;
        default:
            return 0;
        }
    }
    switch (request) {
    case 0x0a: case 0x12:
        return client->request_len >= 3 && error_handle ==
            ble_gatt_client_get_u16(client->request_pdu + 1);
    case 0x0c:
        return client->request_len >= 5 && error_handle ==
            ble_gatt_client_get_u16(client->request_pdu + 1);
    case 0x16:
        return client->request_len >= 5 && error_handle ==
            ble_gatt_client_get_u16(client->request_pdu + 1);
    case 0x0e: case 0x20:
        if (client->request_len < 5 || ((client->request_len - 1) & 1))
            return 0;
        for (uint16_t offset = 1; offset < client->request_len; offset += 2)
            if (error_handle == ble_gatt_client_get_u16(
                    client->request_pdu + offset)) return 1;
        return 0;
    case 0x18:
        // Execute Write has no handle in its request. An application error
        // may identify the queued attribute that failed, whose handle is not
        // retained by this transaction primitive.
        return 1;
    case 0x04: case 0x06: case 0x08: case 0x10:
        if (client->request_len < 5) return 0;
        {
            uint16_t first = ble_gatt_client_get_u16(client->request_pdu + 1);
            uint16_t last = ble_gatt_client_get_u16(client->request_pdu + 3);
            return error_handle == first ||
                (first <= last && error_handle >= first &&
                 error_handle <= last);
        }
    default:
        return 0;
    }
}

static inline void ble_gatt_client_put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static inline int ble_gatt_client_range_valid(uint16_t start, uint16_t end) {
    return start != 0 && start <= end;
}

static inline int ble_gatt_client_exchange_mtu(ble_gatt_client *client,
    uint16_t mtu, uint32_t now_ms) {
    uint8_t pdu[3] = {0x02};
    if (mtu < 23 || mtu > BLE_GATT_CLIENT_MTU_MAX) return 0;
    ble_gatt_client_put_u16(pdu + 1, mtu);
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x03, now_ms);
}

static inline int ble_gatt_client_discover_services(ble_gatt_client *client,
    uint16_t start, uint16_t end, const uint8_t *uuid, uint8_t uuid_len,
    uint32_t now_ms) {
    uint8_t pdu[23];
    if (!ble_gatt_client_range_valid(start, end) ||
        (uuid && uuid_len != 2 && uuid_len != 16) || (!uuid && uuid_len))
        return 0;
    if (!uuid) {
        pdu[0] = 0x10;
        ble_gatt_client_put_u16(pdu + 1, start);
        ble_gatt_client_put_u16(pdu + 3, end);
        ble_gatt_client_put_u16(pdu + 5, 0x2800);
        return ble_gatt_client_request(client, pdu, 7, 0x11, now_ms);
    }
    pdu[0] = 0x06;
    ble_gatt_client_put_u16(pdu + 1, start);
    ble_gatt_client_put_u16(pdu + 3, end);
    ble_gatt_client_put_u16(pdu + 5, 0x2800);
    memcpy(pdu + 7, uuid, uuid_len);
    return ble_gatt_client_request(client, pdu, 7 + uuid_len, 0x07, now_ms);
}

static inline int ble_gatt_client_discover_included_services(
    ble_gatt_client *client, uint16_t start, uint16_t end, uint32_t now_ms) {
    uint8_t pdu[7] = {0x08};
    if (!ble_gatt_client_range_valid(start, end)) return 0;
    ble_gatt_client_put_u16(pdu + 1, start);
    ble_gatt_client_put_u16(pdu + 3, end);
    ble_gatt_client_put_u16(pdu + 5, 0x2802);
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x09, now_ms);
}

static inline int ble_gatt_client_discover_characteristics(
    ble_gatt_client *client, uint16_t start, uint16_t end,
    const uint8_t *uuid, uint8_t uuid_len, uint32_t now_ms) {
    uint8_t pdu[21] = {0x08};
    if (!ble_gatt_client_range_valid(start, end) ||
        (uuid && uuid_len != 2 && uuid_len != 16) || (!uuid && uuid_len))
        return 0;
    ble_gatt_client_put_u16(pdu + 1, start);
    ble_gatt_client_put_u16(pdu + 3, end);
    if (uuid) {
        memcpy(pdu + 5, uuid, uuid_len);
        return ble_gatt_client_request(client, pdu, 5 + uuid_len, 0x09, now_ms);
    }
    ble_gatt_client_put_u16(pdu + 5, 0x2803);
    return ble_gatt_client_request(client, pdu, 7, 0x09, now_ms);
}

static inline int ble_gatt_client_discover_descriptors(ble_gatt_client *client,
    uint16_t start, uint16_t end, uint32_t now_ms) {
    uint8_t pdu[5] = {0x04};
    if (!ble_gatt_client_range_valid(start, end)) return 0;
    ble_gatt_client_put_u16(pdu + 1, start);
    ble_gatt_client_put_u16(pdu + 3, end);
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x05, now_ms);
}

static inline int ble_gatt_client_read(ble_gatt_client *client,
    uint16_t handle, uint32_t now_ms) {
    uint8_t pdu[3] = {0x0a};
    if (!handle) return 0;
    ble_gatt_client_put_u16(pdu + 1, handle);
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x0b, now_ms);
}

// Automatically fetch successive Read Blob chunks. The assembled value is
// available in client->long_value/client->long_value_len at the completion
// callback; the ordinary response callback fires once for the whole read.
static inline int ble_gatt_client_read_long(ble_gatt_client *client,
    uint16_t handle, uint32_t now_ms) {
    if (!client || !handle || client->operation) return 0;
    client->operation = 1;
    client->operation_handle = handle;
    client->operation_offset = client->long_value_len = 0;
    uint8_t pdu[3] = {0x0a};
    ble_gatt_client_put_u16(pdu + 1, handle);
    if (!ble_gatt_client_request(client, pdu, sizeof(pdu), 0x0b, now_ms)) {
        client->operation = 0;
        return 0;
    }
    return 1;
}

static inline int ble_gatt_client_read_blob(ble_gatt_client *client,
    uint16_t handle, uint16_t offset, uint32_t now_ms) {
    uint8_t pdu[5] = {0x0c};
    if (!handle || offset > BLE_GATT_ATT_VALUE_MAX) return 0;
    ble_gatt_client_put_u16(pdu + 1, handle);
    ble_gatt_client_put_u16(pdu + 3, offset);
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x0d, now_ms);
}

static inline int ble_gatt_client_read_by_uuid(ble_gatt_client *client,
    uint16_t start, uint16_t end, const uint8_t *uuid, uint8_t uuid_len,
    uint32_t now_ms) {
    uint8_t pdu[21] = {0x08};
    if (!ble_gatt_client_range_valid(start, end) || !uuid ||
        (uuid_len != 2 && uuid_len != 16)) return 0;
    ble_gatt_client_put_u16(pdu + 1, start);
    ble_gatt_client_put_u16(pdu + 3, end);
    memcpy(pdu + 5, uuid, uuid_len);
    return ble_gatt_client_request(client, pdu, 5 + uuid_len, 0x09, now_ms);
}

static inline int ble_gatt_client_read_multiple(ble_gatt_client *client,
    const uint16_t *handles, uint8_t count, uint8_t variable,
    uint32_t now_ms) {
    uint8_t pdu[1 + 2 * (BLE_GATT_CLIENT_MTU_MAX / 2)];
    if (!handles || count < 2 || count > (client ? (client->mtu - 1) / 2 : 0))
        return 0;
    pdu[0] = variable ? 0x20 : 0x0e;
    for (uint8_t i = 0; i < count; i++) {
        if (!handles[i]) return 0;
        ble_gatt_client_put_u16(pdu + 1 + 2 * i, handles[i]);
    }
    return ble_gatt_client_request(client, pdu, 1 + 2 * count,
                                   variable ? 0x21 : 0x0f, now_ms);
}

static inline int ble_gatt_client_write(ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len, uint32_t now_ms) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || !handle || (!value && len) ||
        len > BLE_GATT_ATT_VALUE_MAX || len > client->mtu - 3)
        return 0;
    pdu[0] = 0x12;
    ble_gatt_client_put_u16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);
    return ble_gatt_client_request(client, pdu, len + 3, 0x13, now_ms);
}

static inline int ble_gatt_client_set_cccd(ble_gatt_client *client,
    uint16_t cccd_handle, uint8_t notifications, uint8_t indications,
    uint32_t now_ms) {
    uint8_t value[2] = {(uint8_t)((notifications ? 1 : 0) |
                                  (indications ? 2 : 0)), 0};
    return ble_gatt_client_write(client, cccd_handle, value, sizeof(value),
                                 now_ms);
}

static inline int ble_gatt_client_write_long(ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len, uint32_t now_ms) {
    if (!client || !handle || (!value && len) ||
        len > BLE_GATT_CLIENT_VALUE_MAX || client->operation) return 0;
    if (len <= client->mtu - 3)
        return ble_gatt_client_write(client, handle, value, len, now_ms);
    memcpy(client->long_value, value, len);
    client->long_value_len = len;
    client->operation = 2;
    client->operation_handle = handle;
    client->operation_offset = 0;
    client->operation_total = len;
    uint16_t chunk = client->mtu - 5;
    if (chunk > len) chunk = len;
    client->operation_chunk = chunk;
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX] = {0x16};
    ble_gatt_client_put_u16(pdu + 1, handle);
    if (chunk) memcpy(pdu + 5, client->long_value, chunk);
    if (!ble_gatt_client_request(client, pdu, chunk + 5, 0x17, now_ms)) {
        client->operation = 0;
        client->operation_total = client->operation_chunk = 0;
        return 0;
    }
    return 1;
}

// Commands do not receive an ATT response and therefore do not occupy the
// request/response transaction slot.
static inline int ble_gatt_client_write_command(ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || client->bearer_failed || !client->send || !handle ||
        (!value && len) || len > BLE_GATT_ATT_VALUE_MAX ||
        len > client->mtu - 3) return 0;
    pdu[0] = 0x52;
    ble_gatt_client_put_u16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);
    return client->send(client->context, pdu, len + 3);
}

// Signed Write is a command (no ATT response). The signer owns the bonded
// CSRK and monotonically increasing sign counter.
static inline int ble_gatt_client_write_signed(ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || client->bearer_failed || !client->send || !client->sign ||
        !handle ||
        (!value && len) || len > client->mtu - 15) return 0;
    pdu[0] = 0xd2;
    ble_gatt_client_put_u16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);
    uint16_t signed_len = len + 3;
    if (!client->sign(client->context, pdu, signed_len, pdu + signed_len))
        return 0;
    return client->send(client->context, pdu, signed_len + 12);
}

static inline int ble_gatt_client_prepare_write(ble_gatt_client *client,
    uint16_t handle, uint16_t offset, const uint8_t *value, uint16_t len,
    uint32_t now_ms) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || !handle || (!value && len) ||
        (uint32_t)offset + len > BLE_GATT_ATT_VALUE_MAX ||
        len > client->mtu - 5)
        return 0;
    pdu[0] = 0x16;
    ble_gatt_client_put_u16(pdu + 1, handle);
    ble_gatt_client_put_u16(pdu + 3, offset);
    if (len) memcpy(pdu + 5, value, len);
    return ble_gatt_client_request(client, pdu, len + 5, 0x17, now_ms);
}

static inline int ble_gatt_client_execute_write(ble_gatt_client *client,
    uint8_t commit, uint32_t now_ms) {
    const uint8_t pdu[2] = {0x18, (uint8_t)(commit != 0)};
    return ble_gatt_client_request(client, pdu, sizeof(pdu), 0x19, now_ms);
}

static inline int ble_gatt_client_write_long_next(ble_gatt_client *client) {
    if (client->operation_offset < client->operation_total) {
        uint16_t remaining = client->operation_total - client->operation_offset;
        uint16_t chunk = client->mtu - 5;
        if (chunk > remaining) chunk = remaining;
        uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX] = {0x16};
        ble_gatt_client_put_u16(pdu + 1, client->operation_handle);
        ble_gatt_client_put_u16(pdu + 3, client->operation_offset);
        memcpy(pdu + 5, client->long_value + client->operation_offset, chunk);
        client->operation_chunk = chunk;
        return ble_gatt_client_request(client, pdu, chunk + 5, 0x17,
                                       client->now_ms);
    }
    const uint8_t execute[2] = {0x18, 1};
    return ble_gatt_client_request(client, execute, sizeof(execute), 0x19,
                                   client->now_ms);
}

// Return 1 when a timeout was reported. ATT requests are not blindly replayed:
// a delayed response or non-idempotent write could otherwise be misapplied.
// The bearer is poisoned at timeout and must be replaced before more traffic.
static inline int ble_gatt_client_poll(ble_gatt_client *client,
                                      uint32_t now_ms) {
    if (!client || !client->pending ||
        (int32_t)(now_ms - client->deadline_ms) < 0) return 0;
    client->pending = 0;
    client->request_opcode = client->expected_opcode = 0;
    client->request_len = 0;
    client->operation = 0;
    client->bearer_failed = 1;
    if (client->result)
        client->result(client->context, BLE_GATT_CLIENT_TIMEOUT, NULL, 0);
    return 1;
}

static inline int ble_gatt_client_receive(ble_gatt_client *client,
    const uint8_t *pdu, uint16_t len) {
    if (!client || client->bearer_failed || !pdu || !len) return -1;
    uint8_t opcode = pdu[0];
    if ((opcode == 0x1b || opcode == 0x1d) &&
        (len < 3 || len > client->mtu)) return -1;
    if (opcode == 0x1b || opcode == 0x1d) {
        uint16_t handle = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;
        if (!handle || len - 3 > BLE_GATT_ATT_VALUE_MAX) {
            if (opcode == 0x1d) {
                const uint8_t confirmation = 0x1e;
                if (!client->send || !client->send(client->context,
                                                   &confirmation, 1)) return -1;
            }
            return 1;
        }
        ble_gatt_client_event_fn event = opcode == 0x1b ?
            client->notification : client->indication;
        if (event) event(client->context, handle, pdu + 3, len - 3);
        if (opcode == 0x1d) {
            const uint8_t confirmation = 0x1e;
            if (!client->send || !client->send(client->context,
                                               &confirmation, 1)) return -1;
        }
        return 1;
    }
    if (opcode == 0x23) { // Multiple Handle Value Notification
        if (len < 9 || len > client->mtu) return -1;
        uint16_t offset = 1, tuple_count = 0;
        while (offset < len) {
            if (len - offset < 4) return -1;
            uint16_t value_len = ble_gatt_client_get_u16(pdu + offset + 2);
            offset += 4;
            if (value_len > len - offset) return -1;
            offset += value_len;
            tuple_count++;
        }
        if (offset != len || tuple_count < 2) return -1;
        if (client->notification) {
            offset = 1;
            while (offset < len) {
                uint16_t handle = ble_gatt_client_get_u16(pdu + offset);
                uint16_t value_len = ble_gatt_client_get_u16(pdu + offset + 2);
                offset += 4;
                if (handle && value_len <= BLE_GATT_ATT_VALUE_MAX)
                    client->notification(client->context, handle,
                                         pdu + offset, value_len);
                offset += value_len;
            }
        }
        return 1;
    }
    if (!client->pending) {
        switch (opcode) {
        case 0x01: case 0x03: case 0x05: case 0x07: case 0x09:
        case 0x0b: case 0x0d: case 0x0f: case 0x11: case 0x13:
        case 0x17: case 0x19: case 0x21:
            return -1; // A response without an outstanding request is invalid.
        default:
            return 0;
        }
    }

    if (opcode == 0x01) {
        if (len != 5 || pdu[1] != client->request_opcode || !pdu[4]) {
            client->pending = 0;
            client->request_opcode = client->expected_opcode = 0;
            client->request_len = 0;
            client->operation = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }
        if (!ble_gatt_client_error_handle_valid(client, pdu)) {
            client->pending = 0;
            client->request_opcode = client->expected_opcode = 0;
            client->request_len = 0;
            client->operation = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }
        uint8_t request_opcode = client->request_opcode;
        const uint8_t *reported_error = pdu;
        uint16_t reported_len = len;
        client->pending = 0;
        client->request_opcode = client->expected_opcode = 0;
        client->request_len = 0;
        if (client->operation == 1 && request_opcode == 0x0c &&
            (pdu[4] == 0x07 || pdu[4] == 0x0b)) {
            client->operation = 0;
            client->result(client->context, 0, pdu, len);
            return 1;
        }
        if (client->operation == 2 && request_opcode == 0x16) {
            memcpy(client->operation_error, pdu, 5);
            client->operation = 3;
            const uint8_t cancel[2] = {0x18, 0};
            if (ble_gatt_client_request(client, cancel, sizeof(cancel),
                                        0x19, client->now_ms)) return 1;
            client->operation = 0;
        } else if (client->operation == 3) {
            reported_error = client->operation_error;
            reported_len = 5;
            client->operation = 0;
        } else {
            client->operation = 0;
        }
        client->result(client->context, BLE_GATT_CLIENT_REMOTE_ERROR,
                       reported_error, reported_len);
        return 1;
    }
    if (opcode != client->expected_opcode) {
        client->pending = 0;
        client->request_opcode = client->expected_opcode = 0;
        client->request_len = 0;
        client->operation = 0;
        client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                       pdu, len);
        return -1;
    }
    uint8_t operation = client->operation;
    client->pending = 0;
    client->request_opcode = client->expected_opcode = 0;
    if (!ble_gatt_client_response_valid(client, pdu, len)) {
        client->request_len = 0;
        client->operation = 0;
        client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                       pdu, len);
        return -1;
    }
    client->request_len = 0;
    if (opcode == 0x03) {
        uint16_t peer_mtu = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;
        if (peer_mtu < 23) {
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }
        client->mtu = peer_mtu < client->local_mtu ? peer_mtu :
                      client->local_mtu;
    }
    if (operation == 1 && (opcode == 0x0b || opcode == 0x0d)) {
        uint16_t part_len = len - 1;
        if (part_len > BLE_GATT_CLIENT_VALUE_MAX - client->long_value_len) {
            client->operation = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }
        if (part_len) memcpy(client->long_value + client->long_value_len,
                             pdu + 1, part_len);
        client->long_value_len += part_len;
        if (part_len == client->mtu - 1 &&
            client->long_value_len < BLE_GATT_CLIENT_VALUE_MAX) {
            uint8_t blob[5] = {0x0c};
            ble_gatt_client_put_u16(blob + 1, client->operation_handle);
            ble_gatt_client_put_u16(blob + 3, client->long_value_len);
            if (!ble_gatt_client_request(client, blob, sizeof(blob), 0x0d,
                                         client->now_ms)) {
                client->operation = 0;
                client->result(client->context,
                    BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
                return -1;
            }
            return 1;
        }
        client->operation = 0;
        client->result(client->context, 0, pdu, len);
        return 1;
    }
    if (operation == 2 && opcode == 0x17) {
        client->operation_offset += client->operation_chunk;
        if (!ble_gatt_client_write_long_next(client)) {
            client->operation = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }
        return 1;
    }
    if (operation == 2 && opcode == 0x19) {
        client->operation = 0;
        client->operation_total = client->operation_chunk = 0;
        client->result(client->context, 0, pdu, len);
        return 1;
    }
    if (operation == 3 && opcode == 0x19) {
        client->operation = 0;
        client->result(client->context, BLE_GATT_CLIENT_REMOTE_ERROR,
                       client->operation_error, 5);
        return 1;
    }
    client->result(client->context, 0, pdu, len);
    return 1;
}

#endif
