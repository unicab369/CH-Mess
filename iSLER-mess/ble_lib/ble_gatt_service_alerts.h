#ifndef BLE_GATT_SERVICE_ALERTS_H
#define BLE_GATT_SERVICE_ALERTS_H

// Protocol behavior for Immediate Alert, Link Loss, and Phone Alert Status.
// The application observes the state fields to perform device-specific
// actions.

// Internal
static uint8_t gatt_alert_u8_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1)
        return ATT_ERR_INVALID_OFFSET;

    if (!offset && *inout_len) {
        out[0] = *(uint8_t *)context;
        *inout_len = 1;
    }
    else {
        *inout_len = 0;
    }
    return 0;
}

// Internal
static uint8_t gatt_alert_level_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 2)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    *(uint8_t *)context = value[0];
    return 0;
}

// Internal
static uint8_t gatt_immediate_alert_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 2)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_immediate_alert_service *service = context;
    service->alert_level = value[0];
    service->request_count++;
    return 0;
}

// Internal
static uint8_t gatt_phone_ringer_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] < 1 || value[0] > 3)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_phone_alert_status_service *service = context;
    service->last_ringer_control = value[0];
    service->ringer_control_count++;

    if (value[0] == 1)
        service->ringer_setting = 0;
    if (value[0] == 3)
        service->ringer_setting = 1;
    return 0;
}

// Application facing
// Add Immediate Alert. Valid writes update alert_level and increment
// request_count so the application can detect repeated requests.
static inline int gatt_immediate_alert_service_add(
    ble_gatt_server *server, ble_gatt_immediate_alert_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2)
        return 0;

    memset(service, 0, sizeof(*service));
    service->alert_level = alert_level;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &service->alert_level, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_immediate_alert_write, .context = service}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add_with_handlers(server, 0x1802, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application facing
// Add Link Loss Alert Level with readable state and validated writes.
static inline int gatt_link_loss_service_add(
    ble_gatt_server *server, ble_gatt_link_loss_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2)
        return 0;

    memset(service, 0, sizeof(*service));
    service->alert_level = alert_level;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         &service->alert_level, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.read = gatt_alert_u8_read, .write = gatt_alert_level_write,
         .context = &service->alert_level}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add_with_handlers(server, 0x1803, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application facing
// Add Phone Alert Status. Valid Ringer Control Point writes update the
// exposed state; the application performs the corresponding phone behavior
// and sends notifications when the values change.
static inline int gatt_phone_alert_status_service_add(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    const uint8_t alert_status[1], uint8_t ringer_setting
) {
    if (!service || !alert_status || (*alert_status & (uint8_t)~0x07) ||
        ringer_setting > 1
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->alert_status = *alert_status;
    service->ringer_setting = ringer_setting;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a3f, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->alert_status, 1},
        {0x2a41, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->ringer_setting, 1},
        {0x2a40, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         NULL, 0, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.read = gatt_alert_u8_read, .context = &service->alert_status},
        {.read = gatt_alert_u8_read, .context = &service->ringer_setting},
        {.write = gatt_phone_ringer_control_write, .context = service}
    };
    uint16_t values[3], cccds[3];

    if (!gatt_sig_service_add_with_handlers(server, 0x180e, chars, 3,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->alert_status_handle = values[0];
    service->alert_status_cccd_handle = cccds[0];
    service->ringer_setting_handle = values[1];
    service->ringer_setting_cccd_handle = cccds[1];
    service->ringer_control_point_handle = values[2];
    return 1;
}

// Application facing
static inline int gatt_phone_alert_status_service_set_alert_status(
    ble_gatt_phone_alert_status_service *service, uint8_t alert_status
) {
    if (!service || !service->alert_status_handle ||
        (alert_status & (uint8_t)~0x07)
    ) return 0;

    service->alert_status = alert_status;
    return 1;
}

// Application facing
// Update Alert Status and queue its notification.
static inline int gatt_phone_alert_status_service_notify_alert_status(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    uint8_t alert_status
) {
    if (!server ||
        !gatt_phone_alert_status_service_set_alert_status(service,
            alert_status)
    ) return 0;

    return gatt_queue_event(server, service->alert_status_handle,
        &service->alert_status, 1, 0);
}

// Application facing
static inline int gatt_phone_alert_status_service_set_ringer_setting(
    ble_gatt_phone_alert_status_service *service, uint8_t ringer_setting
) {
    if (!service || !service->ringer_setting_handle || ringer_setting > 1)
        return 0;

    service->ringer_setting = ringer_setting;
    return 1;
}

// Application facing
// Update Ringer Setting and queue its notification.
static inline int gatt_phone_alert_status_service_notify_ringer_setting(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    uint8_t ringer_setting
) {
    if (!server ||
        !gatt_phone_alert_status_service_set_ringer_setting(service,
            ringer_setting)
    ) return 0;

    return gatt_queue_event(server, service->ringer_setting_handle,
        &service->ringer_setting, 1, 0);
}


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
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 2)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t cmd = value[0];
    uint8_t category = value[1];

    if (cmd > 5 || (category > 9 && category != GATT_ALERT_CATEGORY_ALL))
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_alert_notification_service *service = context;
    uint16_t supported = (cmd == 1 || cmd == 3 || cmd == 5) ?
        service->supported_unread_alerts : service->supported_new_alerts;
    uint16_t category_bit = category == GATT_ALERT_CATEGORY_ALL ?
        supported : (uint16_t)(1u << category);

    if (category_bit & (uint16_t)~supported)
        return ATT_ERR_VALUE_NOT_ALLOWED;

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
    return gatt_queue_event(server, service->new_alert_handle,
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
    return gatt_queue_event(server, service->unread_alert_status_handle,
        value, sizeof(value), 0);
}
#endif
