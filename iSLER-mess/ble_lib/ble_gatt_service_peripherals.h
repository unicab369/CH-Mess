#ifndef BLE_GATT_SERVICE_PERIPHERALS_H
#define BLE_GATT_SERVICE_PERIPHERALS_H

// Contents: Automation IO, Location and Navigation, Indoor Positioning, HID,
// HTTP Proxy, Object Transfer, Electronic Shelf Label, and sensor services.
// Automation IO Service
typedef struct {
    uint16_t service_handle, aggregate_handle, aggregate_cccd_handle;
    uint8_t channel_count;
    uint16_t channel_handles[8], channel_cccd_handles[8];
} ble_gatt_aio_service;

// Application-facing API
// Add Automation IO Digital and Analog characteristics, optionally including
// an Aggregate characteristic whose value lists the channel value handles.
// AIO means Automation IO Service.
static inline int gatt_aio_add(
    ble_gatt_server *server, ble_gatt_aio_service *service,
    const ble_gatt_sig_characteristic *channels, uint8_t channel_count,
    uint8_t include_aggregate, uint8_t aggregate_properties
) {
    if (!server || !service || !channels || !channel_count ||
        channel_count > 8 || include_aggregate > 1
    ) return 0;

    uint8_t aggregate_notify = aggregate_properties &
        (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

    if (include_aggregate &&
        (!(aggregate_properties & BLE_GATT_PROP_READ) ||
         (aggregate_properties & (BLE_GATT_PROP_WRITE |
                                  BLE_GATT_PROP_WRITE_NO_RSP |
                                  BLE_GATT_PROP_BROADCAST |
                                  BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                  BLE_GATT_PROP_EXTENDED)) ||
         (aggregate_notify == (BLE_GATT_PROP_NOTIFY |
                               BLE_GATT_PROP_INDICATE)))
    ) return 0;

    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t notify = channels[i].properties &
            (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

        if ((channels[i].uuid != 0x2a56 && channels[i].uuid != 0x2a58) ||
            (channels[i].properties & (BLE_GATT_PROP_BROADCAST |
                                       BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                       BLE_GATT_PROP_EXTENDED)) ||
            (notify == (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE)) ||
            (channels[i].cccd != !!notify) ||
            (!include_aggregate && aggregate_properties) ||
            (include_aggregate && notify) ||
            ((channels[i].properties & BLE_GATT_PROP_READ) &&
             !(channels[i].permissions & BLE_GATT_PERM_READ)) ||
            ((channels[i].properties & (BLE_GATT_PROP_WRITE |
                                        BLE_GATT_PROP_WRITE_NO_RSP)) &&
             !(channels[i].permissions & BLE_GATT_PERM_WRITE)) ||
            (channels[i].value_len && !channels[i].value)
        ) return 0;
    }

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[9];
    memcpy(characteristics, channels,
           (size_t)channel_count * sizeof(*channels));
    uint8_t count = channel_count;
    uint8_t aggregate_value[16] = {0};

    if (include_aggregate) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a5a,
            .properties = aggregate_properties,
            .cccd = !!aggregate_notify,
            .permissions = BLE_GATT_PERM_READ,
            .value = aggregate_value,
            .value_len = (uint16_t)(channel_count * 2),
            .value_capacity = (uint16_t)(channel_count * 2)
        };
    }

    uint16_t values[9], cccds[9];

    if (!gatt_sig_add(server, 0x1815, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->channel_count = channel_count;
    for (uint8_t i = 0; i < channel_count; i++) {
        service->channel_handles[i] = values[i];
        service->channel_cccd_handles[i] = cccds[i];
    }

    if (include_aggregate) {
        service->aggregate_handle = values[channel_count];
        service->aggregate_cccd_handle = cccds[channel_count];
        ble_gatt_attribute *aggregate = gatt_server_find(
            server, service->aggregate_handle);
        uint8_t *value = aggregate ? gatt_attribute_value(server, aggregate) : NULL;

        if (!value) return 0;

        for (uint8_t i = 0; i < channel_count; i++) {
            value[i * 2] = (uint8_t)values[i];
            value[i * 2 + 1] = (uint8_t)(values[i] >> 8);
        }
    }

    return 1;
}

// Location and Navigation Service
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
} ble_gatt_lns_service;

// Internal helper
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

// Application-facing API
// Reset connection-scoped state when a new peer connection is established.
// LNS means Location and Navigation Service.
static inline int gatt_lns_reset_connection(
    ble_gatt_server *server, ble_gatt_lns_service *service
) {
    if (!server || !service || service->server != server)
        return 0;

    service->location_speed_content_mask = 0;
    service->control_point_pending_opcode = 0;
    service->control_point_request_parameter = 0;
    return 1;
}

// Internal helper
static inline void gatt_lns_put_le24(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
}

// Internal helper
static inline void gatt_lns_put_le32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

