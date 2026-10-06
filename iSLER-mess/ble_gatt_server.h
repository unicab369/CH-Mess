#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

// Transport-independent GATT/ATT server core. Applications register attributes
// and call ble_gatt_server_att() with each complete ATT PDU; the GAP/L2CAP
// adapter is responsible for carrying the returned response.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef BLE_GATT_SERVER_MAX_ATTRIBUTES
#define BLE_GATT_SERVER_MAX_ATTRIBUTES 64
#endif
#ifndef BLE_GATT_SERVER_VALUE_MAX
#define BLE_GATT_SERVER_VALUE_MAX 256
#endif
#ifndef BLE_GATT_SERVER_MTU_MAX
#define BLE_GATT_SERVER_MTU_MAX 517
#endif

#define BLE_GATT_UUID16_LEN 2
#define BLE_GATT_UUID128_LEN 16

enum {
    BLE_GATT_PERM_READ = 1u << 0,
    BLE_GATT_PERM_WRITE = 1u << 1,
    BLE_GATT_PERM_READ_ENCRYPTED = 1u << 2,
    BLE_GATT_PERM_WRITE_ENCRYPTED = 1u << 3,
    BLE_GATT_PERM_READ_AUTHENTICATED = 1u << 4,
    BLE_GATT_PERM_WRITE_AUTHENTICATED = 1u << 5
};

enum {
    BLE_GATT_PROP_BROADCAST = 1u << 0,
    BLE_GATT_PROP_READ = 1u << 1,
    BLE_GATT_PROP_WRITE_NO_RSP = 1u << 2,
    BLE_GATT_PROP_WRITE = 1u << 3,
    BLE_GATT_PROP_NOTIFY = 1u << 4,
    BLE_GATT_PROP_INDICATE = 1u << 5,
    BLE_GATT_PROP_AUTH_SIGNED_WRITE = 1u << 6,
    BLE_GATT_PROP_EXTENDED = 1u << 7
};

enum {
    BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE = 1u << 0,
    BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE = 1u << 1,
    BLE_GATT_ATTRIBUTE_CHARACTERISTIC = 1u << 2,
    BLE_GATT_ATTRIBUTE_CCCD = 1u << 3
};

enum {
    BLE_GATT_ATT_ERR_INVALID_HANDLE = 0x01,
    BLE_GATT_ATT_ERR_READ_NOT_PERMITTED = 0x02,
    BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED = 0x03,
    BLE_GATT_ATT_ERR_INVALID_PDU = 0x04,
    BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED = 0x06,
    BLE_GATT_ATT_ERR_INVALID_OFFSET = 0x07,
    BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND = 0x0a,
    BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH = 0x0d,
    BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION = 0x0f,
    BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION = 0x05,
    BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED = 0x13
};

typedef struct {
    uint8_t len;
    uint8_t value[BLE_GATT_UUID128_LEN];
} ble_gatt_uuid;

struct ble_gatt_attribute;
typedef uint8_t (*ble_gatt_read_fn)(void *context, uint16_t offset,
                                    uint8_t *out, uint16_t *inout_len);
typedef uint8_t (*ble_gatt_write_fn)(void *context, uint16_t offset,
                                    const uint8_t *value, uint16_t len,
                                    uint8_t command);

typedef struct ble_gatt_attribute {
    uint16_t handle;
    ble_gatt_uuid uuid;
    uint16_t permissions;
    uint8_t properties;
    uint8_t flags;
    uint16_t value_len, value_capacity;
    uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
    uint16_t cccd;
    ble_gatt_read_fn read;
    ble_gatt_write_fn write;
    void *context;
} ble_gatt_attribute;

typedef struct {
    ble_gatt_attribute attributes[BLE_GATT_SERVER_MAX_ATTRIBUTES];
    uint16_t count, next_handle, mtu, local_mtu;
    uint8_t mtu_exchanged, encrypted, authenticated;
} ble_gatt_server;

