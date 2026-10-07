#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

// Transport-independent GATT/ATT server core. Applications register attributes
// and call ble_gatt_server_att() with each complete ATT PDU; the GAP/L2CAP
// adapter is responsible for carrying the returned response.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt_crypto.h"

#ifndef BLE_GATT_ATT_VALUE_MAX
#define BLE_GATT_ATT_VALUE_MAX 512u
#endif
#ifndef BLE_GATT_SERVER_MAX_ATTRIBUTES
#define BLE_GATT_SERVER_MAX_ATTRIBUTES 64
#endif
#ifndef BLE_GATT_SERVER_VALUE_MAX
#define BLE_GATT_SERVER_VALUE_MAX 512
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
#ifndef BLE_GATT_SERVER_EVENT_QUEUE_SIZE
#define BLE_GATT_SERVER_EVENT_QUEUE_SIZE 8
#endif
#ifndef BLE_GATT_SERVER_EVENT_BYTES
#define BLE_GATT_SERVER_EVENT_BYTES 512
#endif
#ifndef BLE_GATT_SERVER_INDICATION_TIMEOUT_MS
#define BLE_GATT_SERVER_INDICATION_TIMEOUT_MS 30000u
#endif
#ifndef BLE_GATT_SERVER_MTU_MAX
#define BLE_GATT_SERVER_MTU_MAX 517
#endif
#if BLE_GATT_SERVER_VALUE_MAX > BLE_GATT_ATT_VALUE_MAX
#error "BLE_GATT_SERVER_VALUE_MAX cannot exceed the ATT attribute value limit"
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
    BLE_GATT_PERM_WRITE_AUTHENTICATED = 1u << 5,
    BLE_GATT_PERM_READ_AUTHORIZED = 1u << 6,
    BLE_GATT_PERM_WRITE_AUTHORIZED = 1u << 7,
    BLE_GATT_PERM_WRITE_SIGNED = 1u << 8
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
    BLE_GATT_ATTRIBUTE_CCCD = 1u << 3,
    BLE_GATT_ATTRIBUTE_INCLUDED_SERVICE = 1u << 4,
    BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH = 1u << 5
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
    BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION = 0x08,
    BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION = 0x05,
    BLE_GATT_ATT_ERR_UNSUPPORTED_GROUP_TYPE = 0x10,
    BLE_GATT_ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT = 0x0c,
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
// Prepare validates and stages one fragment without publishing it. If it
// returns an error, it must leave its staging state unchanged. Execute is
// called once at commit (commit=1) or cancel (commit=0); commit must not fail.
typedef uint8_t (*ble_gatt_prepare_fn)(void *context, uint16_t offset,
                                      const uint8_t *value, uint16_t len);
typedef void (*ble_gatt_execute_fn)(void *context, uint8_t commit);
// Verify the exact ATT Signed Write Command bytes excluding its final
// 12-byte authentication signature. The callback must check and persist the
// peer's sign counter to reject replayed writes.
typedef int (*ble_gatt_signed_verify_fn)(void *context,
    const uint8_t *signed_pdu, uint16_t signed_len,
    const uint8_t signature[12]);
typedef int (*ble_gatt_authorize_fn)(void *context, uint16_t handle,
                                     uint8_t write);
typedef uint16_t (*ble_gatt_cccd_load_fn)(void *context,
                                          uint16_t value_handle);
typedef void (*ble_gatt_cccd_store_fn)(void *context, uint16_t value_handle,
                                       uint16_t configuration);
typedef int (*ble_gatt_database_hash_load_fn)(void *context, uint8_t hash[16]);
typedef void (*ble_gatt_database_hash_store_fn)(void *context,
                                                const uint8_t hash[16]);

typedef struct ble_gatt_attribute {
    uint16_t handle;
    ble_gatt_uuid uuid;
    uint16_t permissions;
    uint8_t min_key_size;
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
    uint16_t service_handle;
    uint16_t service_changed_handle;
    uint16_t service_changed_cccd_handle;
    uint16_t database_hash_handle;
} ble_gatt_standard_service_handles;

typedef struct {
    uint16_t handle, offset, len, data_offset;
} ble_gatt_prepared_write;

typedef struct {
    uint16_t handle, len, data_offset;
    uint8_t indication;
} ble_gatt_server_event;

typedef struct {
    ble_gatt_attribute attributes[BLE_GATT_SERVER_MAX_ATTRIBUTES];
    uint16_t count, next_handle, mtu, local_mtu;
    uint16_t value_used;
    uint8_t value_pool[BLE_GATT_SERVER_VALUE_POOL_SIZE];
    ble_gatt_prepared_write prepared[BLE_GATT_SERVER_PREPARE_QUEUE_SIZE];
    uint8_t prepare_data[BLE_GATT_SERVER_PREPARE_BYTES];
    uint16_t prepare_count, prepare_used;
    ble_gatt_server_event events[BLE_GATT_SERVER_EVENT_QUEUE_SIZE];
    uint8_t event_data[BLE_GATT_SERVER_EVENT_BYTES];
    uint16_t event_count, event_used;
    uint8_t database_sealed;
    uint8_t mtu_exchanged, encrypted, authenticated, indication_pending;
    uint8_t indication_timeout_armed;
    uint8_t encryption_key_size;
    uint8_t database_hash[16];
    uint8_t database_hash_available, database_hash_update_pending;
    ble_gatt_signed_verify_fn signed_verify;
    void *signed_context;
    ble_gatt_authorize_fn authorize;
    void *authorize_context;
    ble_gatt_cccd_load_fn cccd_load;
    ble_gatt_cccd_store_fn cccd_store;
    void *cccd_context;
    ble_gatt_database_hash_load_fn database_hash_load;
    ble_gatt_database_hash_store_fn database_hash_store;
    void *database_hash_context;
    uint16_t indication_handle;
    uint32_t indication_started_ms;
} ble_gatt_server;

