#ifndef BLE_GATT_SERVICE_SCAN_PARAMETERS_H
#define BLE_GATT_SERVICE_SCAN_PARAMETERS_H

// Scan Parameters Service protocol behavior. The application applies valid
// parameters to its scanner and decides when to request a client refresh.
// Internal
static uint8_t gatt_scan_parameters_window_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (len != 4)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint16_t interval = ble_get_le16(value);
    uint16_t window = ble_get_le16(value + 2);

    if (interval < 4 || interval > 0x4000 || window < 4 ||
        window > interval
    ) return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_scan_parameters_service *service = context;
    service->scan_interval = interval;
    service->scan_window = window;
    return 0;
}

// Application facing
// Add Scan Parameters Service with a validated initial interval and window.
// Valid client writes update service state; the application applies the new
// parameters to its scanner.
static inline int gatt_scan_parameters_service_add(
    ble_gatt_server *server, ble_gatt_scan_parameters_service *service,
    const uint8_t scan_interval_window[4]
) {
    if (!service || !scan_interval_window)
        return 0;

    uint16_t interval = ble_get_le16(scan_interval_window);
    uint16_t window = ble_get_le16(scan_interval_window + 2);

    if (interval < 4 || interval > 0x4000 || window < 4 ||
        window > interval
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->scan_interval = interval;
    service->scan_window = window;
    uint8_t value[4];
    ble_put_le16(value, interval);
    ble_put_le16(value + 2, window);
    uint8_t refresh_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a4f, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP, 0,
         BLE_GATT_PERM_WRITE, value, sizeof(value)},
        {0x2a31, BLE_GATT_PROP_NOTIFY, 1, 0, &refresh_value, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_scan_parameters_window_write, .context = service},
        {0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add_with_handlers(server, 0x1813, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->scan_interval_window_handle = values[0];
    service->scan_refresh_handle = values[1];
    service->scan_refresh_cccd_handle = cccds[1];
    return 1;
}

// Application facing
// Notify the client that it should refresh its scan parameters.
static inline int gatt_scan_parameters_service_notify_refresh(
    ble_gatt_server *server,
    const ble_gatt_scan_parameters_service *service
) {
    if (!server || !service || !service->scan_refresh_handle)
        return 0;

    const uint8_t refresh = 0;
    return gatt_server_queue_event(server, service->scan_refresh_handle,
        &refresh, sizeof(refresh), 0);
}

#endif