static uint16_t ble_gatt_server_u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static void ble_gatt_server_put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static int ble_gatt_uuid_valid(const ble_gatt_uuid *uuid) {
    return uuid && (uuid->len == BLE_GATT_UUID16_LEN ||
                    uuid->len == BLE_GATT_UUID128_LEN);
}

static int ble_gatt_uuid_equal(const ble_gatt_uuid *a,
                               const ble_gatt_uuid *b) {
    return a->len == b->len && !memcmp(a->value, b->value, a->len);
}

void ble_gatt_server_init(ble_gatt_server *server, uint16_t local_mtu) {
    if (!server) return;
    memset(server, 0, sizeof(*server));
    if (local_mtu < 23) local_mtu = 23;
    if (local_mtu > BLE_GATT_SERVER_MTU_MAX)
        local_mtu = BLE_GATT_SERVER_MTU_MAX;
    server->local_mtu = local_mtu;
    server->mtu = 23;
    server->next_handle = 1;
}

void ble_gatt_server_link_reset(ble_gatt_server *server) {
    if (!server) return;
    server->mtu = 23;
    server->mtu_exchanged = 0;
    server->encrypted = server->authenticated = 0;
    for (uint16_t i = 0; i < server->count; i++)
        if (server->attributes[i].flags & BLE_GATT_ATTRIBUTE_CCCD)
            server->attributes[i].cccd = 0;
}

void ble_gatt_server_set_security(ble_gatt_server *server, int encrypted,
                                  int authenticated) {
    if (!server) return;
    server->encrypted = encrypted != 0;
    server->authenticated = authenticated != 0;
}

static ble_gatt_attribute *ble_gatt_server_find(ble_gatt_server *server,
                                                 uint16_t handle) {
    if (!server || !handle) return NULL;
    for (uint16_t i = 0; i < server->count; i++)
        if (server->attributes[i].handle == handle)
            return &server->attributes[i];
    return NULL;
}

static int ble_gatt_server_add(ble_gatt_server *server,
                               const ble_gatt_uuid *uuid, uint16_t permissions,
                               uint8_t properties, uint8_t flags,
                               const uint8_t *value, uint16_t value_len,
                               uint16_t value_capacity,
                               ble_gatt_read_fn read,
                               ble_gatt_write_fn write, void *context,
                               uint16_t *handle_out) {
    if (!server || !ble_gatt_uuid_valid(uuid) ||
        server->count >= BLE_GATT_SERVER_MAX_ATTRIBUTES ||
        server->next_handle == 0 || value_len > value_capacity ||
        value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !value)) return 0;
    ble_gatt_attribute *a = &server->attributes[server->count++];
    memset(a, 0, sizeof(*a));
    a->handle = server->next_handle++;
    a->uuid = *uuid;
    a->permissions = permissions;
    a->properties = properties;
    a->flags = flags;
    a->value_len = value_len;
    a->value_capacity = value_capacity;
    a->read = read;
    a->write = write;
    a->context = context;
    if (value_len) memcpy(a->value, value, value_len);
    if (handle_out) *handle_out = a->handle;
    return 1;
}

int ble_gatt_server_add_attribute(ble_gatt_server *server,
                                  const ble_gatt_uuid *uuid,
                                  uint16_t permissions, const uint8_t *value,
                                  uint16_t value_len, uint16_t value_capacity,
                                  ble_gatt_read_fn read,
                                  ble_gatt_write_fn write, void *context,
                                  uint16_t *handle_out) {
    return ble_gatt_server_add(server, uuid, permissions, 0, 0, value,
        value_len, value_capacity, read, write, context, handle_out);
}

static ble_gatt_uuid ble_gatt_uuid16(uint16_t value) {
    ble_gatt_uuid uuid = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
    return uuid;
}