// Application-facing API
// Queue one Location and Speed notification. Fields are supplied in the
// characteristic's fixed wire units; utc_time is the seven-byte Date Time
// value when flag 6 is present.
// LNS means Location and Navigation Service.
static inline int gatt_lns_notify_location(
    ble_gatt_server *server, ble_gatt_lns_service *service,
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

// Application-facing API
// Queue one Navigation notification using the standard field order.
// LNS means Location and Navigation Service.
static inline int gatt_lns_notify_nav(
    ble_gatt_server *server, ble_gatt_lns_service *service,
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

// Internal helper
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

// Internal helper
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

// Internal helper
static inline uint8_t gatt_lns_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_lns_service *service = context;

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

// Application-facing API
// Complete an asynchronous LN Control Point route operation.
// LNS means Location and Navigation Service.
static inline int gatt_lns_complete_control(
    ble_gatt_server *server, ble_gatt_lns_service *service,
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

// Application-facing API
// LNS means Location and Navigation Service.
static inline int gatt_lns_add(
    ble_gatt_server *server, ble_gatt_lns_service *service,
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
        handlers[control_index].write_request = gatt_lns_control_point_write;
        handlers[control_index].context = service;
    }

    service->server = server;
    uint16_t values[5], cccds[5];

    if (!gatt_sig_add_handlers(server, 0x1819, chars, count,
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

// Indoor Positioning Service
// Indoor Positioning
typedef struct ble_gatt_ips_service
    ble_gatt_ips_service;

typedef struct {
    ble_gatt_ips_service *service;
    uint16_t handle, fixed_length;
    uint8_t variable_length;
} ble_gatt_ips_write_ctx;

typedef struct {
    const uint8_t *local_north, *local_east;
    const uint8_t *floor_number, *altitude, *uncertainty;
    const uint8_t *location_name;
    uint16_t location_name_len, location_name_capacity;
} ble_gatt_ips_values;

struct ble_gatt_ips_service {
    uint16_t service_handle, configuration_handle, latitude_handle;
    uint16_t longitude_handle;
    uint16_t local_north_handle, local_east_handle, floor_number_handle;
    uint16_t altitude_handle, uncertainty_handle, location_name_handle;
    ble_gatt_server *server;
    uint8_t broadcast_data_changed;
    ble_gatt_ips_write_ctx write_context[9];
};

static uint8_t gatt_indoor_positioning_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ips_write_ctx *write = context;
    ble_gatt_ips_service *service =
        write ? write->service : NULL;

    if (!service || !service->server || !write || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      write->handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored)
        return ATT_ERR_UNLIKELY_ERROR;

    if (!write->variable_length) {
        if (offset)
            return ATT_ERR_INVALID_OFFSET;
        if (len != write->fixed_length)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (attribute->value_capacity < write->fixed_length)
            return ATT_ERR_UNLIKELY_ERROR;
        memcpy(stored, value, write->fixed_length);
    }
    else {
        if (offset > attribute->value_len)
            return ATT_ERR_INVALID_OFFSET;
        if (len > attribute->value_capacity - offset)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (len)
            memcpy(stored + offset, value, len);
        if (!offset)
            attribute->value_len = len;
        else if (offset + len > attribute->value_len)
            attribute->value_len = offset + len;
    }
    if (write->handle != service->location_name_handle)
        service->broadcast_data_changed = 1;
    return 0;
}

// IPS means Indoor Positioning Service.
static inline int gatt_ips_append(
    ble_gatt_ips_service *service,
    ble_gatt_sig_characteristic characteristics[9],
    ble_gatt_sig_handlers handlers[9], uint8_t *count,
    uint16_t uuid, const uint8_t *value, uint16_t value_len,
    uint16_t value_capacity, uint8_t variable_length
) {
    uint8_t index = *count;
    if (index >= 9 || value_len > value_capacity ||
        value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !value))
        return 0;
    characteristics[index] = (ble_gatt_sig_characteristic){
        uuid, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        value, value_len, value_capacity, variable_length
    };
    service->write_context[index].service = service;
    service->write_context[index].fixed_length = value_len;
    service->write_context[index].variable_length = variable_length;
    handlers[index].write_request = gatt_indoor_positioning_write;
    handlers[index].context = &service->write_context[index];
    (*count)++;
    return 1;
}

// Add mandatory Indoor Positioning characteristics and optional values.
// Configured optional broadcast fields must have corresponding values.
// IPS means Indoor Positioning Service.
static inline int gatt_ips_add_full(
    ble_gatt_server *server, ble_gatt_ips_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4],
    const ble_gatt_ips_values *optional
) {
    if (!server || !service || !latitude || !longitude ||
        server->database_sealed ||
        ((configuration & 0x01) && (configuration & 0x02) &&
         (!optional || !optional->local_north || !optional->local_east)) ||
        ((configuration & 0x08) && (!optional || !optional->altitude)) ||
        ((configuration & 0x10) && (!optional || !optional->floor_number)) ||
        ((configuration & 0x20) && (!optional || !optional->uncertainty)) ||
        ((configuration & 0x40) && (!optional || !optional->location_name)) ||
        (optional && (!!optional->local_north != !!optional->local_east)) ||
        (optional && (optional->location_name_len >
                      optional->location_name_capacity ||
                      optional->location_name_capacity >
                      BLE_GATT_SERVER_VALUE_MAX ||
                      (optional->location_name_len &&
                       !optional->location_name) ||
                      (!optional->location_name &&
                       (optional->location_name_len ||
                        optional->location_name_capacity)) ||
                      (optional->location_name &&
                       !optional->location_name_capacity)))
    )
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[9];
    ble_gatt_sig_handlers handlers[9] = {0};
    uint8_t count = 0;
    if (!gatt_ips_append(service, chars, handlers, &count,
            0x2aad, &configuration, 1, 1, 0) ||
        !gatt_ips_append(service, chars, handlers, &count,
            0x2aae, latitude, 4, 4, 0) ||
        !gatt_ips_append(service, chars, handlers, &count,
            0x2aaf, longitude, 4, 4, 0))
        return 0;
    if (optional && optional->local_north &&
        (!gatt_ips_append(service, chars, handlers, &count,
             0x2ab0, optional->local_north, 2, 2, 0) ||
         !gatt_ips_append(service, chars, handlers, &count,
             0x2ab1, optional->local_east, 2, 2, 0)))
        return 0;
    if (optional && optional->floor_number &&
        !gatt_ips_append(service, chars, handlers, &count,
            0x2ab2, optional->floor_number, 1, 1, 0))
        return 0;
    if (optional && optional->altitude &&
        !gatt_ips_append(service, chars, handlers, &count,
            0x2ab3, optional->altitude, 2, 2, 0))
        return 0;
    if (optional && optional->uncertainty &&
        !gatt_ips_append(service, chars, handlers, &count,
            0x2ab4, optional->uncertainty, 1, 1, 0))
        return 0;
    if (optional && optional->location_name &&
        !gatt_ips_append(service, chars, handlers, &count,
            0x2ab5, optional->location_name, optional->location_name_len,
            optional->location_name_capacity, 1))
        return 0;
    uint16_t values[9];
    service->server = server;
    if (!gatt_sig_add_handlers(server, 0x1821, chars, count,
            &service->service_handle, values, NULL, handlers)
    ) {
        service->server = NULL;
        return 0;
    }

    for (uint8_t i = 0; i < count; i++)
        service->write_context[i].handle = values[i];
    service->configuration_handle = values[0];
    service->latitude_handle = values[1];
    service->longitude_handle = values[2];
    uint8_t i = 3;
    if (optional && optional->local_north) {
        service->local_north_handle = values[i++];
        service->local_east_handle = values[i++];
    }

    if (optional && optional->floor_number)
        service->floor_number_handle = values[i++];
    if (optional && optional->altitude)
        service->altitude_handle = values[i++];
    if (optional && optional->uncertainty)
        service->uncertainty_handle = values[i++];
    if (optional && optional->location_name)
        service->location_name_handle = values[i++];
    service->broadcast_data_changed = 1;
    return 1;
}

// Return and clear the flag after regenerating Indoor Positioning advertising
// data from the readable characteristic values.
// IPS means Indoor Positioning Service.
static inline int gatt_ips_take_broadcast_update(
    ble_gatt_ips_service *service
) {
    if (!service || !service->server || !service->broadcast_data_changed)
        return 0;
    service->broadcast_data_changed = 0;
    return 1;
}

// Encode the Indoor Positioning Service AD structure from the current GATT
// values. tx_power_present must describe a Tx Power AD field supplied by GAP.
// IPS means Indoor Positioning Service.
static inline int gatt_ips_encode_advertising_data(
    const ble_gatt_ips_service *service,
    uint8_t tx_power_present, int8_t tx_power,
    uint8_t *out, uint16_t capacity, uint16_t *out_len
) {
    if (!service || !service->server || !out || !out_len ||
        tx_power_present > 1)
        return 0;
    *out_len = 0;
    ble_gatt_attribute *configuration_attribute = gatt_server_find(
        service->server, service->configuration_handle);
    const uint8_t *configuration_value = configuration_attribute ?
        gatt_attribute_value(service->server, configuration_attribute) : NULL;
    if (!configuration_value || configuration_attribute->value_len != 1)
        return 0;
    uint8_t configuration = configuration_value[0] & 0x7f;
    if ((configuration & 0x04) && !tx_power_present)
        return 0;

    uint8_t value[20];
    uint16_t len = 1;
    value[len++] = 0x25; // Indoor Positioning AD type.
    if (configuration) value[len++] = configuration;

    if (configuration & 0x01) {
        uint16_t first_handle = (configuration & 0x02) ?
            service->local_north_handle : service->latitude_handle;
        uint16_t second_handle = (configuration & 0x02) ?
            service->local_east_handle : service->longitude_handle;
        uint16_t coordinate_len = (configuration & 0x02) ? 2 : 4;
        ble_gatt_attribute *first = gatt_server_find(service->server,
                                                      first_handle);
        ble_gatt_attribute *second = gatt_server_find(service->server,
                                                       second_handle);
        const uint8_t *first_value = first ?
            gatt_attribute_value(service->server, first) : NULL;
        const uint8_t *second_value = second ?
            gatt_attribute_value(service->server, second) : NULL;
        if (!first_value || !second_value ||
            first->value_len != coordinate_len ||
            second->value_len != coordinate_len)
            return 0;
        memcpy(value + len, first_value, coordinate_len);
        len += coordinate_len;
        memcpy(value + len, second_value, coordinate_len);
        len += coordinate_len;
    }
    if (configuration & 0x04)
        value[len++] = (uint8_t)tx_power;

    const uint8_t flags[] = {0x10, 0x08, 0x20};
    const uint16_t handles[] = {
        service->floor_number_handle,
        service->altitude_handle,
        service->uncertainty_handle
    };
    const uint8_t lengths[] = {1, 2, 1};
    for (uint8_t i = 0; i < 3; i++) {
        if (!(configuration & flags[i])) continue;
        ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                          handles[i]);
        const uint8_t *field = attribute ?
            gatt_attribute_value(service->server, attribute) : NULL;
        if (!field || attribute->value_len != lengths[i])
            return 0;
        memcpy(value + len, field, lengths[i]);
        len += lengths[i];
    }

    if (len - 1 > 0xff || len > capacity)
        return 0;
    value[0] = (uint8_t)(len - 1);
    memcpy(out, value, len);
    *out_len = len;
    return 1;
}

// HID and HTTP Proxy
#define BLE_GATT_HID_MAX_REPORTS 8
typedef struct {
    uint8_t id, type;
    const uint8_t *value;
    uint16_t value_len;
} ble_gatt_hid_report;

typedef struct {
    uint16_t service_handle, information_handle, report_map_handle;
    uint16_t control_point_handle, protocol_mode_handle;
    uint16_t report_handles[BLE_GATT_HID_MAX_REPORTS];
    uint16_t report_cccd_handles[BLE_GATT_HID_MAX_REPORTS];
    uint8_t report_count;
} ble_gatt_hid_service;

// Application-facing API
// Add the HID core characteristics and up to eight Report characteristics.
// Report type is 1 (input), 2 (output), or 3 (feature); report values and the
// HID Report Map are copied into the server database.
// HID means Human Interface Device.
static inline int gatt_hid_add(
    ble_gatt_server *server, ble_gatt_hid_service *service,
    const uint8_t information[4], const uint8_t *report_map,
    uint16_t report_map_len, uint8_t protocol_mode,
    const ble_gatt_hid_report *reports, uint8_t report_count
) {
    if (!server || !service || !information || !report_map ||
        !report_map_len || report_map_len > BLE_GATT_SERVER_VALUE_MAX ||
        protocol_mode > 1 || report_count > BLE_GATT_HID_MAX_REPORTS ||
        (report_count && !reports)
    ) return 0;

    uint8_t control_point = 0;
    uint8_t protocol_mode_value = protocol_mode;
    ble_gatt_sig_characteristic characteristics[4 + BLE_GATT_HID_MAX_REPORTS] = {
        {0x2a4a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            information, 4},
        {0x2a4b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            report_map, report_map_len},
        {0x2a4c, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
            &control_point, 1},
        {0x2a4e, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE_NO_RSP, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &protocol_mode_value, 1}
    };
    uint8_t report_references[BLE_GATT_HID_MAX_REPORTS][2];
    ble_gatt_sig_descriptor descriptors[BLE_GATT_HID_MAX_REPORTS];
    for (uint8_t i = 0; i < report_count; i++) {
        if (!reports[i].id || reports[i].type < 1 || reports[i].type > 3 ||
            !reports[i].value || !reports[i].value_len
        ) return 0;

        report_references[i][0] = reports[i].id;
        report_references[i][1] = reports[i].type;
        descriptors[i] = (ble_gatt_sig_descriptor){
            0x2908, BLE_GATT_PERM_READ, report_references[i], 2, 2
        };
        uint8_t properties = reports[i].type == 1 ? BLE_GATT_PROP_NOTIFY :
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE;
        uint16_t permissions = reports[i].type == 1 ? 0 :
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE;
        characteristics[4 + i] = (ble_gatt_sig_characteristic){
            0x2a4d, properties, reports[i].type == 1, permissions,
            reports[i].value, reports[i].value_len,
            reports[i].value_len, 0, &descriptors[i], 1
        };
    }

    memset(service, 0, sizeof(*service));
    uint16_t values[4 + BLE_GATT_HID_MAX_REPORTS];
    uint16_t cccds[4 + BLE_GATT_HID_MAX_REPORTS];

    if (!gatt_sig_add(server, 0x1812, characteristics,
            (uint8_t)(4 + report_count), &service->service_handle,
            values, cccds)
    ) return 0;

    service->information_handle = values[0];
    service->report_map_handle = values[1];
    service->control_point_handle = values[2];
    service->protocol_mode_handle = values[3];
    service->report_count = report_count;
    for (uint8_t i = 0; i < report_count; i++) {
        service->report_handles[i] = values[4 + i];
        service->report_cccd_handles[i] = cccds[4 + i];
    }
    return 1;
}

typedef struct {
    uint16_t service_handle, properties_handle, operation_mode_handle;
    uint16_t operation_mode_cccd_handle;
} ble_gatt_hid_iso_service;

// Application-facing API
// Add HID ISO Properties and LE HID Operation Mode. Properties are the
// caller-encoded Properties value; the application
// implements mode switching and ISO report transport.
// HID means Human Interface Device.
static inline int gatt_hid_iso_add(
    ble_gatt_server *server, ble_gatt_hid_iso_service *service,
    const uint8_t *properties, uint16_t properties_len,
    const uint8_t *operation_mode,
    uint16_t operation_mode_len, uint16_t operation_mode_capacity,
    uint8_t mode_change_notify
) {
    if (!server || !service || !properties || properties_len < 3 ||
        properties_len > BLE_GATT_SERVER_VALUE_MAX || !operation_mode ||
        !operation_mode_capacity || operation_mode_len >
        operation_mode_capacity || operation_mode_capacity >
        BLE_GATT_SERVER_VALUE_MAX || mode_change_notify > 1
    ) return 0;

    ble_gatt_sig_characteristic chars[2] = {
        {0x2c23, BLE_GATT_PROP_READ, 0, 0, properties, properties_len},
        {0x2c24, BLE_GATT_PROP_WRITE |
            (mode_change_notify ? BLE_GATT_PROP_INDICATE : 0),
            mode_change_notify, 0, operation_mode, operation_mode_len,
            operation_mode_capacity, 1}
    };
    uint16_t values[2], cccds[2];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x185c, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->properties_handle = values[0];
    service->operation_mode_handle = values[1];
    service->operation_mode_cccd_handle = cccds[1];
    return 1;
}

// HTTP Proxy Service (HPS).
typedef struct {
    uint16_t service_handle, uri_handle, headers_handle;
    uint16_t status_code_handle, entity_body_handle;
    uint16_t control_point_handle, https_security_handle;
    ble_gatt_server *server;
    uint8_t request_pending, request_taken, request_opcode;
    uint8_t cancel_requested, request_count;
    uint8_t uri_configured, headers_configured, entity_body_configured;
    uint8_t response_headers_received, response_body_received;
    uint8_t (*request_validate)(void *context, uint8_t opcode,
        const uint8_t *uri, uint16_t uri_len,
        const uint8_t *headers, uint16_t headers_len,
        const uint8_t *body, uint16_t body_len);
    void *request_validate_context;
} ble_gatt_hps_service;

enum {
    GATT_HTTP_PROXY_ERR_INVALID_REQUEST = 0x81,
    GATT_HTTP_PROXY_ERR_NETWORK_NOT_AVAILABLE = 0x82,
    GATT_HTTP_PROXY_ERR_PROCEDURE_ALREADY_IN_PROGRESS = 0xfe,
    GATT_HTTP_PROXY_DATA_STATUS_HEADERS_RECEIVED = 0x01,
    GATT_HTTP_PROXY_DATA_STATUS_HEADERS_TRUNCATED = 0x02,
    GATT_HTTP_PROXY_DATA_STATUS_BODY_RECEIVED = 0x04,
    GATT_HTTP_PROXY_DATA_STATUS_BODY_TRUNCATED = 0x08
};

static void gatt_http_proxy_value_written(
    void *context, uint16_t handle, const uint8_t *value, uint16_t len
) {
    (void)value;
    (void)len;
    ble_gatt_hps_service *service = context;
    if (!service) return;
    if (handle == service->uri_handle) service->uri_configured = 1;
    else if (handle == service->headers_handle)
        service->headers_configured = 1;
    else if (handle == service->entity_body_handle)
        service->entity_body_configured = 1;
}

static uint8_t gatt_http_proxy_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_hps_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len != 1) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->status_code_handle);
    if (!cccd || !(cccd->cccd & 1)) return 0xfd;

    uint8_t opcode = value[0];
    if (opcode == 0x0b) {
        if (!service->request_pending)
            return GATT_HTTP_PROXY_ERR_INVALID_REQUEST;
        service->cancel_requested = 1;
        service->request_count++;
        return 0;
    }
    if (opcode < 0x01 || opcode > 0x0a)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (service->request_pending)
        return GATT_HTTP_PROXY_ERR_PROCEDURE_ALREADY_IN_PROGRESS;
    if (!service->uri_configured || !service->headers_configured ||
        !service->entity_body_configured)
        return GATT_HTTP_PROXY_ERR_INVALID_REQUEST;
    ble_gatt_attribute *uri = gatt_server_find(service->server,
        service->uri_handle);
    if (!uri || !uri->value_len)
        return GATT_HTTP_PROXY_ERR_INVALID_REQUEST;
    if (service->request_validate) {
        ble_gatt_attribute *headers = gatt_server_find(service->server,
            service->headers_handle);
        ble_gatt_attribute *body = gatt_server_find(service->server,
            service->entity_body_handle);
        const uint8_t *uri_value = gatt_attribute_value(service->server, uri);
        const uint8_t *headers_value = headers ?
            gatt_attribute_value(service->server, headers) : NULL;
        const uint8_t *body_value = body ?
            gatt_attribute_value(service->server, body) : NULL;
        if (!headers || !body || !uri_value ||
            (headers->value_len && !headers_value) ||
            (body->value_len && !body_value))
            return ATT_ERR_UNLIKELY_ERROR;
        uint8_t error = service->request_validate(
            service->request_validate_context, opcode,
            uri_value, uri->value_len, headers_value, headers->value_len,
            body_value, body->value_len);
        if (error) return error;
    }
    service->request_opcode = opcode;
    service->request_pending = 1;
    service->request_taken = 0;
    service->cancel_requested = 0;
    service->response_headers_received = 0;
    service->response_body_received = 0;
    if (opcode >= 0x06) {
        ble_gatt_attribute *https = gatt_server_find(service->server,
            service->https_security_handle);
        uint8_t *value = https ? gatt_attribute_value(service->server,
            https) : NULL;
        if (value && https->value_capacity) value[0] = 0;
    }
    service->request_count++;
    return 0;
}

