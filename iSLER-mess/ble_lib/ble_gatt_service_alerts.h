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
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;

    if (offset > 1)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;

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
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;

    if (len != 1)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] > 2)
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

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
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;

    if (len != 1)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] > 2)
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

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
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;

    if (len != 1)
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] < 1 || value[0] > 3)
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;

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

    return gatt_server_queue_event(server, service->alert_status_handle,
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

    return gatt_server_queue_event(server, service->ringer_setting_handle,
        &service->ringer_setting, 1, 0);
}

#endif