int ble_gatt_server_add_service(ble_gatt_server *server,
                                const ble_gatt_uuid *service_uuid,
                                int primary, uint16_t *service_handle) {
    if (!ble_gatt_uuid_valid(service_uuid)) return 0;
    ble_gatt_uuid type = ble_gatt_uuid16(primary ? 0x2800 : 0x2801);
    return ble_gatt_server_add(server, &type, BLE_GATT_PERM_READ, 0,
        primary ? BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE :
                  BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE,
        service_uuid->value, service_uuid->len, service_uuid->len,
        NULL, NULL, NULL, service_handle);
}

int ble_gatt_server_add_characteristic(ble_gatt_server *server,
                                       const ble_gatt_uuid *uuid,
                                       uint8_t properties,
                                       uint16_t permissions,
                                       const uint8_t *initial_value,
                                       uint16_t value_len,
                                       uint16_t value_capacity,
                                       ble_gatt_read_fn read,
                                       ble_gatt_write_fn write,
                                       void *context,
                                       uint16_t *declaration_handle,
                                       uint16_t *value_handle) {
    if (!server || !ble_gatt_uuid_valid(uuid) ||
        value_len > value_capacity || value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !initial_value) ||
        server->count + 2 > BLE_GATT_SERVER_MAX_ATTRIBUTES ||
        server->next_handle > 0xfffd) return 0;
    uint16_t decl_h = server->next_handle;
    uint16_t val_h = (uint16_t)(decl_h + 1);
    uint8_t decl[3 + BLE_GATT_UUID128_LEN];
    decl[0] = properties;
    ble_gatt_server_put_u16(decl + 1, val_h);
    memcpy(decl + 3, uuid->value, uuid->len);
    ble_gatt_uuid declaration_uuid = ble_gatt_uuid16(0x2803);
    if (!ble_gatt_server_add(server, &declaration_uuid, BLE_GATT_PERM_READ,
            0, BLE_GATT_ATTRIBUTE_CHARACTERISTIC, decl,
            (uint16_t)(3 + uuid->len), (uint16_t)(3 + uuid->len),
            NULL, NULL, NULL, declaration_handle)) return 0;
    if (!ble_gatt_server_add(server, uuid, permissions, properties, 0,
            initial_value, value_len, value_capacity, read, write, context,
            value_handle)) {
        server->count--;
        server->next_handle--;
        return 0;
    }
    return 1;
}

int ble_gatt_server_add_descriptor(ble_gatt_server *server,
                                   const ble_gatt_uuid *uuid,
                                   uint16_t permissions, const uint8_t *value,
                                   uint16_t value_len, uint16_t value_capacity,
                                   ble_gatt_read_fn read,
                                   ble_gatt_write_fn write, void *context,
                                   uint16_t *handle_out) {
    if (uuid && uuid->len == 2 && ble_gatt_server_u16(uuid->value) == 0x2902) {
        uint8_t zero[2] = {0, 0};
        return ble_gatt_server_add(server, uuid,
            permissions | BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, 0,
            BLE_GATT_ATTRIBUTE_CCCD, zero, 2, 2, NULL, NULL, context,
            handle_out);
    }
    return ble_gatt_server_add_attribute(server, uuid, permissions, value,
        value_len, value_capacity, read, write, context, handle_out);
}

static uint8_t ble_gatt_server_security_error(const ble_gatt_server *server,
                                              uint16_t permissions,
                                              int write) {
    uint16_t encrypted = write ? BLE_GATT_PERM_WRITE_ENCRYPTED :
                                 BLE_GATT_PERM_READ_ENCRYPTED;
    uint16_t authenticated = write ? BLE_GATT_PERM_WRITE_AUTHENTICATED :
                                     BLE_GATT_PERM_READ_AUTHENTICATED;
    if ((permissions & encrypted) && !server->encrypted)
        return BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION;
    if ((permissions & authenticated) && !server->authenticated)
        return BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION;
    return 0;
}

