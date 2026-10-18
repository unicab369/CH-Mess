#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

// Transport-independent GATT/ATT server core. Applications register attributes
// and call gatt_server_att() with each complete ATT PDU; the GAP/L2CAP
// adapter is responsible for carrying the returned response.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ble_gatt.h"
#include "ble_crypto.h"

// =============================================================================
// Server limits
// =============================================================================
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

// =============================================================================
// Attribute permissions, properties, and flags
// =============================================================================
enum {
    BLE_GATT_PERM_READ = 1u << 0,
    BLE_GATT_PERM_WRITE = 1u << 1,
    BLE_GATT_PERM_READ_ENC = 1u << 2,
    BLE_GATT_PERM_WRITE_ENC = 1u << 3,
    BLE_GATT_PERM_READ_AUTHN = 1u << 4,
    BLE_GATT_PERM_WRITE_AUTHN = 1u << 5,
    BLE_GATT_PERM_READ_AUTHZ = 1u << 6,
    BLE_GATT_PERM_WRITE_AUTHZ = 1u << 7,
    BLE_GATT_PERM_WRITE_SIGNED = 1u << 8,
    // Security constraints for server-originated notifications/indications.
    // These are separate from read permissions so event-only characteristics
    // can be secured without advertising an unsupported Read property.
    BLE_GATT_PERM_EVENT_ENC = 1u << 9,
    BLE_GATT_PERM_EVENT_AUTHN = 1u << 10,
    BLE_GATT_PERM_EVENT_AUTHZ = 1u << 11
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
    BLE_GATT_ATTR_PRIMARY_SERVICE = 1u << 0,
    BLE_GATT_ATTR_SECONDARY_SERVICE = 1u << 1,
    BLE_GATT_ATTR_CHAR = 1u << 2,
    BLE_GATT_ATTR_CCCD = 1u << 3,
    BLE_GATT_ATTR_INCLUDED_SERVICE = 1u << 4,
    BLE_GATT_ATTR_VAR_LEN = 1u << 5,
    BLE_GATT_ATTR_CLIENT_FEATURES = 1u << 6
};

enum {
    BLE_GATT_FEATURE_ROBUST_CACHE = 1u << 0,
    BLE_GATT_FEATURE_EATT = 1u << 1,
    BLE_GATT_FEATURE_MULTI_NOTIFY = 1u << 2,
    BLE_GATT_FEATURE_MASK = 0x07
};

// =============================================================================
// Attribute and callback types
// =============================================================================
typedef struct {
    uint8_t len;
    uint8_t value[BLE_GATT_UUID128_LEN];
} ble_gatt_uuid;

struct ble_gatt_attribute;
typedef uint8_t (*ble_gatt_read_fn)(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
);
// `command` is nonzero for an ATT Write Command.
typedef uint8_t (*ble_gatt_write_fn)(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
// Observers run after a characteristic value write has been committed.
typedef void (*ble_gatt_write_observer_fn)(
    void *context, uint16_t handle, const uint8_t *value, uint16_t len
);
// Prepare validates and stages one fragment without publishing it. If it
// returns an error, it must leave its staging state unchanged. Execute is
// called once at commit (commit=1) or cancel (commit=0); commit must not fail.
typedef uint8_t (*ble_gatt_prepare_fn)(
    void *context, uint16_t offset, const uint8_t *value, uint16_t len
);
typedef uint8_t (*ble_gatt_prepare_commit_validate_fn)(
    void *context, const uint8_t *value, uint16_t len
);
typedef void (*ble_gatt_execute_fn)(void *context, uint8_t commit);
// Verify the exact ATT Signed Write Command bytes excluding its final
// 12-byte authentication signature. The callback must check and persist the
// peer's sign counter to reject replayed writes.
typedef int (*ble_gatt_signed_verify_fn)(void *context,
    const uint8_t *signed_pdu, uint16_t signed_len,
    const uint8_t signature[12]);
typedef int (*ble_gatt_authorize_fn)(
    void *context, uint16_t handle, uint8_t write
);
typedef uint16_t (*ble_gatt_cccd_load_fn)(
    void *context, uint16_t value_handle
);
typedef void (*ble_gatt_cccd_store_fn)(
    void *context, uint16_t value_handle, uint16_t configuration
);
typedef int (*ble_gatt_database_hash_load_fn)(
    void *context, uint8_t hash[16]
);
typedef void (*ble_gatt_database_hash_store_fn)(
    void *context, const uint8_t hash[16]
);
typedef uint8_t (*ble_gatt_client_features_load_fn)(void *context);
typedef void (*ble_gatt_client_features_store_fn)(
    void *context, uint8_t features
);

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
    ble_gatt_read_fn read_value;
    ble_gatt_write_fn write_request;
    ble_gatt_prepare_fn prepare;
    ble_gatt_prepare_commit_validate_fn prepare_commit_validate;
    ble_gatt_execute_fn execute;
    ble_gatt_write_observer_fn write_observer;
    void *write_observer_context;
    void *context;
} ble_gatt_attribute;

typedef struct {
    uint16_t service_handle;
    uint16_t service_changed_handle;
    uint16_t service_changed_cccd_handle;
    uint16_t database_hash_handle;
    uint16_t client_supported_features_handle;
} ble_gatt_standard_service_handles;

typedef struct {
    uint16_t handle, offset, len, data_offset;
} ble_gatt_prepared_write;

typedef struct {
    uint16_t handle, len, data_offset;
    uint8_t indication;
} ble_gatt_server_event;

typedef struct {
    uint16_t mtu, local_mtu;
    ble_gatt_prepared_write prepared[BLE_GATT_SERVER_PREPARE_QUEUE_SIZE];
    uint8_t prepare_data[BLE_GATT_SERVER_PREPARE_BYTES];
    uint16_t prepare_count, prepare_used;
    uint8_t mtu_exchanged, indication_pending;
    uint8_t indication_timeout_armed;
    uint16_t indication_handle;
    uint32_t indication_started_ms;
} ble_gatt_server_bearer_state;

// =============================================================================
// Server database and link state
// =============================================================================
typedef struct {
    // Registered database and its server-owned value storage.
    ble_gatt_attribute attrs[BLE_GATT_SERVER_MAX_ATTRIBUTES];
    uint16_t count;
    uint16_t next_handle;
    uint16_t value_used;
    uint8_t value_pool[BLE_GATT_SERVER_VALUE_POOL_SIZE];

    // ATT bearer state. Anonymous aliases keep the server API field names
    // while allowing EATT to copy this shared state as one value.
    union {
        ble_gatt_server_bearer_state bearer;
        struct {
            uint16_t mtu, local_mtu;
            ble_gatt_prepared_write prepared[BLE_GATT_SERVER_PREPARE_QUEUE_SIZE];
            uint8_t prepare_data[BLE_GATT_SERVER_PREPARE_BYTES];
            uint16_t prepare_count, prepare_used;
            uint8_t mtu_exchanged, indication_pending;
            uint8_t indication_timeout_armed;
            uint16_t indication_handle;
            uint32_t indication_started_ms;
        };
    };

    // Queued notifications and indications.
    ble_gatt_server_event events[BLE_GATT_SERVER_EVENT_QUEUE_SIZE];
    uint8_t event_data[BLE_GATT_SERVER_EVENT_BYTES];
    uint16_t event_count, event_used;

    // Per-link security, negotiated features, and database hash state.
    uint8_t database_sealed;
    uint8_t encrypted, authenticated;
    uint8_t encryption_key_size;
    uint8_t database_hash[16];
    uint8_t database_hash_available, database_hash_update_pending;

    // Application callbacks and their shared contexts.
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
    ble_gatt_client_features_load_fn client_features_load;
    ble_gatt_client_features_store_fn client_features_store;
    void *client_features_context;

} ble_gatt_server;

static ble_gatt_attribute *gatt_server_find(
    ble_gatt_server *server, uint16_t handle
) {
    if (!server || !handle) return NULL;

    for (uint16_t i = 0; i < server->count; i++)
        if (server->attrs[i].handle == handle)
            return &server->attrs[i];

    return NULL;
}

static void gatt_cancel_prepares(
    ble_gatt_server *server, uint16_t extra_handle
) {
    if (!server) return;

    uint8_t extra_seen = 0;
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        uint16_t handle = server->prepared[i].handle;
        uint8_t first = 1;

        for (uint16_t j = 0; j < i; j++)
            if (server->prepared[j].handle == handle)
                first = 0;

        if (!first) continue;

        if (handle == extra_handle) extra_seen = 1;
        ble_gatt_attribute *attr = gatt_server_find(server, handle);
        if (attr && attr->execute) attr->execute(attr->context, 0);
    }

    if (extra_handle && !extra_seen) {
        ble_gatt_attribute *attr = gatt_server_find(server, extra_handle);
        if (attr && attr->execute) attr->execute(attr->context, 0);
    }
}

// =============================================================================
// UUID and attribute helpers
// =============================================================================
static int gatt_uuid_equal(
    const ble_gatt_uuid *uuid_a, const ble_gatt_uuid *uuid_b
) {
    if (!uuid_a || !uuid_b ||
        (uuid_a->len != BLE_GATT_UUID16_LEN &&
         uuid_a->len != BLE_GATT_UUID128_LEN) ||
        (uuid_b->len != BLE_GATT_UUID16_LEN &&
         uuid_b->len != BLE_GATT_UUID128_LEN)
    ) return 0;

    if (uuid_a->len == uuid_b->len)
        return !memcmp(uuid_a->value, uuid_b->value, uuid_a->len);

    const ble_gatt_uuid *short_uuid = uuid_a->len == 2 ? uuid_a : uuid_b;
    const ble_gatt_uuid *long_uuid = uuid_a->len == 16 ? uuid_a : uuid_b;
    static const uint8_t bluetooth_base_prefix[14] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00,
        0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00
    };
    return !memcmp(long_uuid->value + 14, short_uuid->value, 2) &&
           !memcmp(long_uuid->value, bluetooth_base_prefix,
                   sizeof(bluetooth_base_prefix));
}

static uint16_t gatt_uuid_assigned16(const ble_gatt_uuid *uuid) {
    if (uuid->len == 2) return ble_get_le16(uuid->value);
    if (uuid->len == 16) return ble_get_le16(uuid->value + 14);
    return 0;
}

static uint8_t *gatt_attribute_value(
    ble_gatt_server *server, ble_gatt_attribute *attr
) {
    if (!attr->value_capacity ||
        attr->value_offset > server->value_used ||
        attr->value_capacity > server->value_used - attr->value_offset
    ) return NULL;
    return server->value_pool + attr->value_offset;
}

static void gatt_write_committed(
    ble_gatt_server *server, ble_gatt_attribute *attr
) {
    if (!server || !attr || !attr->write_observer) return;
    uint8_t *value = gatt_attribute_value(server, attr);
    if (!value && attr->value_len) return;

    attr->write_observer(attr->write_observer_context,
                attr->handle, value, attr->value_len);
}

// Application facing
// Observe writes after the GATT server has committed the characteristic
// value. This includes normal and reliable (prepare/execute) writes.
static inline int gatt_set_write_observer(
    ble_gatt_server *server, uint16_t value_handle,
    ble_gatt_write_observer_fn observer, void *context
) {
    if (!server) return 0;
    ble_gatt_attribute *attr = gatt_server_find(server, value_handle);
    if (!attr || !attr->value_capacity) return 0;
    attr->write_observer = observer;
    attr->write_observer_context = context;
    return 1;
}

