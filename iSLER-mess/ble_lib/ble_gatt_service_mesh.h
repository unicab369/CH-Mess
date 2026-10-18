#ifndef BLE_GATT_SERVICE_MESH_H
#define BLE_GATT_SERVICE_MESH_H

// Bluetooth Mesh Provisioning and Proxy services.
typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_provisioning_service;

typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_proxy_service;

// Application facing
// Add the Bluetooth Mesh Provisioning Service. The application handles the
// provisioning protocol and sends Data Out notifications through the returned
// characteristic and CCCD handles.
static inline int gatt_mesh_provisioning_service_add(
    ble_gatt_server *server, ble_gatt_mesh_provisioning_service *service
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

    if (!gatt_sig_service_add(server, 0x1827, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

// Application facing
// Add the Bluetooth Mesh Proxy Service. The application handles proxy PDU
// processing and sends Data Out notifications using the returned handles.
static inline int gatt_mesh_proxy_service_add(
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

    if (!gatt_sig_service_add(server, 0x1828, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

#endif
