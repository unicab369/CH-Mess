#ifndef BLE_GATT_SERVICE_LOCATION_NAVIGATION_H
#define BLE_GATT_SERVICE_LOCATION_NAVIGATION_H

typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t location_speed_handle, location_speed_cccd_handle;
    uint16_t navigation_handle, navigation_cccd_handle;
    uint16_t position_quality_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
    ble_gatt_server *server;
    uint32_t total_distance;
    int32_t elevation;
    uint16_t location_speed_content_mask, selected_route;
    uint16_t control_point_request_parameter;
    uint8_t fix_rate, navigation_command, navigation_running;
    uint8_t control_point_pending_opcode, control_point_request_count;
} ble_gatt_location_navigation_service;

// Internal
static inline int gatt_lns_feature32(
    ble_gatt_server *server, uint16_t handle, uint32_t *features
) {
    if (!server || !handle || !features)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, handle);

    if (!attribute || attribute->value_len < 4)
        return 0;

    const uint8_t *value = gatt_attribute_value(server, attribute);

    if (!value) return 0;

    *features = (uint32_t)value[0] | (uint32_t)value[1] << 8 |
        (uint32_t)value[2] << 16 | (uint32_t)value[3] << 24;
    return 1;
}

// Application facing
// Reset connection-scoped state when a new peer connection is established.
static inline int gatt_location_navigation_service_reset_connection(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service
) {
    if (!server || !service || service->server != server)
        return 0;

    service->location_speed_content_mask = 0;
    service->control_point_pending_opcode = 0;
    service->control_point_request_parameter = 0;
    return 1;
}

// Internal
static inline void gatt_lns_put_le24(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
}

// Internal
static inline void gatt_lns_put_le32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

// Application facing
// Queue one Location and Speed notification. Fields are supplied in the
// characteristic's fixed wire units; utc_time is the seven-byte Date Time
// value when flag 6 is present.
static inline int gatt_location_navigation_service_notify_location_speed(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint16_t flags, uint16_t instantaneous_speed, uint32_t total_distance,
    int32_t latitude, int32_t longitude, int32_t elevation,
    uint16_t heading, uint8_t rolling_time, const uint8_t *utc_time
) {
    if (!server || !service || service->server != server ||
        !service->location_speed_handle || flags & 0xe000u ||
        total_distance > 0xffffffu || elevation < -8388608 ||
        elevation > 8388607 || latitude < -900000000 ||
        latitude > 900000000 || longitude < -1800000000 ||
        longitude > 1800000000
    ) return 0;

    uint32_t features = 0;

    if (!gatt_lns_feature32(server, service->feature_handle, &features))
        return 0;

    uint16_t content_flags = flags & 0x007fu;
    uint16_t supported = (uint16_t)(features & 0x007fu);

    if ((content_flags & ~supported) ||
        ((flags & 0x0180u) && !(features & (1u << 20)))
    ) return 0;

    flags &= (uint16_t)~service->location_speed_content_mask;

    if (!(flags & (1u << 3)))
        flags &= (uint16_t)~(3u << 10);
    if (!(flags & (1u << 4)))
        flags &= (uint16_t)~(1u << 12);
    if ((flags & (1u << 6)) && !utc_time)
        return 0;

    uint8_t value[BLE_GATT_ATT_VALUE_MAX];
    uint16_t index = 2;
    ble_put_le16(value, flags);

    if (flags & (1u << 0)) {
        ble_put_le16(value + index, instantaneous_speed);
        index += 2;
    }

    if (flags & (1u << 1)) {
        gatt_lns_put_le24(value + index, total_distance);
        index += 3;
    }

    if (flags & (1u << 2)) {
        gatt_lns_put_le32(value + index, (uint32_t)latitude);
        gatt_lns_put_le32(value + index + 4, (uint32_t)longitude);
        index += 8;
    }

    if (flags & (1u << 3)) {
        gatt_lns_put_le24(value + index, (uint32_t)elevation);
        index += 3;
    }

    if (flags & (1u << 4)) {
        ble_put_le16(value + index, heading);
        index += 2;
    }

    if (flags & (1u << 5))
        value[index++] = rolling_time;
    if (flags & (1u << 6)) {
        memcpy(value + index, utc_time, 7);
        index += 7;
    }

    if (server->mtu < 3 || index > server->mtu - 3)
        return 0;

    return gatt_queue_event(server, service->location_speed_handle,
                                   value, index, 0);
}