// Application facing
// =============================================================================
// Database hash and server lifecycle
// =============================================================================
static inline void gatt_server_init(ble_gatt_server *server, uint16_t local_mtu) {
    if (!server) return;
    memset(server, 0, sizeof(*server));
    if (local_mtu < 23) local_mtu = 23;
    if (local_mtu > BLE_GATT_SERVER_MTU_MAX)
        local_mtu = BLE_GATT_SERVER_MTU_MAX;

    server->local_mtu = local_mtu;
    server->mtu = 23;
    server->next_handle = 1;
}

// Application facing
static inline void gatt_link_reset(ble_gatt_server *server) {
    if (!server) return;

    gatt_cancel_prepares(server, 0);
    server->mtu = 23;
    server->mtu_exchanged = 0;
    server->encrypted = 0;
    server->authenticated = 0;
    server->encryption_key_size = 0;
    server->indication_pending = 0;
    server->indication_timeout_armed = 0;
    server->indication_handle = 0;
    server->indication_started_ms = 0;
    server->database_hash_update_pending = 0;
    memset(server->event_data, 0, server->event_used);
    server->event_count = 0;
    server->event_used = 0;
    memset(server->prepare_data, 0, sizeof(server->prepare_data));
    server->prepare_count = 0;
    server->prepare_used = 0;

    for (uint16_t i = 0; i < server->count; i++) {
        if (server->attrs[i].flags & BLE_GATT_ATTR_CCCD)
            server->attrs[i].cccd = 0;

        if (server->attrs[i].flags &
            BLE_GATT_ATTR_CLIENT_FEATURES) {
            uint8_t *features = gatt_attribute_value(server, &server->attrs[i]);
            if (features) features[0] = 0;
            server->attrs[i].value_len = 1;
        }
    }
}

// Application facing
// Set the minimum LE encryption key size required for an attribute (7-16
// octets). Configure it while building the database, before sealing it.
static inline int gatt_set_min_key_size(
    ble_gatt_server *server, uint16_t handle, uint8_t octets
) {
    if (!server || server->database_sealed || octets < 7 || octets > 16)
        return 0;
    ble_gatt_attribute *attr = gatt_server_find(server, handle);
    if (!attr) return 0;
    attr->min_key_size = octets;
    return 1;
}

// Application facing
// Configure server-owned attribute storage as variable or fixed length.
// Storage with spare capacity is variable by default; a write callback owns
// its own length semantics and cannot be reconfigured here.
static inline int gatt_set_variable_length(
    ble_gatt_server *server, uint16_t handle, int variable
) {
    if (!server || server->database_sealed) return 0;
    ble_gatt_attribute *attr = gatt_server_find(server, handle);

    if (!attr || attr->write_request ||
        attr->prepare ||
        attr->execute ||
        (attr->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                        BLE_GATT_ATTR_SECONDARY_SERVICE |
                        BLE_GATT_ATTR_CCCD |
                        BLE_GATT_ATTR_INCLUDED_SERVICE |
                        BLE_GATT_ATTR_CLIENT_FEATURES))
    ) return 0;

    if (variable) attr->flags |= BLE_GATT_ATTR_VAR_LEN;
    else attr->flags &= (uint8_t)~BLE_GATT_ATTR_VAR_LEN;
    return 1;
}

static int gatt_user_desc_writable(
    ble_gatt_server *server, const ble_gatt_attribute *description
) {
    if (gatt_uuid_assigned16(&description->uuid) != 0x2901)
        return 1;

    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *extended = &server->attrs[i];

        if (gatt_uuid_assigned16(&extended->uuid) != 0x2900 ||
            extended->parent_handle != description->parent_handle ||
            extended->value_len != 2
        ) continue;

        const uint8_t *value = gatt_attribute_value(server, extended);
        return value && (ble_get_le16(value) & 0x0002);
    }
    return 0;
}

// Application facing
static inline int gatt_set_prepare(
    ble_gatt_server *server, uint16_t handle,
    ble_gatt_prepare_fn prepare, ble_gatt_execute_fn execute
) {
    if (!server) return 0;
    ble_gatt_attribute *attr = gatt_server_find(server, handle);

    if (!attr || !prepare || !execute || server->prepare_count ||
        !(attr->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENC |
                            BLE_GATT_PERM_WRITE_AUTHN |
                            BLE_GATT_PERM_WRITE_AUTHZ))
    ) return 0;

    attr->prepare = prepare;
    attr->execute = execute;
    return 1;
}

// Application facing
static inline int gatt_set_validator(
    ble_gatt_server *server, uint16_t handle,
    ble_gatt_prepare_commit_validate_fn validate
) {
    if (!server || !validate || server->database_sealed ||
        server->prepare_count
    ) return 0;

    ble_gatt_attribute *attr = gatt_server_find(server, handle);
    if (!attr || !attr->prepare || !attr->execute) return 0;
    attr->prepare_commit_validate = validate;
    return 1;
}

static int gatt_server_add(
    ble_gatt_server *server,
    const ble_gatt_uuid *uuid, uint16_t permissions,
    uint8_t properties, uint8_t flags,
    const uint8_t *value, uint16_t value_len,
    uint16_t value_capacity,
    ble_gatt_read_fn read,
    ble_gatt_write_fn write, void *context,
    uint16_t *handle_out
) {
    if (!server || server->database_sealed || !uuid ||
        (uuid->len != BLE_GATT_UUID16_LEN &&
         uuid->len != BLE_GATT_UUID128_LEN) ||
        server->count >= BLE_GATT_SERVER_MAX_ATTRIBUTES ||
        server->next_handle == 0 || value_len > value_capacity ||
        value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        value_capacity > BLE_GATT_SERVER_VALUE_POOL_SIZE - server->value_used ||
        (value_len && !value)
    ) return 0;

    ble_gatt_attribute *attr = &server->attrs[server->count++];
    memset(attr, 0, sizeof(*attr));
    attr->handle = server->next_handle++;
    attr->uuid = *uuid;
    attr->permissions = permissions;
    attr->properties = properties;
    attr->flags = flags;

    if (value_capacity > value_len)
        attr->flags |= BLE_GATT_ATTR_VAR_LEN;
    attr->value_len = value_len;
    attr->value_capacity = value_capacity;
    attr->value_offset = server->value_used;
    attr->read_value = read;
    attr->write_request = write;
    attr->context = context;

    if (value_len) memcpy(server->value_pool + attr->value_offset, value, value_len);
    server->value_used += value_capacity;
    if (handle_out) *handle_out = attr->handle;
    return 1;
}