static uint8_t ble_gatt_server_read(ble_gatt_server *server,
                                    ble_gatt_attribute *a, uint16_t offset,
                                    uint8_t *out, uint16_t *out_len) {
    if (!(a->permissions & (BLE_GATT_PERM_READ |
                            BLE_GATT_PERM_READ_ENCRYPTED |
                            BLE_GATT_PERM_READ_AUTHENTICATED)))
        return BLE_GATT_ATT_ERR_READ_NOT_PERMITTED;
    uint8_t security = ble_gatt_server_security_error(server, a->permissions, 0);
    if (security) return security;
    if (a->flags & BLE_GATT_ATTRIBUTE_CCCD) {
        uint8_t value[2];
        ble_gatt_server_put_u16(value, a->cccd);
        if (offset > 2) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
        uint16_t size = (uint16_t)(2 - offset);
        if (size > *out_len) size = *out_len;
        memcpy(out, value + offset, size);
        *out_len = size;
        return 0;
    }
    if (a->read) return a->read(a->context, offset, out, out_len);
    if (offset > a->value_len) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    uint16_t size = (uint16_t)(a->value_len - offset);
    if (size > *out_len) size = *out_len;
    if (size) memcpy(out, a->value + offset, size);
    *out_len = size;
    return 0;
}