static void ble_gatt_server_prepare_cancel_all(ble_gatt_server *server,
                                                uint16_t extra_handle);
static ble_gatt_attribute *ble_gatt_server_find(ble_gatt_server *server,
                                                uint16_t handle);
static inline int ble_gatt_server_check_database_version(
    ble_gatt_server *server);

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
    if (!ble_gatt_uuid_valid(a) || !ble_gatt_uuid_valid(b)) return 0;
    if (a->len == b->len) return !memcmp(a->value, b->value, a->len);
    const ble_gatt_uuid *short_uuid = a->len == 2 ? a : b;
    const ble_gatt_uuid *long_uuid = a->len == 16 ? a : b;
    static const uint8_t bluetooth_base_prefix[14] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00,
        0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00
    };
    return !memcmp(long_uuid->value, bluetooth_base_prefix,
                   sizeof(bluetooth_base_prefix)) &&
           !memcmp(long_uuid->value + 14, short_uuid->value, 2);
}

static uint16_t ble_gatt_uuid_assigned16(const ble_gatt_uuid *uuid) {
    if (uuid->len == 2) return ble_gatt_server_u16(uuid->value);
    if (uuid->len == 16) return ble_gatt_server_u16(uuid->value + 14);
    return 0;
}

static uint8_t *ble_gatt_attribute_value(ble_gatt_server *server,
                                         ble_gatt_attribute *attribute) {
    if (!attribute->value_capacity ||
        attribute->value_offset > server->value_used ||
        attribute->value_capacity > server->value_used - attribute->value_offset)
        return NULL;
    return server->value_pool + attribute->value_offset;
}

// Return 1 when only handle/type are hashed, 2 when the value is also hashed,
// and 0 for attribute types omitted by Core GATT Database Hash calculation.
static uint8_t ble_gatt_server_hash_attribute_kind(
    const ble_gatt_attribute *attribute) {
    static const uint16_t value_types[] = {
        0x2800, 0x2801, 0x2802, 0x2803, 0x2900
    };
    static const uint16_t handle_only_types[] = {
        0x2901, 0x2902, 0x2903, 0x2904, 0x2905
    };
    for (size_t i = 0; i < sizeof(value_types) / sizeof(value_types[0]); i++) {
        uint16_t value = value_types[i];
        ble_gatt_uuid type = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
        if (ble_gatt_uuid_equal(&attribute->uuid, &type)) return 2;
    }
    for (size_t i = 0; i < sizeof(handle_only_types) /
         sizeof(handle_only_types[0]); i++) {
        uint16_t value = handle_only_types[i];
        ble_gatt_uuid type = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
        if (ble_gatt_uuid_equal(&attribute->uuid, &type)) return 1;
    }
    return 0;
}

// Compute the Core GATT Database Hash over the currently registered
// attributes. The output uses the AES-CMAC byte order defined by Core GATT.
static inline int ble_gatt_server_compute_database_hash(
    ble_gatt_server *server, uint8_t hash[16]) {
    if (!server || !hash) return 0;
    static const uint8_t zero_key[16] = {0};
    ble_gatt_cmac cmac;
    ble_gatt_cmac_init(&cmac, zero_key);
    for (uint16_t i = 0; i < server->count; i++) {
        const ble_gatt_attribute *attribute = &server->attributes[i];
        uint8_t kind = ble_gatt_server_hash_attribute_kind(attribute);
        if (!kind) continue;
        uint8_t handle[2] = {(uint8_t)attribute->handle,
                             (uint8_t)(attribute->handle >> 8)};
        ble_gatt_cmac_update(&cmac, handle, sizeof(handle));
        ble_gatt_cmac_update(&cmac, attribute->uuid.value,
                             attribute->uuid.len);
        if (kind != 2 || !attribute->value_len) continue;
        if (attribute->value_len > BLE_GATT_ATT_VALUE_MAX) {
            memset(&cmac, 0, sizeof(cmac));
            return 0;
        }
        uint8_t *value = ble_gatt_attribute_value(server,
            (ble_gatt_attribute *)attribute);
        if (!value) {
            memset(&cmac, 0, sizeof(cmac));
            return 0;
        }
        ble_gatt_cmac_update(&cmac, value, attribute->value_len);
    }
    ble_gatt_cmac_final(&cmac, hash);
    return 1;
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
    server->encryption_key_size = 0;
    server->indication_pending = 0;
    server->indication_timeout_armed = 0;
    server->indication_handle = 0;
    server->indication_started_ms = 0;
    server->database_hash_update_pending = 0;
    memset(server->event_data, 0, server->event_used);
    server->event_count = server->event_used = 0;
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
    if (!server->encrypted) server->encryption_key_size = 0;
}

static inline void ble_gatt_server_set_encryption_key_size(
    ble_gatt_server *server, uint8_t octets) {
    if (server) server->encryption_key_size =
        octets >= 7 && octets <= 16 ? octets : 0;
}

// Set the minimum LE encryption key size required for an attribute (7-16
// octets). Configure it while building the database, before sealing it.
static inline int ble_gatt_server_set_min_key_size(ble_gatt_server *server,
    uint16_t handle, uint8_t octets) {
    if (!server || server->database_sealed || octets < 7 || octets > 16)
        return 0;
    ble_gatt_attribute *attribute = ble_gatt_server_find(server, handle);
    if (!attribute) return 0;
    attribute->min_key_size = octets;
    return 1;
}