// Application-facing API
// Add the HTTP Proxy attributes and validate/capture HTTP request opcodes.
// HPS means HTTP Proxy Service.
static inline int gatt_hps_add(
    ble_gatt_server *server, ble_gatt_hps_service *service,
    uint16_t uri_capacity, uint16_t headers_capacity,
    uint16_t entity_body_capacity, uint16_t control_point_capacity
) {
    if (!server || !service || !uri_capacity || !headers_capacity ||
        !entity_body_capacity || !control_point_capacity ||
        uri_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        headers_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        entity_body_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2ab6, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &unused_value, 0, uri_capacity, 1},
        {0x2ab7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &unused_value, 0, headers_capacity, 1},
        {0x2ab8, BLE_GATT_PROP_NOTIFY, 1, 0,
            &unused_value, 0, 0, 1},
        {0x2ab9, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &unused_value, 0, entity_body_capacity, 1},
        {0x2aba, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &unused_value, 0, control_point_capacity, 1},
        {0x2abb, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            &unused_value, 1}
    };
    uint16_t values[6], cccds[6];
    ble_gatt_sig_handlers handlers[6] = {0};
    handlers[4].write_request = gatt_http_proxy_control_write;
    handlers[4].context = service;

    if (!gatt_sig_add_handlers(server, 0x1823, chars, 6,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->uri_handle = values[0];
    service->headers_handle = values[1];
    service->status_code_handle = values[2];
    service->entity_body_handle = values[3];
    service->control_point_handle = values[4];
    service->https_security_handle = values[5];
    service->server = server;
    if (!gatt_set_write_observer(server, service->uri_handle,
            gatt_http_proxy_value_written, service) ||
        !gatt_set_write_observer(server, service->headers_handle,
            gatt_http_proxy_value_written, service) ||
        !gatt_set_write_observer(server, service->entity_body_handle,
            gatt_http_proxy_value_written, service)
    ) return 0;
    return 1;
}

// Install application validation for HTTP syntax, URI policy, and synchronous
// network availability checks. Return zero to accept or an ATT error code.
// HPS means HTTP Proxy Service.
static inline int gatt_hps_set_validator(
    ble_gatt_hps_service *service,
    uint8_t (*validate)(void *context, uint8_t opcode,
        const uint8_t *uri, uint16_t uri_len,
        const uint8_t *headers, uint16_t headers_len,
        const uint8_t *body, uint16_t body_len),
    void *context
) {
    if (!service) return 0;
    service->request_validate = validate;
    service->request_validate_context = context;
    return 1;
}

// HPS means HTTP Proxy Service.
static inline int gatt_hps_take_request(
    ble_gatt_hps_service *service, uint8_t *opcode
) {
    if (!service || !service->request_pending || service->request_taken ||
        !opcode) return 0;
    *opcode = service->request_opcode;
    service->request_taken = 1;
    return 1;
}

// HPS means HTTP Proxy Service.
static inline int gatt_hps_take_cancel(
    ble_gatt_hps_service *service
) {
    if (!service || !service->cancel_requested) return 0;
    service->cancel_requested = 0;
    return 1;
}

// Complete one HTTP request. HPS sends the HTTP status code followed by the
// three data-status flags defined by the service specification.
// HPS means HTTP Proxy Service.
static inline int gatt_hps_complete_with_data_status(
    ble_gatt_hps_service *service, uint16_t status_code,
    uint8_t data_status
) {
    if (!service || !service->server || !service->request_pending ||
        status_code < 100 || status_code > 599 || (data_status & 0xf0))
        return 0;
    uint8_t status[3] = {(uint8_t)status_code,
                         (uint8_t)(status_code >> 8), data_status};
    if (!gatt_queue_event(service->server, service->status_code_handle,
            status, sizeof(status), 0)) return 0;
    service->request_pending = 0;
    service->request_taken = 0;
    service->request_opcode = 0;
    service->cancel_requested = 0;
    service->uri_configured = 0;
    service->headers_configured = 0;
    service->entity_body_configured = 0;
    service->response_headers_received = 0;
    service->response_body_received = 0;
    return 1;
}

// Convenience completion marks non-empty response fields as received. Use
// complete_with_data_status() when the response was truncated or when an
// empty response field was still received.
// HPS means HTTP Proxy Service.
static inline int gatt_hps_complete(
    ble_gatt_hps_service *service, uint16_t status_code
) {
    if (!service || !service->server) return 0;
    uint8_t data_status =
        (service->response_headers_received ?
            GATT_HTTP_PROXY_DATA_STATUS_HEADERS_RECEIVED : 0) |
        (service->response_body_received ?
            GATT_HTTP_PROXY_DATA_STATUS_BODY_RECEIVED : 0);
    return gatt_hps_complete_with_data_status(
        service, status_code, data_status);
}

// HPS means HTTP Proxy Service.
static inline int gatt_hps_set_response(
    ble_gatt_hps_service *service, uint16_t handle,
    const uint8_t *value, uint16_t value_len
) {
    if (!service || !service->server || !service->request_pending ||
        (value_len && !value) ||
        (handle != service->headers_handle &&
         handle != service->entity_body_handle)) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored || value_len > attribute->value_capacity) return 0;
    if (value_len) memcpy(stored, value, value_len);
    if (value_len < attribute->value_capacity)
        memset(stored + value_len, 0, attribute->value_capacity - value_len);
    attribute->value_len = value_len;
    if (handle == service->headers_handle)
        service->response_headers_received = 1;
    else service->response_body_received = 1;
    return 1;
}