static uint8_t ble_gatt_server_write(ble_gatt_server *server,
                                     ble_gatt_attribute *a, uint16_t offset,
                                     const uint8_t *value, uint16_t len,
                                     uint8_t command) {
    if (!(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED)))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t security = ble_gatt_server_security_error(server, a->permissions, 1);
    if (security) return security;
    if (a->flags & BLE_GATT_ATTRIBUTE_CCCD) {
        if (offset || len != 2) return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        uint16_t bits = ble_gatt_server_u16(value);
        uint16_t allowed = (uint16_t)((a->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                                      (a->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
        if (bits & (uint16_t)~allowed) return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;
        a->cccd = bits;
        return 0;
    }
    if (a->write) return a->write(a->context, offset, value, len, command);
    if (offset > a->value_capacity || len > a->value_capacity - offset)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (offset && offset + len > a->value_len)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (len) memcpy(a->value + offset, value, len);
    if (offset + len > a->value_len) a->value_len = offset + len;
    return 0;
}

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

// Process one complete ATT request PDU. Returns 1 when a response is present,
// 0 for commands/notifications that require no response, and -1 on bad args.
int ble_gatt_server_att(ble_gatt_server *server, const uint8_t *req,
                        uint16_t req_len, uint8_t *rsp, uint16_t rsp_capacity,
                        uint16_t *rsp_len) {
    if (!server || !req || !req_len || !rsp || !rsp_len) return -1;
    *rsp_len = 0;
    uint8_t op = req[0];
    uint16_t mtu = server->mtu;
    if (op == 0x02) { // Exchange MTU Request
        if (req_len != 3 || ble_gatt_server_u16(req + 1) < 23)
            return ble_gatt_server_error_rsp(op, 0, BLE_GATT_ATT_ERR_INVALID_PDU,
                                              rsp, rsp_capacity, rsp_len);
        if (server->mtu_exchanged)
            return ble_gatt_server_error_rsp(op, 0, 0x06, rsp, rsp_capacity, rsp_len);
        uint16_t peer_mtu = ble_gatt_server_u16(req + 1);
        server->mtu = peer_mtu < server->local_mtu ? peer_mtu : server->local_mtu;
        server->mtu_exchanged = 1;
        if (rsp_capacity < 3) return 0;
        rsp[0] = 0x03;
        ble_gatt_server_put_u16(rsp + 1, server->local_mtu);
        *rsp_len = 3;
        return 1;
    }
    if (op == 0x10 || op == 0x06 || op == 0x08) {
        uint8_t group = op == 0x10;
        uint8_t find = op == 0x06;
        uint16_t min_len = group ? 7 : find ? 7 : 7;
        if (req_len < min_len) goto invalid_pdu;
        uint16_t first = ble_gatt_server_u16(req + 1);
        uint16_t last = ble_gatt_server_u16(req + 3);
        uint8_t uuid_len = group ? (uint8_t)(req_len - 5) :
                           find ? (uint8_t)(req_len - 7) :
                           (uint8_t)(req_len - 5);
        if (first == 0 || first > last || !ble_gatt_uuid_valid(&(ble_gatt_uuid){uuid_len,{0}}))
            return ble_gatt_server_error_rsp(op, first, 0x01, rsp,
                                              rsp_capacity, rsp_len);
        ble_gatt_uuid type;
        if (!ble_gatt_server_uuid_from_wire(req + (find ? 5 : 5), 2, &type))
            goto invalid_pdu;
        ble_gatt_uuid sought;
        if (find) {
            uint8_t value_len = (uint8_t)(req_len - 7);
            if (!ble_gatt_server_uuid_from_wire(req + 7, value_len, &sought))
                goto invalid_pdu;
            if (ble_gatt_server_u16(type.value) != 0x2800 &&
                ble_gatt_server_u16(type.value) != 0x2801)
                return ble_gatt_server_error_rsp(op, first,
                    BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
            if (rsp_capacity < 5) return 0;
            rsp[0] = 0x07;
            uint16_t n = 1;
            for (uint16_t i = 0; i < server->count; i++) {
                ble_gatt_attribute *a = &server->attributes[i];
                if (a->handle < first || a->handle > last ||
                    !ble_gatt_uuid_equal(&a->uuid, &type) ||
                    a->value_len != sought.len ||
                    memcmp(a->value, sought.value, sought.len)) continue;
                if (n + 4 > rsp_capacity || n + 4 > mtu) break;
                ble_gatt_server_put_u16(rsp + n, a->handle); n += 2;
                uint16_t end = a->handle;
                for (uint16_t j = i + 1; j < server->count; j++) {
                    if (server->attributes[j].flags &
                        (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                         BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)) break;
                    end = server->attributes[j].handle;
                }
                ble_gatt_server_put_u16(rsp + n, end); n += 2;
            }
            if (n == 1) return ble_gatt_server_error_rsp(op, first,
                BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
            *rsp_len = n; return 1;
        }
        if (uuid_len != 2 && uuid_len != 16) goto invalid_pdu;
        if (!ble_gatt_server_uuid_from_wire(req + 5, uuid_len, &type)) goto invalid_pdu;
        if (group && type.len != 2) return ble_gatt_server_error_rsp(op, first,
            BLE_GATT_ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);
        uint8_t response_op = group ? 0x11 : 0x09;
        uint16_t n = group ? 2 : 2;
        uint8_t entry_len = 0;
        rsp[0] = response_op;
        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *a = &server->attributes[i];
            if (a->handle < first || a->handle > last) continue;
            if (group) {
                uint16_t group_type = ble_gatt_server_u16(type.value);
                if (!((group_type == 0x2800 &&
                       (a->flags & BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE)) ||
                      (group_type == 0x2801 &&
                       (a->flags & BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)))) continue;
            } else if (!ble_gatt_uuid_equal(&a->uuid, &type)) continue;
            uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
            uint16_t value_len = a->value_len;
            if (a->read) {
                uint16_t capacity = sizeof(value);
                if (ble_gatt_server_read(server, a, 0, value, &capacity)) continue;
                value_len = capacity;
            } else memcpy(value, a->value, value_len);
            uint8_t this_len = (uint8_t)(group ? 4 + value_len : 2 + value_len);
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
        uint8_t format = 0, n = 2;
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
    if (op == 0x12 || op == 0x52) { // Write Request / Write Command
        uint8_t command = op == 0x52;
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
        if (rsp_capacity < 1) return 0;
        rsp[0] = 0x13; *rsp_len = 1; return 1;
    }
    if (op & 0x40) return 0; // ATT commands never receive a response.
    return ble_gatt_server_error_rsp(op, req_len >= 3 ?
        ble_gatt_server_u16(req + 1) : 0,
        BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED, rsp, rsp_capacity, rsp_len);

invalid_pdu:
    return ble_gatt_server_error_rsp(op, 0, BLE_GATT_ATT_ERR_INVALID_PDU,
                                      rsp, rsp_capacity, rsp_len);
}

#endif