// Configure server-owned attribute storage as variable or fixed length.
// Storage with spare capacity is variable by default; a write callback owns
// its own length semantics and cannot be reconfigured here.
static inline int ble_gatt_server_set_variable_length(ble_gatt_server *server,
    uint16_t handle, int variable) {
    if (!server || server->database_sealed) return 0;
    ble_gatt_attribute *attribute = ble_gatt_server_find(server, handle);
    if (!attribute || attribute->write || attribute->prepare ||
        attribute->execute ||
        (attribute->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
         BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE | BLE_GATT_ATTRIBUTE_CCCD |
         BLE_GATT_ATTRIBUTE_INCLUDED_SERVICE))) return 0;
    if (variable) attribute->flags |= BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
    else attribute->flags &= (uint8_t)~BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
    return 1;
}

static ble_gatt_attribute *ble_gatt_server_find(ble_gatt_server *server,
                                                 uint16_t handle) {
    if (!server || !handle) return NULL;
    for (uint16_t i = 0; i < server->count; i++)
        if (server->attributes[i].handle == handle)
            return &server->attributes[i];
    return NULL;
}

static int ble_gatt_server_is_service_changed(ble_gatt_server *server,
                                               uint16_t handle) {
    ble_gatt_attribute *attribute = ble_gatt_server_find(server, handle);
    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    return attribute && (attribute->properties & BLE_GATT_PROP_INDICATE) &&
        ble_gatt_uuid_equal(&attribute->uuid, &changed_uuid);
}

static inline int ble_gatt_server_set_transaction_callbacks(
    ble_gatt_server *server, uint16_t handle,
    ble_gatt_prepare_fn prepare, ble_gatt_execute_fn execute) {
    if (!server) return 0;
    ble_gatt_attribute *a = ble_gatt_server_find(server, handle);
    if (!a || !prepare || !execute || server->prepare_count ||
        !(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED |
                            BLE_GATT_PERM_WRITE_AUTHORIZED))) return 0;
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
    if (!server || server->database_sealed || !ble_gatt_uuid_valid(uuid) ||
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
    if (value_capacity > value_len)
        a->flags |= BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
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

static inline int ble_gatt_server_seal_database(ble_gatt_server *server) {
    if (!server || server->database_sealed) return server != NULL;
    const uint16_t attribute_read_permissions = BLE_GATT_PERM_READ |
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHENTICATED |
        BLE_GATT_PERM_READ_AUTHORIZED;
    const uint16_t write_permissions = BLE_GATT_PERM_WRITE |
        BLE_GATT_PERM_WRITE_ENCRYPTED | BLE_GATT_PERM_WRITE_AUTHENTICATED |
        BLE_GATT_PERM_WRITE_AUTHORIZED;
    const uint16_t secured_read_permissions = BLE_GATT_PERM_READ_ENCRYPTED |
        BLE_GATT_PERM_READ_AUTHENTICATED | BLE_GATT_PERM_READ_AUTHORIZED;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *declaration = &server->attributes[i];
        if (!(declaration->flags & BLE_GATT_ATTRIBUTE_CHARACTERISTIC)) continue;
        uint8_t *declaration_value = ble_gatt_attribute_value(server,
                                                               declaration);
        if (!declaration_value || declaration->value_len < 5) return 0;
        uint8_t properties = declaration_value[0];
        uint16_t value_handle = ble_gatt_server_u16(declaration_value + 1);
        uint8_t uuid_len = (uint8_t)(declaration->value_len - 3);
        ble_gatt_attribute *value_attribute = i + 1 < server->count ?
            &server->attributes[i + 1] : NULL;
        if (value_handle != declaration->handle + 1 ||
            (uuid_len != 2 && uuid_len != 16) || !value_attribute ||
            value_attribute->handle != value_handle ||
            value_attribute->properties != properties ||
            value_attribute->uuid.len != uuid_len ||
            memcmp(value_attribute->uuid.value,
                   declaration_value + 3, uuid_len)) return 0;
        if (!!(properties & BLE_GATT_PROP_READ) !=
                !!(value_attribute->permissions & attribute_read_permissions) ||
            !!(properties & (BLE_GATT_PROP_WRITE |
                            BLE_GATT_PROP_WRITE_NO_RSP)) !=
                !!(value_attribute->permissions & write_permissions) ||
            !!(properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE) !=
                !!(value_attribute->permissions & BLE_GATT_PERM_WRITE_SIGNED))
            return 0;

        uint8_t cccd_count = 0, extended_count = 0, server_config_count = 0;
        for (uint16_t j = i + 2; j < server->count; j++) {
            ble_gatt_attribute *candidate = &server->attributes[j];
            if (candidate->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE |
                BLE_GATT_ATTRIBUTE_CHARACTERISTIC)) break;
            uint16_t type = ble_gatt_uuid_assigned16(&candidate->uuid);
            if (type != 0x2900 && type != 0x2902 && type != 0x2903) continue;
            if (candidate->value_len != 2) return 0;
            const uint8_t *descriptor_value =
                ble_gatt_attribute_value(server, candidate);
            if (!descriptor_value) return 0;
            uint16_t bits = ble_gatt_server_u16(descriptor_value);
            if (type == 0x2902) {
                uint16_t allowed = (uint16_t)(
                    (properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                    (properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
                if (++cccd_count > 1 ||
                    !(properties & (BLE_GATT_PROP_NOTIFY |
                                    BLE_GATT_PROP_INDICATE)) ||
                    !(candidate->flags & BLE_GATT_ATTRIBUTE_CCCD) ||
                    candidate->parent_handle != value_handle ||
                    !(candidate->permissions & BLE_GATT_PERM_READ) ||
                    (candidate->permissions & secured_read_permissions) ||
                    (candidate->permissions & write_permissions) == 0 ||
                    (bits & (uint16_t)~allowed)) return 0;
            } else if (type == 0x2900) {
                if (++extended_count > 1 ||
                    !(properties & BLE_GATT_PROP_EXTENDED) ||
                    candidate->permissions != BLE_GATT_PERM_READ ||
                    (bits & (uint16_t)~0x0003)) return 0;
            } else {
                if (++server_config_count > 1 ||
                    !(properties & BLE_GATT_PROP_BROADCAST) ||
                    !(candidate->permissions & BLE_GATT_PERM_READ) ||
                    (candidate->permissions & secured_read_permissions) ||
                    (candidate->permissions & write_permissions) == 0 ||
                    (bits & (uint16_t)~0x0001) ||
                    ((bits & 1) && !(properties & BLE_GATT_PROP_BROADCAST)))
                    return 0;
            }
        }

        uint16_t required[3];
        uint8_t required_count = 0;
        if (properties & (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE))
            required[required_count++] = 0x2902;
        if (properties & BLE_GATT_PROP_EXTENDED)
            required[required_count++] = 0x2900;
        if (properties & BLE_GATT_PROP_BROADCAST)
            required[required_count++] = 0x2903;
        for (uint8_t r = 0; r < required_count; r++) {
            uint8_t matches = 0;
            for (uint16_t j = i + 2; j < server->count; j++) {
                ble_gatt_attribute *candidate = &server->attributes[j];
                if (candidate->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                    BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE |
                    BLE_GATT_ATTRIBUTE_CHARACTERISTIC)) break;
                if (ble_gatt_uuid_assigned16(&candidate->uuid) != required[r])
                    continue;
                if (required[r] == 0x2902 &&
                    (!(candidate->flags & BLE_GATT_ATTRIBUTE_CCCD) ||
                     candidate->parent_handle != value_handle)) return 0;
                if (required[r] == 0x2900 || required[r] == 0x2903) {
                    if (candidate->value_len != 2) return 0;
                    const uint8_t *descriptor_value =
                        ble_gatt_attribute_value(server, candidate);
                    if (!descriptor_value) return 0;
                    uint16_t bits = ble_gatt_server_u16(descriptor_value);
                    uint16_t allowed = required[r] == 0x2900 ? 0x0003 : 0x0001;
                    if ((bits & (uint16_t)~allowed) ||
                        (required[r] == 0x2903 && (bits & 1) &&
                         !(properties & BLE_GATT_PROP_BROADCAST))) return 0;
                }
                matches++;
            }
            if (matches != 1) return 0;
        }
    }
    ble_gatt_attribute *database_hash = NULL;
    ble_gatt_uuid database_hash_uuid = {2, {0x2a, 0x2b}};
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attribute = &server->attributes[i];
        if (!ble_gatt_uuid_equal(&attribute->uuid, &database_hash_uuid))
            continue;
        if (database_hash || i == 0 ||
            !(server->attributes[i - 1].flags & BLE_GATT_ATTRIBUTE_CHARACTERISTIC) ||
            attribute->properties != BLE_GATT_PROP_READ ||
            attribute->permissions != BLE_GATT_PERM_READ ||
            attribute->value_len != 16 || attribute->value_capacity != 16 ||
            attribute->read || attribute->write || attribute->prepare ||
            attribute->execute) return 0;
        database_hash = attribute;
    }
    if (database_hash) {
        uint8_t hash[16];
        if (!ble_gatt_server_compute_database_hash(server, hash)) return 0;
        memcpy(ble_gatt_attribute_value(server, database_hash), hash,
               sizeof(hash));
        memcpy(server->database_hash, hash, sizeof(hash));
        server->database_hash_available = 1;
    }
    server->database_sealed = 1;
    return 1;
}

