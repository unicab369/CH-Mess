#ifndef BLE_GATT_SERVICE_LOCATION_NAVIGATION_H
#define BLE_GATT_SERVICE_LOCATION_NAVIGATION_H

// Internal
static inline int gatt_lns_feature32(
    ble_gatt_server *server, uint16_t handle, uint32_t *features
) {
    if (!server || !handle || !features) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    if (!attribute || attribute->value_len < 4) return 0;
    const uint8_t *value = gatt_attribute_value(server, attribute);
    if (!value) return 0;
    *features = (uint32_t)value[0] | (uint32_t)value[1] << 8 |
        (uint32_t)value[2] << 16 | (uint32_t)value[3] << 24;
    return 1;
}

// Internal
static inline int gatt_lns_control_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;
    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication)
            return 1;
    return 0;
}

// Internal
static inline uint8_t gatt_lns_control_response(
    ble_gatt_server *server, uint16_t handle, uint8_t request_opcode,
    uint8_t response_code, const uint8_t *parameters, uint16_t parameter_len
) {
    uint8_t response[20];
    if (parameter_len > sizeof(response) - 3 ||
        (parameter_len && !parameters))
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    response[0] = 0x20;
    response[1] = request_opcode;
    response[2] = response_code;
    if (parameter_len) memcpy(response + 3, parameters, parameter_len);
    return gatt_server_queue_event(server, handle, response,
        (uint16_t)(parameter_len + 3), 1) ? 0 :
        BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES;
}

// Internal
static inline uint8_t gatt_lns_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_location_navigation_service *service = context;
    if (!service || !service->server || !value)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (!len) return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        server, service->control_point_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;

    if (gatt_lns_control_busy(server, service->control_point_handle) ||
        service->control_point_pending_opcode)
        return 0xfe;

    uint32_t features = 0;
    if (!gatt_lns_feature32(server, service->feature_handle, &features))
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    uint8_t opcode = value[0];
    uint8_t response = 1;
    uint8_t parameters[3];
    uint16_t parameter_len = 0;

    if (opcode == 1) {
        if (!(features & (1u << 1))) response = 2;
        else if (len != 4) response = 3;
        else service->total_distance = (uint32_t)value[1] |
            (uint32_t)value[2] << 8 | (uint32_t)value[3] << 16;
    } else if (opcode == 2) {
        if (len != 3) response = 3;
        else service->location_speed_content_mask = ble_get_le16(value + 1);
    } else if (opcode == 3) {
        if (!(features & (1u << 12))) response = 2;
        else if (len != 2 || value[1] > 5) response = 3;
        else {
            service->navigation_command = value[1];
            if (value[1] == 0 || value[1] == 2)
                service->navigation_running = 0;
            else if (value[1] == 1 || value[1] == 3 || value[1] == 5)
                service->navigation_running = 1;
        }
    } else if (opcode == 4 || opcode == 5 || opcode == 6) {
        uint8_t feature_bit = opcode == 4 ? 10 : opcode == 5 ? 11 : 12;
        if (!(features & (1u << feature_bit))) response = 2;
        else if ((opcode == 4 && len != 1) ||
                 (opcode != 4 && len != 3)) response = 3;
        else {
            service->control_point_pending_opcode = opcode;
            service->control_point_request_parameter = opcode == 4 ? 0 :
                ble_get_le16(value + 1);
            service->control_point_request_count++;
            return 0;
        }
    } else if (opcode == 7) {
        if (len != 2 || !value[1]) response = 3;
        else service->fix_rate = value[1];
    } else if (opcode == 8) {
        if (!(features & (1u << 3))) response = 2;
        else if (len != 4) response = 3;
        else {
            int32_t elevation = (int32_t)value[1] |
                (int32_t)value[2] << 8 | (int32_t)value[3] << 16;
            if (elevation & 0x800000) elevation |= (int32_t)0xff000000;
            service->elevation = elevation;
        }
    } else {
        response = 2;
    }
    return gatt_lns_control_response(server, service->control_point_handle,
                                     opcode, response,
                                     parameters, parameter_len);
}

// Application facing
// Complete an asynchronous LN Control Point route operation.
static inline int gatt_location_navigation_service_control_point_complete(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint8_t response, const uint8_t *parameters, uint16_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending_opcode || response < 1 || response > 4 ||
        (parameter_len && !parameters) || parameter_len > 17 ||
        gatt_lns_control_busy(server, service->control_point_handle))
        return 0;
    if (!gatt_lns_control_response(server, service->control_point_handle,
            service->control_point_pending_opcode, response,
            parameters, parameter_len))
        return 0;
    if (response == 1 && service->control_point_pending_opcode == 6)
        service->selected_route = service->control_point_request_parameter;
    service->control_point_pending_opcode = 0;
    return 1;
}

#endif