// Application facing
static inline int gatt_seal_database(ble_gatt_server *server) {
    if (!server || server->database_sealed)
        return server != NULL;

    const uint16_t attribute_read_permissions = BLE_GATT_PERM_READ |
                                        BLE_GATT_PERM_READ_ENC |
                                        BLE_GATT_PERM_READ_AUTHN |
                                        BLE_GATT_PERM_READ_AUTHZ;
    const uint16_t write_permissions = BLE_GATT_PERM_WRITE |
                                        BLE_GATT_PERM_WRITE_ENC |
                                        BLE_GATT_PERM_WRITE_AUTHN |
                                        BLE_GATT_PERM_WRITE_AUTHZ;
    const uint16_t secured_read_permissions = BLE_GATT_PERM_READ_ENC |
                                        BLE_GATT_PERM_READ_AUTHN |
                                        BLE_GATT_PERM_READ_AUTHZ;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *declaration = &server->attrs[i];

        if (!(declaration->flags & BLE_GATT_ATTR_CHAR))
            continue;

        uint8_t *declaration_value = gatt_attribute_value(server, declaration);
        if (!declaration_value || declaration->value_len < 5)
            return 0;

        uint8_t properties = declaration_value[0];
        uint16_t value_handle = ble_get_le16(declaration_value + 1);
        uint8_t uuid_len = (uint8_t)(declaration->value_len - 3);
        ble_gatt_attribute *value_attribute = i + 1 < server->count ?
                                        &server->attrs[i + 1] : NULL;

        if (value_handle != declaration->handle + 1 ||
            (uuid_len != 2 && uuid_len != 16) || !value_attribute ||
            value_attribute->handle != value_handle ||
            value_attribute->properties != properties ||
            value_attribute->uuid.len != uuid_len ||
            memcmp(value_attribute->uuid.value, declaration_value + 3, uuid_len)
        ) return 0;

        if (!!(properties & BLE_GATT_PROP_READ) !=
                !!(value_attribute->permissions & attribute_read_permissions) ||
            !!(properties & (BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP)) !=
                !!(value_attribute->permissions & write_permissions) ||
            !!(properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE) !=
                !!(value_attribute->permissions & BLE_GATT_PERM_WRITE_SIGNED) ||
            ((value_attribute->permissions &
                (BLE_GATT_PERM_EVENT_ENC |
                 BLE_GATT_PERM_EVENT_AUTHN |
                 BLE_GATT_PERM_EVENT_AUTHZ)) &&
             !(properties & (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE)))
        ) return 0;

        uint8_t cccd_count = 0, extended_count = 0, server_config_count = 0;
        uint8_t user_description_count = 0;

        for (uint16_t j = i + 2; j < server->count; j++) {
            ble_gatt_attribute *candidate = &server->attrs[j];

            if (candidate->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                BLE_GATT_ATTR_SECONDARY_SERVICE |
                BLE_GATT_ATTR_CHAR)
            ) break;

            uint16_t type = gatt_uuid_assigned16(&candidate->uuid);

            if (type == 0x2901) {
                if (++user_description_count > 1)
                    return 0;

                candidate->parent_handle = value_handle;
                continue;
            }

            if (type != 0x2900 && type != 0x2902 && type != 0x2903)
                continue;

            if (candidate->value_len != 2)
                return 0;

            const uint8_t *descriptor_value = gatt_attribute_value(server, candidate);

            if (!descriptor_value)
                return 0;

            uint16_t bits = ble_get_le16(descriptor_value);

            if (type == 0x2902) {
                uint16_t allowed = (uint16_t)(
                    (properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                    (properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
                if (++cccd_count > 1 ||
                    !(properties & (BLE_GATT_PROP_NOTIFY |
                                    BLE_GATT_PROP_INDICATE)) ||
                    !(candidate->flags & BLE_GATT_ATTR_CCCD) ||
                    candidate->parent_handle != value_handle ||
                    !(candidate->permissions & BLE_GATT_PERM_READ) ||
                    (candidate->permissions & secured_read_permissions) ||
                    (candidate->permissions & write_permissions) == 0 ||
                    (bits & (uint16_t)~allowed)
                ) return 0;
            }
            else if (type == 0x2900) {
                if (++extended_count > 1 ||
                    !(properties & BLE_GATT_PROP_EXTENDED) ||
                    candidate->permissions != BLE_GATT_PERM_READ ||
                    (bits & (uint16_t)~0x0003)
                ) return 0;

                candidate->parent_handle = value_handle;
            }
            else {
                if (++server_config_count > 1 ||
                    !(properties & BLE_GATT_PROP_BROADCAST) ||
                    !(candidate->permissions & BLE_GATT_PERM_READ) ||
                    (candidate->permissions & secured_read_permissions) ||
                    (candidate->permissions & write_permissions) == 0 ||
                    (bits & (uint16_t)~0x0001) ||
                    ((bits & 1) && !(properties & BLE_GATT_PROP_BROADCAST))
                ) return 0;

                candidate->parent_handle = value_handle;
            }
        }

        if (user_description_count) {
            for (uint16_t j = i + 2; j < server->count; j++) {
                ble_gatt_attribute *candidate = &server->attrs[j];

                if (candidate->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                    BLE_GATT_ATTR_SECONDARY_SERVICE |
                    BLE_GATT_ATTR_CHAR)
                ) break;

                if (gatt_uuid_assigned16(&candidate->uuid) == 0x2901 &&
                    (candidate->permissions & write_permissions) &&
                    !gatt_user_desc_writable(server,
                                                               candidate)
                ) return 0;
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
                ble_gatt_attribute *candidate = &server->attrs[j];

                if (candidate->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                    BLE_GATT_ATTR_SECONDARY_SERVICE |
                    BLE_GATT_ATTR_CHAR)
                ) break;

                if (gatt_uuid_assigned16(&candidate->uuid) != required[r])
                    continue;

                if (required[r] == 0x2902 &&
                    (!(candidate->flags & BLE_GATT_ATTR_CCCD) ||
                     candidate->parent_handle != value_handle)
                ) return 0;

                if (required[r] == 0x2900 || required[r] == 0x2903) {
                    if (candidate->value_len != 2)
                        return 0;

                    const uint8_t *descriptor_value =
                        gatt_attribute_value(server, candidate);
                    if (!descriptor_value)
                        return 0;

                    uint16_t bits = ble_get_le16(descriptor_value);
                    uint16_t allowed = required[r] == 0x2900 ? 0x0003 : 0x0001;

                    if ((bits & (uint16_t)~allowed) ||
                        (required[r] == 0x2903 && (bits & 1) &&
                         !(properties & BLE_GATT_PROP_BROADCAST))
                    ) return 0;
                }

                matches++;
            }
            if (matches != 1) return 0;
        }
    }
    // The Generic Attribute service must expose Client Supported Features
    // whenever it exposes both Service Changed and Database Hash.
    ble_gatt_uuid gatt_service_uuid = {2, {0x01, 0x18}};
    ble_gatt_uuid service_changed_uuid = {2, {0x05, 0x2a}};
    ble_gatt_uuid database_hash_uuid = {2, {0x2a, 0x2b}};
    ble_gatt_uuid client_features_uuid = {2, {0x29, 0x2b}};
    uint8_t gatt_service_count = 0;
    ble_gatt_attribute *client_features_attribute = NULL;

    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *service = &server->attrs[i];

        if (!(service->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                                BLE_GATT_ATTR_SECONDARY_SERVICE)) ||
            gatt_uuid_assigned16(&service->uuid) != 0x2800 ||
            service->value_len != gatt_service_uuid.len ||
            memcmp(gatt_attribute_value(server, service),
                   gatt_service_uuid.value, gatt_service_uuid.len)
        ) continue;

        if (++gatt_service_count > 1)
            return 0;

        uint8_t has_changed = 0, has_hash = 0, feature_count = 0;
        for (uint16_t j = i + 1; j < server->count; j++) {
            ble_gatt_attribute *candidate = &server->attrs[j];

            if (candidate->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                                    BLE_GATT_ATTR_SECONDARY_SERVICE)
            ) break;

            if (!(candidate->flags & BLE_GATT_ATTR_CHAR))
                continue;

            if (j + 1 >= server->count)
                return 0;

            ble_gatt_attribute *value = &server->attrs[j + 1];

            if (gatt_uuid_equal(&value->uuid, &service_changed_uuid))
                has_changed = 1;
            else if (gatt_uuid_equal(&value->uuid, &database_hash_uuid))
                has_hash = 1;
            else if (gatt_uuid_equal(&value->uuid,
                                         &client_features_uuid)) {
                if (++feature_count > 1 ||
                    value->properties != (BLE_GATT_PROP_READ |
                                          BLE_GATT_PROP_WRITE) ||
                    value->permissions != (BLE_GATT_PERM_READ |
                                           BLE_GATT_PERM_WRITE) ||
                    value->value_len != 1 || value->value_capacity != 1 ||
                    value->read_value || value->write_request || value->prepare ||
                    value->execute || !gatt_attribute_value(server, value)
                ) return 0;

                client_features_attribute = value;
            }
        }

        if (has_changed && has_hash && feature_count != 1)
            return 0;
    }

    ble_gatt_attribute *database_hash = NULL;

    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];
        if (!gatt_uuid_equal(&attr->uuid, &database_hash_uuid))
            continue;

        if (database_hash || i == 0 ||
            !(server->attrs[i - 1].flags & BLE_GATT_ATTR_CHAR) ||
            attr->properties != BLE_GATT_PROP_READ ||
            attr->permissions != BLE_GATT_PERM_READ ||
            attr->value_len != 16 || attr->value_capacity != 16 ||
            attr->read_value || attr->write_request || attr->prepare ||
            attr->execute
        ) return 0;

        database_hash = attr;
    }

    if (database_hash) {
        static const uint8_t zero_key[16] = {0};
        static const uint16_t value_types[] = {
            0x2800, 0x2801, 0x2802, 0x2803, 0x2900
        };
        static const uint16_t handle_only_types[] = {
            0x2901, 0x2902, 0x2903, 0x2904, 0x2905
        };
        ble_gatt_cmac cmac;
        gatt_cmac_init(&cmac, zero_key);

        for (uint16_t i = 0; i < server->count; i++) {
            const ble_gatt_attribute *attr = &server->attrs[i];
            uint8_t kind = 0;

            for (size_t j = 0;
                 j < sizeof(value_types) / sizeof(value_types[0]); j++) {
                uint16_t value = value_types[j];
                ble_gatt_uuid type = {2,{(uint8_t)value, (uint8_t)(value >> 8)}};

                if (gatt_uuid_equal(&attr->uuid, &type)) {
                    kind = 2; break;
                }
            }

            if (!kind) {
                for (size_t j = 0; j < sizeof(handle_only_types) /
                     sizeof(handle_only_types[0]); j++) {
                    uint16_t value = handle_only_types[j];
                    ble_gatt_uuid type = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};

                    if (gatt_uuid_equal(&attr->uuid, &type)) {
                        kind = 1; break;
                    }
                }
            }

            if (!kind) continue;

            uint8_t handle[2] = {(uint8_t)attr->handle,
                                 (uint8_t)(attr->handle >> 8)};
            gatt_cmac_update(&cmac, handle, sizeof(handle));
            gatt_cmac_update(&cmac, attr->uuid.value, attr->uuid.len);

            if (kind != 2 || !attr->value_len)
                continue;

            if (attr->value_len > BLE_GATT_ATT_VALUE_MAX) {
                memset(&cmac, 0, sizeof(cmac));
                return 0;
            }

            uint8_t *value = gatt_attribute_value(server, (ble_gatt_attribute *)attr);
            if (!value) {
                memset(&cmac, 0, sizeof(cmac));
                return 0;
            }

            gatt_cmac_update(&cmac, value, attr->value_len);
        }

        uint8_t hash[16];
        gatt_cmac_final(&cmac, hash);
        memcpy(gatt_attribute_value(server, database_hash), hash, sizeof(hash));
        memcpy(server->database_hash, hash, sizeof(hash));
        server->database_hash_available = 1;
    }

    if (client_features_attribute)
        client_features_attribute->flags |= BLE_GATT_ATTR_CLIENT_FEATURES;
    server->database_sealed = 1;
    return 1;
}

// Application facing
static inline void gatt_set_sign_verifier(
    ble_gatt_server *server, ble_gatt_signed_verify_fn verify, void *context
) {
    if (!server) return;
    server->signed_verify = verify;
    server->signed_context = context;
}

// Application facing
static inline void gatt_set_authorizer(
    ble_gatt_server *server, ble_gatt_authorize_fn authorize, void *context
) {
    if (!server) return;
    server->authorize = authorize;
    server->authorize_context = context;
}

// Application facing
// App-owned CCCD persistence. Callbacks use their context to identify the
// current peer; load returns zero for peers without a bonded stored value.
static inline void gatt_set_cccd_persistence(
    ble_gatt_server *server,
    ble_gatt_cccd_load_fn load, ble_gatt_cccd_store_fn store, void *context
) {
    if (!server) return;
    server->cccd_load = load;
    server->cccd_store = store;
    server->cccd_context = context;
}

// Application facing
// Optional per-peer database-version persistence. Install callbacks with a
// context identifying the current bonded peer before its connection starts.
// The new hash is stored after a Service Changed indication is confirmed.
static inline void gatt_set_hash_persistence(
    ble_gatt_server *server, ble_gatt_database_hash_load_fn load,
    ble_gatt_database_hash_store_fn store, void *context
) {
    if (!server || (!!load != !!store)) return;
    server->database_hash_load = load;
    server->database_hash_store = store;
    server->database_hash_context = context;
}

// Application facing
// Client Supported Features is scoped to the connected client. These
// callbacks let the application persist it for bonded peers; the context
// should identify the current peer. Without a load callback it resets to zero.
static inline void gatt_set_features_persistence(
    ble_gatt_server *server, ble_gatt_client_features_load_fn load,
    ble_gatt_client_features_store_fn store, void *context
) {
    if (!server || (!!load != !!store)) return;
    server->client_features_load = load;
    server->client_features_store = store;
    server->client_features_context = context;
}

// =============================================================================
// Database builders
// =============================================================================
static ble_gatt_uuid gatt_uuid16(uint16_t value) {
    ble_gatt_uuid uuid = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
    return uuid;
}

// Application facing
static inline int gatt_server_add_service(
    ble_gatt_server *server,
    const ble_gatt_uuid *service_uuid, int primary, uint16_t *service_handle
) {
    if (!service_uuid ||
        (service_uuid->len != BLE_GATT_UUID16_LEN &&
         service_uuid->len != BLE_GATT_UUID128_LEN)
    ) return 0;

    ble_gatt_uuid type = gatt_uuid16(primary ? 0x2800 : 0x2801);
    return gatt_server_add(server, &type, BLE_GATT_PERM_READ, 0,
                        primary ? BLE_GATT_ATTR_PRIMARY_SERVICE :
                                BLE_GATT_ATTR_SECONDARY_SERVICE,
                        service_uuid->value, service_uuid->len, service_uuid->len,
                        NULL, NULL, NULL, service_handle);
}

// Application facing
// Add an Include declaration to the current service. The referenced service
// must already be closed by a later service declaration so its end handle is
// stable. ATT includes carry the UUID only for 16-bit service UUIDs.
static inline int gatt_add_included_service(
    ble_gatt_server *server, uint16_t included_service_handle,
    uint16_t *include_handle
) {
    if (!server || !included_service_handle) return 0;
    ble_gatt_attribute *service = gatt_server_find(server, included_service_handle);

    if (!service || !(service->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                                        BLE_GATT_ATTR_SECONDARY_SERVICE))
    ) return 0;

    ble_gatt_attribute *current_service = NULL;

    for (uint16_t i = server->count; i > 0; i--) {
        ble_gatt_attribute *attr = &server->attrs[i - 1];

        if (attr->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                        BLE_GATT_ATTR_SECONDARY_SERVICE)
        ) {
            current_service = attr;
            break;
        }
    }

    if (!current_service || included_service_handle >= current_service->handle)
        return 0;

    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];

        if (attr->handle <= current_service->handle)
            continue;

        if (attr->flags & BLE_GATT_ATTR_CHAR)
            return 0;
    }

    uint16_t end_handle = 0;
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];

        if (attr->handle <= included_service_handle)
            continue;

        if (attr->flags & (BLE_GATT_ATTR_PRIMARY_SERVICE |
                        BLE_GATT_ATTR_SECONDARY_SERVICE)
        ) {
            end_handle = (uint16_t)(attr->handle - 1);
            break;
        }
    }

    if (!end_handle) return 0;

    uint8_t value[6];
    ble_put_le16(value, included_service_handle);
    ble_put_le16(value + 2, end_handle);
    uint16_t value_len = 4;

    if (service->value_len == 2) {
        memcpy(value + 4, gatt_attribute_value(server, service), 2);
        value_len = 6;
    }

    ble_gatt_uuid include_uuid = gatt_uuid16(0x2802);

    if (!gatt_server_add(server, &include_uuid, BLE_GATT_PERM_READ, 0,
            BLE_GATT_ATTR_INCLUDED_SERVICE, value, value_len, value_len,
            NULL, NULL, NULL, include_handle)
    ) return 0;

    return 1;
}

