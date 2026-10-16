#ifndef BLE_GATT_SERVICE_ENVIRONMENTAL_SENSING_H
#define BLE_GATT_SERVICE_ENVIRONMENTAL_SENSING_H

// Application facing
// Update a stored Environmental Sensing measurement value.
static inline int gatt_environmental_sensing_service_set_value(
    ble_gatt_server *server,
    const ble_gatt_environmental_sensing_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || !value)
        return 0;
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;
    if (!stored || len > attribute->value_capacity ||
        (!(attribute->flags & BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH) &&
         len != attribute->value_len))
        return 0;
    memcpy(stored, value, len);
    attribute->value_len = len;
    return 1;
}

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