// Application facing
// Queue one Navigation notification using the standard field order.
static inline int gatt_location_navigation_service_notify_navigation(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint16_t flags, uint16_t bearing, uint16_t heading,
    uint32_t remaining_distance, int32_t remaining_vertical_distance,
    const uint8_t *estimated_time_of_arrival
) {
    if (!server || !service || service->server != server ||
        !service->navigation_handle || flags & 0xfe00u ||
        remaining_distance > 0xffffffu || remaining_vertical_distance < -8388608 ||
        remaining_vertical_distance > 8388607
    ) return 0;

    uint32_t features = 0;

    if (!gatt_lns_feature32(server, service->feature_handle, &features))
        return 0;

    if (((flags & (1u << 0)) && !(features & (1u << 7))) ||
        ((flags & (1u << 1)) && !(features & (1u << 8))) ||
        ((flags & (1u << 2)) && !(features & (1u << 9))) ||
        ((flags & 0x0018u) && !(features & (1u << 20))) ||
        ((flags & (1u << 2)) && !estimated_time_of_arrival)
    ) return 0;

    uint8_t value[19];
    uint16_t index = 2;
    ble_put_le16(value, flags);
    ble_put_le16(value + index, bearing);
    ble_put_le16(value + index + 2, heading);
    index += 4;

    if (flags & (1u << 0)) {
        gatt_lns_put_le24(value + index, remaining_distance);
        index += 3;
    }

    if (flags & (1u << 1)) {
        gatt_lns_put_le24(value + index,
                          (uint32_t)remaining_vertical_distance);
        index += 3;
    }

    if (flags & (1u << 2)) {
        memcpy(value + index, estimated_time_of_arrival, 7);
        index += 7;
    }

    if (server->mtu < 3 || index > server->mtu - 3)
        return 0;

    return gatt_queue_event(server, service->navigation_handle,
                                   value, index, 0);
}

// Internal
static inline int gatt_lns_control_busy(
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

// Internal
static inline uint8_t gatt_lns_control_response(
    ble_gatt_server *server, uint16_t handle, uint8_t request_opcode,
    uint8_t response_code, const uint8_t *parameters, uint16_t parameter_len
) {
    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    if (!server || !handle || parameter_len > sizeof(response) - 3 ||
        server->mtu < 6 ||
        parameter_len > server->mtu - 6 ||
        (parameter_len && !parameters)
    ) return ATT_ERR_UNLIKELY_ERROR;

    response[0] = 0x20;
    response[1] = request_opcode;
    response[2] = response_code;

    if (parameter_len)
        memcpy(response + 3, parameters, parameter_len);
    return gatt_queue_event(server, handle, response,
        (uint16_t)(parameter_len + 3), 1) ? 0 :
        ATT_ERR_INSUFFICIENT_RESOURCES;
}

// Internal
static inline uint8_t gatt_lns_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_location_navigation_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (gatt_lns_control_busy(server, service->control_point_handle) ||
        service->control_point_pending_opcode
    ) return 0xfe;

    uint32_t features = 0;

    if (!gatt_lns_feature32(server, service->feature_handle, &features))
        return ATT_ERR_UNLIKELY_ERROR;

    uint8_t opcode = value[0];
    uint8_t response = 1;
    uint8_t parameters[3];
    uint16_t parameter_len = 0;
    uint8_t commit_state = 0;
    uint32_t next_total_distance = service->total_distance;
    int32_t next_elevation = service->elevation;
    uint16_t next_content_mask = service->location_speed_content_mask;
    uint8_t next_fix_rate = service->fix_rate;
    uint8_t next_navigation_command = service->navigation_command;
    uint8_t next_navigation_running = service->navigation_running;

    if (opcode == 1) {
        if (!(features & (1u << 1)))
            response = 2;
        else if (len != 4) response = 3;
        else {
            next_total_distance = (uint32_t)value[1] |
                (uint32_t)value[2] << 8 | (uint32_t)value[3] << 16;
            commit_state = 1;
        }
    }
    else if (opcode == 2) {
        if (!(features & (1u << 17)))
            response = 2;
        else if (len != 3 || (ble_get_le16(value + 1) & ~0x007fu))
            response = 3;
        else {
            next_content_mask = ble_get_le16(value + 1);
            commit_state = 1;
        }
    }
    else if (opcode == 3) {
        if (!service->navigation_handle)
            response = 2;
        else if (len != 2 || value[1] > 5)
            response = 3;
        else {
            next_navigation_command = value[1];
            if (value[1] == 0 || value[1] == 2)
                next_navigation_running = 0;
            else if (value[1] == 1 || value[1] == 3 || value[1] == 5)
                next_navigation_running = 1;
            commit_state = 1;
        }
    }
    else if (opcode == 4 || opcode == 5 || opcode == 6) {
        if (!service->navigation_handle)
            response = 2;
        else if ((opcode == 4 && len != 1) ||
                 (opcode != 4 && len != 3)) response = 3;
        else {
            service->control_point_pending_opcode = opcode;
            service->control_point_request_parameter = opcode == 4 ? 0 :
                ble_get_le16(value + 1);
            service->control_point_request_count++;
            return 0;
        }
    }
    else if (opcode == 7) {
        if (!(features & (1u << 18)))
            response = 2;
        else if (len != 2 || !value[1])
            response = 3;
        else {
            next_fix_rate = value[1];
            commit_state = 1;
        }
    }
    else if (opcode == 8) {
        if (!(features & (1u << 19)))
            response = 2;
        else if (len != 4) response = 3;
        else {
            int32_t elevation = (int32_t)value[1] |
                (int32_t)value[2] << 8 | (int32_t)value[3] << 16;
            if (elevation & 0x800000)
                elevation |= (int32_t)0xff000000;
            next_elevation = elevation;
            commit_state = 1;
        }
    }
    else {
        response = 2;
    }
    uint8_t error = gatt_lns_control_response(server,
        service->control_point_handle, opcode, response, parameters,
        parameter_len);

    if (!error && commit_state) {
        service->total_distance = next_total_distance;
        service->location_speed_content_mask = next_content_mask;
        service->navigation_command = next_navigation_command;
        service->navigation_running = next_navigation_running;
        service->fix_rate = next_fix_rate;
        service->elevation = next_elevation;
    }

    return error;
}