// Clear per-client request state when the owning ATT link is reset.
// HPS means HTTP Proxy Service.
static inline void gatt_hps_link_reset(
    ble_gatt_hps_service *service
) {
    if (!service || !service->server) return;
    const uint16_t handles[] = {service->uri_handle, service->headers_handle,
        service->entity_body_handle, service->https_security_handle};
    for (uint8_t i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
        ble_gatt_attribute *attribute = gatt_server_find(service->server,
            handles[i]);
        uint8_t *value = attribute ?
            gatt_attribute_value(service->server, attribute) : NULL;
        if (!attribute || !value) continue;
        memset(value, 0, attribute->value_capacity);
        attribute->value_len = i == 3 ? 1 : 0;
    }
    service->request_pending = 0;
    service->request_taken = 0;
    service->request_opcode = 0;
    service->cancel_requested = 0;
    service->uri_configured = 0;
    service->headers_configured = 0;
    service->entity_body_configured = 0;
    service->response_headers_received = 0;
    service->response_body_received = 0;
}

// Report the validity of the HTTP server certificate for the active HTTPS URI.
// Set this before completion so the client can read it before the status event.
// HPS means HTTP Proxy Service.
static inline int gatt_hps_set_cert_validity(
    ble_gatt_hps_service *service, uint8_t valid
) {
    if (!service || !service->server || !service->request_pending ||
        service->request_opcode < 0x06 || valid > 1) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->https_security_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored || attribute->value_capacity < 1) return 0;
    stored[0] = valid;
    attribute->value_len = 1;
    return 1;
}

// Object Transfer Service (OTS)
typedef struct {
    uint8_t id[6];
    uint32_t properties;
} ble_gatt_ots_object;

typedef uint8_t (*ble_gatt_ots_select_fn)(
    void *context, const uint8_t object_id[6]
);
typedef uint8_t (*ble_gatt_ots_order_fn)(
    void *context, uint8_t order, ble_gatt_ots_object *objects,
    uint16_t count
);
typedef uint8_t (*ble_gatt_ots_clear_fn)(
    void *context, ble_gatt_ots_object *objects, uint16_t count
);

enum {
    GATT_OTS_OBJECT_DELETE = 1u << 0,
    GATT_OTS_OBJECT_EXECUTE = 1u << 1,
    GATT_OTS_OBJECT_READ = 1u << 2,
    GATT_OTS_OBJECT_WRITE = 1u << 3,
    GATT_OTS_OBJECT_APPEND = 1u << 4,
    GATT_OTS_OBJECT_TRUNCATE = 1u << 5,
    GATT_OTS_OBJECT_PATCH = 1u << 6,
    GATT_OTS_OBJECT_MARKED = 1u << 7
};

typedef struct {
    uint16_t service_handle;
    uint16_t features_handle;
    uint16_t object_name_handle;
    uint16_t object_type_handle;
    uint16_t object_size_handle;
    uint16_t object_first_created_handle;
    uint16_t object_last_modified_handle;
    uint16_t object_properties_handle;
    uint16_t action_control_point_handle;
    uint16_t action_control_point_cccd_handle;
    uint16_t object_id_handle;
    uint16_t object_list_control_point_handle;
    uint16_t object_list_control_point_cccd_handle;
    uint16_t object_list_filter_handle;
    uint16_t object_list_filter_handles[3];
    ble_gatt_server *server;
    uint8_t features[8];
    uint8_t request_pending, request_taken, request_opcode;
    uint8_t active_control_point;
    uint8_t current_object_valid;
    uint16_t request_parameter_len;
    uint8_t request_parameters[BLE_GATT_ATT_VALUE_MAX - 1];
    uint8_t oacp_prepared[21], oacp_prepared_coverage[21];
    uint8_t oacp_prepare_active;
    uint8_t olcp_pending, olcp_taken, olcp_opcode, olcp_order;
    uint8_t olcp_parameter[6];
    ble_gatt_ots_object *objects;
    uint16_t object_count, current_object_index;
    uint8_t object_list_configured;
    ble_gatt_ots_select_fn select_object;
    ble_gatt_ots_order_fn order_objects;
    ble_gatt_ots_clear_fn clear_marking;
    void *object_list_context;
} ble_gatt_ots_service;

static uint8_t gatt_ots_oacp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static void gatt_ots_oacp_execute(void *context, uint8_t commit);

static inline uint8_t gatt_ots_oacp_feature_bit(uint8_t opcode) {
    switch (opcode) {
    case 0x01: return 0;
    case 0x02: return 1;
    case 0x03: return 2;
    case 0x04: return 3;
    case 0x05: return 4;
    case 0x06: return 5;
    case 0x07: return 9;
    default: return 0xff;
    }
}

static inline uint32_t gatt_ots_read_u32(const uint8_t *value) {
    return (uint32_t)value[0] | (uint32_t)value[1] << 8 |
           (uint32_t)value[2] << 16 | (uint32_t)value[3] << 24;
}

static inline uint8_t gatt_ots_object_property(
    ble_gatt_ots_service *service, uint8_t bit
) {
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->object_properties_handle);
    const uint8_t *properties = attribute ? gatt_attribute_value(
        service->server, attribute) : NULL;
    return properties && attribute->value_len == 4 &&
           (properties[bit / 8] & (1u << (bit % 8)));
}

static inline uint32_t gatt_ots_object_size(
    ble_gatt_ots_service *service, uint8_t offset
) {
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->object_size_handle);
    const uint8_t *size = attribute ? gatt_attribute_value(
        service->server, attribute) : NULL;
    return size && attribute->value_len == 8 ?
        gatt_ots_read_u32(size + offset) : 0;
}