static inline void ble_gatt_server_set_signed_write_verifier(
    ble_gatt_server *server, ble_gatt_signed_verify_fn verify, void *context) {
    if (!server) return;
    server->signed_verify = verify;
    server->signed_context = context;
}

static inline void ble_gatt_server_set_authorizer(ble_gatt_server *server,
    ble_gatt_authorize_fn authorize, void *context) {
    if (!server) return;
    server->authorize = authorize;
    server->authorize_context = context;
}

// App-owned CCCD persistence. Callbacks use their context to identify the
// current peer; load returns zero for peers without a bonded stored value.
static inline void ble_gatt_server_set_cccd_persistence(ble_gatt_server *server,
    ble_gatt_cccd_load_fn load, ble_gatt_cccd_store_fn store, void *context) {
    if (!server) return;
    server->cccd_load = load;
    server->cccd_store = store;
    server->cccd_context = context;
}

// Optional per-peer database-version persistence. Install callbacks with a
// context identifying the current bonded peer before its connection starts.
// The new hash is stored after a Service Changed indication is confirmed.
static inline void ble_gatt_server_set_database_hash_persistence(
    ble_gatt_server *server, ble_gatt_database_hash_load_fn load,
    ble_gatt_database_hash_store_fn store, void *context) {
    if (!server || (!!load != !!store)) return;
    server->database_hash_load = load;
    server->database_hash_store = store;
    server->database_hash_context = context;
}

// Server-owned static storage is variable length when value_capacity exceeds
// value_len; call ble_gatt_server_set_variable_length(..., 0) to make it fixed.
// When a write callback is supplied, the application owns value-length rules.
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

