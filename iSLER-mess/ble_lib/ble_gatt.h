#ifndef BLE_GATT_H
#define BLE_GATT_H

// Generic GATT client, server, and transport API. Mesh service definitions
// live in ../ble_mesh/mesh_gatt.h.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_att.h"

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
    // ATT bearer configuration and health.
    uint16_t mtu;
    uint16_t local_mtu;
    uint8_t mtu_exchanged;
    uint8_t bearer_failed;

    // Outstanding request and timeout tracking.
    struct {
        uint8_t pending;
        uint8_t opcode;
        uint8_t expected_opcode;
        uint16_t len;
    } request;
    uint8_t request_pdu[BLE_GATT_CLIENT_MTU_MAX];
    uint32_t deadline_ms;
    uint32_t now_ms;

    // Long read/write operation state and accumulated value.
    struct {
        uint8_t kind;
        uint16_t handle;
        uint16_t offset;
        uint16_t total;
        uint16_t chunk;
        uint8_t error[5];
        uint16_t value_len;
        uint8_t value[BLE_GATT_CLIENT_VALUE_MAX];
    } operation;

    // Application callbacks.
    ble_gatt_client_send_fn send;
    ble_gatt_client_sign_fn sign;
    ble_gatt_client_result_fn result;
    ble_gatt_client_event_fn notification;
    ble_gatt_client_event_fn indication;
    void *context;
} ble_gatt_client;

// Application facing
// =============================================================================
// Transaction state and response validation
// =============================================================================
static inline void gatt_client_init(
    ble_gatt_client *client,
    ble_gatt_client_send_fn send, ble_gatt_client_result_fn result,
    ble_gatt_client_event_fn notification,
    ble_gatt_client_event_fn indication, void *context
) {
    if (!client) return;
    memset(client, 0, sizeof(*client));
    client->mtu = client->local_mtu = 23;
    client->send = send;
    client->result = result;
    client->notification = notification;
    client->indication = indication;
    client->context = context;
}

// Application facing
static inline void gatt_client_set_signer(
    ble_gatt_client *client,
    ble_gatt_client_sign_fn sign
) {
    if (client) client->sign = sign;
}

// Application facing
static inline void gatt_client_reset(ble_gatt_client *client) {
    if (!client) return;
    client->mtu = client->local_mtu = 23;
    client->bearer_failed = 0;
    client->mtu_exchanged = 0;
    client->request.opcode = 0;
    client->request.expected_opcode = 0;
    client->request.len = 0;
    memset(&client->operation, 0, sizeof(client->operation));

    if (client->request.pending) {
        client->request.pending = 0;
        if (client->result)
            client->result(client->context, BLE_GATT_CLIENT_DISCONNECTED, NULL, 0);
    }
}

// Begin one ATT request. Responses are delivered intact to `result`; helpers
// for the standard GATT procedures can build on this single-bearer primitive.
// Internal: applications should use the standard procedure builders below.
static inline int gatt_client_request(
    ble_gatt_client *client,
    const uint8_t *pdu, uint16_t len, uint8_t expected_opcode,
    uint32_t now_ms
) {
    if (!client || client->bearer_failed || !client->send ||
        !client->result || !pdu || !len ||
        len > client->mtu || !expected_opcode || client->request.pending
    ) return 0;

    if (pdu[0] == ATT_OP_EXCHANGE_MTU_REQ && client->mtu_exchanged)
        return 0;
    if (ble_att_response_opcode(pdu[0]) != expected_opcode)
        return 0;
    if (!client->send(client->context, pdu, len))
        return 0;
    if (pdu[0] == ATT_OP_EXCHANGE_MTU_REQ) client->mtu_exchanged = 1;

    memcpy(client->request_pdu, pdu, len);
    client->request.len = len;
    client->request.opcode = pdu[0];

    if (pdu[0] == ATT_OP_EXCHANGE_MTU_REQ && len == 3) {
        uint16_t requested = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;
        client->local_mtu = requested < 23 ? 23 :
                        (requested > BLE_GATT_CLIENT_MTU_MAX ?
                        BLE_GATT_CLIENT_MTU_MAX : requested);
    }

    client->request.expected_opcode = expected_opcode;
    client->deadline_ms = now_ms + BLE_GATT_CLIENT_TIMEOUT_MS;
    client->now_ms = now_ms;
    client->request.pending = 1;
    return 1;
}