static inline uint8_t gatt_ots_oacp_response(
    ble_gatt_ots_service *service, uint8_t opcode, uint8_t result
) {
    uint8_t response[] = {0x60, opcode, result};
    return gatt_queue_event(service->server,
        service->action_control_point_handle, response,
        sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
}

static inline uint8_t gatt_ots_olcp_response(
    ble_gatt_ots_service *service, uint8_t opcode, uint8_t result
) {
    uint8_t response[] = {0x70, opcode, result};
    return gatt_queue_event(service->server,
        service->object_list_control_point_handle, response,
        sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
}

static inline int gatt_ots_id_equal(
    const uint8_t first[6], const uint8_t second[6]
) {
    return !memcmp(first, second, 6);
}

static inline uint8_t gatt_ots_olcp_respond(
    ble_gatt_ots_service *service, uint8_t opcode, uint8_t result,
    uint32_t object_count
) {
    uint8_t response[7] = {0x70, opcode, result};
    uint16_t len = 3;
    if (result == 1 && opcode == 0x07) {
        response[3] = (uint8_t)object_count;
        response[4] = (uint8_t)(object_count >> 8);
        response[5] = (uint8_t)(object_count >> 16);
        response[6] = (uint8_t)(object_count >> 24);
        len = 7;
    }
    return gatt_queue_event(service->server,
        service->object_list_control_point_handle, response, len, 1) ?
        0 : ATT_ERR_INSUFFICIENT_RESOURCES;
}

static inline uint8_t gatt_ots_select_list_object(
    ble_gatt_ots_service *service, uint16_t index
) {
    if (index >= service->object_count)
        return gatt_ots_olcp_response(service, service->olcp_opcode, 0x07);
    if (!service->select_object ||
        !service->select_object(service->object_list_context,
            service->objects[index].id)
    ) return gatt_ots_olcp_response(service, service->olcp_opcode, 0x04);
    ble_gatt_attribute *id_attribute = gatt_server_find(service->server,
        service->object_id_handle);
    const uint8_t *selected_id = id_attribute ? gatt_attribute_value(
        service->server, id_attribute) : NULL;
    if (!selected_id || id_attribute->value_len != 6 ||
        !gatt_ots_id_equal(selected_id, service->objects[index].id))
        return gatt_ots_olcp_response(service, service->olcp_opcode, 0x04);
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_properties_handle);
    const uint8_t *properties = attribute ? gatt_attribute_value(
        service->server, attribute) : NULL;
    if (properties && attribute->value_len == 4)
        service->objects[index].properties = gatt_ots_read_u32(properties);
    service->current_object_index = index;
    service->current_object_valid = 1;
    return gatt_ots_olcp_response(service, service->olcp_opcode, 0x01);
}

static inline uint8_t gatt_ots_process_olcp(
    ble_gatt_ots_service *service, uint8_t opcode
) {
    if (opcode >= 0x01 && opcode <= 0x04) {
        if (!service->object_count)
            return gatt_ots_olcp_response(service, opcode, 0x07);
        if (opcode == 0x01)
            return gatt_ots_select_list_object(service, 0);
        if (opcode == 0x02)
            return gatt_ots_select_list_object(service,
                (uint16_t)(service->object_count - 1));
        if (service->current_object_index >= service->object_count)
            return gatt_ots_olcp_response(service, opcode, 0x07);
        if (opcode == 0x03) {
            if (!service->current_object_index)
                return gatt_ots_olcp_response(service, opcode, 0x05);
            return gatt_ots_select_list_object(service,
                (uint16_t)(service->current_object_index - 1));
        }
        if (service->current_object_index + 1 >= service->object_count)
            return gatt_ots_olcp_response(service, opcode, 0x05);
        return gatt_ots_select_list_object(service,
            (uint16_t)(service->current_object_index + 1));
    }

    if (opcode == 0x05) {
        for (uint16_t i = 0; i < service->object_count; i++)
            if (gatt_ots_id_equal(service->objects[i].id,
                    service->olcp_parameter))
                return gatt_ots_select_list_object(service, i);
        return gatt_ots_olcp_response(service, opcode, 0x08);
    }

    if (opcode == 0x06) {
        if (!service->order_objects)
            return gatt_ots_olcp_response(service, opcode, 0x04);
        uint8_t result = service->order_objects(service->object_list_context,
            service->olcp_order, service->objects, service->object_count);
        if (result < 1 || result > 8)
            result = 0x04;
        if (result != 1)
            return gatt_ots_olcp_response(service, opcode, result);
        if (service->current_object_valid) {
            ble_gatt_attribute *attribute = gatt_server_find(service->server,
                service->object_id_handle);
            const uint8_t *id = attribute ? gatt_attribute_value(
                service->server, attribute) : NULL;
            service->current_object_index = UINT16_MAX;
            for (uint16_t i = 0; id && i < service->object_count; i++)
                if (gatt_ots_id_equal(id, service->objects[i].id)) {
                    service->current_object_index = i;
                    break;
                }
        }
        return gatt_ots_olcp_response(service, opcode, 0x01);
    }

    if (opcode == 0x07)
        return gatt_ots_olcp_respond(service, opcode, 0x01,
                                     service->object_count);

    if (opcode == 0x08) {
        if (service->clear_marking) {
            uint8_t result = service->clear_marking(
                service->object_list_context, service->objects,
                service->object_count);
            if (result < 1 || result > 8)
                result = 0x04;
            if (result != 1)
                return gatt_ots_olcp_response(service, opcode, result);
        }
        for (uint16_t i = 0; i < service->object_count; i++)
            service->objects[i].properties &= ~GATT_OTS_OBJECT_MARKED;
        if (service->current_object_index < service->object_count) {
            ble_gatt_attribute *attribute = gatt_server_find(service->server,
                service->object_properties_handle);
            uint8_t *properties = attribute ? gatt_attribute_value(
                service->server, attribute) : NULL;
            if (properties && attribute->value_len == 4) {
                uint32_t value = service->objects[
                    service->current_object_index].properties;
                properties[0] = (uint8_t)value;
                properties[1] = (uint8_t)(value >> 8);
                properties[2] = (uint8_t)(value >> 16);
                properties[3] = (uint8_t)(value >> 24);
            }
        }
        return gatt_ots_olcp_response(service, opcode, 0x01);
    }

    return gatt_ots_olcp_response(service, opcode, 0x02);
}

static inline int gatt_ots_features_valid(
    const uint8_t features[8], uint8_t multi_object
) {
    if (!features || !(features[0] & 0x3f) ||
        (features[1] & 0xfc) || features[2] || features[3] ||
        (features[4] & 0xf0) || features[5] || features[6] || features[7])
        return 0;
    if (!multi_object && (features[4] || features[5] ||
                          features[6] || features[7])) return 0;
    return 1;
}

static uint8_t gatt_ots_oacp_prepare(
    void *context, uint16_t offset, const uint8_t *value, uint16_t len
) {
    ble_gatt_ots_service *service = context;
    if (!service || (len && !value))
        return ATT_ERR_UNLIKELY_ERROR;
    if (!len || offset > sizeof(service->oacp_prepared) ||
        len > sizeof(service->oacp_prepared) - offset)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (!service->oacp_prepare_active) {
        memset(service->oacp_prepared, 0, sizeof(service->oacp_prepared));
        memset(service->oacp_prepared_coverage, 0,
               sizeof(service->oacp_prepared_coverage));
        service->oacp_prepare_active = 1;
    }
    memcpy(service->oacp_prepared + offset, value, len);
    memset(service->oacp_prepared_coverage + offset, 1, len);
    return 0;
}

static uint8_t gatt_ots_oacp_prepare_commit_validate(
    void *context, const uint8_t *value, uint16_t len
) {
    ble_gatt_ots_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (!service->oacp_prepared_coverage[0])
        return ATT_ERR_INVALID_OFFSET;
    uint8_t opcode = value[0];
    uint8_t valid_length = 0;
    switch (opcode) {
    case 0x01: valid_length = len == 7 || len == 21; break;
    case 0x02:
    case 0x07: valid_length = len == 1; break;
    case 0x03:
    case 0x05: valid_length = len == 9; break;
    case 0x04: valid_length = 1; break;
    case 0x06: valid_length = len == 10; break;
    default: valid_length = 1; break;
    }
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    for (uint16_t i = 0; i < len; i++)
        if (!service->oacp_prepared_coverage[i])
            return ATT_ERR_INVALID_OFFSET;
    ble_gatt_attribute *cccd = gatt_server_find(service->server,
        service->action_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;
    return 0;
}

static inline int gatt_ots_bind_oacp_long_write(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    uint16_t handle
) {
    return gatt_set_prepare(server, handle,
                gatt_ots_oacp_prepare, gatt_ots_oacp_execute) &&
        gatt_set_validator(server, handle,
                gatt_ots_oacp_prepare_commit_validate);
}

static void gatt_ots_oacp_execute(void *context, uint8_t commit) {
    ble_gatt_ots_service *service = context;
    if (!service) return;
    if (commit) {
        uint16_t len = 0;
        while (len < sizeof(service->oacp_prepared) &&
               service->oacp_prepared_coverage[len]) len++;
        uint8_t contiguous = len > 0;
        for (uint16_t i = len; i < sizeof(service->oacp_prepared); i++)
            if (service->oacp_prepared_coverage[i]) contiguous = 0;
        if (contiguous)
            (void)gatt_ots_oacp_write(service, 0, service->oacp_prepared,
                                      len, 0);
    }
    memset(service->oacp_prepared, 0, sizeof(service->oacp_prepared));
    memset(service->oacp_prepared_coverage, 0,
           sizeof(service->oacp_prepared_coverage));
    service->oacp_prepare_active = 0;
}

static uint8_t gatt_ots_oacp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ots_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->action_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point || gatt_dts_cp_busy(service->server,
            service->action_control_point_handle) ||
        (service->object_list_control_point_handle &&
         gatt_dts_cp_busy(service->server,
            service->object_list_control_point_handle))
    ) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t valid_length = 0;
    switch (opcode) {
    case 0x01: valid_length = len == 7 || len == 21; break;
    case 0x02:
    case 0x07: valid_length = len == 1; break;
    case 0x03:
    case 0x05: valid_length = len == 9; break;
    case 0x04: valid_length = 1; break;
    case 0x06: valid_length = len == 10; break;
    default: valid_length = 1; break;
    }
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t feature_bit = gatt_ots_oacp_feature_bit(opcode);
    uint8_t response_code = 0x01;
    if (feature_bit == 0xff ||
        !(service->features[feature_bit / 8] &
          (uint8_t)(1u << (feature_bit % 8)))) response_code = 0x02;
    else if (opcode == 0x06 && (value[9] & 0xfdu))
        response_code = 0x03;
    else if (opcode == 0x06 && (value[9] & 0x02) &&
             !(service->features[0] & 0x80)) response_code = 0x08;

    if (response_code != 0x01)
        return gatt_ots_oacp_response(service, opcode, response_code);

    if (opcode != 0x01) {
        if (!service->current_object_valid)
            return gatt_ots_oacp_response(service, opcode, 0x05);

        uint8_t property_bit = opcode == 0x02 ? 0 :
            opcode == 0x03 || opcode == 0x05 ? 2 :
            opcode == 0x04 ? 1 : opcode == 0x06 ? 3 : 0xff;
        if (property_bit != 0xff &&
            !gatt_ots_object_property(service, property_bit)
        ) return gatt_ots_oacp_response(service, opcode, 0x08);
    }

    if (opcode == 0x03 || opcode == 0x05 || opcode == 0x06) {
        uint32_t offset = gatt_ots_read_u32(value + 1);
        uint32_t length = gatt_ots_read_u32(value + 5);
        uint32_t current_size = gatt_ots_object_size(service, 0);
        uint32_t allocated_size = gatt_ots_object_size(service, 4);
        if (opcode == 0x03 || opcode == 0x05) {
            if (offset > current_size ||
                (length && length > current_size - offset))
                return gatt_ots_oacp_response(service, opcode, 0x03);
        }
        else {
            uint8_t mode = value[9];
            if (offset > current_size || offset > allocated_size ||
                (length && length > allocated_size - offset) ||
                (mode & 0xfdu))
                return gatt_ots_oacp_response(service, opcode, 0x03);
            if (mode & 0x02) {
                if (!(service->features[0] & 0x80) ||
                    !gatt_ots_object_property(service, 5))
                    return gatt_ots_oacp_response(service, opcode, 0x08);
            }
            else if (offset == current_size) {
                if (!(service->features[0] & 0x40) ||
                    !gatt_ots_object_property(service, 4))
                    return gatt_ots_oacp_response(service, opcode, 0x08);
            }
            else if (!(service->features[1] & 0x01) ||
                     !gatt_ots_object_property(service, 6))
                return gatt_ots_oacp_response(service, opcode, 0x08);
        }
    }

    service->request_opcode = opcode;
    service->request_parameter_len = (uint16_t)(len - 1);
    if (len > 1)
        memcpy(service->request_parameters, value + 1, len - 1);
    service->request_pending = 1;
    service->active_control_point = 1;
    return 0;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_take_oacp_request(
    ble_gatt_ots_service *service, uint8_t *opcode,
    uint8_t *parameters, uint16_t *inout_parameter_len
) {
    if (!service || !service->request_pending || service->request_taken || !opcode ||
        !inout_parameter_len ||
        (service->request_parameter_len && !parameters) ||
        *inout_parameter_len < service->request_parameter_len) return 0;
    *opcode = service->request_opcode;
    if (service->request_parameter_len)
        memcpy(parameters, service->request_parameters,
               service->request_parameter_len);
    *inout_parameter_len = service->request_parameter_len;
    service->request_taken = 1;
    return 1;
}

// Complete an accepted OACP request with a standard response indication.
// OTS means Object Transfer Service.
static inline int gatt_ots_oacp_complete(
    ble_gatt_ots_service *service, uint8_t result_code,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->request_pending ||
        result_code < 1 || result_code > 0x0a ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 3 ||
        (result_code == 0x01 && service->request_opcode == 0x03 &&
         response_parameter_len != 4) ||
        ((result_code != 0x01 || service->request_opcode != 0x03) &&
         response_parameter_len)) return 0;
    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = 0x60;
    response[1] = service->request_opcode;
    response[2] = result_code;
    if (response_parameter_len)
        memcpy(response + 3, response_parameter, response_parameter_len);
    uint8_t opcode = service->request_opcode;
    if (!gatt_queue_event(service->server,
            service->action_control_point_handle, response,
            (uint16_t)(response_parameter_len + 3), 1)) return 0;
    service->request_pending = 0;
    service->request_taken = 0;
    service->active_control_point = 0;
    service->request_opcode = 0;
    service->request_parameter_len = 0;
    if (result_code == 0x01 && opcode == 0x01)
        service->current_object_valid = 1;
    else if (result_code == 0x01 && opcode == 0x02)
        service->current_object_valid = 0;
    return 1;
}