// Add an Include declaration to the current service. The referenced service
// must already be closed by a later service declaration so its end handle is
// stable. ATT includes carry the UUID only for 16-bit service UUIDs.
static inline int ble_gatt_server_add_included_service(
    ble_gatt_server *server, uint16_t included_service_handle,
    uint16_t *include_handle) {
    if (!server || !included_service_handle) return 0;
    ble_gatt_attribute *service = ble_gatt_server_find(server,
                                                       included_service_handle);
    if (!service || !(service->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                                        BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)))
        return 0;
    ble_gatt_attribute *current_service = NULL;
    for (uint16_t i = server->count; i > 0; i--) {
        ble_gatt_attribute *a = &server->attributes[i - 1];
        if (a->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                        BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)) {
            current_service = a;
            break;
        }
    }
    if (!current_service || included_service_handle >= current_service->handle)
        return 0;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *a = &server->attributes[i];
        if (a->handle <= current_service->handle) continue;
        if (a->flags & BLE_GATT_ATTRIBUTE_CHARACTERISTIC) return 0;
    }
    uint16_t end_handle = 0;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *a = &server->attributes[i];
        if (a->handle <= included_service_handle) continue;
        if (a->flags & (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
                        BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE)) {
            end_handle = (uint16_t)(a->handle - 1);
            break;
        }
    }
    if (!end_handle) return 0;

    uint8_t value[6];
    ble_gatt_server_put_u16(value, included_service_handle);
    ble_gatt_server_put_u16(value + 2, end_handle);
    uint16_t value_len = 4;
    if (service->value_len == 2) {
        memcpy(value + 4, ble_gatt_attribute_value(server, service), 2);
        value_len = 6;
    }
    ble_gatt_uuid include_uuid = ble_gatt_uuid16(0x2802);
    if (!ble_gatt_server_add(server, &include_uuid, BLE_GATT_PERM_READ, 0,
            BLE_GATT_ATTRIBUTE_INCLUDED_SERVICE, value, value_len, value_len,
            NULL, NULL, NULL, include_handle)) return 0;
    return 1;
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
    uint16_t read_permissions = BLE_GATT_PERM_READ |
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHENTICATED |
        BLE_GATT_PERM_READ_AUTHORIZED;
    uint16_t write_permissions = BLE_GATT_PERM_WRITE |
        BLE_GATT_PERM_WRITE_ENCRYPTED | BLE_GATT_PERM_WRITE_AUTHENTICATED |
        BLE_GATT_PERM_WRITE_AUTHORIZED;
    if (!server || !ble_gatt_uuid_valid(uuid) ||
        value_len > value_capacity || value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !initial_value) ||
        ((properties & BLE_GATT_PROP_READ) && !(permissions & read_permissions)) ||
        ((permissions & read_permissions) &&
         !(properties & BLE_GATT_PROP_READ)) ||
        ((properties & (BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP)) &&
         !(permissions & write_permissions)) ||
        ((properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE) &&
         !(permissions & BLE_GATT_PERM_WRITE_SIGNED)) ||
        ((permissions & BLE_GATT_PERM_WRITE_SIGNED) &&
         !(properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE)) ||
        ((permissions & write_permissions) &&
         !(properties & (BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP))) ||
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
        memset(&server->attributes[server->count - 1], 0,
               sizeof(server->attributes[0]));
        memset(server->value_pool + value_checkpoint, 0,
               server->value_used - value_checkpoint);
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
            (server->attributes[server->count - 1].flags &
             BLE_GATT_ATTRIBUTE_CCCD) ||
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

// Add the standard Generic Attribute service with Service Changed and
// Database Hash characteristics. Sealing the database calculates the hash.
// The application owns bonded-peer CCCD persistence and decides when a known
// service change should be indicated to each connected peer.
static inline int ble_gatt_server_add_standard_gatt_service(
    ble_gatt_server *server, ble_gatt_standard_service_handles *handles) {
    if (!server || !handles || server->database_sealed) return 0;
    ble_gatt_uuid service_uuid = {2, {0x01, 0x18}};
    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    ble_gatt_uuid hash_uuid = {2, {0x2a, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    for (uint16_t i = 0; i < server->count; i++)
        if (ble_gatt_uuid_equal(&server->attributes[i].uuid, &service_uuid) &&
            (server->attributes[i].flags &
             (BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE |
              BLE_GATT_ATTRIBUTE_SECONDARY_SERVICE))) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint8_t service_changed_initial[4] = {0};
    uint8_t hash_initial[16] = {0};
    uint16_t service, changed, cccd, hash, declaration;
    int added = ble_gatt_server_add_service(server, &service_uuid, 1,
                                             &service) &&
        ble_gatt_server_add_characteristic(server, &changed_uuid,
            BLE_GATT_PROP_INDICATE, 0, service_changed_initial,
            sizeof(service_changed_initial), sizeof(service_changed_initial),
            NULL, NULL, NULL, &declaration, &changed) &&
        ble_gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &cccd) &&
        ble_gatt_server_add_characteristic(server, &hash_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, hash_initial,
            sizeof(hash_initial), sizeof(hash_initial), NULL, NULL, NULL,
            &declaration, &hash);
    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        return 0;
    }
    handles->service_handle = service;
    handles->service_changed_handle = changed;
    handles->service_changed_cccd_handle = cccd;
    handles->database_hash_handle = hash;
    return 1;
}

static uint8_t ble_gatt_server_authorization_error(
    const ble_gatt_server *server, const ble_gatt_attribute *attribute,
    uint8_t write) {
    uint16_t permission = write ? BLE_GATT_PERM_WRITE_AUTHORIZED :
                                  BLE_GATT_PERM_READ_AUTHORIZED;
    if (!(attribute->permissions & permission)) return 0;
    if (!server->authorize || !server->authorize(server->authorize_context,
            attribute->handle, write))
        return BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    return 0;
}

static uint8_t ble_gatt_server_access_security_error(
    const ble_gatt_server *server, const ble_gatt_attribute *attribute,
    uint8_t write) {
    uint16_t authenticated = write ? BLE_GATT_PERM_WRITE_AUTHENTICATED :
                                     BLE_GATT_PERM_READ_AUTHENTICATED;
    uint16_t encrypted = write ? BLE_GATT_PERM_WRITE_ENCRYPTED :
                                 BLE_GATT_PERM_READ_ENCRYPTED;
    if ((attribute->permissions & authenticated) && !server->authenticated)
        return BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION;
    uint8_t error = ble_gatt_server_authorization_error(server, attribute,
                                                         write);
    if (error) return error;
    if ((attribute->permissions & encrypted) && !server->encrypted)
        return BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION;
    if (attribute->min_key_size) {
        if (!server->encrypted)
            return BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION;
        if (server->encryption_key_size < attribute->min_key_size)
            return BLE_GATT_ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT;
    }
    return 0;
}

