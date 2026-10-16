#ifndef BLE_GATT_SERVICE_ENVIRONMENTAL_SENSING_H
#define BLE_GATT_SERVICE_ENVIRONMENTAL_SENSING_H

// Application facing
// Queue a value for one caller-selected Environmental Sensing measurement.
static inline int gatt_environmental_sensing_service_send(
    ble_gatt_server *server, const ble_gatt_environmental_sensing_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len,
    int indication
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || (len && !value))
        return 0;
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t property = indication ? BLE_GATT_PROP_INDICATE :
        BLE_GATT_PROP_NOTIFY;
    if (!attribute || !(attribute->properties & property)) return 0;
    return gatt_server_queue_event(server,
        service->measurement_handles[measurement_index], value, len,
        indication != 0);
}

#endif
