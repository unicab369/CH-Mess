#ifndef BLE_ATT_SERVER_H
#define BLE_ATT_SERVER_H

// ATT PDU server procedures. The GATT server supplies the attribute database
// and access callbacks; ATT owns wire opcode handling and response encoding.
#include "../ble_gatt/ble_gatt_server.h"

static int ble_gatt_server_error_rsp(uint8_t request, uint16_t handle,
                                     uint8_t error, uint8_t *rsp,
                                     uint16_t cap, uint16_t *rsp_len) {
    if (cap < 5) return 0;
    rsp[0] = 0x01; rsp[1] = request;
    ble_gatt_server_put_u16(rsp + 2, handle); rsp[4] = error;
    *rsp_len = 5;
    return 1;
}

static int ble_gatt_server_uuid_from_wire(const uint8_t *p, uint8_t len,
                                          ble_gatt_uuid *uuid) {
    if (len != 2 && len != 16) return 0;
    uuid->len = len;
    memcpy(uuid->value, p, len);
    return 1;
}

// Process one complete ATT PDU. Returns 1 when a response is present, 0 for
// one-way PDUs, and -1 for bad arguments or malformed one-way PDUs.
static inline int ble_att_server_process(ble_gatt_server *server, const uint8_t *req,
                        uint16_t req_len, uint8_t *rsp, uint16_t rsp_capacity,
                        uint16_t *rsp_len) {
    if (!server || !req || !req_len || !rsp || !rsp_len) return -1;
    *rsp_len = 0;
    uint8_t op = req[0];
    uint16_t mtu = server->mtu;
    // ATT commands (including Write Command and Signed Write Command) never
    // receive a response, even when malformed or larger than the bearer MTU.
    if (req_len > mtu) {
        if (op & 0x40) return 0;
        if (op == 0x1e) return -1;
        goto invalid_pdu;
    }
    if (op == 0x1e) { // Handle Value Confirmation
        if (req_len != 1) return -1;
        uint16_t confirmed_handle = server->indication_handle;
        uint8_t service_changed_confirmed = server->indication_pending &&
            server->database_hash_update_pending;
        server->indication_pending = 0;
        server->indication_timeout_armed = 0;
        server->indication_handle = 0;
        server->indication_started_ms = 0;
        if (service_changed_confirmed) {
            ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
            service_changed_confirmed = 0;
            for (uint16_t i = 0; i < server->count; i++)
                if (ble_gatt_uuid_equal(&server->attributes[i].uuid,
                                        &changed_uuid) &&
                    server->attributes[i].handle == confirmed_handle)
                    service_changed_confirmed = 1;
            if (service_changed_confirmed && server->database_hash_store) {
                server->database_hash_store(server->database_hash_context,
                                            server->database_hash);
                server->database_hash_update_pending = 0;
            }
        }
        return 0;
    }
    if (op == 0x02) { // Exchange MTU Request
        if (req_len != 3)
            return ble_gatt_server_error_rsp(op, 0, BLE_GATT_ATT_ERR_INVALID_PDU,
                                              rsp, rsp_capacity, rsp_len);
        if (server->mtu_exchanged)
            return ble_gatt_server_error_rsp(op, 0, 0x06, rsp, rsp_capacity, rsp_len);
        if (rsp_capacity < 3) return 0;
        uint16_t peer_mtu = ble_gatt_server_u16(req + 1);
        server->mtu = peer_mtu < 23 ? 23 :
            (peer_mtu < server->local_mtu ? peer_mtu : server->local_mtu);
        server->mtu_exchanged = 1;
        rsp[0] = 0x03;
        ble_gatt_server_put_u16(rsp + 1, server->local_mtu);
        *rsp_len = 3;
        return 1;
    }
    if (op == 0x10 || op == 0x06 || op == 0x08) {
        uint8_t group = op == 0x10;
        uint8_t find = op == 0x06;
        if ((group && req_len != 7 && req_len != 21) ||
            (find && req_len < 7) ||
            (!group && !find && req_len != 7 && req_len != 21)) goto invalid_pdu;
        uint16_t first = ble_gatt_server_u16(req + 1);
        uint16_t last = ble_gatt_server_u16(req + 3);
        uint8_t uuid_len = group ? (uint8_t)(req_len - 5) :
                           find ? (uint8_t)(req_len - 7) :
                           (uint8_t)(req_len - 5);
        if (first == 0 || first > last)
            return ble_gatt_server_error_rsp(op, first, 0x01, rsp,
                                              rsp_capacity, rsp_len);
        if (!find &&
            !ble_gatt_uuid_valid(&(ble_gatt_uuid){uuid_len,{0}}))
            goto invalid_pdu;
        ble_gatt_uuid type;
        const uint8_t *sought_value = NULL;
        uint16_t sought_len = 0;
        if (find) {
            if (!ble_gatt_server_uuid_from_wire(req + 5, 2, &type))
                goto invalid_pdu;
            sought_value = req + 7;
            sought_len = req_len - 7;
            if (rsp_capacity < 5) return 0;
            rsp[0] = 0x07;
            uint16_t n = 1;
            for (uint16_t i = 0; i < server->count; i++) {
                ble_gatt_attribute *a = &server->attributes[i];
                if (a->handle < first || a->handle > last ||
                    !ble_gatt_uuid_equal(&a->uuid, &type)) continue;
                uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t value_len = sizeof(value);
                if (ble_gatt_server_read(server, a, 0, value, &value_len) ||
                    value_len != sought_len ||
                    (sought_len && memcmp(value, sought_value, sought_len))) continue;
                if (n + 4 > rsp_capacity || n + 4 > mtu) break;
                ble_gatt_server_put_u16(rsp + n, a->handle); n += 2;
                uint16_t end = a->handle;
                uint16_t type16 = ble_gatt_server_u16(type.value);
                if (type16 == 0x2800 || type16 == 0x2801) {
                    for (uint16_t j = i + 1; j < server->count; j++) {
                        if (server->attributes[j].flags &
                            (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                             BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)) break;
                        end = server->attributes[j].handle;
                    }
                }
                ble_gatt_server_put_u16(rsp + n, end); n += 2;
            }
            if (n == 1) return ble_gatt_server_error_rsp(op, first,
                BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
            *rsp_len = n; return 1;
        }
        if (uuid_len != 2 && uuid_len != 16) goto invalid_pdu;
        if (!ble_gatt_server_uuid_from_wire(req + 5, uuid_len, &type)) goto invalid_pdu;
        uint16_t group_type = ble_gatt_uuid_assigned16(&type);
        ble_gatt_uuid primary_type = ble_gatt_uuid16(0x2800);
        ble_gatt_uuid secondary_type = ble_gatt_uuid16(0x2801);
        if (group && !ble_gatt_uuid_equal(&type, &primary_type) &&
            !ble_gatt_uuid_equal(&type, &secondary_type))
            return ble_gatt_server_error_rsp(op, first,
                BLE_GATT_ATT_ERR_UNSUPPORTED_GROUP_TYPE, rsp,
                rsp_capacity, rsp_len);
        if (rsp_capacity < 2) return 0;
        uint8_t response_op = group ? 0x11 : 0x09;
        uint16_t n = group ? 2 : 2;
        uint8_t entry_len = 0;
        rsp[0] = response_op;
        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *a = &server->attributes[i];
            if (a->handle < first || a->handle > last) continue;
            if (group) {
                if (!((group_type == 0x2800 &&
                       (a->flags & BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE)) ||
                      (group_type == 0x2801 &&
                       (a->flags & BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)))) continue;
            } else if (!ble_gatt_uuid_equal(&a->uuid, &type)) continue;
            uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
            uint16_t packet_limit = mtu < rsp_capacity ? mtu : rsp_capacity;
            uint16_t fixed = group ? 6 : 4;
            if (packet_limit <= fixed) break;
            uint16_t value_len = packet_limit - fixed;
            if (value_len > sizeof(value)) value_len = sizeof(value);
            uint16_t entry_value_max = (uint16_t)(255 - (group ? 4 : 2));
            if (value_len > entry_value_max) value_len = entry_value_max;
            uint8_t error = ble_gatt_server_read(server, a, 0, value, &value_len);
            if (error) {
                if (n == 2) return ble_gatt_server_error_rsp(op, a->handle,
                    error, rsp, rsp_capacity, rsp_len);
                break;
            }
            uint16_t this_len = (uint16_t)(group ? 4 + value_len : 2 + value_len);
            if (!entry_len) { entry_len = this_len; rsp[1] = entry_len; }
            if (entry_len != this_len || n + this_len > mtu ||
                n + this_len > rsp_capacity) break;
            ble_gatt_server_put_u16(rsp + n, a->handle); n += 2;
            if (group) {
                uint16_t end = a->handle;
                for (uint16_t j = i + 1; j < server->count; j++) {
                    if (server->attributes[j].flags &
                        (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                         BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)) break;
                    end = server->attributes[j].handle;
                }
                ble_gatt_server_put_u16(rsp + n, end); n += 2;
            }
            if (value_len) memcpy(rsp + n, value, value_len);
            n += value_len;
        }
        if (n == 2) return ble_gatt_server_error_rsp(op, first,
            BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
        *rsp_len = n; return 1;
    }
    if (op == 0x04) { // Find Information Request
        if (req_len != 5) goto invalid_pdu;
        uint16_t first = ble_gatt_server_u16(req + 1), last = ble_gatt_server_u16(req + 3);
        if (!first || first > last) return ble_gatt_server_error_rsp(op, first, 0x01,
            rsp, rsp_capacity, rsp_len);
        if (rsp_capacity < 2) return 0;
        uint8_t format = 0;
        uint16_t n = 2;
        rsp[0] = 0x05;
        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *a = &server->attributes[i];
            if (a->handle < first || a->handle > last) continue;
            uint8_t f = a->uuid.len == 2 ? 1 : 2;
            uint8_t entry = (uint8_t)(2 + a->uuid.len);
            if (format && f != format) break;
            if (!format) { format = f; rsp[1] = f; }
            if (n + entry > mtu || n + entry > rsp_capacity) break;
            ble_gatt_server_put_u16(rsp + n, a->handle); n += 2;
            memcpy(rsp + n, a->uuid.value, a->uuid.len); n += a->uuid.len;
        }
        if (n == 2) return ble_gatt_server_error_rsp(op, first,
            BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
        *rsp_len = n; return 1;
    }
    if (op == 0x0a || op == 0x0c) { // Read / Read Blob
        if (req_len != (op == 0x0a ? 3 : 5)) goto invalid_pdu;
        uint16_t h = ble_gatt_server_u16(req + 1);
        ble_gatt_attribute *a = ble_gatt_server_find(server, h);
        if (!a) return ble_gatt_server_error_rsp(op, h,
            BLE_GATT_ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
        uint16_t offset = op == 0x0c ? ble_gatt_server_u16(req + 3) : 0;
        if (rsp_capacity < 1) return 0;
        uint16_t value_len = rsp_capacity - 1;
        if (value_len > mtu - 1) value_len = mtu - 1;
        uint8_t error = ble_gatt_server_read(server, a, offset, rsp + 1, &value_len);
        if (error) return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                    rsp_capacity, rsp_len);
        rsp[0] = op == 0x0a ? 0x0b : 0x0d;
        *rsp_len = value_len + 1; return 1;
    }
    if (op == 0x0e || op == 0x20) { // Read Multiple / Read Multiple Variable
        uint8_t variable = op == 0x20;
        if (req_len < 5 || ((req_len - 1) & 1)) goto invalid_pdu;
        uint16_t n = 1;
        uint16_t packet_limit = mtu < rsp_capacity ? mtu : rsp_capacity;
        if (!packet_limit) return 0;
        rsp[0] = variable ? 0x21 : 0x0f;
        for (uint16_t offset = 1; offset < req_len; offset += 2) {
            uint16_t h = ble_gatt_server_u16(req + offset);
            ble_gatt_attribute *a = ble_gatt_server_find(server, h);
            if (!a) return ble_gatt_server_error_rsp(op, h,
                BLE_GATT_ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
            if (variable) {
                uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t value_len = sizeof(value);
                uint8_t error = ble_gatt_server_read(server, a, 0, value,
                                                      &value_len);
                if (error) return ble_gatt_server_error_rsp(op, h, error,
                    rsp, rsp_capacity, rsp_len);
                if (n >= packet_limit || packet_limit - n < 2) continue;
                uint16_t copy_len = packet_limit - n - 2;
                if (copy_len > value_len) copy_len = value_len;
                ble_gatt_server_put_u16(rsp + n, value_len);
                n += 2;
                if (copy_len) memcpy(rsp + n, value, copy_len);
                n += copy_len;
                continue;
            }
            if (n >= packet_limit) {
                uint8_t discarded[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t discarded_len = sizeof(discarded);
                uint8_t error = ble_gatt_server_read(server, a, 0,
                    discarded, &discarded_len);
                if (error) return ble_gatt_server_error_rsp(op, h, error,
                    rsp, rsp_capacity, rsp_len);
                continue;
            }
            uint16_t len = packet_limit - n;
            uint8_t error = ble_gatt_server_read(server, a, 0, rsp + n, &len);
            if (error) return ble_gatt_server_error_rsp(op, h, error,
                                                         rsp, rsp_capacity, rsp_len);
            n += len;
        }
        if (n == 1) return ble_gatt_server_error_rsp(op, 0,
            BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES, rsp, rsp_capacity, rsp_len);
        *rsp_len = n;
        return 1;
    }
    if (op == 0x16) { // Prepare Write Request
        if (req_len < 5) goto invalid_pdu;
        if (rsp_capacity < req_len) return 0;
        uint16_t h = ble_gatt_server_u16(req + 1);
        uint16_t offset = ble_gatt_server_u16(req + 3);
        uint16_t value_len = req_len - 5;
        ble_gatt_attribute *a = ble_gatt_server_find(server, h);
        if (!a) return ble_gatt_server_error_rsp(op, h,
            BLE_GATT_ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
        uint8_t error = ble_gatt_server_prepare_validate(server, a, offset,
                                                          value_len, 0);
        if (error) return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                     rsp_capacity, rsp_len);
        if (server->prepare_count >= BLE_GATT_SERVER_PREPARE_QUEUE_SIZE ||
            value_len > BLE_GATT_SERVER_PREPARE_BYTES - server->prepare_used)
            return ble_gatt_server_error_rsp(op, h,
                BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES, rsp,
                rsp_capacity, rsp_len);
        if (a->prepare) {
            error = a->prepare(a->context, offset, req + 5, value_len);
            if (error)
                return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                  rsp_capacity, rsp_len);
        }
        ble_gatt_prepared_write *queued =
            &server->prepared[server->prepare_count++];
        queued->handle = h;
        queued->offset = offset;
        queued->len = value_len;
        queued->data_offset = server->prepare_used;
        if (value_len) memcpy(server->prepare_data + server->prepare_used,
                              req + 5, value_len);
        server->prepare_used += value_len;
        memcpy(rsp, req, req_len);
        rsp[0] = 0x17;
        *rsp_len = req_len;
        return 1;
    }
    if (op == 0x18) { // Execute Write Request
        if (req_len != 2 || req[1] > 1) goto invalid_pdu;
        if (rsp_capacity < 1) return 0;
        if (req[1]) {
            uint16_t error_handle = 0;
            uint8_t error = ble_gatt_server_prepare_execute(server,
                                                            &error_handle);
            if (error) {
                ble_gatt_server_prepare_cancel_all(server, 0);
                ble_gatt_server_prepare_clear(server);
                return ble_gatt_server_error_rsp(op, error_handle, error, rsp,
                                                  rsp_capacity, rsp_len);
            }
        } else ble_gatt_server_prepare_cancel_all(server, 0);
        ble_gatt_server_prepare_clear(server);
        rsp[0] = 0x19;
        *rsp_len = 1;
        return 1;
    }
    if (op == 0xd2) { // Signed Write Command
        if (req_len < 15 || server->encrypted || !server->signed_verify)
            return 0;
        uint16_t handle = ble_gatt_server_u16(req + 1);
        ble_gatt_attribute *a = ble_gatt_server_find(server, handle);
        if (!a || !(a->properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE) ||
            !(a->permissions & BLE_GATT_PERM_WRITE_SIGNED)) return 0;
        uint16_t value_len = req_len - 15;
        uint16_t signed_len = req_len - 12;
        if (server->signed_verify(server->signed_context, req, signed_len,
                                  req + signed_len))
            (void)ble_gatt_server_write(server, a, 0, req + 3, value_len, 2);
        return 0;
    }
    if (op == 0x12 || op == 0x52) { // Write Request / Write Command
        uint8_t command = op == 0x52;
        if (!command && rsp_capacity < 1) return 0;
        if (req_len < 3) {
            if (command) return 0;
            goto invalid_pdu;
        }
        uint16_t h = ble_gatt_server_u16(req + 1);
        ble_gatt_attribute *a = ble_gatt_server_find(server, h);
        if (!a) {
            if (command) return 0;
            return ble_gatt_server_error_rsp(op, h,
                BLE_GATT_ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
        }
        uint8_t error = ble_gatt_server_write(server, a, 0, req + 3,
                                                req_len - 3, command);
        if (command) return 0;
        if (error) return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                    rsp_capacity, rsp_len);
        rsp[0] = 0x13; *rsp_len = 1; return 1;
    }
    if (op & 0x40) return 0; // ATT commands never receive a response.
    return ble_gatt_server_error_rsp(op, 0,
        BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED, rsp, rsp_capacity, rsp_len);

invalid_pdu:
    return ble_gatt_server_error_rsp(op, 0, BLE_GATT_ATT_ERR_INVALID_PDU,
                                      rsp, rsp_capacity, rsp_len);
}

#endif