static uint8_t ble_gatt_server_read(ble_gatt_server *server,
                                    ble_gatt_attribute *a, uint16_t offset,
                                    uint8_t *out, uint16_t *out_len) {
    uint8_t security = ble_gatt_server_access_security_error(server, a, 0);
    if (security) return security;
    if (!(a->flags & BLE_GATT_ATTRIBUTE_CCCD) && a->properties &&
        !(a->properties & BLE_GATT_PROP_READ))
        return BLE_GATT_ATT_ERR_READ_NOT_PERMITTED;
    if (!(a->permissions & (BLE_GATT_PERM_READ |
                            BLE_GATT_PERM_READ_ENCRYPTED |
                            BLE_GATT_PERM_READ_AUTHENTICATED |
                            BLE_GATT_PERM_READ_AUTHORIZED)))
        return BLE_GATT_ATT_ERR_READ_NOT_PERMITTED;
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
        if (!error && (*out_len > capacity ||
            (uint32_t)offset + *out_len > BLE_GATT_ATT_VALUE_MAX))
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

static void ble_gatt_server_remove_event(ble_gatt_server *server,
                                         uint16_t index) {
    if (!server || index >= server->event_count) return;
    uint16_t removed_len = server->events[index].len;
    uint16_t removed_offset = server->events[index].data_offset;
    uint16_t data_after = (uint16_t)(server->event_used - removed_offset -
                                     removed_len);
    if (data_after) memmove(server->event_data + removed_offset,
        server->event_data + removed_offset + removed_len, data_after);
    server->event_used -= removed_len;
    for (uint16_t i = index + 1; i < server->event_count; i++)
        server->events[i].data_offset -= removed_len;
    memmove(server->events + index, server->events + index + 1,
        (server->event_count - index - 1) * sizeof(server->events[0]));
    server->event_count--;
    memset(&server->events[server->event_count], 0,
           sizeof(server->events[server->event_count]));
}

static void ble_gatt_server_drop_disabled_events(ble_gatt_server *server,
    uint16_t value_handle, uint16_t configuration) {
    for (uint16_t i = 0; i < server->event_count;) {
        ble_gatt_server_event *event = &server->events[i];
        uint16_t bit = event->indication ? 2 : 1;
        if (event->handle == value_handle && !(configuration & bit))
            ble_gatt_server_remove_event(server, i);
        else
            i++;
    }
    if (server->database_hash_update_pending) {
        ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
        uint16_t changed_handle = 0;
        for (uint16_t i = 0; i < server->count; i++)
            if (ble_gatt_uuid_equal(&server->attributes[i].uuid,
                                    &changed_uuid))
                changed_handle = server->attributes[i].handle;
        if (changed_handle == value_handle && !(configuration & 2) &&
            !(server->indication_pending &&
              server->indication_handle == changed_handle)) {
            uint8_t queued = 0;
            for (uint16_t i = 0; i < server->event_count; i++)
                if (server->events[i].handle == changed_handle &&
                    server->events[i].indication) queued = 1;
            if (!queued) server->database_hash_update_pending = 0;
        }
    }
}

static uint8_t ble_gatt_server_write(ble_gatt_server *server,
                                     ble_gatt_attribute *a, uint16_t offset,
                                     const uint8_t *value, uint16_t len,
                                     uint8_t command) {
    uint8_t signed_command = command == 2;
    uint8_t access_error = signed_command ?
        ble_gatt_server_authorization_error(server, a, 1) :
        ble_gatt_server_access_security_error(server, a, 1);
    if (access_error) return access_error;
    if (!(a->flags & BLE_GATT_ATTRIBUTE_CCCD) && a->properties &&
        !(a->properties & (signed_command ? BLE_GATT_PROP_AUTH_SIGNED_WRITE :
            command ? BLE_GATT_PROP_WRITE_NO_RSP : BLE_GATT_PROP_WRITE)))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    if (signed_command ? !(a->permissions & BLE_GATT_PERM_WRITE_SIGNED) :
        !(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED |
                            BLE_GATT_PERM_WRITE_AUTHORIZED)))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    if (len > BLE_GATT_ATT_VALUE_MAX)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (a->flags & BLE_GATT_ATTRIBUTE_CCCD) {
        if (offset || len != 2) return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        uint16_t bits = ble_gatt_server_u16(value);
        uint16_t allowed = (uint16_t)((a->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                                      (a->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
        if (bits & (uint16_t)~allowed) return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;
        a->cccd = bits;
        ble_gatt_server_drop_disabled_events(server, a->parent_handle, bits);
        if (server->cccd_store)
            server->cccd_store(server->cccd_context, a->parent_handle, bits);
        if ((bits & 2) && ble_gatt_server_is_service_changed(server,
                a->parent_handle))
            (void)ble_gatt_server_check_database_version(server);
        return 0;
    }
    if (a->write) return a->write(a->context, offset, value, len, command);
    if (!(a->flags & BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH)) {
        if (offset > a->value_len)
            return BLE_GATT_ATT_ERR_INVALID_OFFSET;
        if (len > a->value_len - offset)
            return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    }
    if (offset > a->value_capacity || len > a->value_capacity - offset)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (offset && offset + len > a->value_len)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (len) memcpy(ble_gatt_attribute_value(server, a) + offset, value, len);
    if (a->flags & BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH) {
        if (!offset) a->value_len = len;
        else if (offset + len > a->value_len) a->value_len = offset + len;
    }
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

static inline int ble_gatt_server_set_cccd(ble_gatt_server *server,
    uint16_t value_handle, uint16_t configuration) {
    if (!server) return 0;
    ble_gatt_attribute *characteristic = ble_gatt_server_find(server,
                                                               value_handle);
    ble_gatt_attribute *cccd = ble_gatt_server_cccd_for(server, value_handle);
    if (!characteristic || !cccd) return 0;
    uint16_t allowed = (uint16_t)(
        (characteristic->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
        (characteristic->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
    if (configuration & (uint16_t)~allowed) return 0;
    cccd->cccd = configuration;
    ble_gatt_server_drop_disabled_events(server, value_handle, configuration);
    if ((configuration & 2) &&
        ble_gatt_server_is_service_changed(server, value_handle))
        (void)ble_gatt_server_check_database_version(server);
    return 1;
}

// Restore bonded-peer subscriptions after link_reset and before ATT traffic.
// The application callback must return zero for non-bonded peers.
static inline void ble_gatt_server_restore_cccds(ble_gatt_server *server) {
    if (!server || !server->cccd_load) return;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *cccd = &server->attributes[i];
        if (!(cccd->flags & BLE_GATT_ATTRIBUTE_CCCD)) continue;
        uint16_t value_handle = cccd->parent_handle;
        uint16_t configuration = server->cccd_load(server->cccd_context,
                                                    value_handle);
        (void)ble_gatt_server_set_cccd(server, value_handle, configuration);
    }
}

// Build one Handle Value Notification. The application supplies the current
// value; values are capped at 512 octets and truncated to ATT_MTU - 3 bytes.
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
    if (ble_gatt_server_access_security_error(server, characteristic, 0))
        return 0;
    if (value_len > BLE_GATT_ATT_VALUE_MAX) return 0;
    if (value_len > server->mtu - 3) value_len = server->mtu - 3;
    if (att_capacity < value_len + 3) return 0;
    att[0] = 0x1b;
    ble_gatt_server_put_u16(att + 1, value_handle);
    if (value_len) memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    return 1;
}

// Build one Handle Value Indication. Values are capped at 512 octets and
// truncated to ATT_MTU - 3; only one indication may await confirmation. For
// direct sends, poll_event starts timeout tracking on its next call.
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
    if (ble_gatt_server_access_security_error(server, characteristic, 0))
        return 0;
    if (value_len > BLE_GATT_ATT_VALUE_MAX) return 0;
    if (value_len > server->mtu - 3) value_len = server->mtu - 3;
    if (att_capacity < value_len + 3) return 0;
    att[0] = 0x1d;
    ble_gatt_server_put_u16(att + 1, value_handle);
    if (value_len) memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    server->indication_pending = 1;
    server->indication_handle = value_handle;
    server->indication_timeout_armed = 0;
    return 1;
}

// Queue an application value for asynchronous transmission. Values are capped
// at 512 octets and truncated to ATT_MTU - 3. The application calls poll_event
// from its connection event loop with a monotonic millisecond tick; a timed-out
// indication is cleared and reported as -1.
static inline int ble_gatt_server_queue_event(ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    int indication) {
    if (!server || (value_len && !value)) return -1;
    if (value_len > BLE_GATT_ATT_VALUE_MAX ||
        server->event_count >= BLE_GATT_SERVER_EVENT_QUEUE_SIZE) return 0;
    if (value_len > BLE_GATT_SERVER_EVENT_BYTES - server->event_used)
        return 0;
    ble_gatt_attribute *characteristic =
        ble_gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = ble_gatt_server_cccd_for(server, value_handle);
    uint8_t bit = indication ? 2 : 1;
    uint8_t property = indication ? BLE_GATT_PROP_INDICATE : BLE_GATT_PROP_NOTIFY;
    if (!characteristic || !(characteristic->properties & property) ||
        !cccd || !(cccd->cccd & bit) ||
        ble_gatt_server_access_security_error(server, characteristic, 0))
        return 0;
    ble_gatt_server_event *event = &server->events[server->event_count++];
    event->handle = value_handle;
    event->len = value_len;
    event->data_offset = server->event_used;
    event->indication = indication != 0;
    if (value_len) memcpy(server->event_data + server->event_used, value, value_len);
    server->event_used += value_len;
    return 1;
}

// Queue the standard Service Changed indication for a previously detected
// database change. The caller supplies the changed inclusive handle range;
// CCCD persistence and deciding which bonded peers need this range are owned
// by the application.
static inline int ble_gatt_server_service_changed(ble_gatt_server *server,
    uint16_t start_handle, uint16_t end_handle) {
    if (!server || !start_handle || start_handle > end_handle) return 0;
    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attribute = &server->attributes[i];
        if (!ble_gatt_uuid_equal(&attribute->uuid, &changed_uuid) ||
            !(attribute->properties & BLE_GATT_PROP_INDICATE)) continue;
        uint8_t range[4] = {
            (uint8_t)start_handle, (uint8_t)(start_handle >> 8),
            (uint8_t)end_handle, (uint8_t)(end_handle >> 8)
        };
        return ble_gatt_server_queue_event(server, attribute->handle, range,
                                           sizeof(range), 1);
    }
    return 0;
}

// Compare the current sealed database hash with the value stored for the
// current peer. Call after restoring that peer's CCCDs on connection. A new
// peer gets a baseline without an indication; a changed database queues a
// full-range indication only when the peer has enabled Service Changed.
static inline int ble_gatt_server_check_database_version(
    ble_gatt_server *server) {
    if (!server || !server->database_hash_available ||
        !server->database_hash_load || !server->database_hash_store ||
        server->database_hash_update_pending) return 0;
    uint8_t previous[16];
    if (!server->database_hash_load(server->database_hash_context, previous)) {
        server->database_hash_store(server->database_hash_context,
                                    server->database_hash);
        return 1;
    }
    if (!memcmp(previous, server->database_hash, sizeof(previous))) return 1;
    if (ble_gatt_server_service_changed(server, 1, 0xffff) != 1) return 0;
    server->database_hash_update_pending = 1;
    return 1;
}

static inline int ble_gatt_server_poll_event(ble_gatt_server *server,
    uint32_t now_ms, uint8_t *att, uint16_t att_capacity, uint16_t *att_len) {
    if (!server || !att || !att_len) return -1;
    *att_len = 0;
    if (server->indication_pending) {
        if (!server->indication_timeout_armed) {
            server->indication_started_ms = now_ms;
            server->indication_timeout_armed = 1;
            return 0;
        }
        if ((uint32_t)(now_ms - server->indication_started_ms) <
            BLE_GATT_SERVER_INDICATION_TIMEOUT_MS) return 0;
        server->indication_pending = 0;
        server->indication_timeout_armed = 0;
        server->indication_handle = 0;
        server->indication_started_ms = 0;
        return -1;
    }
    if (!server->event_count) return 0;
    ble_gatt_server_event event = server->events[0];
    ble_gatt_attribute *characteristic = ble_gatt_server_find(server,
                                                               event.handle);
    ble_gatt_attribute *cccd = ble_gatt_server_cccd_for(server, event.handle);
    uint16_t bit = event.indication ? 2 : 1;
    uint8_t property = event.indication ? BLE_GATT_PROP_INDICATE :
                                         BLE_GATT_PROP_NOTIFY;
    if (!characteristic || !(characteristic->properties & property) ||
        !cccd || !(cccd->cccd & bit) ||
        ble_gatt_server_access_security_error(server, characteristic, 0)) {
        ble_gatt_server_remove_event(server, 0);
        return 0;
    }
    const uint8_t *value = server->event_data + event.data_offset;
    int result = event.indication ?
        ble_gatt_server_indicate(server, event.handle, value, event.len,
                                  att, att_capacity, att_len) :
        ble_gatt_server_notify(server, event.handle, value, event.len,
                                att, att_capacity, att_len);
    if (result <= 0) return result;
    server->event_count--;
    if (event.len) {
        memmove(server->event_data, server->event_data + event.len,
                server->event_used - event.len);
        server->event_used -= event.len;
        for (uint16_t i = 0; i < server->event_count; i++)
            server->events[i].data_offset -= event.len;
    }
    memmove(server->events, server->events + 1,
            server->event_count * sizeof(server->events[0]));
    return 1;
}

static void ble_gatt_server_prepare_clear(ble_gatt_server *server) {
    memset(server->prepare_data, 0, server->prepare_used);
    server->prepare_count = 0;
    server->prepare_used = 0;
}

static uint8_t ble_gatt_server_prepare_validate(ble_gatt_server *server,
    ble_gatt_attribute *a, uint16_t offset, uint16_t len,
    uint8_t validate_value) {
    uint8_t access_error =
        ble_gatt_server_access_security_error(server, a, 1);
    if (access_error) return access_error;
    if (!(a->flags & BLE_GATT_ATTRIBUTE_CCCD) && a->properties &&
        !(a->properties & BLE_GATT_PROP_WRITE))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    if (!(a->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENCRYPTED |
                            BLE_GATT_PERM_WRITE_AUTHENTICATED |
                            BLE_GATT_PERM_WRITE_AUTHORIZED)))
        return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    if (validate_value && (offset > BLE_GATT_ATT_VALUE_MAX ||
        len > BLE_GATT_ATT_VALUE_MAX - offset))
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (a->prepare || a->execute)
        return (a->prepare && a->execute) ? 0 :
            BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (a->write) return BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED;
    // ATT defers static value offset and length validation until Execute Write.
    if (validate_value && !a->prepare && !a->execute) {
        if (offset > a->value_len)
            return BLE_GATT_ATT_ERR_INVALID_OFFSET;
        if (!(a->flags & BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH) &&
            len > a->value_len - offset)
            return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (offset > a->value_capacity || len > a->value_capacity - offset)
            return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    }
    return 0;
}

static uint8_t ble_gatt_server_prepare_execute(ble_gatt_server *server,
                                               uint16_t *error_handle) {
    // Validate the full batch before changing any attribute.
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        ble_gatt_prepared_write *p = &server->prepared[i];
        ble_gatt_attribute *a = ble_gatt_server_find(server, p->handle);
        if (!a) {
            *error_handle = p->handle;
            return BLE_GATT_ATT_ERR_INVALID_HANDLE;
        }
        uint8_t error = ble_gatt_server_prepare_validate(server, a,
                                                   p->offset, p->len, 1);
        if (error) {
            *error_handle = p->handle;
            return error;
        }
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
            if (next == covered) {
                *error_handle = p->handle;
                return BLE_GATT_ATT_ERR_INVALID_OFFSET;
            }
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
    // ATT commands (including Write Command and Signed Write Command) never
    // receive a response, even when malformed or larger than the bearer MTU.
    if (req_len > mtu) {
        if (op & 0x40) return 0;
        goto invalid_pdu;
    }
    if (op == 0x1e) { // Handle Value Confirmation
        if (req_len != 1) goto invalid_pdu;
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
        if (req_len != 3 || ble_gatt_server_u16(req + 1) < 23)
            return ble_gatt_server_error_rsp(op, 0, BLE_GATT_ATT_ERR_INVALID_PDU,
                                              rsp, rsp_capacity, rsp_len);
        if (server->mtu_exchanged)
            return ble_gatt_server_error_rsp(op, 0, 0x06, rsp, rsp_capacity, rsp_len);
        if (rsp_capacity < 3) return 0;
        uint16_t peer_mtu = ble_gatt_server_u16(req + 1);
        server->mtu = peer_mtu < server->local_mtu ? peer_mtu : server->local_mtu;
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
