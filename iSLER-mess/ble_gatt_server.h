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
#ifndef BLE_GATT_SERVER_VALUE_POOL_SIZE
#define BLE_GATT_SERVER_VALUE_POOL_SIZE 512
#endif
#ifndef BLE_GATT_SERVER_PREPARE_QUEUE_SIZE
#define BLE_GATT_SERVER_PREPARE_QUEUE_SIZE 8
#endif
#ifndef BLE_GATT_SERVER_PREPARE_BYTES
#define BLE_GATT_SERVER_PREPARE_BYTES 256
#endif
#ifndef BLE_GATT_SERVER_MTU_MAX
#define BLE_GATT_SERVER_MTU_MAX 517
#endif
#if BLE_GATT_SERVER_VALUE_POOL_SIZE > 65535
#error "BLE_GATT_SERVER_VALUE_POOL_SIZE must fit in uint16_t"
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
    BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES = 0x11,
    BLE_GATT_ATT_ERR_UNLIKELY_ERROR = 0x0e,
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
// `command` is nonzero for an ATT Write Command.
typedef uint8_t (*ble_gatt_write_fn)(void *context, uint16_t offset,
                                    const uint8_t *value, uint16_t len,
                                    uint8_t command);
// Prepare validates and stages one fragment without publishing it. Execute is
// called once at commit (commit=1) or cancel (commit=0); commit must not fail.
typedef uint8_t (*ble_gatt_prepare_fn)(void *context, uint16_t offset,
                                      const uint8_t *value, uint16_t len);
typedef void (*ble_gatt_execute_fn)(void *context, uint8_t commit);

typedef struct ble_gatt_attribute {
    uint16_t handle;
    ble_gatt_uuid uuid;
    uint16_t permissions;
    uint8_t properties;
    uint8_t flags;
    uint16_t value_len, value_capacity;
    uint16_t value_offset;
    uint16_t parent_handle;
    uint16_t cccd;
    ble_gatt_read_fn read;
    ble_gatt_write_fn write;
    ble_gatt_prepare_fn prepare;
    ble_gatt_execute_fn execute;
    void *context;
} ble_gatt_attribute;

typedef struct {
    uint16_t handle, offset, len, data_offset;
} ble_gatt_prepared_write;

typedef struct {
    ble_gatt_attribute attributes[BLE_GATT_SERVER_MAX_ATTRIBUTES];
    uint16_t count, next_handle, mtu, local_mtu;
    uint16_t value_used;
    uint8_t value_pool[BLE_GATT_SERVER_VALUE_POOL_SIZE];
    ble_gatt_prepared_write prepared[BLE_GATT_SERVER_PREPARE_QUEUE_SIZE];
    uint8_t prepare_data[BLE_GATT_SERVER_PREPARE_BYTES];
    uint16_t prepare_count, prepare_used;
    uint8_t mtu_exchanged, encrypted, authenticated, indication_pending;
    uint16_t indication_handle;
} ble_gatt_server;

static void ble_gatt_server_prepare_cancel_all(ble_gatt_server *server,
                                                uint16_t extra_handle);

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

static uint8_t *ble_gatt_attribute_value(ble_gatt_server *server,
                                         ble_gatt_attribute *attribute) {
    if (!attribute->value_capacity ||
        attribute->value_offset > server->value_used ||
        attribute->value_capacity > server->value_used - attribute->value_offset)
        return NULL;
    return server->value_pool + attribute->value_offset;
}

static inline void ble_gatt_server_init(ble_gatt_server *server, uint16_t local_mtu) {
    if (!server) return;
    memset(server, 0, sizeof(*server));
    if (local_mtu < 23) local_mtu = 23;
    if (local_mtu > BLE_GATT_SERVER_MTU_MAX)
        local_mtu = BLE_GATT_SERVER_MTU_MAX;
    server->local_mtu = local_mtu;
    server->mtu = 23;
    server->next_handle = 1;
}

static inline void ble_gatt_server_link_reset(ble_gatt_server *server) {
    if (!server) return;
    ble_gatt_server_prepare_cancel_all(server, 0);
    server->mtu = 23;
    server->mtu_exchanged = 0;
    server->encrypted = server->authenticated = 0;
    server->indication_pending = 0;
    server->indication_handle = 0;
    memset(server->prepare_data, 0, sizeof(server->prepare_data));
    server->prepare_count = server->prepare_used = 0;
    for (uint16_t i = 0; i < server->count; i++)
        if (server->attributes[i].flags & BLE_GATT_ATTRIBUTE_CCCD)
            server->attributes[i].cccd = 0;
}