// Application facing
// Complete an asynchronous LN Control Point route operation.
static inline int gatt_location_navigation_service_control_point_complete(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint8_t response, const uint8_t *parameters, uint16_t parameter_len
) {
    uint8_t opcode = service ? service->control_point_pending_opcode : 0;
    if (!server || !service || service->server != server ||
        !service->control_point_pending_opcode || response < 1 || response > 4 ||
        (parameter_len && !parameters) ||
        parameter_len > BLE_GATT_ATT_VALUE_MAX - 6 ||
        gatt_lns_control_busy(server, service->control_point_handle)
    ) return 0;

    if ((response != 1 && parameter_len) ||
        (response == 1 && opcode == 4 && parameter_len != 1) ||
        (response == 1 && opcode == 5 && parameter_len < 3) ||
        (response == 1 && opcode == 5 &&
            ble_get_le16(parameters) != service->control_point_request_parameter) ||
        (response == 1 && opcode == 6 && parameter_len)
    ) return 0;

    if (!gatt_lns_control_response(server, service->control_point_handle,
            service->control_point_pending_opcode, response,
            parameters, parameter_len)
    ) return 0;

    if (response == 1 && service->control_point_pending_opcode == 6)
        service->selected_route = service->control_point_request_parameter;
    service->control_point_pending_opcode = 0;
    return 1;
}

// Internal
static inline uint8_t gatt_lns_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

// Application facing
static inline int gatt_location_navigation_service_add(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint32_t features, int include_navigation,
    const uint8_t *position_quality, uint16_t position_quality_len,
    int include_control_point
) {
    uint16_t position_quality_flags = 0;
    if (position_quality && position_quality_len >= 2)
        position_quality_flags = ble_get_le16(position_quality);
    uint16_t position_quality_features =
        (uint16_t)((features >> 10) & 0x7fu);
    uint16_t expected_position_quality_len = 2;

    for (uint8_t bit = 0; bit < 7; bit++) {
        if (position_quality_flags & (1u << bit))
            expected_position_quality_len += bit == 2 ? 2 :
                (bit == 3 || bit == 4 ? 4 : 1);
    }
    if (!server || !service || include_navigation < 0 ||
        include_navigation > 1 || include_control_point < 0 ||
        include_control_point > 1 || (features & 0xffe00000u) ||
        ((!position_quality && position_quality_len) ||
                     (position_quality && position_quality_len < 2)) ||
        (position_quality_features && !position_quality) ||
        (position_quality && (!position_quality_flags ||
            (position_quality_flags & ~0x007fu) ||
            (position_quality_flags & ~position_quality_features) ||
            position_quality_len != expected_position_quality_len))
    ) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[5];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a6a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a67, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    uint8_t navigation_index = 0xff, quality_index = 0xff;
    uint8_t control_index = 0xff;

    if (include_navigation) {
        navigation_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a68, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }

    if (position_quality) {
        quality_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a69, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            position_quality, position_quality_len
        };
    }

    if (include_control_point) {
        control_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a6b,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
    }

    ble_gatt_sig_handlers handlers[5] = {0};

    if (control_index != 0xff) {
        handlers[control_index].write = gatt_lns_control_point_write;
        handlers[control_index].context = service;
    }

    service->server = server;
    uint16_t values[5], cccds[5];

    if (!gatt_sig_service_add_with_handlers(server, 0x1819, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->feature_handle = values[0];
    service->location_speed_handle = values[1];
    service->location_speed_cccd_handle = cccds[1];

    if (navigation_index != 0xff) {
        service->navigation_handle = values[navigation_index];
        service->navigation_cccd_handle = cccds[navigation_index];
    }

    if (quality_index != 0xff)
        service->position_quality_handle = values[quality_index];

    if (control_index != 0xff) {
        service->control_point_handle = values[control_index];
        service->control_point_cccd_handle = cccds[control_index];
    }

    return 1;
}

#endif
