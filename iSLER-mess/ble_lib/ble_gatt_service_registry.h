#ifndef BLE_GATT_SERVICE_REGISTRY_H
#define BLE_GATT_SERVICE_REGISTRY_H

// Generic 16-bit UUID service registration and ATT handler binding.
#include "ble_gatt_server.h"

typedef struct {
    uint16_t uuid;
    uint16_t permissions;
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_sig_descriptor;

typedef struct {
    uint16_t uuid;
    uint8_t properties, cccd;
    uint16_t permissions;
    const uint8_t *value;
    uint16_t value_len;
    uint16_t value_capacity;
    uint8_t variable_length;
    const ble_gatt_sig_descriptor *descriptors;
    uint8_t descriptor_count;
} ble_gatt_sig_characteristic;

typedef struct {
    ble_gatt_read_fn read;
    ble_gatt_write_fn write;
    void *context;
    ble_gatt_prepare_fn prepare;
    ble_gatt_execute_fn execute;
} ble_gatt_sig_handlers;

// Application facing
// Add a 16-bit UUID service and its caller-defined characteristics as one
// transaction. The caller supplies the assigned characteristic UUIDs,
// properties, permissions, values, and optional CCCDs.
static inline int gatt_sig_service_register_with_includes_and_handlers(
    ble_gatt_server *server, uint16_t service_uuid16, uint8_t primary_service,
    const uint16_t *included_service_handles, uint8_t include_count,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    if (!server || (include_count && !included_service_handles) ||
        !characteristics || !count || !service_handle ||
        !value_handles || server->database_sealed
    ) return 0;
    for (uint8_t i = 0; i < count; i++)
        if (characteristics[i].descriptor_count &&
            !characteristics[i].descriptors
        ) return 0;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid service_uuid = {
        2, {(uint8_t)service_uuid16, (uint8_t)(service_uuid16 >> 8)}
    };
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    memset(value_handles, 0, (size_t)count * sizeof(*value_handles));

    if (cccd_handles)
        memset(cccd_handles, 0, (size_t)count * sizeof(*cccd_handles));

    int added = gatt_server_add_service(server, &service_uuid,
                                         primary_service,
                                         service_handle);
    for (uint8_t i = 0; added && i < include_count; i++)
        added = gatt_server_add_included_service(server,
            included_service_handles[i], NULL);
    for (uint8_t i = 0; added && i < count; i++) {
        ble_gatt_uuid uuid = {
            2, {(uint8_t)characteristics[i].uuid,
                (uint8_t)(characteristics[i].uuid >> 8)}
        };
        added = gatt_server_add_characteristic(server, &uuid,
            characteristics[i].properties, characteristics[i].permissions,
            characteristics[i].value, characteristics[i].value_len,
            characteristics[i].value_capacity ?
                characteristics[i].value_capacity :
                characteristics[i].value_len,
            handlers ? handlers[i].read : NULL,
            handlers ? handlers[i].write : NULL,
            handlers ? handlers[i].context : NULL, NULL,
            &value_handles[i]);

        if (added && handlers && (handlers[i].prepare ||
                                  handlers[i].execute)) {
            if (!handlers[i].prepare || !handlers[i].execute) {
                added = 0;
            } else {
                ble_gatt_attribute *attribute = gatt_server_find(
                    server, value_handles[i]);
                if (!attribute ||
                    !(attribute->permissions & (BLE_GATT_PERM_WRITE |
                        BLE_GATT_PERM_WRITE_ENCRYPTED |
                        BLE_GATT_PERM_WRITE_AUTHENTICATED |
                        BLE_GATT_PERM_WRITE_AUTHORIZED))) {
                    added = 0;
                } else {
                    attribute->prepare = handlers[i].prepare;
                    attribute->execute = handlers[i].execute;
                }
            }
        }

        if (added && characteristics[i].variable_length)
            server->attributes[server->count - 1].flags |=
                BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;

        if (added && characteristics[i].cccd) {
            if (!cccd_handles) {
                added = 0;
            } else {
                added = gatt_server_add_descriptor(server, &cccd_uuid,
                    BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
                    NULL, 0, 0, NULL, NULL, NULL, &cccd_handles[i]);
            }
        }
        for (uint8_t j = 0; added &&
             j < characteristics[i].descriptor_count; j++) {
            const ble_gatt_sig_descriptor *descriptor =
                &characteristics[i].descriptors[j];
            ble_gatt_uuid descriptor_uuid = {
                2, {(uint8_t)descriptor->uuid,
                    (uint8_t)(descriptor->uuid >> 8)}
            };
            added = gatt_server_add_descriptor(server, &descriptor_uuid,
                descriptor->permissions, descriptor->value,
                descriptor->value_len, descriptor->value_capacity,
                NULL, NULL, NULL, NULL);
        }
    }

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(value_handles, 0, (size_t)count * sizeof(*value_handles));
        if (cccd_handles)
            memset(cccd_handles, 0, (size_t)count * sizeof(*cccd_handles));
        *service_handle = 0;
        return 0;
    }
    return 1;
}

// Application facing
// Add a primary service with included services and per-characteristic handlers.
static inline int gatt_sig_service_add_with_includes_and_handlers(
    ble_gatt_server *server, uint16_t service_uuid16,
    const uint16_t *included_service_handles, uint8_t include_count,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_service_register_with_includes_and_handlers(server,
        service_uuid16, 1, included_service_handles, include_count,
        characteristics, count, service_handle, value_handles,
        cccd_handles, handlers);
}

// Application facing
// Add a service with caller-selected included services and per-characteristic
// ATT handlers as one rollback-safe database operation.
static inline int gatt_sig_service_add_with_handlers(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_service_add_with_includes_and_handlers(server,
        service_uuid16, NULL, 0, characteristics, count, service_handle,
        value_handles, cccd_handles, handlers);
}

// Application facing
// Add a secondary service with caller-supplied characteristics and handlers.
static inline int gatt_sig_secondary_service_add_with_handlers(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_service_register_with_includes_and_handlers(server,
        service_uuid16, 0, NULL, 0, characteristics, count, service_handle,
        value_handles, cccd_handles, handlers);
}

// Application facing
// Add a service with per-characteristic read, write, and reliable-write
// handlers. Service modules use this to connect profile procedures to the
// generic ATT server; simple static databases can use gatt_sig_service_add().
static inline int gatt_sig_service_add(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles
) {
    return gatt_sig_service_add_with_handlers(server, service_uuid16,
        characteristics, count, service_handle, value_handles, cccd_handles,
        NULL);
}


#endif