// Application facing
static inline int gatt_add_characteristic(
    ble_gatt_server *server,
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
    uint16_t *value_handle
) {
    uint16_t read_permissions = BLE_GATT_PERM_READ |
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_READ_AUTHN |
        BLE_GATT_PERM_READ_AUTHZ;
    uint16_t write_permissions = BLE_GATT_PERM_WRITE |
        BLE_GATT_PERM_WRITE_ENC | BLE_GATT_PERM_WRITE_AUTHN |
        BLE_GATT_PERM_WRITE_AUTHZ;

    if (!server || !uuid ||
        (uuid->len != BLE_GATT_UUID16_LEN &&
         uuid->len != BLE_GATT_UUID128_LEN) ||
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
        ((permissions & (BLE_GATT_PERM_EVENT_ENC |
                         BLE_GATT_PERM_EVENT_AUTHN |
                         BLE_GATT_PERM_EVENT_AUTHZ)) &&
         !(properties & (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE))) ||
        server->count + 2 > BLE_GATT_SERVER_MAX_ATTRIBUTES ||
        server->next_handle > 0xfffd
    ) return 0;

    uint16_t decl_h = server->next_handle;
    uint16_t val_h = (uint16_t)(decl_h + 1);
    uint16_t value_checkpoint = server->value_used;
    uint8_t decl[3 + BLE_GATT_UUID128_LEN];
    decl[0] = properties;
    ble_put_le16(decl + 1, val_h);
    memcpy(decl + 3, uuid->value, uuid->len);
    ble_gatt_uuid declaration_uuid = gatt_uuid16(0x2803);

    if (!gatt_server_add(server, &declaration_uuid, BLE_GATT_PERM_READ,
            0, BLE_GATT_ATTR_CHAR, decl,
            (uint16_t)(3 + uuid->len), (uint16_t)(3 + uuid->len),
            NULL, NULL, NULL, declaration_handle)
    ) return 0;

    if (!gatt_server_add(server, uuid, permissions, properties, 0,
            initial_value, value_len, value_capacity, read, write, context,
            value_handle)) {
        memset(&server->attrs[server->count - 1], 0,
               sizeof(server->attrs[0]));
        memset(server->value_pool + value_checkpoint, 0,
               server->value_used - value_checkpoint);
        server->count--;
        server->next_handle--;
        server->value_used = value_checkpoint;
        return 0;
    }

    return 1;
}

// Application facing
static inline int gatt_add_descriptor(
    ble_gatt_server *server,
    const ble_gatt_uuid *uuid,
    uint16_t permissions, const uint8_t *value,
    uint16_t value_len, uint16_t value_capacity,
    ble_gatt_read_fn read,
    ble_gatt_write_fn write, void *context,
    uint16_t *handle_out
) {
    if (uuid && uuid->len == 2 && ble_get_le16(uuid->value) == 0x2902) {
        if (!server || !server->count ||
            (server->attrs[server->count - 1].flags &
             BLE_GATT_ATTR_CCCD) ||
            !(server->attrs[server->count - 1].properties &
              (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE))
        ) return 0;

        uint8_t zero[2] = {0, 0};
        int added = gatt_server_add(server, uuid,
            permissions | BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, 0,
            BLE_GATT_ATTR_CCCD, zero, 2, 2, NULL, NULL, context,
            handle_out);

        if (added) {
            server->attrs[server->count - 1].properties =
                server->attrs[server->count - 2].properties;
            server->attrs[server->count - 1].parent_handle =
                server->attrs[server->count - 2].handle;
        }

        return added;
    }

    return gatt_server_add(server, uuid, permissions, 0, 0, value,
        value_len, value_capacity, read, write, context, handle_out);
}

// Application facing
// Add the standard Generic Attribute service with Service Changed, Database
// Hash, and Client Supported Features characteristics. Sealing the database
// calculates the hash. The application owns per-peer bonded-state persistence
// and decides when a known service change should be indicated to each peer.
static inline int gatt_add_standard_service(
    ble_gatt_server *server, ble_gatt_standard_service_handles *handles
) {
    if (!server || !handles || server->database_sealed)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x01, 0x18}};
    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    ble_gatt_uuid hash_uuid = {2, {0x2a, 0x2b}};
    ble_gatt_uuid client_features_uuid = {2, {0x29, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};

    for (uint16_t i = 0; i < server->count; i++)
        if (gatt_uuid_equal(&server->attrs[i].uuid, &service_uuid) &&
            (server->attrs[i].flags &
             (BLE_GATT_ATTR_PRIMARY_SERVICE |
              BLE_GATT_ATTR_SECONDARY_SERVICE))
        ) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint8_t service_changed_initial[4] = {0};
    uint8_t hash_initial[16] = {0};
    uint8_t client_features_initial[1] = {0};
    uint16_t service, changed, cccd, hash, client_features, declaration;

    int added = gatt_server_add_service(server, &service_uuid, 1, &service) &&
        gatt_add_characteristic(server, &changed_uuid,
            BLE_GATT_PROP_INDICATE, 0, service_changed_initial,
            sizeof(service_changed_initial), sizeof(service_changed_initial),
            NULL, NULL, NULL, &declaration, &changed) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &cccd) &&
        gatt_add_characteristic(server, &hash_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, hash_initial,
            sizeof(hash_initial), sizeof(hash_initial), NULL, NULL, NULL,
            &declaration, &hash) &&
        gatt_add_characteristic(server, &client_features_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            client_features_initial, sizeof(client_features_initial),
            sizeof(client_features_initial), NULL, NULL, NULL,
            &declaration, &client_features);

    if (!added) {
        memset(server->attrs + old_count, 0,
               (server->count - old_count) * sizeof(server->attrs[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        return 0;
    }

    server->attrs[server->count - 1].flags |= BLE_GATT_ATTR_CLIENT_FEATURES;
    handles->service_handle = service;
    handles->service_changed_handle = changed;
    handles->service_changed_cccd_handle = cccd;
    handles->database_hash_handle = hash;
    handles->client_supported_features_handle = client_features;
    return 1;
}

// =============================================================================
// ATT access checks and read/write procedures
// =============================================================================
static uint8_t gatt_authz_error(
    const ble_gatt_server *server, const ble_gatt_attribute *attr, uint8_t write
) {
    uint16_t permission = write ? BLE_GATT_PERM_WRITE_AUTHZ :
                                  BLE_GATT_PERM_READ_AUTHZ;
    if (!(attr->permissions & permission)) return 0;

    if (!server->authorize || !server->authorize(server->authorize_context,
            attr->handle, write)
    ) return ATT_ERR_INSUFFICIENT_AUTHORIZATION;

    return 0;
}

static uint8_t gatt_access_error(
    const ble_gatt_server *server, const ble_gatt_attribute *attr,
    uint8_t write
) {
    uint16_t authenticated = write ? BLE_GATT_PERM_WRITE_AUTHN :
                                     BLE_GATT_PERM_READ_AUTHN;
    uint16_t encrypted = write ? BLE_GATT_PERM_WRITE_ENC :
                                 BLE_GATT_PERM_READ_ENC;

    if ((attr->permissions & authenticated) && !server->authenticated)
        return ATT_ERR_INSUFFICIENT_AUTHENTICATION;

    uint8_t error = gatt_authz_error(server, attr, write);
    if (error) return error;

    if ((attr->permissions & encrypted) && !server->encrypted)
        return ATT_ERR_INSUFFICIENT_ENCRYPTION;

    if (attr->min_key_size) {
        if (!server->encrypted)
            return ATT_ERR_INSUFFICIENT_ENCRYPTION;
        if (server->encryption_key_size < attr->min_key_size)
            return ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT;
    }

    return 0;
}

static uint8_t gatt_server_event_security_error(
    const ble_gatt_server *server, const ble_gatt_attribute *attr
) {
    uint16_t permissions = attr->permissions;
    if ((permissions & BLE_GATT_PERM_EVENT_AUTHN) &&
        !server->authenticated
    ) return ATT_ERR_INSUFFICIENT_AUTHENTICATION;

    if ((permissions & BLE_GATT_PERM_EVENT_AUTHZ) &&
        (!server->authorize || !server->authorize(server->authorize_context,
            attr->handle, 0))
    ) return ATT_ERR_INSUFFICIENT_AUTHORIZATION;

    if ((permissions & BLE_GATT_PERM_EVENT_ENC) && !server->encrypted)
        return ATT_ERR_INSUFFICIENT_ENCRYPTION;

    uint16_t read_permissions = BLE_GATT_PERM_READ |
                            BLE_GATT_PERM_READ_ENC |
                            BLE_GATT_PERM_READ_AUTHN |
                            BLE_GATT_PERM_READ_AUTHZ;

    if (permissions & read_permissions)
        return gatt_access_error(server, attr, 0);

    uint16_t write_permissions = BLE_GATT_PERM_WRITE |
                                BLE_GATT_PERM_WRITE_ENC |
                                BLE_GATT_PERM_WRITE_AUTHN |
                                BLE_GATT_PERM_WRITE_AUTHZ |
                                BLE_GATT_PERM_WRITE_SIGNED;

    if (permissions & write_permissions)
        return gatt_access_error(server, attr, 1);

    if (attr->min_key_size) {
        if (!server->encrypted)
            return ATT_ERR_INSUFFICIENT_ENCRYPTION;

        if (server->encryption_key_size < attr->min_key_size)
            return ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT;
    }

    return 0;
}

static uint8_t gatt_server_read(
    ble_gatt_server *server,
    ble_gatt_attribute *attr, uint16_t offset,
    uint8_t *out, uint16_t *out_len
) {
    uint8_t security = gatt_access_error(server, attr, 0);
    if (security) return security;

    if (!(attr->flags & BLE_GATT_ATTR_CCCD)
        && attr->properties &&
        !(attr->properties & BLE_GATT_PROP_READ)
    ) return ATT_ERR_READ_NOT_PERMITTED;

    if (!(attr->permissions & (BLE_GATT_PERM_READ |
                            BLE_GATT_PERM_READ_ENC |
                            BLE_GATT_PERM_READ_AUTHN |
                            BLE_GATT_PERM_READ_AUTHZ))
    ) return ATT_ERR_READ_NOT_PERMITTED;

    if (attr->flags & BLE_GATT_ATTR_CCCD) {
        uint8_t value[2];
        ble_put_le16(value, attr->cccd);
        if (offset > 2) return ATT_ERR_INVALID_OFFSET;

        uint16_t size = (uint16_t)(2 - offset);

        if (size > *out_len) size = *out_len;
        memcpy(out, value + offset, size);
        *out_len = size;
        return 0;
    }

    if (attr->read_value) {
        uint16_t capacity = *out_len;
        uint8_t error = attr->read_value(attr->context, offset, out, out_len);

        if (!error && (*out_len > capacity ||
            (uint32_t)offset + *out_len > BLE_GATT_ATT_VALUE_MAX)
        ) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        return error;
    }

    if (offset > attr->value_len)
        return ATT_ERR_INVALID_OFFSET;

    uint16_t size = (uint16_t)(attr->value_len - offset);

    if (size > *out_len) size = *out_len;
    if (size) memcpy(out, gatt_attribute_value(server, attr) + offset, size);
    *out_len = size;
    return 0;
}

static void gatt_remove_event(ble_gatt_server *server, uint16_t index) {
    if (!server || index >= server->event_count) return;
    uint16_t removed_len = server->events[index].len;
    uint16_t removed_offset = server->events[index].data_offset;
    uint16_t data_after = (uint16_t)(server->event_used - removed_offset - removed_len);

    if (data_after)
        memmove(server->event_data + removed_offset,
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

static void gatt_drop_disabled_events(
    ble_gatt_server *server, uint16_t value_handle, uint16_t configuration
) {
    for (uint16_t i = 0; i < server->event_count;) {
        ble_gatt_server_event *event = &server->events[i];
        uint16_t bit = event->indication ? 2 : 1;

        if (event->handle == value_handle && !(configuration & bit))
            gatt_remove_event(server, i);
        else i++;
    }
    if (server->database_hash_update_pending) {
        ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
        uint16_t changed_handle = 0;

        for (uint16_t i = 0; i < server->count; i++)
            if (gatt_uuid_equal(&server->attrs[i].uuid, &changed_uuid)
            ) changed_handle = server->attrs[i].handle;

        if (changed_handle == value_handle && !(configuration & 2) &&
            !(server->indication_pending &&
              server->indication_handle == changed_handle)) {
            uint8_t queued = 0;

            for (uint16_t i = 0; i < server->event_count; i++)
                if (server->events[i].handle == changed_handle &&
                    server->events[i].indication
                ) queued = 1;

            if (!queued)
                server->database_hash_update_pending = 0;
        }
    }
}

static ble_gatt_attribute *gatt_server_cccd_for(
    ble_gatt_server *server, uint16_t value_handle
) {
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];

        if ((attr->flags & BLE_GATT_ATTR_CCCD) &&
            attr->parent_handle == value_handle
        ) return attr;
    }
    return NULL;
}

// Application facing
// Queue an application value for asynchronous transmission. Values are capped
// at 512 octets and truncated to ATT_MTU - 3. A zero-capacity characteristic
// can carry event-only values without attribute storage.
// The application calls poll_event
// from its connection event loop with a monotonic millisecond tick; a timed-out
// indication is cleared and reported as -1.
static inline int gatt_queue_event(
    ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    int indication
) {
    if (!server || (value_len && !value)) return -1;

    if (value_len > BLE_GATT_ATT_VALUE_MAX ||
        server->event_count >= BLE_GATT_SERVER_EVENT_QUEUE_SIZE
    ) return 0;

    if (value_len > BLE_GATT_SERVER_EVENT_BYTES - server->event_used)
        return 0;

    ble_gatt_attribute *characteristic = gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, value_handle);
    uint8_t bit = indication ? 2 : 1;
    uint8_t property = indication ? BLE_GATT_PROP_INDICATE : BLE_GATT_PROP_NOTIFY;

    if (!characteristic ||
        (characteristic->value_capacity &&
         value_len > characteristic->value_capacity) ||
        !(characteristic->properties & property) ||
        !cccd || !(cccd->cccd & bit) ||
        gatt_server_event_security_error(server, characteristic)
    ) return 0;

    ble_gatt_server_event *event = &server->events[server->event_count++];
    event->handle = value_handle;
    event->len = value_len;
    event->data_offset = server->event_used;
    event->indication = indication != 0;

    if (value_len) memcpy(server->event_data + server->event_used, value, value_len);
    server->event_used += value_len;
    return 1;
}

// Application facing
// Queue the standard Service Changed indication for a previously detected
// database change. The caller supplies the changed inclusive handle range;
// CCCD persistence and deciding which bonded peers need this range are owned
// by the application.
static inline int gatt_service_changed(
    ble_gatt_server *server, uint16_t start_handle, uint16_t end_handle
) {
    if (!server || !start_handle || start_handle > end_handle)
        return 0;

    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];

        if (!gatt_uuid_equal(&attr->uuid, &changed_uuid) ||
            !(attr->properties & BLE_GATT_PROP_INDICATE)
        ) continue;

        uint8_t range[4] = {
            (uint8_t)start_handle, (uint8_t)(start_handle >> 8),
            (uint8_t)end_handle, (uint8_t)(end_handle >> 8)
        };
        return gatt_queue_event(server, attr->handle, range,
                                           sizeof(range), 1);
    }
    return 0;
}

