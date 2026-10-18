#ifndef BLE_GATT_SERVICES_H
#define BLE_GATT_SERVICES_H

// Bluetooth SIG GATT service database builders.
// Profile procedures and application state belong in service-specific modules.
#include "../ble_gatt_server.h"

// Generic 16-bit UUID service registration and ATT handler binding.
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
    ble_gatt_read_fn read_value;
    ble_gatt_write_fn write_request;
    void *context;
    ble_gatt_prepare_fn prepare;
    ble_gatt_execute_fn execute;
} ble_gatt_sig_handlers;

// Application-facing service registration.
// Add a 16-bit UUID service and its caller-defined characteristics as one
// transaction. The caller supplies the assigned characteristic UUIDs,
// properties, permissions, values, and optional CCCDs.
static inline int gatt_sig_register(
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
        added = gatt_add_included_service(server,
            included_service_handles[i], NULL);
    for (uint8_t i = 0; added && i < count; i++) {
        ble_gatt_uuid uuid = {
            2, {(uint8_t)characteristics[i].uuid,
                (uint8_t)(characteristics[i].uuid >> 8)}
        };
        added = gatt_add_characteristic(server, &uuid,
            characteristics[i].properties, characteristics[i].permissions,
            characteristics[i].value, characteristics[i].value_len,
            characteristics[i].value_capacity ?
                characteristics[i].value_capacity :
                characteristics[i].value_len,
            handlers ? handlers[i].read_value : NULL,
            handlers ? handlers[i].write_request : NULL,
            handlers ? handlers[i].context : NULL, NULL,
            &value_handles[i]);

        if (added && handlers && (handlers[i].prepare ||
                                  handlers[i].execute)) {
            if (!handlers[i].prepare || !handlers[i].execute) {
                added = 0;
            }
            else {
                ble_gatt_attribute *attribute = gatt_server_find(
                    server, value_handles[i]);
                if (!attribute ||
                    !(attribute->permissions & (BLE_GATT_PERM_WRITE |
                        BLE_GATT_PERM_WRITE_ENC |
                        BLE_GATT_PERM_WRITE_AUTHN |
                        BLE_GATT_PERM_WRITE_AUTHZ))) {
                    added = 0;
                }
                else {
                    attribute->prepare = handlers[i].prepare;
                    attribute->execute = handlers[i].execute;
                }
            }
        }

        if (added && characteristics[i].variable_length)
            server->attrs[server->count - 1].flags |=
                BLE_GATT_ATTR_VAR_LEN;

        if (added && characteristics[i].cccd) {
            if (!cccd_handles) {
                added = 0;
            }
            else {
                added = gatt_add_descriptor(server, &cccd_uuid,
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
            added = gatt_add_descriptor(server, &descriptor_uuid,
                descriptor->permissions, descriptor->value,
                descriptor->value_len, descriptor->value_capacity,
                NULL, NULL, NULL, NULL);
        }
    }

    if (!added) {
        memset(server->attrs + old_count, 0,
               (server->count - old_count) * sizeof(server->attrs[0]));
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

// Add a primary service with included services and per-characteristic handlers.
static inline int gatt_sig_add_full(
    ble_gatt_server *server, uint16_t service_uuid16,
    const uint16_t *included_service_handles, uint8_t include_count,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_register(server,
        service_uuid16, 1, included_service_handles, include_count,
        characteristics, count, service_handle, value_handles,
        cccd_handles, handlers);
}

// Add a service with caller-selected included services and per-characteristic
// ATT handlers as one rollback-safe database operation.
static inline int gatt_sig_add_handlers(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_add_full(server,
        service_uuid16, NULL, 0, characteristics, count, service_handle,
        value_handles, cccd_handles, handlers);
}

// Add a secondary service with caller-supplied characteristics and handlers.
static inline int gatt_sig_add_secondary(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles, const ble_gatt_sig_handlers *handlers
) {
    return gatt_sig_register(server,
        service_uuid16, 0, NULL, 0, characteristics, count, service_handle,
        value_handles, cccd_handles, handlers);
}

// Add a service with per-characteristic read, write, and reliable-write
// handlers. Service modules use this to connect profile procedures to the
// generic ATT server; simple static databases can use gatt_sig_add().
static inline int gatt_sig_add(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles
) {
    return gatt_sig_add_handlers(server, service_uuid16,
        characteristics, count, service_handle, value_handles, cccd_handles,
        NULL);
}

// Helpers shared by service modules.
static inline void gatt_service_rollback(
    ble_gatt_server *server, uint16_t old_count, uint16_t old_next_handle,
    uint16_t old_value_used
) {
    memset(server->attrs + old_count, 0,
           (server->count - old_count) * sizeof(server->attrs[0]));
    memset(server->value_pool + old_value_used, 0,
           server->value_used - old_value_used);
    server->count = old_count;
    server->next_handle = old_next_handle;
    server->value_used = old_value_used;
}

static inline int gatt_dts_cp_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication
        ) return 1;

    return 0;
}

// Backing storage shared by service value objects.
typedef struct {
    const uint8_t *data;
    uint16_t len, capacity;
} ble_gatt_service_value;

// Bluetooth Mesh Provisioning and Proxy GATT services.
typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_prov_service;

typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_proxy_service;

// Application-facing builders. The application implements the Mesh protocol.
static inline int gatt_mesh_prov_add(
    ble_gatt_server *server, ble_gatt_mesh_prov_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2adb, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 65, 1},
        {0x2adc, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_add(server, 0x1827, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

static inline int gatt_mesh_proxy_add(
    ble_gatt_server *server, ble_gatt_mesh_proxy_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2add, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 66, 1},
        {0x2ade, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_add(server, 0x1828, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

#define GATT_MESH_SOLICIT_UUID 0x1859u
#define GATT_MESH_SOLICIT_PDU_LEN 17u

// Application-facing advertising helper
// Append the Mesh Proxy Solicitation service-data AD structure. The caller
// supplies the encrypted and authenticated PDU; Mesh key handling and
// sequence management remain in the Mesh layer. This is advertising data,
// not a GATT attribute service.
static inline int gatt_mesh_proxy_solicit(
    uint8_t *ad, size_t capacity, size_t *len,
    const uint8_t pdu[GATT_MESH_SOLICIT_PDU_LEN]
) {
    const size_t ad_len = GATT_MESH_SOLICIT_PDU_LEN + 4u;

    if (!ad || !len || !pdu || *len > capacity ||
        ad_len > capacity - *len
    ) return 0;

    uint8_t *out = ad + *len;
    memmove(out + 4, pdu, GATT_MESH_SOLICIT_PDU_LEN);
    out[0] = (uint8_t)(GATT_MESH_SOLICIT_PDU_LEN + 3u);
    out[1] = 0x16; // Service Data - 16-bit UUID
    out[2] = (uint8_t)GATT_MESH_SOLICIT_UUID;
    out[3] = (uint8_t)(GATT_MESH_SOLICIT_UUID >> 8);
    *len += ad_len;
    return 1;
}

// Service-specific database builders.
#include "ble_gatt_service_sport.h"
#include "ble_gatt_service_health.h"
#include "ble_gatt_service_audio.h"
#include "ble_gatt_service_user.h"
#include "ble_gatt_service_device.h"
#include "ble_gatt_service_peripherals.h"

#endif