static uint8_t gatt_ots_olcp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ots_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 7)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->object_list_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t valid_length = opcode >= 1 && opcode <= 4 ? len == 1 :
        opcode == 5 ? len == 7 :
        opcode == 6 ? len == 2 :
        (opcode == 7 || opcode == 8) ? len == 1 : 1;
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (service->object_list_control_point_handle &&
        gatt_dts_cp_busy(service->server,
            service->object_list_control_point_handle)
    ) return 0xfe;
    uint8_t response_code = 0x01;
    if (opcode < 0x01 || opcode > 0x08)
        response_code = 0x02;
    else if (opcode >= 0x05 &&
        !(service->features[4] & (uint8_t)(1u << (opcode - 0x05))))
        response_code = 0x02;
    else if (opcode == 0x06 && !(
        (value[1] >= 0x01 && value[1] <= 0x05) ||
        (value[1] >= 0x11 && value[1] <= 0x15))) response_code = 0x03;
    if (response_code != 0x01)
        return gatt_ots_olcp_response(service, opcode, response_code);

    service->olcp_opcode = opcode;
    if (opcode == 0x05) memcpy(service->olcp_parameter, value + 1, 6);
    else if (opcode == 0x06) service->olcp_order = value[1];
    if (service->object_list_configured)
        return gatt_ots_process_olcp(service, opcode);

    service->olcp_pending = 1;
    service->active_control_point = 2;
    return 0;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_take_olcp_request(
    ble_gatt_ots_service *service, uint8_t *opcode,
    uint8_t object_id[6]
) {
    if (!service || !service->olcp_pending || service->olcp_taken || !opcode ||
        (service->olcp_opcode == 0x05 && !object_id)) return 0;
    *opcode = service->olcp_opcode;
    if (service->olcp_opcode == 0x05)
        memcpy(object_id, service->olcp_parameter, 6);
    service->olcp_taken = 1;
    return 1;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_get_olcp_order(
    const ble_gatt_ots_service *service, uint8_t *order
) {
    if (!service || !service->olcp_pending || !service->olcp_taken ||
        service->olcp_opcode != 0x06 || !order) return 0;
    *order = service->olcp_order;
    return 1;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_olcp_complete(
    ble_gatt_ots_service *service, uint8_t result_code,
    uint32_t object_count
) {
    if (!service || !service->server || !service->olcp_pending ||
        result_code < 1 || result_code > 8 ||
        (result_code != 1 && object_count) ||
        (service->olcp_opcode != 0x07 && object_count)) return 0;
    uint8_t response[7] = {0x70, service->olcp_opcode, result_code};
    uint16_t response_len = 3;
    if (result_code == 1 && service->olcp_opcode == 0x07) {
        response[3] = (uint8_t)object_count;
        response[4] = (uint8_t)(object_count >> 8);
        response[5] = (uint8_t)(object_count >> 16);
        response[6] = (uint8_t)(object_count >> 24);
        response_len = 7;
    }
    if (!gatt_queue_event(service->server,
            service->object_list_control_point_handle, response,
            response_len, 1)) return 0;
    uint8_t opcode = service->olcp_opcode;
    service->olcp_pending = 0;
    service->olcp_taken = 0;
    service->olcp_opcode = 0;
    service->active_control_point = 0;
    service->olcp_order = 0;
    memset(service->olcp_parameter, 0, sizeof(service->olcp_parameter));
    if (result_code == 1 && opcode >= 0x01 && opcode <= 0x05)
        service->current_object_valid = 1;
    return 1;
}

// Application-facing API
// Add the single-object OTS layout. Multi-object servers also need Object ID,
// OLCP, and the Object List Filter characteristics. The application handles
// OACP procedures and the separate L2CAP object-transfer channel.
// OTS means Object Transfer Service.
static inline int gatt_ots_add_type_full(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t first_created[7], const uint8_t last_modified[7],
    uint8_t has_realtime_clock, const uint8_t object_properties[4]
) {
    uint16_t object_name_capacity = BLE_GATT_SERVER_VALUE_MAX < 120 ?
        BLE_GATT_SERVER_VALUE_MAX : 120;

    if (!server || !service || !gatt_ots_features_valid(features, 0) ||
        !object_size ||
        !first_created || !last_modified || !object_properties ||
        has_realtime_clock > 1 || (object_name_len && !object_name) ||
        object_name_len > object_name_capacity || !object_type ||
        (object_type_len != 2 && object_type_len != 16)
    ) return 0;

    uint8_t action_control_point = 0;
    uint8_t first_created_properties = BLE_GATT_PROP_READ;
    uint16_t first_created_permissions = BLE_GATT_PERM_READ_ENC;
    if (features[0] & 0x01) {
        first_created_properties |= BLE_GATT_PROP_WRITE;
        first_created_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    uint8_t last_modified_properties = BLE_GATT_PROP_READ;
    uint16_t last_modified_permissions = BLE_GATT_PERM_READ_ENC;
    if (!has_realtime_clock) {
        last_modified_properties |= BLE_GATT_PROP_WRITE;
        last_modified_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    ble_gatt_sig_characteristic characteristics[8] = {
        {0x2abd, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            features, 8},
        {0x2abe, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            object_name, object_name_len, object_name_capacity, 1},
        {0x2abf, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_type, object_type_len},
        {0x2ac0, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_size, 8},
        {0x2ac1, first_created_properties, 0,
            first_created_permissions, first_created, 7},
        {0x2ac2, last_modified_properties, 0,
            last_modified_permissions, last_modified, 7},
        {0x2ac4, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_properties, 4},
        {0x2ac5, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &action_control_point, 0, 21, 1}
    };
    uint16_t value_handles[8], cccd_handles[8];
    ble_gatt_sig_handlers handlers[8] = {0};
    handlers[7].write_request = gatt_ots_oacp_write;
    handlers[7].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_register(server, 0x1825,
            primary_service, NULL, 0, characteristics, 8,
            &service->service_handle, value_handles, cccd_handles, handlers)
    ) return 0;

    service->features_handle = value_handles[0];
    service->object_name_handle = value_handles[1];
    service->object_type_handle = value_handles[2];
    service->object_size_handle = value_handles[3];
    service->object_first_created_handle = value_handles[4];
    service->object_last_modified_handle = value_handles[5];
    service->object_properties_handle = value_handles[6];
    service->action_control_point_handle = value_handles[7];
    service->action_control_point_cccd_handle = cccd_handles[7];
    service->server = server;
    service->current_object_valid = 1;
    memcpy(service->features, features, sizeof(service->features));
    return gatt_ots_bind_oacp_long_write(server, service,
        service->action_control_point_handle);
}

// OTS means Object Transfer Service.
static inline int gatt_ots_add_type(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_ots_add_type_full(server, service,
        primary_service, features, object_name, object_name_len, object_type,
        object_type_len, object_size, unknown_date, unknown_date, 1,
        object_properties);
}

// OTS means Object Transfer Service.
static inline int gatt_ots_add(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_ots_add_type(server, service, 1,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Add the secondary OTS instance required when an MCS references OTS objects.
// OTS means Object Transfer Service.
static inline int gatt_ots_add_secondary(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_ots_add_type(server, service, 0,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Application-facing API
// Add the multi-object OTS attributes to the single-object OTS layout. The
// caller owns the Object List Filter state and implements OACP/OLCP procedures
// and object transfers.
// OTS means Object Transfer Service.
static inline int gatt_ots_add_multi_type_full(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t first_created[7], const uint8_t last_modified[7],
    uint8_t has_realtime_clock, const uint8_t object_properties[4],
    const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    uint16_t object_name_capacity = BLE_GATT_SERVER_VALUE_MAX < 120 ?
        BLE_GATT_SERVER_VALUE_MAX : 120;

    if (!server || !service || !gatt_ots_features_valid(features, 1) ||
        !object_id || !object_size || !first_created || !last_modified ||
        has_realtime_clock > 1 ||
        !object_properties || !object_type ||
        (object_type_len != 2 && object_type_len != 16) ||
        (object_name_len && !object_name) ||
        object_name_len > object_name_capacity || !filter_capacity ||
        filter_len > filter_capacity || (filter_len && !object_list_filter)
    ) return 0;

    uint8_t initial_control_point = 0;
    uint8_t initial_filter = 0;
    uint8_t first_created_properties = BLE_GATT_PROP_READ;
    uint16_t first_created_permissions = BLE_GATT_PERM_READ_ENC;
    if (features[0] & 0x01) {
        first_created_properties |= BLE_GATT_PROP_WRITE;
        first_created_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    uint8_t last_modified_properties = BLE_GATT_PROP_READ;
    uint16_t last_modified_permissions = BLE_GATT_PERM_READ_ENC;
    if (!has_realtime_clock) {
        last_modified_properties |= BLE_GATT_PROP_WRITE;
        last_modified_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    ble_gatt_sig_characteristic characteristics[13] = {
        {0x2abd, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            features, 8},
        {0x2abe, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            object_name, object_name_len, object_name_capacity, 1},
        {0x2abf, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_type, object_type_len},
        {0x2ac0, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_size, 8},
        {0x2ac1, first_created_properties, 0,
            first_created_permissions, first_created, 7},
        {0x2ac2, last_modified_properties, 0,
            last_modified_permissions, last_modified, 7},
        {0x2ac3, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_id, 6},
        {0x2ac4, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_properties, 4},
        {0x2ac5, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &initial_control_point, 0, 21, 1},
        {0x2ac6, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &initial_control_point, 0, 7, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            filter_len ? object_list_filter : &initial_filter,
            filter_len, filter_capacity, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &initial_filter, 0, filter_capacity, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &initial_filter, 0, filter_capacity, 1}
    };
    uint16_t values[13], cccds[13];
    ble_gatt_sig_handlers handlers[13] = {0};
    handlers[8].write_request = gatt_ots_oacp_write;
    handlers[8].context = service;
    handlers[9].write_request = gatt_ots_olcp_write;
    handlers[9].context = service;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_register(server,
            0x1825, primary_service, NULL, 0, characteristics, 13,
            &service->service_handle, values, cccds, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->features_handle = values[0];
    service->object_name_handle = values[1];
    service->object_type_handle = values[2];
    service->object_size_handle = values[3];
    service->object_first_created_handle = values[4];
    service->object_last_modified_handle = values[5];
    service->object_id_handle = values[6];
    service->object_properties_handle = values[7];
    service->action_control_point_handle = values[8];
    service->action_control_point_cccd_handle = cccds[8];
    service->object_list_control_point_handle = values[9];
    service->object_list_control_point_cccd_handle = cccds[9];
    service->object_list_filter_handle = values[10];
    service->object_list_filter_handles[0] = values[10];
    service->object_list_filter_handles[1] = values[11];
    service->object_list_filter_handles[2] = values[12];
    service->server = server;
    service->current_object_valid = 1;
    memcpy(service->features, features, sizeof(service->features));
    if (!gatt_ots_bind_oacp_long_write(server, service,
            service->action_control_point_handle)) return 0;
    for (uint8_t i = 8; i <= 9; i++) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[i]);

        if (!cccd) {
            gatt_service_rollback(server, old_count, old_next_handle,
                old_value_used);
            memset(service, 0, sizeof(*service));
            return 0;
        }

        cccd->permissions = BLE_GATT_PERM_READ_ENC |
            BLE_GATT_PERM_WRITE_ENC;
    }
    return 1;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_add_multi_type(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_ots_add_multi_type_full(server,
        service, primary_service, features, object_name, object_name_len,
        object_type, object_type_len, object_size, unknown_date, unknown_date,
        1, object_properties, object_id, object_list_filter, filter_len,
        filter_capacity);
}

// OTS means Object Transfer Service.
static inline int gatt_ots_set_list_filter(
    ble_gatt_ots_service *service, uint8_t index,
    const uint8_t *filter, uint16_t filter_len
) {
    if (!service || !service->server || index >= 3 ||
        (filter_len && !filter)) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_list_filter_handles[index]);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored || filter_len > attribute->value_capacity)
        return 0;
    if (filter_len) memcpy(stored, filter, filter_len);
    if (filter_len < attribute->value_capacity)
        memset(stored + filter_len, 0, attribute->value_capacity - filter_len);
    attribute->value_len = filter_len;
    return 1;
}

// OTS means Object Transfer Service.
// Attach a caller-owned object table and enable built-in OLCP navigation.
// The table and callbacks must remain valid for the service lifetime. The
// selection callback should call gatt_ots_set_object_metadata() for the ID.
static inline int gatt_ots_set_objects(
    ble_gatt_ots_service *service, ble_gatt_ots_object *objects,
    uint16_t count, ble_gatt_ots_select_fn select_object,
    ble_gatt_ots_order_fn order_objects,
    ble_gatt_ots_clear_fn clear_marking, void *context
) {
    if (!service || !service->server ||
        !service->object_list_control_point_handle ||
        (count && (!objects || !select_object))) return 0;
    for (uint16_t i = 0; i < count; i++)
        for (uint16_t j = 0; j < i; j++)
            if (gatt_ots_id_equal(objects[i].id, objects[j].id)) return 0;

    service->objects = objects;
    service->object_count = count;
    service->current_object_index = UINT16_MAX;
    service->select_object = select_object;
    service->order_objects = order_objects;
    service->clear_marking = clear_marking;
    service->object_list_context = context;
    service->object_list_configured = 1;
    service->current_object_valid = 0;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_id_handle);
    const uint8_t *id = attribute ? gatt_attribute_value(
        service->server, attribute) : NULL;
    if (id && attribute->value_len == 6) {
        for (uint16_t i = 0; i < count; i++)
            if (gatt_ots_id_equal(id, objects[i].id)) {
                service->current_object_index = i;
                service->current_object_valid = 1;
                break;
            }
    }
    return 1;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_set_object_metadata(
    ble_gatt_ots_service *service,
    const uint8_t *name, uint16_t name_len,
    const uint8_t *type, uint8_t type_len,
    const uint8_t size[8], const uint8_t first_created[7],
    const uint8_t last_modified[7], const uint8_t object_id[6],
    const uint8_t properties[4]
) {
    if (!service || !service->server || (name_len && !name) || !type ||
        (type_len != 2 && type_len != 16) || !size || !first_created ||
        !last_modified || !properties ||
        (service->object_id_handle && !object_id)) return 0;
    const uint16_t handles[] = {service->object_name_handle,
        service->object_type_handle, service->object_size_handle,
        service->object_first_created_handle,
        service->object_last_modified_handle, service->object_id_handle,
        service->object_properties_handle};
    const uint8_t *values[] = {name, type, size, first_created,
        last_modified, object_id, properties};
    const uint16_t lengths[] = {name_len, type_len, 8, 7, 7,
        object_id ? 6 : 0, 4};
    ble_gatt_attribute *attributes[7];
    uint8_t *stored[7];
    for (uint8_t i = 0; i < 7; i++) {
        if (!handles[i]) continue;
        attributes[i] = gatt_server_find(service->server, handles[i]);
        stored[i] = attributes[i] ?
            gatt_attribute_value(service->server, attributes[i]) : NULL;
        if (!attributes[i] || !stored[i] ||
            lengths[i] > attributes[i]->value_capacity) return 0;
    }
    for (uint8_t i = 0; i < 7; i++) {
        if (!handles[i]) continue;
        if (lengths[i]) memcpy(stored[i], values[i], lengths[i]);
        if (lengths[i] < attributes[i]->value_capacity)
            memset(stored[i] + lengths[i], 0,
                   attributes[i]->value_capacity - lengths[i]);
        attributes[i]->value_len = lengths[i];
    }
    service->current_object_valid = 1;
    return 1;
}

// OTS means Object Transfer Service.
static inline int gatt_ots_add_multi(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_ots_add_multi_type(server, service,
        1, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

// Add a secondary multi-object OTS to be included by MCS or another profile.
// OTS means Object Transfer Service.
static inline int gatt_ots_add_multi_secondary(
    ble_gatt_server *server, ble_gatt_ots_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_ots_add_multi_type(server, service,
        0, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

// Electronic Shelf Label Service
typedef struct {
    uint16_t service_handle;
    uint16_t handles[9], cccd_handles[9];
} ble_gatt_esl_service;

// Application-facing API
// Add the ESL GATT database. The nine values are ordered as Address, AP Sync
// Key Material, Response Key Material, Current Absolute Time, Display Info,
// Image Info, Sensor Info, LED Info, and Control Point. The application
// implements ESL synchronization, PAwR, key handling, and control commands.
// ESL means Electronic Shelf Label Service.
static inline int gatt_esl_add(
    ble_gatt_server *server, ble_gatt_esl_service *service,
    const ble_gatt_service_value values_in[9]
) {
    if (!server || !service || !values_in)
        return 0;

    static const uint16_t uuids[9] = {
        0x2bf6, 0x2bf7, 0x2bf8, 0x2bf9, 0x2bfa,
        0x2bfb, 0x2bfc, 0x2bfd, 0x2bfe
    };
    ble_gatt_sig_characteristic chars[9];
    for (uint8_t i = 0; i < 9; i++) {
        const ble_gatt_service_value *value = &values_in[i];

        if (!value->data || value->len > value->capacity ||
            value->capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        uint8_t properties = (i <= 3 || i == 8) ?
            BLE_GATT_PROP_WRITE : BLE_GATT_PROP_READ;
        uint8_t cccd = 0;

        if (i == 8) {
            properties |= BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_NOTIFY;
            cccd = 1;
        }

        uint16_t permissions = 0;

        if (properties & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;

        if (properties & (BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP))
            permissions |= BLE_GATT_PERM_WRITE_ENC;
        chars[i] = (ble_gatt_sig_characteristic){
            uuids[i], properties, cccd, permissions,
            value->data, value->len, value->capacity, 1
        };
    }
    uint16_t cccds[9];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x1857, chars, 9,
            &service->service_handle, service->handles, cccds)
    ) return 0;

    memcpy(service->cccd_handles, cccds, sizeof(cccds));

    if (cccds[8]) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[8]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ_ENC |
                                      BLE_GATT_PERM_WRITE_ENC;
    }

    return 1;
}

// Cookware, Industrial Measurement Device, Tire Pressure, Environmental
// Sensing, and Binary Sensor services.
// Cookware Service
typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[12], cccd_handles[12];
    uint8_t characteristic_count;
} ble_gatt_cws_service;

// Application-facing API
// Add selected CWS characteristics from the assigned Cookware range. The
// application chooses characteristics for its supported cooking features
// and implements their value formats and recipe procedures.
// CWS means Cookware Service.
static inline int gatt_cws_add(
    ble_gatt_server *server, ble_gatt_cws_service *service,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count
) {
    if (!server || !service || !characteristics || !count || count > 12)
        return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = characteristics[i].uuid;

        if (uuid < 0x2c25 || uuid > 0x2c30)
            return 0;

        for (uint8_t j = 0; j < i; j++)
            if (characteristics[j].uuid == uuid)
                return 0;
    }
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x185d, characteristics, count,
            &service->service_handle, service->value_handles,
            service->cccd_handles)
    ) return 0;

    service->characteristic_count = count;
    return 1;
}

// Indoor Monitoring Service
typedef struct {
    uint16_t service_handle;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
    uint8_t measurement_count;
    uint16_t status_handle, status_cccd_handle;
    uint16_t descriptor_changed_handle, descriptor_changed_cccd_handle;
} ble_gatt_imd_service;

// Application-facing API
// Add IMDS with one to eight permitted live measurements and optional status
// characteristics. Measurement descriptors and history procedures remain
// application-defined.
static inline int gatt_imd_service_add(
    ble_gatt_server *server, ble_gatt_imd_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count,
    const uint8_t *status, uint16_t status_len,
    const uint8_t *descriptor_changed, uint16_t descriptor_changed_len
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        (status_len && !status) || (descriptor_changed_len &&
        !descriptor_changed)
    ) return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = measurements[i].uuid;
        uint8_t permitted = uuid == 0x2a6e ||
                            (uuid >= 0x2c06 && uuid <= 0x2c0b);

        if (!permitted ||
            !(measurements[i].properties & BLE_GATT_PROP_READ) ||
            measurements[i].permissions || measurements[i].cccd > 1
        ) return 0;

        for (uint8_t j = 0; j < i; j++)
            if (measurements[j].uuid == uuid)
                return 0;
    }
    ble_gatt_sig_characteristic chars[10];
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++) chars[n++] = measurements[i];
    uint8_t status_index = 0xff, changed_index = 0xff;

    if (status) {
        status_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0c, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1, 0,
            status, status_len, status_len, 1
        };
    }

    if (descriptor_changed) {
        changed_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0d, BLE_GATT_PROP_INDICATE, 1, 0,
            descriptor_changed, descriptor_changed_len,
            descriptor_changed_len, 1
        };
    }

    uint16_t values[10], cccds[10];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x185a, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->measurement_handles[i] = values[i];
        service->measurement_cccd_handles[i] = cccds[i];
    }

    if (status_index != 0xff) {
        service->status_handle = values[status_index];
        service->status_cccd_handle = cccds[status_index];
    }

    if (changed_index != 0xff) {
        service->descriptor_changed_handle = values[changed_index];
        service->descriptor_changed_cccd_handle = cccds[changed_index];
    }

    return 1;
}