// Application facing
// Compare the current sealed database hash with the value stored for the
// current peer. Call after restoring that peer's CCCDs on connection. A new
// peer gets a baseline without an indication; a changed database queues a
// full-range indication only when the peer has enabled Service Changed.
static inline int gatt_check_db_version(
    ble_gatt_server *server
) {
    if (!server || !server->database_hash_available ||
        !server->database_hash_load || !server->database_hash_store ||
        server->database_hash_update_pending
    ) return 0;

    uint8_t previous[16];

    if (!server->database_hash_load(server->database_hash_context, previous)) {
        server->database_hash_store(server->database_hash_context,
                                    server->database_hash);
        return 1;
    }

    if (!memcmp(previous, server->database_hash, sizeof(previous)))
        return 1;

    if (gatt_service_changed(server, 1, 0xffff) != 1)
        return 0;

    server->database_hash_update_pending = 1;
    return 1;
}

static int gatt_is_service_changed(
    ble_gatt_server *server, uint16_t handle
) {
    ble_gatt_attribute *attr = gatt_server_find(server, handle);
    ble_gatt_uuid changed_uuid = {2, {0x05, 0x2a}};
    return attr && (attr->properties & BLE_GATT_PROP_INDICATE) &&
        gatt_uuid_equal(&attr->uuid, &changed_uuid);
}

static uint8_t gatt_server_write(
    ble_gatt_server *server,
    ble_gatt_attribute *attr, uint16_t offset,
    const uint8_t *value, uint16_t len,
    uint8_t command
) {
    uint8_t signed_command = command == 2;
    uint8_t access_error = signed_command ?
        gatt_authz_error(server, attr, 1) :
        gatt_access_error(server, attr, 1);

    if (access_error)
        return access_error;

    if (!(attr->flags & BLE_GATT_ATTR_CCCD) && attr->properties &&
        !(attr->properties & (signed_command ? BLE_GATT_PROP_AUTH_SIGNED_WRITE :
            command ? BLE_GATT_PROP_WRITE_NO_RSP : BLE_GATT_PROP_WRITE))
    ) return ATT_ERR_WRITE_NOT_PERMITTED;

    if (signed_command ? !(attr->permissions & BLE_GATT_PERM_WRITE_SIGNED) :
        !(attr->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENC |
                            BLE_GATT_PERM_WRITE_AUTHN |
                            BLE_GATT_PERM_WRITE_AUTHZ))
    ) return ATT_ERR_WRITE_NOT_PERMITTED;

    if (attr->flags & BLE_GATT_ATTR_CLIENT_FEATURES) {
        if (offset || len != 1)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        uint8_t *stored = gatt_attribute_value(server, attr);

        if (!stored)
            return ATT_ERR_UNLIKELY_ERROR;

        uint8_t features = value[0];

        if ((features & (uint8_t)~BLE_GATT_FEATURE_MASK) ||
            (features & stored[0]) != stored[0]
        ) return ATT_ERR_VALUE_NOT_ALLOWED;

        stored[0] = features;
        attr->value_len = 1;
        if (server->client_features_store)
            server->client_features_store(server->client_features_context,
                                           features);
        return 0;
    }

    if (!gatt_user_desc_writable(server, attr))
        return ATT_ERR_WRITE_NOT_PERMITTED;

    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (attr->flags & BLE_GATT_ATTR_CCCD) {
        if (offset || len != 2)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        uint16_t bits = ble_get_le16(value);
        uint16_t allowed = (uint16_t)((attr->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                                      (attr->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
        if (bits & (uint16_t)~allowed)
            return ATT_ERR_VALUE_NOT_ALLOWED;
        if (attr->write_request) {
            uint8_t error = attr->write_request(
                attr->context, offset, value, len, command);
            if (error) return error;
        }

        attr->cccd = bits;
        gatt_drop_disabled_events(server, attr->parent_handle, bits);
        if (server->cccd_store)
            server->cccd_store(server->cccd_context, attr->parent_handle, bits);
        if ((bits & 2) && gatt_is_service_changed(server,
                attr->parent_handle)
        ) (void)gatt_check_db_version(server);
        return 0;
    }

    if (gatt_uuid_assigned16(&attr->uuid) == 0x2903) {
        if (offset || len != 2)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        uint16_t bits = ble_get_le16(value);
        ble_gatt_attribute *characteristic = gatt_server_find(
            server, attr->parent_handle);
        if ((bits & (uint16_t)~1u) ||
            ((bits & 1) && (!characteristic ||
                !(characteristic->properties & BLE_GATT_PROP_BROADCAST)))
        ) return ATT_ERR_VALUE_NOT_ALLOWED;
    }

    if (attr->write_request) {
        uint8_t error = attr->write_request(
            attr->context, offset, value, len, command);
        if (!error) gatt_write_committed(server, attr);
        return error;
    }

    if (!(attr->flags & BLE_GATT_ATTR_VAR_LEN)) {
        if (offset > attr->value_len)
            return ATT_ERR_INVALID_OFFSET;
        if (len > attr->value_len - offset)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    }

    if (offset > attr->value_capacity || len > attr->value_capacity - offset)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (offset && offset + len > attr->value_len)
        return ATT_ERR_INVALID_OFFSET;

    if (len)
        memcpy(gatt_attribute_value(server, attr) + offset, value, len);

    if (attr->flags & BLE_GATT_ATTR_VAR_LEN) {
        if (!offset) attr->value_len = len;
        else if (offset + len > attr->value_len)
            attr->value_len = offset + len;
    }

    gatt_write_committed(server, attr);

    return 0;
}

// Application facing
static inline int gatt_server_set_cccd(
    ble_gatt_server *server, uint16_t value_handle, uint16_t configuration
) {
    if (!server) return 0;

    ble_gatt_attribute *characteristic = gatt_server_find(server,
                                                               value_handle);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, value_handle);

    if (!characteristic || !cccd)
        return 0;

    uint16_t allowed = (uint16_t)(
        (characteristic->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
        (characteristic->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));

    if (configuration & (uint16_t)~allowed)
        return 0;

    cccd->cccd = configuration;
    gatt_drop_disabled_events(server, value_handle, configuration);

    if ((configuration & 2) &&
        gatt_is_service_changed(server, value_handle)
    ) (void)gatt_check_db_version(server);
    return 1;
}

// Application facing
// Restore the Client Supported Features octet after link reset and before ATT
// traffic. Unknown/reserved bits in application storage are discarded.
static inline void gatt_restore_features(ble_gatt_server *server) {
    if (!server) return;

    for (uint16_t i = 0; i < server->count; i++) {
        ble_gatt_attribute *attr = &server->attrs[i];

        if (!(attr->flags & BLE_GATT_ATTR_CLIENT_FEATURES))
            continue;

        uint8_t *value = gatt_attribute_value(server, attr);
        if (!value || attr->value_capacity < 1)
            continue;

        value[0] = server->client_features_load ?
            (uint8_t)(server->client_features_load(
                server->client_features_context) &
                BLE_GATT_FEATURE_MASK) : 0;
        attr->value_len = 1;
    }
}

// Application facing
// =============================================================================
// Notifications, indications, and event queue
// =============================================================================
// Build one Handle Value Notification. The application supplies the current
// value; values are capped at 512 octets and truncated to ATT_MTU - 3 bytes.
// A zero-capacity characteristic can carry an event-only value without storing
// that value in the attribute database.
static inline int gatt_notify(
    ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    uint8_t *att, uint16_t att_capacity, uint16_t *att_len
) {
    if (!server || !att || !att_len || (value_len && !value))
        return -1;

    *att_len = 0;
    ble_gatt_attribute *characteristic = gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, value_handle);

    if (!characteristic ||
        (characteristic->value_capacity &&
         value_len > characteristic->value_capacity) ||
        !(characteristic->properties & BLE_GATT_PROP_NOTIFY) ||
        !cccd || !(cccd->cccd & 1)
    ) return 0;

    if (gatt_server_event_security_error(server, characteristic))
        return 0;

    if (value_len > BLE_GATT_ATT_VALUE_MAX)
        return 0;

    if (value_len > server->mtu - 3)
        value_len = server->mtu - 3;
    if (att_capacity < value_len + 3)
        return 0;

    att[0] = 0x1b;
    ble_put_le16(att + 1, value_handle);

    if (value_len) memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    return 1;
}

// Application facing
// Build one Handle Value Indication. Values are capped at 512 octets and
// truncated to ATT_MTU - 3; only one indication may await confirmation. For
// direct sends, poll_event starts timeout tracking on its next call. A
// zero-capacity characteristic can carry an event-only value.
static inline int gatt_indicate(
    ble_gatt_server *server,
    uint16_t value_handle, const uint8_t *value, uint16_t value_len,
    uint8_t *att, uint16_t att_capacity, uint16_t *att_len
) {
    if (!server || !att || !att_len || (value_len && !value))
        return -1;

    *att_len = 0;
    ble_gatt_attribute *characteristic = gatt_server_find(server, value_handle);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, value_handle);

    if (!characteristic ||
        (characteristic->value_capacity &&
         value_len > characteristic->value_capacity) ||
        !(characteristic->properties & BLE_GATT_PROP_INDICATE) ||
        !cccd || !(cccd->cccd & 2) || server->indication_pending
    ) return 0;

    if (gatt_server_event_security_error(server, characteristic))
        return 0;

    if (value_len > BLE_GATT_ATT_VALUE_MAX)
        return 0;

    if (value_len > server->mtu - 3)
        value_len = server->mtu - 3;
    if (att_capacity < value_len + 3)
        return 0;

    att[0] = 0x1d;
    ble_put_le16(att + 1, value_handle);

    if (value_len)
        memcpy(att + 3, value, value_len);
    *att_len = value_len + 3;
    server->indication_pending = 1;
    server->indication_handle = value_handle;
    server->indication_timeout_armed = 0;
    return 1;
}