static inline void ble_gatt_server_set_security(ble_gatt_server *server, int encrypted,
                                  int authenticated) {
    if (!server) return;
    server->encrypted = encrypted != 0 || authenticated != 0;
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

static inline int ble_gatt_server_set_transaction_callbacks(
    ble_gatt_server *server, uint16_t handle,
    ble_gatt_prepare_fn prepare, ble_gatt_execute_fn execute) {
    if (!server) return 0;
    ble_gatt_attribute *a = ble_gatt_server_find(server, handle);
    if (!a || !prepare || !execute || server->prepare_count ||
        !(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED))) return 0;
    a->prepare = prepare;
    a->execute = execute;
    return 1;
}

static void ble_gatt_server_prepare_cancel_all(ble_gatt_server *server,
                                                uint16_t extra_handle) {
    if (!server) return;
    uint8_t extra_seen = 0;
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        uint16_t handle = server->prepared[i].handle;
        uint8_t first = 1;
        for (uint16_t j = 0; j < i; j++)
            if (server->prepared[j].handle == handle) first = 0;
        if (!first) continue;
        if (handle == extra_handle) extra_seen = 1;
        ble_gatt_attribute *a = ble_gatt_server_find(server, handle);
        if (a && a->execute) a->execute(a->context, 0);
    }
    if (extra_handle && !extra_seen) {
        ble_gatt_attribute *a = ble_gatt_server_find(server, extra_handle);
        if (a && a->execute) a->execute(a->context, 0);
    }
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
        value_capacity > BLE_GATT_SERVER_VALUE_POOL_SIZE - server->value_used ||
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
    a->value_offset = server->value_used;
    a->read = read;
    a->write = write;
    a->context = context;
    if (value_len) memcpy(server->value_pool + a->value_offset, value, value_len);
    server->value_used += value_capacity;
    if (handle_out) *handle_out = a->handle;
    return 1;
}

