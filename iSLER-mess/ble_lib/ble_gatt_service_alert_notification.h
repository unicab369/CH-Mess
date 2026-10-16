#ifndef BLE_GATT_SERVICE_ALERT_NOTIFICATION_H
#define BLE_GATT_SERVICE_ALERT_NOTIFICATION_H

// Alert Notification Profile control point protocol. The application owns
// alert records and sends any requested notifications.
#define GATT_ALERT_CATEGORY_MASK 0x03ffu
#define GATT_ALERT_CATEGORY_ALL  0xffu

// Internal
static uint8_t gatt_alert_notification_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (len != 2)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t cmd = value[0];
    uint8_t category = value[1];

    if (cmd > 5 || (category > 9 && category != GATT_ALERT_CATEGORY_ALL))
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_alert_notification_service *service = context;
    uint16_t supported = (cmd == 1 || cmd == 3 || cmd == 5) ?
        service->supported_unread_alerts : service->supported_new_alerts;
    uint16_t category_bit = category == GATT_ALERT_CATEGORY_ALL ?
        supported : (uint16_t)(1u << category);

    if (category_bit & (uint16_t)~supported)
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

    if (cmd == 0 || cmd == 1) {
        uint16_t *enabled = cmd == 0 ? &service->enabled_new_alerts :
                                      &service->enabled_unread_alerts;
        *enabled |= category_bit;
    }
    else if (cmd == 2 || cmd == 3) {
        uint16_t *enabled = cmd == 2 ? &service->enabled_new_alerts :
                                      &service->enabled_unread_alerts;
        *enabled &= (uint16_t)~category_bit;
    }

    service->last_command = cmd;
    service->last_category = category;
    service->control_count++;
    return 0;
}

// Application facing
// Add Alert Notification Service with supported categories. Control point
// writes update enable masks and the last-command fields; the application
// handles alert data and sends requested notifications.
static inline int gatt_alert_notification_service_add(
    ble_gatt_server *server, ble_gatt_alert_notification_service *service,
    uint16_t supported_new_alerts, uint16_t supported_unread_alerts
) {
    if (!service ||
        (supported_new_alerts & (uint16_t)~GATT_ALERT_CATEGORY_MASK) ||
        (supported_unread_alerts & (uint16_t)~GATT_ALERT_CATEGORY_MASK)
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->supported_new_alerts = supported_new_alerts;
    service->supported_unread_alerts = supported_unread_alerts;
    uint8_t new_categories[2];
    uint8_t unread_categories[2];
    ble_put_le16(new_categories, supported_new_alerts);
    ble_put_le16(unread_categories, supported_unread_alerts);
    uint8_t alert_value[2] = {0, 0};
    ble_gatt_sig_characteristic chars[] = {
        {0x2a47, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         new_categories, sizeof(new_categories)},
        {0x2a48, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         unread_categories, sizeof(unread_categories)},
        {0x2a46, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {0x2a45, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {.uuid = 0x2a44,
         .properties = BLE_GATT_PROP_WRITE,
         .permissions = BLE_GATT_PERM_WRITE,
         .value_capacity = 2,
         .variable_length = 1}
    };
    ble_gatt_sig_handlers handlers[5] = {0};
    handlers[4].write = gatt_alert_notification_control_write;
    handlers[4].context = service;
    uint16_t values[5], cccds[5];

    if (!gatt_sig_service_add_with_handlers(server, 0x1811, chars, 5,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->supported_new_alerts_handle = values[0];
    service->supported_unread_alerts_handle = values[1];
    service->new_alert_handle = values[2];
    service->new_alert_cccd_handle = cccds[2];
    service->unread_alert_status_handle = values[3];
    service->unread_alert_status_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    return 1;
}

// Application facing
// Queue a New Alert notification for an enabled, supported category. Text is
// optional and limited to the 18 octets allowed by the characteristic format.
static inline int gatt_alert_notification_service_notify_new(
    ble_gatt_server *server,
    const ble_gatt_alert_notification_service *service,
    uint8_t category, uint8_t count, const uint8_t *text, uint16_t text_len
) {
    if (!server || !service || category > 9 || text_len > 18 ||
        (text_len && !text)
    ) return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_new_alerts & category_bit) ||
        !(service->enabled_new_alerts & category_bit)
    ) return 0;

    uint8_t value[20] = {category, count};

    if (text_len)
        memcpy(value + 2, text, text_len);
    return gatt_server_queue_event(server, service->new_alert_handle,
        value, (uint16_t)(2 + text_len), 0);
}

// Application facing
// Queue an Unread Alert Status notification for an enabled, supported
// category.
static inline int gatt_alert_notification_service_notify_unread(
    ble_gatt_server *server,
    const ble_gatt_alert_notification_service *service,
    uint8_t category, uint8_t count
) {
    if (!server || !service || category > 9)
        return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_unread_alerts & category_bit) ||
        !(service->enabled_unread_alerts & category_bit)
    ) return 0;

    uint8_t value[2] = {category, count};
    return gatt_server_queue_event(server, service->unread_alert_status_handle,
        value, sizeof(value), 0);
}

#endif