// Application facing
static inline int gatt_poll(
    ble_gatt_server *server,
    uint32_t now_ms, uint8_t *att, uint16_t att_capacity, uint16_t *att_len
) {
    if (!server || !att || !att_len) return -1;
    *att_len = 0;

    if (server->indication_pending) {
        if (!server->indication_timeout_armed) {
            server->indication_started_ms = now_ms;
            server->indication_timeout_armed = 1;
            return 0;
        }

        if ((uint32_t)(now_ms - server->indication_started_ms) <
            BLE_GATT_SERVER_INDICATION_TIMEOUT_MS
        ) return 0;

        server->indication_pending = 0;
        server->indication_timeout_armed = 0;
        server->indication_handle = 0;
        server->indication_started_ms = 0;
        return -1;
    }

    if (!server->event_count) return 0;
    ble_gatt_server_event event = server->events[0];
    ble_gatt_attribute *characteristic = gatt_server_find(server, event.handle);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, event.handle);
    uint16_t bit = event.indication ? 2 : 1;
    uint8_t property = event.indication ? BLE_GATT_PROP_INDICATE :
                                         BLE_GATT_PROP_NOTIFY;

    if (!characteristic || !(characteristic->properties & property) ||
        !cccd || !(cccd->cccd & bit) ||
        gatt_server_event_security_error(server, characteristic)) {
        gatt_remove_event(server, 0);
        return 0;
    }

    const uint8_t *value = server->event_data + event.data_offset;
    uint8_t multiple_notifications_enabled = 0;

    if (!event.indication) {
        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *features = &server->attrs[i];

            if (!(features->flags & BLE_GATT_ATTR_CLIENT_FEATURES))
                continue;

            const uint8_t *feature_value = gatt_attribute_value(server, features);
            multiple_notifications_enabled = feature_value &&
                features->value_len == 1 &&
                (feature_value[0] & BLE_GATT_FEATURE_MULTI_NOTIFY);
            break;
        }
    }

    if (multiple_notifications_enabled) {
        uint16_t packet_limit = att_capacity < server->mtu ?
                                att_capacity : server->mtu;
        uint16_t encoded = 1;
        uint16_t count = 0;

        for (uint16_t i = 0; i < server->event_count; i++) {
            ble_gatt_server_event *candidate = &server->events[i];
            if (candidate->indication)
                break;

            ble_gatt_attribute *characteristic = gatt_server_find(server, candidate->handle);
            ble_gatt_attribute *candidate_cccd = gatt_server_cccd_for(server, candidate->handle);

            if (!characteristic ||
                !(characteristic->properties & BLE_GATT_PROP_NOTIFY) ||
                !candidate_cccd || !(candidate_cccd->cccd & 1) ||
                gatt_server_event_security_error(server, characteristic)
            ) break;

            uint16_t tuple_len = (uint16_t)(4 + candidate->len);

            if (encoded >= packet_limit ||
                tuple_len > packet_limit - encoded
            ) break;

            encoded += tuple_len;
            count++;
        }

        if (count >= 2) {
            att[0] = 0x23;
            uint16_t offset = 1;

            for (uint16_t i = 0; i < count; i++) {
                ble_gatt_server_event *candidate = &server->events[i];
                ble_put_le16(att + offset, candidate->handle);
                ble_put_le16(att + offset + 2, candidate->len);

                if (candidate->len)
                    memcpy(att + offset + 4,
                        server->event_data + candidate->data_offset,
                        candidate->len);
                offset = (uint16_t)(offset + 4 + candidate->len);
            }
            *att_len = offset;
            while (count--) gatt_remove_event(server, 0);
            return 1;
        }
    }

    int result = event.indication ?
        gatt_indicate(server, event.handle, value, event.len,
                                  att, att_capacity, att_len) :
        gatt_notify(server, event.handle, value, event.len,
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

// =============================================================================
// Prepared writes and ATT dispatch
// =============================================================================
static void gatt_prepare_cccd(
    const ble_gatt_server *server, const ble_gatt_attribute *attr,
    uint8_t value[2]
) {
    ble_put_le16(value, attr->cccd);
    for (uint16_t i = 0; i < server->prepare_count; i++) {
        const ble_gatt_prepared_write *part = &server->prepared[i];

        if (part->handle != attr->handle)
            continue;

        for (uint16_t j = 0; j < part->len; j++)
            value[part->offset + j] =
                server->prepare_data[part->data_offset + j];
    }
}

static uint8_t gatt_prepare_validate(
    ble_gatt_server *server,
    ble_gatt_attribute *attr, uint16_t offset, uint16_t len,
    uint8_t validate_value
) {
    uint8_t access_error = gatt_access_error(server, attr, 1);
    if (access_error) return access_error;

    if (!(attr->flags & BLE_GATT_ATTR_CCCD) && attr->properties &&
        !(attr->properties & BLE_GATT_PROP_WRITE)
    ) return ATT_ERR_WRITE_NOT_PERMITTED;

    if (!(attr->permissions & (BLE_GATT_PERM_WRITE |
                            BLE_GATT_PERM_WRITE_ENC |
                            BLE_GATT_PERM_WRITE_AUTHN |
                            BLE_GATT_PERM_WRITE_AUTHZ))
    ) return ATT_ERR_WRITE_NOT_PERMITTED;

    if (attr->flags & BLE_GATT_ATTR_CLIENT_FEATURES)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;

    if (!gatt_user_desc_writable(server, attr))
        return ATT_ERR_WRITE_NOT_PERMITTED;

    if (validate_value && (offset > BLE_GATT_ATT_VALUE_MAX ||
        len > BLE_GATT_ATT_VALUE_MAX - offset)
    ) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (attr->prepare || attr->execute)
        return (attr->prepare && attr->execute) ? 0 :
            ATT_ERR_REQUEST_NOT_SUPPORTED;

    if (attr->write_request) return ATT_ERR_REQUEST_NOT_SUPPORTED;

    // ATT defers static value offset and length validation until Execute Write.

    if (validate_value && !attr->prepare && !attr->execute) {
        if (offset > attr->value_len)
            return ATT_ERR_INVALID_OFFSET;

        if (!(attr->flags & BLE_GATT_ATTR_VAR_LEN) &&
            len > attr->value_len - offset
        ) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        if (offset > attr->value_capacity || len > attr->value_capacity - offset)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    }

    return 0;
}

static int gatt_error_response(
    uint8_t request, uint16_t handle, uint8_t error, uint8_t *rsp,
    uint16_t cap, uint16_t *rsp_len
) {
    if (cap < 5) return 0;
    rsp[0] = 0x01;
    rsp[1] = request;
    ble_put_le16(rsp + 2, handle);
    rsp[4] = error;
    *rsp_len = 5;
    return 1;
}

static int gatt_uuid_from_wire(
    const uint8_t *p, uint8_t len, ble_gatt_uuid *uuid
) {
    if (len != 2 && len != 16) return 0;
    uuid->len = len;
    memcpy(uuid->value, p, len);
    return 1;
}

static uint16_t gatt_group_end(
    const ble_gatt_server *server, uint16_t index, uint16_t handle
) {
    for (uint16_t i = index + 1; i < server->count; i++) {
        if (server->attrs[i].flags &
            (BLE_GATT_ATTR_PRIMARY_SERVICE |
             BLE_GATT_ATTR_SECONDARY_SERVICE)
        ) break;

        handle = server->attrs[i].handle;
    }
    return handle;
}

// =============================================================================
// ATT request parsing and response encoding
// =============================================================================
// Process one complete ATT PDU. Returns 1 when a response is present, 0 for
// one-way PDUs, and -1 for bad arguments or malformed one-way PDUs.
static inline int gatt_server_att(
    ble_gatt_server *server, const uint8_t *req,
    uint16_t req_len, uint8_t *rsp, uint16_t rsp_capacity,
    uint16_t *rsp_len
) {
    if (!server || !req || !req_len || !rsp || !rsp_len) return -1;

    *rsp_len = 0;
    uint8_t op = req[0];
    uint16_t mtu = server->mtu;
    // ATT commands (including Write Command and Signed Write Command) never
    // receive a response, even when malformed or larger than the bearer MTU.

    if (req_len > mtu) {
        if (op & 0x40) return 0;
        if (op == 0x1e) return -1;

        return gatt_error_response(op, 0,
                    ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);
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
            for (uint16_t i = 0; i < server->count; i++) {
                if (gatt_uuid_equal(&server->attrs[i].uuid, &changed_uuid) &&
                    server->attrs[i].handle == confirmed_handle
                ) service_changed_confirmed = 1;
            }

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
            return gatt_error_response(op, 0, ATT_ERR_INVALID_PDU,
                                        rsp, rsp_capacity, rsp_len);

        if (server->mtu_exchanged)
            return gatt_error_response(op, 0, 0x06, rsp, rsp_capacity, rsp_len);

        if (rsp_capacity < 3) return 0;

        uint16_t peer_mtu = ble_get_le16(req + 1);
        server->mtu = peer_mtu < 23 ? 23 :
            (peer_mtu < server->local_mtu ? peer_mtu : server->local_mtu);
        server->mtu_exchanged = 1;
        rsp[0] = 0x03;
        ble_put_le16(rsp + 1, server->local_mtu);
        *rsp_len = 3;
        return 1;
    }

    if (op == 0x10 || op == 0x06 || op == 0x08) {
        uint8_t group = op == 0x10;
        uint8_t find = op == 0x06;

        if ((group && req_len != 7 && req_len != 21) ||
            (find && req_len < 7) ||
            (!group && !find && req_len != 7 && req_len != 21)
        ) return gatt_error_response(op, 0,
            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        uint16_t first = ble_get_le16(req + 1);
        uint16_t last = ble_get_le16(req + 3);
        uint8_t uuid_len = (uint8_t)(req_len - 5);

        if (find) uuid_len = (uint8_t)(req_len - 7);
        if (first == 0 || first > last)
            return gatt_error_response(op, first, 0x01, rsp,
                                        rsp_capacity, rsp_len);

        if (!find &&
            (uuid_len != BLE_GATT_UUID16_LEN &&
             uuid_len != BLE_GATT_UUID128_LEN)
        ) return gatt_error_response(op, 0,
            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        ble_gatt_uuid type;
        const uint8_t *sought_value = NULL;
        uint16_t sought_len = 0;

        if (find) {
            if (!gatt_uuid_from_wire(req + 5, 2, &type))
                return gatt_error_response(op, 0,
                            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

            sought_value = req + 7;
            sought_len = req_len - 7;
            if (rsp_capacity < 5) return 0;

            rsp[0] = 0x07;
            uint16_t n = 1;

            for (uint16_t i = 0; i < server->count; i++) {
                ble_gatt_attribute *attr = &server->attrs[i];

                if (attr->handle < first || attr->handle > last ||
                    !gatt_uuid_equal(&attr->uuid, &type)
                ) continue;

                uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t value_len = sizeof(value);

                if (gatt_server_read(server, attr, 0, value, &value_len) ||
                    value_len != sought_len ||
                    (sought_len && memcmp(value, sought_value, sought_len))
                ) continue;

                if (n + 4 > rsp_capacity || n + 4 > mtu)
                    break;

                ble_put_le16(rsp + n, attr->handle);
                n += 2;
                uint16_t end = attr->handle;
                uint16_t type16 = ble_get_le16(type.value);

                if (type16 == 0x2800 || type16 == 0x2801)
                    end = gatt_group_end(server, i, end);
                ble_put_le16(rsp + n, end);
                n += 2;
            }

            if (n == 1)
                return gatt_error_response(op, first,
                ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);

            *rsp_len = n;
            return 1;
        }

        if (uuid_len != 2 && uuid_len != 16)
            return gatt_error_response(op, 0,
            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        if (!gatt_uuid_from_wire(req + 5, uuid_len, &type))
            return gatt_error_response(op, 0,
            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        uint16_t group_type = gatt_uuid_assigned16(&type);
        ble_gatt_uuid primary_type = gatt_uuid16(0x2800);
        ble_gatt_uuid secondary_type = gatt_uuid16(0x2801);

        if (group && !gatt_uuid_equal(&type, &primary_type) &&
            !gatt_uuid_equal(&type, &secondary_type)
        ) return gatt_error_response(op, first, ATT_ERR_UNSUPPORTED_GROUP_TYPE,
                                    rsp, rsp_capacity, rsp_len);

        if (rsp_capacity < 2) return 0;

        uint16_t n = 2;
        uint8_t entry_len = 0;
        rsp[0] = group ? 0x11 : 0x09;

        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *attr = &server->attrs[i];

            if (attr->handle < first || attr->handle > last)
                continue;

            if (group) {
                if (!((group_type == 0x2800 &&
                    (attr->flags & BLE_GATT_ATTR_PRIMARY_SERVICE)) ||
                    (group_type == 0x2801 &&
                    (attr->flags & BLE_GATT_ATTR_SECONDARY_SERVICE)))
                ) continue;
            }
            else if (!gatt_uuid_equal(&attr->uuid, &type))
                continue;

            uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
            uint16_t packet_limit = mtu < rsp_capacity ? mtu : rsp_capacity;
            uint16_t fixed = group ? 6 : 4;

            if (packet_limit <= fixed)
                break;

            uint16_t value_len = packet_limit - fixed;
            if (value_len > sizeof(value)) value_len = sizeof(value);
            uint16_t entry_value_max = (uint16_t)(255 - (group ? 4 : 2));

            if (value_len > entry_value_max) value_len = entry_value_max;
            uint8_t error = gatt_server_read(server, attr, 0, value, &value_len);

            if (error) {
                if (n == 2)
                    return gatt_error_response(op, attr->handle,
                    error, rsp, rsp_capacity, rsp_len);
                break;
            }

            uint16_t this_len = (uint16_t)(group ? 4 + value_len : 2 + value_len);
            if (!entry_len) {
                entry_len = this_len;
                rsp[1] = entry_len;
            }

            if (entry_len != this_len || n + this_len > mtu ||
                n + this_len > rsp_capacity
            ) break;

            ble_put_le16(rsp + n, attr->handle);
            n += 2;

            if (group) {
                uint16_t end = gatt_group_end(server, i, attr->handle);
                ble_put_le16(rsp + n, end);
                n += 2;
            }

            if (value_len) memcpy(rsp + n, value, value_len);
            n += value_len;
        }

        if (n == 2)
            return gatt_error_response(op, first,
            ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);

        *rsp_len = n;
        return 1;
    }

    if (op == 0x04) { // Find Information Request
        if (req_len != 5)
            return gatt_error_response(op, 0,
                    ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        uint16_t first = ble_get_le16(req + 1);
        uint16_t last = ble_get_le16(req + 3);

        if (!first || first > last)
            return gatt_error_response(op, first, 0x01, rsp, rsp_capacity, rsp_len);

        if (rsp_capacity < 2) return 0;

        uint8_t format = 0;
        uint16_t n = 2;
        rsp[0] = 0x05;

        for (uint16_t i = 0; i < server->count; i++) {
            ble_gatt_attribute *attr = &server->attrs[i];

            if (attr->handle < first || attr->handle > last)
                continue;

            uint8_t f = attr->uuid.len == 2 ? 1 : 2;
            uint8_t entry = (uint8_t)(2 + attr->uuid.len);

            if (format && f != format)
                break;

            if (!format) {
                format = f;
                rsp[1] = f;
            }

            if (n + entry > mtu || n + entry > rsp_capacity)
                break;

            ble_put_le16(rsp + n, attr->handle);
            n += 2;
            memcpy(rsp + n, attr->uuid.value, attr->uuid.len);
            n += attr->uuid.len;
        }

        if (n == 2)
            return gatt_error_response(op, first,
            ATT_ERR_ATTRIBUTE_NOT_FOUND, rsp, rsp_capacity, rsp_len);

        *rsp_len = n;
        return 1;
    }

    if (op == 0x0a || op == 0x0c) { // Read / Read Blob
        if (req_len != (op == 0x0a ? 3 : 5))
            return gatt_error_response(op, 0,
                    ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        uint16_t h = ble_get_le16(req + 1);
        ble_gatt_attribute *attr = gatt_server_find(server, h);

        if (!attr)
            return gatt_error_response(op, h,
            ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);

        uint16_t offset = op == 0x0c ? ble_get_le16(req + 3) : 0;

        if (rsp_capacity < 1) return 0;
        uint16_t value_len = rsp_capacity - 1;

        if (value_len > mtu - 1) value_len = mtu - 1;
        uint8_t error = gatt_server_read( server, attr, offset, rsp + 1, &value_len);

        if (error)
            return gatt_error_response(op, h, error, rsp, rsp_capacity, rsp_len);

        rsp[0] = op == 0x0a ? 0x0b : 0x0d;
        *rsp_len = value_len + 1;
        return 1;
    }

    if (op == 0x0e || op == 0x20) { // Read Multiple / Read Multiple Variable
        uint8_t variable = op == 0x20;

        if (req_len < 5 || ((req_len - 1) & 1))
            return gatt_error_response(op, 0,
                    ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        uint16_t n = 1;
        uint16_t packet_limit = mtu < rsp_capacity ? mtu : rsp_capacity;
        if (!packet_limit) return 0;

        rsp[0] = variable ? 0x21 : 0x0f;
        for (uint16_t offset = 1; offset < req_len; offset += 2) {
            uint16_t h = ble_get_le16(req + offset);
            ble_gatt_attribute *attr = gatt_server_find(server, h);

            if (!attr)
                return gatt_error_response(op, h,
                        ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);

            if (variable) {
                uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t value_len = sizeof(value);
                uint8_t error = gatt_server_read(server, attr, 0, value, &value_len);

                if (error)
                    return gatt_error_response(op, h, error, rsp, rsp_capacity, rsp_len);

                if (n >= packet_limit || packet_limit - n < 2)
                    continue;

                uint16_t copy_len = packet_limit - n - 2;

                if (copy_len > value_len) copy_len = value_len;
                ble_put_le16(rsp + n, value_len);
                n += 2;
                if (copy_len) memcpy(rsp + n, value, copy_len);
                n += copy_len;
                continue;
            }

            if (n >= packet_limit) {
                uint8_t discarded[BLE_GATT_SERVER_VALUE_MAX];
                uint16_t discarded_len = sizeof(discarded);
                uint8_t error = gatt_server_read(server, attr, 0, discarded, &discarded_len);

                if (error)
                    return gatt_error_response(op, h, error, rsp, rsp_capacity, rsp_len);

                continue;
            }

            uint16_t len = packet_limit - n;
            uint8_t error = gatt_server_read(server, attr, 0, rsp + n, &len);

            if (error)
                return gatt_error_response(op, h, error,
                rsp, rsp_capacity, rsp_len);

            n += len;
        }

        if (n == 1)
            return gatt_error_response(op, 0,
                    ATT_ERR_INSUFFICIENT_RESOURCES, rsp, rsp_capacity, rsp_len);

        *rsp_len = n;
        return 1;
    }

    if (op == 0x16) { // Prepare Write Request
        if (req_len < 5)
            return gatt_error_response(op, 0,
                    ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        if (rsp_capacity < req_len) return 0;

        uint16_t h = ble_get_le16(req + 1);
        uint16_t offset = ble_get_le16(req + 3);
        uint16_t value_len = req_len - 5;
        ble_gatt_attribute *attr = gatt_server_find(server, h);

        if (!attr)
            return gatt_error_response(op, h,
                    ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);

        uint8_t error = gatt_prepare_validate( server, attr, offset, value_len, 0);
        if (error)
            return gatt_error_response(op, h, error, rsp,
            rsp_capacity, rsp_len);

        if (server->prepare_count >= BLE_GATT_SERVER_PREPARE_QUEUE_SIZE ||
            value_len > BLE_GATT_SERVER_PREPARE_BYTES - server->prepare_used
        ) return gatt_error_response(op, h, ATT_ERR_INSUFFICIENT_RESOURCES, rsp,
                                    rsp_capacity, rsp_len);

        if (attr->prepare) {
            error = attr->prepare(attr->context, offset, req + 5, value_len);
            if (error)
                return gatt_error_response(op, h, error, rsp, rsp_capacity, rsp_len);
        }

        ble_gatt_prepared_write *queued = &server->prepared[server->prepare_count++];
        queued->handle = h;
        queued->offset = offset;
        queued->len = value_len;
        queued->data_offset = server->prepare_used;

        if (value_len)
            memcpy(server->prepare_data + server->prepare_used, req + 5, value_len);
        server->prepare_used += value_len;
        memcpy(rsp, req, req_len);
        rsp[0] = 0x17;
        *rsp_len = req_len;
        return 1;
    }

    if (op == 0x18) { // Execute Write Request
        if (req_len != 2 || req[1] > 1)
            return gatt_error_response(op, 0,
                        ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);

        if (rsp_capacity < 1) return 0;

        if (req[1]) {
            uint16_t error_handle = 0;
            uint8_t error = 0;

            do {
                // Validate the full batch before changing any attribute.
                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    ble_gatt_prepared_write *p = &server->prepared[i];
                    ble_gatt_attribute *attr = gatt_server_find(server, p->handle);

                    if (!attr) {
                        error_handle = p->handle;
                        error = ATT_ERR_INVALID_HANDLE;
                        break;
                    }

                    error = gatt_prepare_validate(server, attr, p->offset, p->len, 1);
                    if (error) {
                        error_handle = p->handle;
                        break;
                    }
                }

                if (error) break;

                // Validate resulting CCCD values after checking all fragment
                // ranges, since a prepared update may span both octets.
                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    ble_gatt_prepared_write *p = &server->prepared[i];
                    ble_gatt_attribute *attr = gatt_server_find(server, p->handle);

                    if (!(attr->flags & BLE_GATT_ATTR_CCCD) &&
                        gatt_uuid_assigned16(&attr->uuid) != 0x2903
                    ) continue;

                    uint8_t first = 1;
                    for (uint16_t j = 0; j < i; j++)
                        if (server->prepared[j].handle == p->handle)
                            first = 0;
                    if (!first) continue;

                    uint8_t value[2];

                    if (attr->flags & BLE_GATT_ATTR_CCCD) {
                        gatt_prepare_cccd(server, attr, value);
                    }
                    else {
                        uint8_t *stored = gatt_attribute_value(server, attr);
                        value[0] = stored[0];
                        value[1] = stored[1];

                        for (uint16_t j = 0; j < server->prepare_count; j++) {
                            ble_gatt_prepared_write *part = &server->prepared[j];

                            if (part->handle != attr->handle)
                                continue;

                            for (uint16_t k = 0; k < part->len; k++)
                                value[part->offset + k] = server->prepare_data[part->data_offset + k];
                        }
                    }
                    uint16_t bits = ble_get_le16(value);
                    uint8_t invalid = 0;

                    if (attr->flags & BLE_GATT_ATTR_CCCD) {
                        uint16_t allowed = (uint16_t)(
                            (attr->properties & BLE_GATT_PROP_NOTIFY ? 1 : 0) |
                            (attr->properties & BLE_GATT_PROP_INDICATE ? 2 : 0));
                        invalid = (bits & (uint16_t)~allowed) != 0;
                    }
                    else {
                        ble_gatt_attribute *characteristic = gatt_server_find(server, attr->parent_handle);
                        invalid = (bits & (uint16_t)~1u) != 0 ||
                                    ((bits & 1) && (!characteristic ||
                                    !(characteristic->properties &
                                    BLE_GATT_PROP_BROADCAST)));
                    }

                    if (invalid) {
                        error_handle = p->handle;
                        error = ATT_ERR_VALUE_NOT_ALLOWED;
                        break;
                    }
                }

                if (error) break;

                // Require new bytes to be covered continuously from the old
                // value length, so queued gaps cannot expose stale bytes.
                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    ble_gatt_prepared_write *p = &server->prepared[i];
                    ble_gatt_attribute *attr = gatt_server_find(server, p->handle);
                    if (attr->execute) continue;

                    uint16_t end = attr->value_len;
                    for (uint16_t j = 0; j < server->prepare_count; j++) {
                        ble_gatt_prepared_write *part = &server->prepared[j];
                        uint16_t part_end = part->offset + part->len;

                        if (part->handle == p->handle && part_end > end)
                            end = part_end;
                    }

                    uint16_t covered = attr->value_len;
                    while (covered < end) {
                        uint16_t next = covered;
                        for (uint16_t j = 0; j < server->prepare_count; j++) {
                            ble_gatt_prepared_write *part = &server->prepared[j];
                            uint16_t part_end = part->offset + part->len;

                            if (part->handle == p->handle &&
                                part->offset <= covered && part_end > next
                            ) next = part_end;
                        }

                        if (next == covered) {
                            error_handle = p->handle;
                            error = ATT_ERR_INVALID_OFFSET;
                            break;
                        }
                        covered = next;
                    }
                    if (error) break;
                }

                if (error) break;

                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    ble_gatt_prepared_write *p = &server->prepared[i];
                    uint8_t first = 1;
                    for (uint16_t j = 0; j < i; j++)
                        if (server->prepared[j].handle == p->handle)
                            first = 0;
                    if (!first) continue;

                    ble_gatt_attribute *attr = gatt_server_find(server, p->handle);
                    if (!attr || !attr->prepare_commit_validate) continue;
                    uint16_t value_len = attr->value_len;

                    for (uint16_t j = 0; j < server->prepare_count; j++) {
                        ble_gatt_prepared_write *part = &server->prepared[j];
                        uint16_t end = part->offset + part->len;
                        if (part->handle == attr->handle && end > value_len)
                            value_len = end;
                    }

                    if (value_len > BLE_GATT_ATT_VALUE_MAX) {
                        error_handle = attr->handle;
                        error = ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
                        break;
                    }

                    uint8_t value[BLE_GATT_ATT_VALUE_MAX];
                    memset(value, 0, value_len);
                    uint8_t *stored = gatt_attribute_value(server, attr);
                    if (attr->value_len && !stored) {
                        error_handle = attr->handle;
                        error = ATT_ERR_UNLIKELY_ERROR;
                        break;
                    }

                    if (attr->value_len) memcpy(value, stored, attr->value_len);
                    for (uint16_t j = 0; j < server->prepare_count; j++) {
                        ble_gatt_prepared_write *part = &server->prepared[j];
                        if (part->handle != attr->handle)
                            continue;

                        memcpy(value + part->offset,
                            server->prepare_data + part->data_offset,
                            part->len);
                    }

                    error = attr->prepare_commit_validate(attr->context, value, value_len);
                    if (error) {
                        error_handle = attr->handle;
                        break;
                    }
                }

                if (error) break;

                // Callback commits are infallible after their prepare phase.
                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    uint16_t handle = server->prepared[i].handle;
                    uint8_t first = 1;
                    for (uint16_t j = 0; j < i; j++)
                        if (server->prepared[j].handle == handle)
                            first = 0;
                    if (!first) continue;

                    ble_gatt_attribute *attr = gatt_server_find(server, handle);

                    if (attr && attr->execute)
                        attr->execute(attr->context, 1);
                }

                // Static commits are bounded copies into registered storage.
                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    ble_gatt_prepared_write *p = &server->prepared[i];
                    ble_gatt_attribute *attr = gatt_server_find(server, p->handle);
                    if (attr->execute) continue;

                    if (attr->flags & BLE_GATT_ATTR_CCCD) {
                        uint8_t first = 1;
                        for (uint16_t j = 0; j < i; j++)
                            if (server->prepared[j].handle == p->handle)
                                first = 0;
                        if (!first) continue;

                        uint8_t value[2];
                        gatt_prepare_cccd(server, attr, value);
                        error = gatt_server_write(server, attr, 0, value, sizeof(value), 0);

                        if (error) {
                            error_handle = p->handle;
                            break;
                        }

                        continue;
                    }

                    uint8_t *value = gatt_attribute_value(server, attr);

                    if (p->len)
                        memcpy(value + p->offset, server->prepare_data + p->data_offset, p->len);
                    if (p->offset + p->len > attr->value_len)
                        attr->value_len = p->offset + p->len;
                }

                for (uint16_t i = 0; i < server->prepare_count; i++) {
                    uint16_t handle = server->prepared[i].handle;
                    uint8_t first = 1;

                    for (uint16_t j = 0; j < i; j++)
                        if (server->prepared[j].handle == handle)
                            first = 0;
                    if (!first) continue;

                    gatt_write_committed(server, gatt_server_find(server, handle));
                }
            } while (0);

            if (error) {
                gatt_cancel_prepares(server, 0);
                memset(server->prepare_data, 0, server->prepare_used);
                server->prepare_count = 0;
                server->prepare_used = 0;
                return gatt_error_response(op, error_handle, error, rsp,
                                            rsp_capacity, rsp_len);
            }
        }
        else {
            gatt_cancel_prepares(server, 0);
        }

        memset(server->prepare_data, 0, server->prepare_used);
        server->prepare_count = 0;
        server->prepare_used = 0;
        rsp[0] = 0x19;
        *rsp_len = 1;
        return 1;
    }

    if (op == 0xd2) { // Signed Write Command
        if (req_len < 15 || server->encrypted || !server->signed_verify)
            return 0;

        uint16_t handle = ble_get_le16(req + 1);
        ble_gatt_attribute *attr = gatt_server_find(server, handle);

        if (!attr || !(attr->properties & BLE_GATT_PROP_AUTH_SIGNED_WRITE) ||
            !(attr->permissions & BLE_GATT_PERM_WRITE_SIGNED)
        ) return 0;

        uint16_t value_len = req_len - 15;
        uint16_t signed_len = req_len - 12;

        if (server->signed_verify(server->signed_context, req, signed_len,
                                req + signed_len)
        ) (void)gatt_server_write(server, attr, 0, req + 3, value_len, 2);
        return 0;
    }

    if (op == 0x12 || op == 0x52) { // Write Request / Write Command
        uint8_t command = op == 0x52;

        if (!command && rsp_capacity < 1)
            return 0;

        if (req_len < 3) {
            if (command) return 0;

            return gatt_error_response(op, 0,
            ATT_ERR_INVALID_PDU, rsp, rsp_capacity, rsp_len);
        }

        uint16_t h = ble_get_le16(req + 1);
        ble_gatt_attribute *attr = gatt_server_find(server, h);

        if (!attr) {
            if (command) return 0;

            return gatt_error_response(op, h,
                        ATT_ERR_INVALID_HANDLE, rsp, rsp_capacity, rsp_len);
        }

        uint8_t error = gatt_server_write(server, attr, 0, req + 3, req_len - 3, command);
        if (command) return 0;

        if (error)
            return gatt_error_response(op, h, error, rsp, rsp_capacity, rsp_len);

        rsp[0] = 0x13;
        *rsp_len = 1;
        return 1;
    }

    if (op & 0x40)
        return 0; // ATT commands never receive a response.

    return gatt_error_response(op, 0, ATT_ERR_REQUEST_NOT_SUPPORTED,
                                rsp, rsp_capacity, rsp_len);

}

#endif