static inline int ble_gatt_server_add_attribute(ble_gatt_server *server,
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

static inline int ble_gatt_server_add_service(ble_gatt_server *server,
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

static inline int ble_gatt_server_add_characteristic(ble_gatt_server *server,
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
    uint16_t value_checkpoint = server->value_used;
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
        server->value_used = value_checkpoint;
        return 0;
    }
    return 1;
}

static inline int ble_gatt_server_add_descriptor(ble_gatt_server *server,
                                   const ble_gatt_uuid *uuid,
                                   uint16_t permissions, const uint8_t *value,
                                   uint16_t value_len, uint16_t value_capacity,
                                   ble_gatt_read_fn read,
                                   ble_gatt_write_fn write, void *context,
                                   uint16_t *handle_out) {
    if (uuid && uuid->len == 2 && ble_gatt_server_u16(uuid->value) == 0x2902) {
        if (!server || !server->count ||
            !(server->attributes[server->count - 1].properties &
              (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE))) return 0;
        uint8_t zero[2] = {0, 0};
        int added = ble_gatt_server_add(server, uuid,
            permissions | BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, 0,
            BLE_GATT_ATTRIBUTE_CCCD, zero, 2, 2, NULL, NULL, context,
            handle_out);
        if (added) {
            server->attributes[server->count - 1].properties =
                server->attributes[server->count - 2].properties;
            server->attributes[server->count - 1].parent_handle =
                server->attributes[server->count - 2].handle;
        }
        return added;
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
    if (a->read) {
        uint16_t capacity = *out_len;
        uint8_t error = a->read(a->context, offset, out, out_len);
        if (!error && *out_len > capacity)
            return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        return error;
    }
    if (offset > a->value_len) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    uint16_t size = (uint16_t)(a->value_len - offset);
    if (size > *out_len) size = *out_len;
    if (size) memcpy(out, ble_gatt_attribute_value(server, a) + offset, size);
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
    if (len) memcpy(ble_gatt_attribute_value(server, a) + offset, value, len);
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

static ble_gatt_attribute *ble_gatt_server_cccd_for(
    ble_gatt_server *server, uint16_t value_handle) {
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *a = &server->attributes[i];
        if ((a->flags & BLE_GATT_ATTRIBUTE_CCCD) &&
            a->parent_handle == value_handle) return a;
    }
    return NULL;
}

// Build one Handle Value Notification. The application supplies the current
// value; notifications are unacknowledged and limited to ATT_MTU - 3 bytes.
static inline int ble_gatt_server_notify(ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    uint8_t *att, uint16_t att_capacity, uint16_t *att_len) {
    if (!server || !att || !att_len || (value_len && !value)) return -1;
    *att_len = 0;
    ble_gatt_attribute *characteristic =
        ble_gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = ble_gatt_server_cccd_for(server, value_handle);
    if (!characteristic || !(characteristic->properties & BLE_GATT_PROP_NOTIFY) ||
        !cccd || !(cccd->cccd & 1)) return 0;
    if (ble_gatt_server_security_error(server, characteristic->permissions, 0))
        return 0;
    if (value_len > server->mtu - 3 || att_capacity < value_len + 3) return 0;
    att[0] = 0x1b;
    ble_gatt_server_put_u16(att + 1, value_handle);
    if (value_len) memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    return 1;
}

// Build one Handle Value Indication. Only one indication may await confirmation.
static inline int ble_gatt_server_indicate(ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    uint8_t *att, uint16_t att_capacity, uint16_t *att_len) {
    if (!server || !att || !att_len || (value_len && !value)) return -1;
    *att_len = 0;
    ble_gatt_attribute *characteristic =
        ble_gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = ble_gatt_server_cccd_for(server, value_handle);
    if (!characteristic || !(characteristic->properties & BLE_GATT_PROP_INDICATE) ||
        !cccd || !(cccd->cccd & 2) || server->indication_pending) return 0;
    if (ble_gatt_server_security_error(server, characteristic->permissions, 0))
        return 0;
    if (value_len > server->mtu - 3 || att_capacity < value_len + 3) return 0;
    att[0] = 0x1d;
    ble_gatt_server_put_u16(att + 1, value_handle);
    if (value_len) memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    server->indication_pending = 1;
    server->indication_handle = value_handle;
    return 1;
}

static void ble_gatt_server_prepare_clear(ble_gatt_server *server) {
    memset(server->prepare_data, 0, server->prepare_used);
    server->prepare_count = 0;
    server->prepare_used = 0;
}

static uint8_t ble_gatt_server_prepare_validate(ble_gatt_server *server,
    ble_gatt_attribute *a, uint16_t offset, uint16_t len) {
    if (!(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED)))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t security = ble_gatt_server_security_error(server, a->permissions, 1);
    if (security) return security;
    if (a->prepare || a->execute)
        return (a->prepare && a->execute) ? 0 :
            BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (a->write) return BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (offset > a->value_capacity || len > a->value_capacity - offset)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    return 0;
}

static uint8_t ble_gatt_server_prepare_execute(ble_gatt_server *server) {
    // Validate the full batch before changing any attribute.
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        ble_gatt_prepared_write *p = &server->prepared[i];
        ble_gatt_attribute *a = ble_gatt_server_find(server, p->handle);
        if (!a) return BLE_GATT_ATT_ERR_INVALID_HANDLE;
        uint8_t error = ble_gatt_server_prepare_validate(server, a,
                                                          p->offset, p->len);
        if (error) return error;
        if (a->execute) continue;
    }
    // New bytes must be covered continuously from the old value length; this
    // prevents stale bytes from becoming visible through gaps in queued writes.
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        ble_gatt_prepared_write *p = &server->prepared[i];
        ble_gatt_attribute *a = ble_gatt_server_find(server, p->handle);
        if (a->execute) continue;
        uint16_t end = a->value_len;
        for (uint16_t j = 0; j < server->prepare_count; j++)
            if (server->prepared[j].handle == p->handle &&
                server->prepared[j].offset + server->prepared[j].len > end)
                end = server->prepared[j].offset + server->prepared[j].len;
        uint16_t covered = a->value_len;
        while (covered < end) {
            uint16_t next = covered;
            for (uint16_t j = 0; j < server->prepare_count; j++) {
                ble_gatt_prepared_write *part = &server->prepared[j];
                uint16_t part_end = part->offset + part->len;
                if (part->handle == p->handle && part->offset <= covered &&
                    part_end > next) next = part_end;
            }
            if (next == covered) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
            covered = next;
        }
    }
    // Callback commits are contractually infallible after their prepare phase.
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        uint16_t handle = server->prepared[i].handle;
        uint8_t first = 1;
        for (uint16_t j = 0; j < i; j++)
            if (server->prepared[j].handle == handle) first = 0;
        if (!first) continue;
        ble_gatt_attribute *a = ble_gatt_server_find(server, handle);
        if (a && a->execute) a->execute(a->context, 1);
    }
    // Static commits are bounded copies into registered storage.
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        ble_gatt_prepared_write *p = &server->prepared[i];
        ble_gatt_attribute *a = ble_gatt_server_find(server, p->handle);
        if (a->execute) continue;
        uint8_t *value = ble_gatt_attribute_value(server, a);
        if (p->len) memcpy(value + p->offset,
                           server->prepare_data + p->data_offset, p->len);
        if (p->offset + p->len > a->value_len)
            a->value_len = p->offset + p->len;
    }
    return 0;
}