// Application facing
// =============================================================================
// Standard GATT request builders
// =============================================================================
static inline int gatt_client_mtu(
    ble_gatt_client *client,
    uint16_t mtu, uint32_t now_ms
) {
    uint8_t pdu[3] = {ATT_OP_EXCHANGE_MTU_REQ};
    if (mtu < 23 || mtu > BLE_GATT_CLIENT_MTU_MAX) return 0;
    ble_put_le16(pdu + 1, mtu);
    return gatt_client_request(client, pdu, sizeof(pdu),
                                ATT_OP_EXCHANGE_MTU_RSP, now_ms);
}

// Application facing
static inline int gatt_client_services(
    ble_gatt_client *client,
    uint16_t start, uint16_t end,
    const uint8_t *uuid, uint8_t uuid_len, uint32_t now_ms
) {
    uint8_t pdu[23];
    if (!start || start > end ||
        (!uuid && uuid_len) ||
        (uuid && uuid_len != 2 && uuid_len != 16)
    ) return 0;

    if (!uuid) {
        pdu[0] = ATT_OP_READ_BY_GROUP_TYPE_REQ;
        ble_put_le16(pdu + 1, start);
        ble_put_le16(pdu + 3, end);
        ble_put_le16(pdu + 5, 0x2800);
        return gatt_client_request(client, pdu, 7,
                                   ATT_OP_READ_BY_GROUP_TYPE_RSP, now_ms);
    }

    pdu[0] = ATT_OP_FIND_BY_TYPE_VALUE_REQ;
    ble_put_le16(pdu + 1, start);
    ble_put_le16(pdu + 3, end);
    ble_put_le16(pdu + 5, 0x2800);
    memcpy(pdu + 7, uuid, uuid_len);
    return gatt_client_request(client, pdu, 7 + uuid_len,
                               ATT_OP_FIND_BY_TYPE_VALUE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_included(
    ble_gatt_client *client, uint16_t start, uint16_t end, uint32_t now_ms
) {
    uint8_t pdu[7] = {ATT_OP_READ_BY_TYPE_REQ};
    if (!start || start > end) return 0;
    ble_put_le16(pdu + 1, start);
    ble_put_le16(pdu + 3, end);
    ble_put_le16(pdu + 5, 0x2802);
    return gatt_client_request(client, pdu, sizeof(pdu),
                               ATT_OP_READ_BY_TYPE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_characteristics(
    ble_gatt_client *client, uint16_t start, uint16_t end,
    const uint8_t *uuid, uint8_t uuid_len, uint32_t now_ms
) {
    uint8_t pdu[21] = {ATT_OP_READ_BY_TYPE_REQ};
    if (!start || start > end ||
        (uuid && uuid_len != 2 && uuid_len != 16) || (!uuid && uuid_len)
    ) return 0;

    ble_put_le16(pdu + 1, start);
    ble_put_le16(pdu + 3, end);

    if (uuid) {
        memcpy(pdu + 5, uuid, uuid_len);
        return gatt_client_request(client, pdu, 5 + uuid_len,
                                   ATT_OP_READ_BY_TYPE_RSP, now_ms);
    }

    ble_put_le16(pdu + 5, 0x2803);
    return gatt_client_request(client, pdu, 7,
                               ATT_OP_READ_BY_TYPE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_discover_descriptors(
    ble_gatt_client *client,
    uint16_t start, uint16_t end, uint32_t now_ms
) {
    uint8_t pdu[5] = {ATT_OP_FIND_INFO_REQ};
    if (!start || start > end) return 0;
    ble_put_le16(pdu + 1, start);
    ble_put_le16(pdu + 3, end);
    return gatt_client_request(client, pdu, sizeof(pdu),
                               ATT_OP_FIND_INFO_RSP, now_ms);
}

// Application facing
static inline int gatt_client_read(
    ble_gatt_client *client, uint16_t handle, uint32_t now_ms
) {
    uint8_t pdu[3] = {ATT_OP_READ_REQ};
    if (!handle) return 0;
    ble_put_le16(pdu + 1, handle);
    return gatt_client_request(client, pdu, sizeof(pdu),
                               ATT_OP_READ_RSP, now_ms);
}

// Application facing
// Automatically fetch successive Read Blob chunks. The assembled value is
// available in client->operation.value/client->operation.value_len at the completion
// callback; the ordinary response callback fires once for the whole read.
static inline int gatt_client_read_long(
    ble_gatt_client *client,
    uint16_t handle, uint32_t now_ms
) {
    if (!client || !handle || client->operation.kind)
        return 0;
    client->operation.kind = 1;
    client->operation.handle = handle;
    client->operation.offset = client->operation.value_len = 0;

    uint8_t pdu[3] = {ATT_OP_READ_REQ};
    ble_put_le16(pdu + 1, handle);

    if (!gatt_client_request(client, pdu, sizeof(pdu), ATT_OP_READ_RSP, now_ms)) {
        client->operation.kind = 0;
        return 0;
    }
    return 1;
}

// Application facing
static inline int gatt_client_read_blob(
    ble_gatt_client *client, uint16_t handle,
    uint16_t offset, uint32_t now_ms
) {
    uint8_t pdu[5] = {ATT_OP_READ_BLOB_REQ};
    if (!handle || offset > BLE_GATT_ATT_VALUE_MAX) return 0;
    ble_put_le16(pdu + 1, handle);
    ble_put_le16(pdu + 3, offset);
    return gatt_client_request(client, pdu, sizeof(pdu),
                               ATT_OP_READ_BLOB_RSP, now_ms);
}

// Application facing
static inline int gatt_client_read_by_uuid(
    ble_gatt_client *client,
    uint16_t start, uint16_t end,
    const uint8_t *uuid, uint8_t uuid_len, uint32_t now_ms
) {
    uint8_t pdu[21] = {ATT_OP_READ_BY_TYPE_REQ};
    if (!start || start > end || !uuid ||
        (uuid_len != 2 && uuid_len != 16)
    ) return 0;

    ble_put_le16(pdu + 1, start);
    ble_put_le16(pdu + 3, end);
    memcpy(pdu + 5, uuid, uuid_len);
    return gatt_client_request(client, pdu, 5 + uuid_len,
                               ATT_OP_READ_BY_TYPE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_read_multiple(
    ble_gatt_client *client,
    const uint16_t *handles, uint8_t count, uint8_t variable,
    uint32_t now_ms
) {
    uint8_t pdu[1 + 2 * (BLE_GATT_CLIENT_MTU_MAX / 2)];
    if (!handles || count < 2 || count > (client ? (client->mtu - 1) / 2 : 0))
        return 0;
    pdu[0] = variable ? ATT_OP_READ_MULTIPLE_VARIABLE_REQ : ATT_OP_READ_MULTIPLE_REQ;

    for (uint8_t i = 0; i < count; i++) {
        if (!handles[i]) return 0;
        ble_put_le16(pdu + 1 + 2 * i, handles[i]);
    }
    return gatt_client_request(client, pdu, 1 + 2 * count,
            variable ? ATT_OP_READ_MULT_VAR_RSP : ATT_OP_READ_MULTIPLE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_write(
    ble_gatt_client *client,
    uint16_t handle, const uint8_t *value,
    uint16_t len, uint32_t now_ms
) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || !handle || (!value && len) ||
        len > BLE_GATT_ATT_VALUE_MAX || len > client->mtu - 3
    ) return 0;

    pdu[0] = ATT_OP_WRITE_REQ;
    ble_put_le16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);
    return gatt_client_request(client, pdu, len + 3, ATT_OP_WRITE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_set_cccd(
    ble_gatt_client *client,
    uint16_t cccd_handle, uint8_t notifications,
    uint8_t indications, uint32_t now_ms
) {
    uint8_t value[2] = {(uint8_t)((notifications ? 1 : 0) |
                                  (indications ? 2 : 0)), 0};
    return gatt_client_write(client, cccd_handle, value, sizeof(value), now_ms);
}

// Application facing
static inline int gatt_client_write_long(
    ble_gatt_client *client,
    uint16_t handle, const uint8_t *value,
    uint16_t len, uint32_t now_ms
) {
    if (!client || !handle || (!value && len) ||
        len > BLE_GATT_CLIENT_VALUE_MAX || client->operation.kind
    ) return 0;

    if (len <= client->mtu - 3)
        return gatt_client_write(client, handle, value, len, now_ms);

    memcpy(client->operation.value, value, len);
    client->operation.value_len = len;
    client->operation.kind = 2;
    client->operation.handle = handle;
    client->operation.offset = 0;
    client->operation.total = len;
    uint16_t chunk = client->mtu - 5;
    if (chunk > len) chunk = len;
    client->operation.chunk = chunk;

    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX] = {ATT_OP_PREPARE_WRITE_REQ};
    ble_put_le16(pdu + 1, handle);

    if (chunk) memcpy(pdu + 5, client->operation.value, chunk);

    if (!gatt_client_request(client, pdu, chunk + 5,
                             ATT_OP_PREPARE_WRITE_RSP, now_ms)
    ) {
        client->operation.kind = 0;
        client->operation.total = client->operation.chunk = 0;
        return 0;
    }

    return 1;
}

// Application facing
// Commands do not receive an ATT response and therefore do not occupy the
// request/response transaction slot.
static inline int gatt_client_write_command(
    ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len
) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || client->bearer_failed ||
        !client->send || !handle ||
        (!value && len) || len > BLE_GATT_ATT_VALUE_MAX ||
        len > client->mtu - 3
    ) return 0;
    pdu[0] = ATT_OP_WRITE_COMMAND;

    ble_put_le16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);
    return client->send(client->context, pdu, len + 3);
}

// Application facing
// Signed Write is a command (no ATT response). The signer owns the bonded
// CSRK and monotonically increasing sign counter.
static inline int gatt_client_write_signed(
    ble_gatt_client *client,
    uint16_t handle, const uint8_t *value, uint16_t len
) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || client->bearer_failed ||
        !client->send || !client->sign ||
        !handle ||
        (!value && len) || len > client->mtu - 15
    ) return 0;

    pdu[0] = ATT_OP_SIGNED_WRITE_COMMAND;
    ble_put_le16(pdu + 1, handle);
    if (len) memcpy(pdu + 3, value, len);

    uint16_t signed_len = len + 3;
    if (!client->sign(client->context, pdu, signed_len, pdu + signed_len))
        return 0;
    return client->send(client->context, pdu, signed_len + 12);
}

// Application facing
static inline int gatt_client_prepare_write(
    ble_gatt_client *client,
    uint16_t handle, uint16_t offset,
    const uint8_t *value, uint16_t len, uint32_t now_ms
) {
    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX];
    if (!client || !handle || (!value && len) ||
        (uint32_t)offset + len > BLE_GATT_ATT_VALUE_MAX ||
        len > client->mtu - 5
    ) return 0;

    pdu[0] = ATT_OP_PREPARE_WRITE_REQ;
    ble_put_le16(pdu + 1, handle);
    ble_put_le16(pdu + 3, offset);
    if (len) memcpy(pdu + 5, value, len);
    return gatt_client_request(client, pdu, len + 5,
                               ATT_OP_PREPARE_WRITE_RSP, now_ms);
}

// Application facing
static inline int gatt_client_execute_write(
    ble_gatt_client *client, uint8_t commit, uint32_t now_ms
) {
    const uint8_t pdu[2] = { ATT_OP_EXECUTE_WRITE_REQ, (uint8_t)(commit != 0) };
    return gatt_client_request(client, pdu, sizeof(pdu),
                               ATT_OP_EXECUTE_WRITE_RSP, now_ms);
}

// Internal transport integration: pass each complete ATT PDU received on
// this bearer.
static inline int gatt_client_receive(
    ble_gatt_client *client, const uint8_t *pdu, uint16_t len
) {
    if (!client || client->bearer_failed || !pdu || !len) return -1;
    uint8_t opcode = pdu[0];
    if ((opcode == ATT_OP_HANDLE_VALUE_NOTIFICATION ||
         opcode == ATT_OP_HANDLE_VALUE_INDICATION) &&
        (len < 3 || len > client->mtu)
    ) return -1;

    if (opcode == ATT_OP_HANDLE_VALUE_NOTIFICATION ||
        opcode == ATT_OP_HANDLE_VALUE_INDICATION) {
        uint16_t handle = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;

        if (!handle || len - 3 > BLE_GATT_ATT_VALUE_MAX) {
            if (opcode == ATT_OP_HANDLE_VALUE_INDICATION) {
                const uint8_t confirmation = ATT_OP_HANDLE_VALUE_CONFIRMATION;
                if (!client->send || !client->send(client->context, &confirmation, 1))
                    return -1;
            }
            return 1;
        }

        ble_gatt_client_event_fn event = opcode == ATT_OP_HANDLE_VALUE_NOTIFICATION ?
                                client->notification : client->indication;
        if (event) event(client->context, handle, pdu + 3, len - 3);

        if (opcode == ATT_OP_HANDLE_VALUE_INDICATION) {
            const uint8_t confirmation = ATT_OP_HANDLE_VALUE_CONFIRMATION;
            if (!client->send || !client->send(client->context, &confirmation, 1))
                return -1;
        }

        return 1;
    }

    if (opcode == ATT_OP_MULT_HANDLE_VALUE_NOTIFICATION) {
        if (len < 9 || len > client->mtu) return -1;
        uint16_t offset = 1, tuple_count = 0;

        while (offset < len) {
            if (len - offset < 4) return -1;
            uint16_t value_len = ble_get_le16(pdu + offset + 2);
            offset += 4;
            if (value_len > len - offset) return -1;
            offset += value_len;
            tuple_count++;
        }

        if (offset != len || tuple_count < 2) return -1;

        if (client->notification) {
            offset = 1;

            while (offset < len) {
                uint16_t handle = ble_get_le16(pdu + offset);
                uint16_t value_len = ble_get_le16(pdu + offset + 2);
                offset += 4;
                if (handle && value_len <= BLE_GATT_ATT_VALUE_MAX)
                    client->notification(client->context, handle,
                                         pdu + offset, value_len);
                offset += value_len;
            }
        }

        return 1;
    }

    if (!client->request.pending) {
        switch (opcode) {
        case ATT_OP_ERROR_RSP: case ATT_OP_EXCHANGE_MTU_RSP:
        case ATT_OP_FIND_INFO_RSP: case ATT_OP_FIND_BY_TYPE_VALUE_RSP:
        case ATT_OP_READ_BY_TYPE_RSP: case ATT_OP_READ_RSP:
        case ATT_OP_READ_BLOB_RSP: case ATT_OP_READ_MULTIPLE_RSP:
        case ATT_OP_READ_BY_GROUP_TYPE_RSP: case ATT_OP_WRITE_RSP:
        case ATT_OP_PREPARE_WRITE_RSP:
        case ATT_OP_EXECUTE_WRITE_RSP:
        case ATT_OP_READ_MULT_VAR_RSP:
            return -1; // A response without an outstanding request is invalid.
        default:
            return 0;
        }
    }

    if (opcode == ATT_OP_ERROR_RSP) {
        if (len != 5 || pdu[1] != client->request.opcode || !pdu[4]) {
            memset(&client->request, 0, sizeof(client->request));
            client->operation.kind = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }

        uint16_t error_handle = ble_get_le16(pdu + 2);
        uint8_t request = client->request.opcode;
        uint8_t error_handle_valid = 0;

        if (!error_handle) {
            if (pdu[4] == ATT_OP_FIND_INFO_REQ ||
                pdu[4] == ATT_OP_FIND_BY_TYPE_VALUE_REQ ||
                request == ATT_OP_EXCHANGE_MTU_REQ ||
                request == ATT_OP_EXECUTE_WRITE_REQ) {
                error_handle_valid = 1;
            } else {
                switch (request) {
                case ATT_OP_READ_REQ: case ATT_OP_WRITE_REQ:
                    error_handle_valid = client->request.len >= 3 &&
                        !ble_get_le16(client->request_pdu + 1);
                    break;
                case ATT_OP_READ_BLOB_REQ: case ATT_OP_PREPARE_WRITE_REQ:
                case ATT_OP_FIND_INFO_REQ: case ATT_OP_FIND_BY_TYPE_VALUE_REQ:
                case ATT_OP_READ_BY_TYPE_REQ:
                case ATT_OP_READ_BY_GROUP_TYPE_REQ:
                    error_handle_valid = client->request.len >= 5 &&
                        !ble_get_le16(client->request_pdu + 1);
                    break;
                case ATT_OP_READ_MULTIPLE_REQ:
                case ATT_OP_READ_MULTIPLE_VARIABLE_REQ:
                    if (client->request.len >= 5 &&
                        !((client->request.len - 1) & 1)) {
                        for (uint16_t offset = 1;
                             offset < client->request.len; offset += 2) {
                            if (!ble_get_le16(client->request_pdu + offset)) {
                                error_handle_valid = 1;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        } else {
            switch (request) {
            case ATT_OP_READ_REQ: case ATT_OP_WRITE_REQ:
                error_handle_valid = client->request.len >= 3 &&
                    error_handle == ble_get_le16(client->request_pdu + 1);
                break;
            case ATT_OP_READ_BLOB_REQ: case ATT_OP_PREPARE_WRITE_REQ:
                error_handle_valid = client->request.len >= 5 &&
                    error_handle == ble_get_le16(client->request_pdu + 1);
                break;
            case ATT_OP_READ_MULTIPLE_REQ:
            case ATT_OP_READ_MULTIPLE_VARIABLE_REQ:
                if (client->request.len >= 5 &&
                    !((client->request.len - 1) & 1)) {
                    for (uint16_t offset = 1;
                         offset < client->request.len; offset += 2) {
                        if (error_handle == ble_get_le16(
                                client->request_pdu + offset)) {
                            error_handle_valid = 1;
                            break;
                        }
                    }
                }
                break;
            case ATT_OP_EXECUTE_WRITE_REQ:
                // The execute-write request has no handle for comparison.
                error_handle_valid = 1;
                break;
            case ATT_OP_FIND_INFO_REQ:
            case ATT_OP_FIND_BY_TYPE_VALUE_REQ:
            case ATT_OP_READ_BY_TYPE_REQ:
            case ATT_OP_READ_BY_GROUP_TYPE_REQ: {
                if (client->request.len >= 5) {
                    uint16_t first = ble_get_le16(client->request_pdu + 1);
                    uint16_t last = ble_get_le16(client->request_pdu + 3);
                    error_handle_valid = error_handle == first ||
                        (first <= last && error_handle >= first &&
                         error_handle <= last);
                }
                break;
            }
            }
        }

        if (!error_handle_valid) {
            memset(&client->request, 0, sizeof(client->request));
            client->operation.kind = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR,
                           pdu, len);
            return -1;
        }

        uint8_t request_opcode = client->request.opcode;
        const uint8_t *reported_error = pdu;
        uint16_t reported_len = len;
        memset(&client->request, 0, sizeof(client->request));

        if (client->operation.kind == 1 &&
            request_opcode == ATT_OP_READ_BLOB_REQ &&
            (pdu[4] == BLE_ATT_ERR_INVALID_OFFSET ||
             pdu[4] == BLE_ATT_ERR_ATTRIBUTE_NOT_LONG)
        ) {
            client->operation.kind = 0;
            client->result(client->context, 0, pdu, len);
            return 1;
        }

        if (client->operation.kind == 2 &&
            request_opcode == ATT_OP_PREPARE_WRITE_REQ) {
            memcpy(client->operation.error, pdu, 5);
            client->operation.kind = 3;
            const uint8_t cancel[2] = {ATT_OP_EXECUTE_WRITE_REQ, 0};
            if (gatt_client_request(client, cancel, sizeof(cancel),
                                        ATT_OP_EXECUTE_WRITE_RSP,
                                        client->now_ms)
            ) return 1;

            client->operation.kind = 0;
        }
        else if (client->operation.kind == 3) {
            reported_error = client->operation.error;
            reported_len = 5;
            client->operation.kind = 0;
        }
        else {
            client->operation.kind = 0;
        }

        client->result(client->context, BLE_GATT_CLIENT_REMOTE_ERROR,
                       reported_error, reported_len);
        return 1;
    }

    if (opcode != client->request.expected_opcode) {
        memset(&client->request, 0, sizeof(client->request));
        client->operation.kind = 0;
        client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
        return -1;
    }

    uint8_t response_valid = 0;
    if (client && pdu && len && len <= client->mtu) {
        switch (pdu[0]) {
        case ATT_OP_EXCHANGE_MTU_RSP:
            response_valid = len == 3;
            break;
        case ATT_OP_FIND_INFO_RSP:
            response_valid = client->request.len == 5 &&
                client->request_pdu[0] == ATT_OP_FIND_INFO_REQ && len >= 6 &&
                (pdu[1] == 1 || pdu[1] == 2) &&
                (pdu[1] == 1 ? (len - 2) % 4 == 0 : (len - 2) % 18 == 0);

            if (response_valid) {
                uint16_t first = ble_get_le16(client->request_pdu + 1);
                uint16_t last = ble_get_le16(client->request_pdu + 3);
                uint16_t previous = 0;
                uint8_t entry_len = pdu[1] == 1 ? 4 : 18;
                response_valid = first && first <= last;

                for (uint16_t offset = 2;
                     response_valid && offset < len; offset += entry_len
                ) {
                    uint16_t handle = ble_get_le16(pdu + offset);
                    if (!handle || handle < first || handle > last || handle <= previous)
                        response_valid = 0;
                    previous = handle;
                }
            }
            break;
        case ATT_OP_FIND_BY_TYPE_VALUE_RSP: {
            response_valid = client->request.len >= 7 &&
                client->request_pdu[0] == ATT_OP_FIND_BY_TYPE_VALUE_REQ &&
                len >= 5 && (len - 1) % 4 == 0;

            if (response_valid) {
                uint16_t first = ble_get_le16(client->request_pdu + 1);
                uint16_t last = ble_get_le16(client->request_pdu + 3);
                uint16_t type = ble_get_le16(client->request_pdu + 5);
                uint16_t previous_end = 0;
                response_valid = first && first <= last;

                for (uint16_t offset = 1; response_valid && offset < len; offset += 4) {
                    uint16_t found = ble_get_le16(pdu + offset);
                    uint16_t group_end = ble_get_le16(pdu + offset + 2);

                    if (!found || found < first || found > last ||
                        group_end < found || found <= previous_end ||
                        ((type != 0x2800 && type != 0x2801) &&
                         group_end != found))
                        response_valid = 0;
                    previous_end = group_end;
                }
            }
            break;
        }
        case ATT_OP_READ_BY_TYPE_RSP: {
            response_valid = (client->request.len == 7 || client->request.len == 21) &&
                            client->request_pdu[0] == ATT_OP_READ_BY_TYPE_REQ &&
                            len >= 4 && pdu[1] >= 2 &&
                            (len - 2) % pdu[1] == 0;

            if (response_valid) {
                uint16_t first = ble_get_le16(client->request_pdu + 1);
                uint16_t last = ble_get_le16(client->request_pdu + 3);
                const uint8_t *uuid = client->request_pdu + 5;
                uint8_t uuid_len = (uint8_t)(client->request.len - 5);

                static const uint8_t base_prefix[14] = {
                    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00,
                    0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00
                };
                uint16_t type = uuid_len == 2 ? ble_get_le16(uuid) :
                    uuid_len == 16 &&
                    !memcmp(uuid, base_prefix, sizeof(base_prefix)) ?
                        ble_get_le16(uuid + 14) : 0;
                uint16_t previous = 0;

                response_valid = first && first <= last &&
                    !((type == 0x2803 && pdu[1] != 7 && pdu[1] != 21) ||
                      (type == 0x2802 && pdu[1] != 6 && pdu[1] != 8));

                for (uint16_t offset = 2;
                     response_valid && offset < len; offset += pdu[1]
                ) {
                    uint16_t handle = ble_get_le16(pdu + offset);
                    if (!handle || handle < first || handle > last || handle <= previous) {
                        response_valid = 0;
                    }
                    else if (type == 0x2803) {
                        uint16_t value_handle = ble_get_le16(pdu + offset + 3);
                        if (!value_handle || value_handle <= handle)
                            response_valid = 0;
                    }
                    else if (type == 0x2802) {
                        uint16_t service_start = ble_get_le16(pdu + offset + 2);
                        uint16_t service_end = ble_get_le16(pdu + offset + 4);
                        if (!service_start || service_start > service_end)
                            response_valid = 0;
                    }
                    previous = handle;
                }
            }
            break;
        }
        case ATT_OP_READ_RSP:
            response_valid = len - 1 <= BLE_GATT_ATT_VALUE_MAX;
            break;

        case ATT_OP_READ_BLOB_RSP:
            response_valid = len - 1 <= BLE_GATT_ATT_VALUE_MAX &&
                            client->request.len == 5 &&
                            client->request_pdu[0] == ATT_OP_READ_BLOB_REQ;
            if (response_valid) {
                uint16_t offset = ble_get_le16(client->request_pdu + 3);
                response_valid = offset <= BLE_GATT_ATT_VALUE_MAX &&
                                (uint32_t)offset + len - 1 <= BLE_GATT_ATT_VALUE_MAX;
            }
            break;

        case ATT_OP_READ_MULTIPLE_RSP:
            response_valid = 1;
            break;

        case ATT_OP_READ_BY_GROUP_TYPE_RSP: {
            response_valid = (client->request.len == 7 || client->request.len == 21) &&
                            client->request_pdu[0] == ATT_OP_READ_BY_GROUP_TYPE_REQ &&
                            len >= 8 &&
                            (pdu[1] == 6 || pdu[1] == 20) && (len - 2) % pdu[1] == 0;

            if (response_valid) {
                uint16_t first = ble_get_le16(client->request_pdu + 1);
                uint16_t last = ble_get_le16(client->request_pdu + 3);
                uint16_t previous_end = 0;
                response_valid = first && first <= last;

                for (uint16_t offset = 2; response_valid && offset < len; offset += pdu[1]) {
                    uint16_t start = ble_get_le16(pdu + offset);
                    uint16_t end = ble_get_le16(pdu + offset + 2);

                    if (!start || start < first || start > last || end < start ||
                        start <= previous_end
                    ) response_valid = 0;

                    previous_end = end;
                }
            }
            break;
        }
        case ATT_OP_WRITE_RSP: case ATT_OP_EXECUTE_WRITE_RSP:
            response_valid = len == 1;
            break;

        case ATT_OP_PREPARE_WRITE_RSP:
            response_valid = client->request.len == len && len >= 5 &&
                            !memcmp(client->request_pdu + 1, pdu + 1, len - 1);
            break;

        case ATT_OP_READ_MULT_VAR_RSP: {
            uint16_t offset = 1;
            uint16_t tuples = 0;
            response_valid = client->request.len &&
                            client->request_pdu[0] == ATT_OP_READ_MULTIPLE_VARIABLE_REQ &&
                            client->request.len >= 5 &&
                            !((client->request.len - 1) & 1) && len >= 3;
            uint16_t requested = response_valid ? (client->request.len - 1) / 2 : 0;
            uint8_t truncated = 0;

            while (response_valid && offset < len) {
                if (len - offset < 2) {
                    response_valid = 0;
                    break;
                }
                uint16_t value_len = (uint16_t)pdu[offset] |
                                     (uint16_t)pdu[offset + 1] << 8;
                offset += 2;

                if (value_len > BLE_GATT_ATT_VALUE_MAX || ++tuples > requested) {
                    response_valid = 0;
                    break;
                }

                if (value_len > len - offset) {
                    response_valid = len == client->mtu;
                    truncated = 1;
                    break;
                }
                offset += value_len;
            }

            if (response_valid && !truncated)
                response_valid = offset == len && tuples != 0;
            break;
        }
        default:
            response_valid = 0;
            break;
        }
    }

    if (!response_valid) {
        memset(&client->request, 0, sizeof(client->request));
        client->operation.kind = 0;
        client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
        return -1;
    }

    memset(&client->request, 0, sizeof(client->request));
    if (opcode == ATT_OP_EXCHANGE_MTU_RSP) {
        uint16_t peer_mtu = (uint16_t)pdu[1] | (uint16_t)pdu[2] << 8;
        client->mtu = peer_mtu < 23 ? 23 :
                    (peer_mtu < client->local_mtu ? peer_mtu : client->local_mtu);
    }

    if (client->operation.kind == 1 &&
        (opcode == ATT_OP_READ_RSP || opcode == ATT_OP_READ_BLOB_RSP)) {
        uint16_t part_len = len - 1;
        if (part_len > BLE_GATT_CLIENT_VALUE_MAX - client->operation.value_len) {
            client->operation.kind = 0;
            client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
            return -1;
        }

        if (part_len) memcpy(client->operation.value + client->operation.value_len,
                             pdu + 1, part_len);
        client->operation.value_len += part_len;

        if (part_len == client->mtu - 1 &&
            client->operation.value_len < BLE_GATT_CLIENT_VALUE_MAX) {
            uint8_t blob[5] = {ATT_OP_READ_BLOB_REQ};
            ble_put_le16(blob + 1, client->operation.handle);
            ble_put_le16(blob + 3, client->operation.value_len);

            if (!gatt_client_request(client, blob, sizeof(blob),
                                     ATT_OP_READ_BLOB_RSP, client->now_ms)) {
                client->operation.kind = 0;
                client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
                return -1;
            }

            return 1;
        }

        client->operation.kind = 0;
        client->result(client->context, 0, pdu, len);
        return 1;
    }

    if (client->operation.kind == 2 &&
        opcode == ATT_OP_PREPARE_WRITE_RSP) {
        client->operation.offset += client->operation.chunk;

        if (client->operation.offset < client->operation.total) {
            uint16_t remaining = client->operation.total - client->operation.offset;
            uint16_t chunk = client->mtu - 5;
            if (chunk > remaining) chunk = remaining;
            uint8_t next_pdu[BLE_GATT_CLIENT_MTU_MAX] = {
                ATT_OP_PREPARE_WRITE_REQ
            };
            ble_put_le16(next_pdu + 1, client->operation.handle);
            ble_put_le16(next_pdu + 3, client->operation.offset);
            memcpy(next_pdu + 5,
                   client->operation.value + client->operation.offset, chunk);
            client->operation.chunk = chunk;
            if (gatt_client_request(client, next_pdu, chunk + 5,
                                    ATT_OP_PREPARE_WRITE_RSP,
                                    client->now_ms))
                return 1;
        } else {
            const uint8_t execute[2] = {ATT_OP_EXECUTE_WRITE_REQ, 1};
            if (gatt_client_request(client, execute, sizeof(execute),
                                    ATT_OP_EXECUTE_WRITE_RSP,
                                    client->now_ms))
                return 1;
        }

        client->operation.kind = 0;
        client->result(client->context, BLE_GATT_CLIENT_PROTOCOL_ERROR, pdu, len);
        return -1;
    }

    if (client->operation.kind == 2 &&
        opcode == ATT_OP_EXECUTE_WRITE_RSP) {
        client->operation.kind = 0;
        client->operation.total = client->operation.chunk = 0;
        client->result(client->context, 0, pdu, len);
        return 1;
    }

    if (client->operation.kind == 3 &&
        opcode == ATT_OP_EXECUTE_WRITE_RSP) {
        client->operation.kind = 0;
        client->result(client->context, BLE_GATT_CLIENT_REMOTE_ERROR,
                       client->operation.error, 5);
        return 1;
    }

    client->result(client->context, 0, pdu, len);
    return 1;
}

#include "ble_gatt_server.h"
#include "ble_gatt_transport.h"
#include "ble_gatt_eatt.h"

#endif