// Tire Pressure Monitoring Service
typedef struct {
    uint16_t service_handle;
    uint16_t handles[7];
    uint16_t cccd_handles[7];
} ble_gatt_tpms_service;

// Application-facing API
// Add the mandatory TPMS Properties and Tire Pressure characteristics with
// optional temperature, acceleration, duty-cycle, position, and signing key.
// The application encodes each present value and handles TPMS signing.
// TPMS means Tire Pressure Monitoring Service.
static inline int gatt_tpms_add(
    ble_gatt_server *server, ble_gatt_tpms_service *service,
    const ble_gatt_service_value values_in[7]
) {
    if (!server || !service || !values_in || !values_in[0].data ||
        !values_in[1].data
    ) return 0;

    static const uint16_t uuids[7] = {
        0x2c3e, 0x2c3b, 0x2c3c, 0x2c3d, 0x2c3f, 0x2c40, 0x2c41
    };
    uint8_t indexes[7], n = 0;
    ble_gatt_sig_characteristic chars[7];
    for (uint8_t i = 0; i < 7; i++) {
        if (!values_in[i].data)
            continue;

        if (values_in[i].len > values_in[i].capacity ||
            values_in[i].capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        indexes[n] = i;
        uint8_t char_properties = BLE_GATT_PROP_READ;

        if (i == 1 || i == 2)
            char_properties |= BLE_GATT_PROP_NOTIFY;
        chars[n++] = (ble_gatt_sig_characteristic){
            uuids[i], char_properties, (uint8_t)(i == 1 || i == 2), 0,
            values_in[i].data, values_in[i].len, values_in[i].capacity, 1
        };
    }
    uint16_t values[7], cccds[7];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x1860, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    for (uint8_t i = 0; i < n; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];
    }
    return 1;
}