// Process one complete ATT request PDU. Returns 1 when a response is present,
// 0 for commands/notifications that require no response, and -1 on bad args.
static inline int ble_gatt_server_att(ble_gatt_server *server, const uint8_t *req,
                        uint16_t req_len, uint8_t *rsp, uint16_t rsp_capacity,
                        uint16_t *rsp_len) {
    if (!server || !req || !req_len || !rsp || !rsp_len) return -1;
    *rsp_len = 0;
    uint8_t op = req[0];
    uint16_t mtu = server->mtu;
    if (req_len > mtu) goto invalid_pdu;
    if (op == 0x1e) { // Handle Value Confirmation
        if (req_len != 1) goto invalid_pdu;
        server->indication_pending = 0;
        server->indication_handle = 0;
        return 0;
    }
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
        if ((group && req_len != 7) || (find && req_len != 9 && req_len != 23) ||
            (!group && !find && req_len != 7 && req_len != 21)) goto invalid_pdu;
        uint16_t first = ble_gatt_server_u16(req + 1);
        uint16_t last = ble_gatt_server_u16(req + 3);
        uint8_t uuid_len = group ? (uint8_t)(req_len - 5) :
                           find ? (uint8_t)(req_len - 7) :
                           (uint8_t)(req_len - 5);
        if (first == 0 || first > last)
            return ble_gatt_server_error_rsp(op, first, 0x01, rsp,
                                              rsp_capacity, rsp_len);
        if (!ble_gatt_uuid_valid(&(ble_gatt_uuid){uuid_len,{0}})) goto invalid_pdu;
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
                    memcmp(ble_gatt_attribute_value(server, a),
                           sought.value, sought.len)) continue;
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
    if (op == 0x0e || op == 0x20) { // Read Multiple / Read Multiple Variable
        uint8_t variable = op == 0x20;
        if (req_len < 5 || ((req_len - 1) & 1)) goto invalid_pdu;
        uint16_t n = 1;
        rsp[0] = variable ? 0x21 : 0x0f;
        for (uint16_t offset = 1; offset < req_len; offset += 2) {
            uint16_t h = ble_gatt_server_u16(req + offset);
            ble_gatt_attribute *a = ble_gatt_server_find(server, h);
            if (!a) return ble_gatt_server_error_rsp(op, h,
                BLE_GATT_ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
            uint16_t fixed = variable ? 2 : 0;
            if (n + fixed > mtu || n + fixed > rsp_capacity) break;
            uint16_t len = (uint16_t)((mtu < rsp_capacity ? mtu : rsp_capacity) - n - fixed);
            uint8_t error = ble_gatt_server_read(server, a, 0, rsp + n + fixed, &len);
            if (error) return ble_gatt_server_error_rsp(op, h, error,
                                                         rsp, rsp_capacity, rsp_len);
            if (variable) {
                ble_gatt_server_put_u16(rsp + n, len);
                n += 2;
            }
            n += len;
            if (n == mtu || n == rsp_capacity) break;
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
                                                          value_len);
        if (error) return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                     rsp_capacity, rsp_len);
        if (server->prepare_count >= BLE_GATT_SERVER_PREPARE_QUEUE_SIZE ||
            value_len > BLE_GATT_SERVER_PREPARE_BYTES - server->prepare_used)
            return ble_gatt_server_error_rsp(op, h,
                BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES, rsp,
                rsp_capacity, rsp_len);
        if (a->prepare) {
            error = a->prepare(a->context, offset, req + 5, value_len);
            if (error) {
                ble_gatt_server_prepare_cancel_all(server, h);
                ble_gatt_server_prepare_clear(server);
                return ble_gatt_server_error_rsp(op, h, error, rsp,
                                                  rsp_capacity, rsp_len);
            }
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
            uint8_t error = ble_gatt_server_prepare_execute(server);
            if (error) {
                ble_gatt_server_prepare_cancel_all(server, 0);
                ble_gatt_server_prepare_clear(server);
                return ble_gatt_server_error_rsp(op, 0, error, rsp,
                                                  rsp_capacity, rsp_len);
            }
        } else ble_gatt_server_prepare_cancel_all(server, 0);
        ble_gatt_server_prepare_clear(server);
        rsp[0] = 0x19;
        *rsp_len = 1;
        return 1;
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