// Environmental Sensing Service
typedef struct {
    uint16_t service_handle;
    uint8_t measurement_count;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
} ble_gatt_ess_service;

// Application-facing API
// ESS means Environmental Sensing Service.
static inline int gatt_ess_add(
    ble_gatt_server *server, ble_gatt_ess_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        server->database_sealed
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x181a, measurements, count,
            &service->service_handle, service->measurement_handles,
            service->measurement_cccd_handles)
    ) return 0;

    service->measurement_count = count;
    return 1;
}

// Application-facing API
// Update a stored Environmental Sensing measurement value.
// ESS means Environmental Sensing Service.
static inline int gatt_ess_set_value(
    ble_gatt_server *server,
    const ble_gatt_ess_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || !value
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || len > attribute->value_capacity ||
        (!(attribute->flags & BLE_GATT_ATTR_VAR_LEN) &&
         len != attribute->value_len)
    ) return 0;

    memcpy(stored, value, len);
    attribute->value_len = len;
    return 1;
}

// Application-facing API
// Queue a value for one caller-selected Environmental Sensing measurement.
// ESS means Environmental Sensing Service.
static inline int gatt_ess_send(
    ble_gatt_server *server, const ble_gatt_ess_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len,
    int indication
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || (len && !value) ||
        server->mtu < 3 || len > server->mtu - 3
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t property = indication ? BLE_GATT_PROP_INDICATE :
        BLE_GATT_PROP_NOTIFY;

    if (!attribute || !(attribute->properties & property))
        return 0;

    return gatt_queue_event(server,
        service->measurement_handles[measurement_index], value, len,
        indication != 0);
}

// Binary Sensor Service
typedef struct {
    uint16_t service_handle, control_point_handle;
    uint16_t response_handle, response_cccd_handle;
    ble_gatt_server *server;
    uint8_t assembly_active, assembly_next_sequence;
    uint16_t assembly_len;
    uint8_t assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t request_pending, request_message_id, request_parameter_count;
    uint16_t request_len;
    uint8_t request[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_bss_service;

static uint8_t gatt_binary_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_bss_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len < 3 || len > BLE_GATT_SERVER_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t header = value[0];
    uint8_t sequence = (header >> 1) & 0x1f;
    uint8_t execute = header & 1;
    if ((header & 0x40) || (header & 0x80))
        return ATT_ERR_VALUE_NOT_ALLOWED;
    if (!sequence) {
        service->assembly_active = 1;
        service->assembly_next_sequence = 1;
        service->assembly_len = 0;
    }
    else if (!service->assembly_active ||
             sequence != service->assembly_next_sequence) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    uint16_t fragment_len = (uint16_t)(len - 1);
    if (fragment_len > sizeof(service->assembly) - service->assembly_len) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    memcpy(service->assembly + service->assembly_len, value + 1,
           fragment_len);
    service->assembly_len = (uint16_t)(service->assembly_len + fragment_len);
    service->assembly_next_sequence = (uint8_t)((sequence + 1) & 0x1f);
    if (!execute) return 0;

    service->assembly_active = 0;
    const uint8_t *message = service->assembly;
    uint16_t message_len = service->assembly_len;
    uint8_t valid = message_len >= 4 && !message[0] && !message[2] &&
        message[3] && (message[1] == 0x00 || message[1] == 0x02);
    uint16_t offset = 4;
    uint8_t sensor_type_seen = 0, report_status_seen = 0;

    for (uint16_t i = 0; valid && i < message[3]; i++) {
        if (message_len - offset < 4 || message[offset + 2] ||
            message[offset + 3] ||
            message[offset + 1] > message_len - offset - 4) {
            valid = 0;
            break;
        }

        uint8_t id = message[offset];
        uint8_t value_len = message[offset + 1];
        const uint8_t *parameter = message + offset + 4;

        if (id == 0x02) {
            if (sensor_type_seen++ || value_len != 1 ||
                !((parameter[0] <= 2) ||
                  (parameter[0] >= 0x80 && parameter[0] <= 0x82)))
                valid = 0;
        }
        else if (id == 0x03) {
            if (report_status_seen++ || value_len != 1 || parameter[0] > 1)
                valid = 0;
        }
        else if (id == 0x0c) {
            if (message[1] != 0x02 || !value_len || value_len > 32)
                valid = 0;
        }
        else {
            valid = 0;
        }
        offset = (uint16_t)(offset + 4 + value_len);
    }

    if (offset != message_len || !sensor_type_seen ||
        (message[1] == 0x00 &&
         (message[3] != 1 || report_status_seen)) ||
        (message[1] == 0x02 && (message[3] < 2 || !report_status_seen)))
        valid = 0;

    if (!valid) {
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    if (service->request_pending)
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    memcpy(service->request, service->assembly, service->assembly_len);
    service->request_len = service->assembly_len;
    service->request_message_id = service->assembly[1];
    service->request_parameter_count = service->assembly[3];
    service->request_pending = 1;
    service->assembly_len = 0;
    return 0;
}

// Application-facing API
// Add the Binary Sensor Control Point and Response attributes. Control Point
// writes are reassembled and validated before they are exposed as requests.
// BSS means Binary Sensor Service.
static inline int gatt_bss_add_full(
    ble_gatt_server *server, ble_gatt_bss_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2b2b, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, control_point_capacity, 1},
        {0x2b2c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write_request = gatt_binary_sensor_control_write;
    handlers[0].context = service;

    if (!gatt_sig_add_handlers(server, 0x183b, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->control_point_handle = values[0];
    service->response_handle = values[1];
    service->response_cccd_handle = cccds[1];
    service->server = server;
    return 1;
}

// Take one fully reassembled and validated Control Point message. The
// returned bytes contain the BSS Message Header and parameter records.
// BSS means Binary Sensor Service.
static inline int gatt_bss_take_request(
    ble_gatt_bss_service *service, uint8_t *message,
    uint16_t *inout_len, uint8_t *message_id, uint8_t *parameter_count
) {
    if (!service || !service->request_pending || !message || !inout_len ||
        !message_id || !parameter_count || *inout_len < service->request_len)
        return 0;
    memcpy(message, service->request, service->request_len);
    *inout_len = service->request_len;
    *message_id = service->request_message_id;
    *parameter_count = service->request_parameter_count;
    service->request_pending = 0;
    service->request_len = 0;
    return 1;
}

// Queue one complete response/event message. Long messages are segmented
// according to the negotiated ATT MTU and BSS five-bit sequence field.
// BSS means Binary Sensor Service.
static inline int gatt_bss_indicate_message(
    ble_gatt_server *server, ble_gatt_bss_service *service,
    const uint8_t *message, uint16_t message_len
) {
    if (!server || !service || !message || message_len < 2 ||
        message_len > BLE_GATT_SERVER_VALUE_MAX || server->mtu < 5)
        return 0;
    uint16_t payload_limit = (uint16_t)(server->mtu - 4);
    uint16_t fragment_count = (uint16_t)((message_len + payload_limit - 1) /
                                          payload_limit);
    uint32_t queue_bytes = (uint32_t)message_len + fragment_count;
    if (fragment_count > BLE_GATT_SERVER_EVENT_QUEUE_SIZE - server->event_count ||
        queue_bytes > BLE_GATT_SERVER_EVENT_BYTES - server->event_used)
        return 0;
    uint16_t offset = 0;
    uint8_t sequence = 0;
    do {
        uint16_t chunk = (uint16_t)(message_len - offset);
        if (chunk > payload_limit) chunk = payload_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t final = (uint8_t)(chunk == message_len - offset);
        packet[0] = (uint8_t)((sequence & 0x1f) << 1) | final | 0x80;
        memcpy(packet + 1, message + offset, chunk);
        if (!gatt_queue_event(server, service->response_handle,
                packet, (uint16_t)(chunk + 1), 1)) return 0;
        offset = (uint16_t)(offset + chunk);
        sequence = (uint8_t)((sequence + 1) & 0x1f);
    } while (offset < message_len);
    return 1;
}

#endif
