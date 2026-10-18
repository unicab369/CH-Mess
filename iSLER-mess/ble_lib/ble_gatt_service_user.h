#ifndef BLE_GATT_SERVICE_USER_H
#define BLE_GATT_SERVICE_USER_H

// Contents: User Data, Voice Assistant, Hearing Access, Telephone Bearer,
// Coordinated Set Identification, and Emergency Configuration.
// User Data Service
typedef struct {
    uint16_t service_handle, database_change_handle;
    uint16_t database_change_cccd_handle, user_index_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t registered_user_handle, registered_user_cccd_handle;
    ble_gatt_server *server;
    uint32_t database_change_increment;
    uint8_t user_index, request_opcode, request_parameter_len;
    uint8_t request_parameter[15], request_pending, request_count;
} ble_gatt_uds_service;

enum {
    GATT_UDS_OP_REGISTER_NEW_USER = 1,
    GATT_UDS_OP_CONSENT = 2,
    GATT_UDS_OP_DELETE_USER_DATA = 3,
    GATT_UDS_OP_LIST_ALL_USERS = 4,
    GATT_UDS_OP_DELETE_USERS = 5,
    GATT_UDS_OP_RESPONSE_CODE = 0x20,
    GATT_UDS_RESPONSE_SUCCESS = 1,
    GATT_UDS_RESPONSE_NOT_SUPPORTED = 2,
    GATT_UDS_RESPONSE_INVALID_PARAMETER = 3,
    GATT_UDS_RESPONSE_FAILED = 4,
    GATT_UDS_RESPONSE_USER_NOT_AUTHORIZED = 5
};

// Internal helper
static uint8_t gatt_user_data_database_change_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 4)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_uds_service *service = context;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->database_change_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!stored || attribute->value_len != 4)
        return ATT_ERR_UNLIKELY_ERROR;

    memcpy(stored, value, 4);
    service->database_change_increment = (uint32_t)value[0] |
        (uint32_t)value[1] << 8 | (uint32_t)value[2] << 16 |
        (uint32_t)value[3] << 24;
    return 0;
}

// Internal helper
static uint8_t gatt_user_data_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len || len > 16)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_uds_service *service = context;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;
    if (service->request_pending)
        return 0xfe;

    uint8_t opcode = value[0];
    uint8_t parameter_len = (uint8_t)(len - 1);

    if ((opcode == GATT_UDS_OP_REGISTER_NEW_USER && parameter_len != 2) ||
        (opcode == GATT_UDS_OP_CONSENT && parameter_len != 3) ||
        ((opcode == GATT_UDS_OP_DELETE_USER_DATA ||
          opcode == GATT_UDS_OP_LIST_ALL_USERS) && parameter_len != 0) ||
        (opcode == GATT_UDS_OP_DELETE_USERS && !parameter_len)
    ) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    service->request_opcode = opcode;
    service->request_parameter_len = parameter_len;

    if (parameter_len)
        memcpy(service->request_parameter, value + 1, parameter_len);
    service->request_pending = 1;
    service->request_count++;
    return 0;
}

// Application-facing API
// UDS means User Data Service.
static inline int gatt_uds_add(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint32_t database_change_increment, uint8_t user_index
) {
    if (!server || !service) return 0;

    memset(service, 0, sizeof(*service));
    service->server = server;
    service->database_change_increment = database_change_increment;
    service->user_index = user_index;
    uint8_t increment[4] = {
        (uint8_t)database_change_increment,
        (uint8_t)(database_change_increment >> 8),
        (uint8_t)(database_change_increment >> 16),
        (uint8_t)(database_change_increment >> 24)
    };
    static const uint8_t control_point_initial = 0;
    static const uint8_t registered_user_initial = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a99, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                    BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, increment, 4},
        {0x2a9a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         &user_index, 1},
        {.uuid = 0x2a9f,
         .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
         .cccd = 1,
         .permissions = BLE_GATT_PERM_WRITE_AUTHN,
         .value = &control_point_initial,
         .value_len = 0,
         .value_capacity = 16,
         .variable_length = 1},
        {.uuid = 0x2b37,
         .properties = BLE_GATT_PROP_INDICATE,
         .cccd = 1,
         .permissions = 0,
         .value = &registered_user_initial,
         .value_len = 0,
         .value_capacity = BLE_GATT_SERVER_VALUE_MAX,
         .variable_length = 1}
    };
    ble_gatt_sig_handlers handlers[4] = {0};
    handlers[0].write = gatt_user_data_database_change_write;
    handlers[0].context = service;
    handlers[2].write = gatt_user_data_control_write;
    handlers[2].context = service;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_service_add_with_handlers(server, 0x181c, chars, 4,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->database_change_handle = values[0];
    service->database_change_cccd_handle = cccds[0];
    service->user_index_handle = values[1];
    service->control_point_handle = values[2];
    service->control_point_cccd_handle = cccds[2];
    service->registered_user_handle = values[3];
    service->registered_user_cccd_handle = cccds[3];
    return 1;
}

// Application-facing API
// Queue one Registered User indication segment. Increment the 6-bit segment
// counter for each segment and wrap it from 63 back to 0.
// UDS means User Data Service.
static inline int gatt_uds_indicate_user_segment(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint8_t segment_number, uint8_t first_segment, uint8_t last_segment,
    uint8_t user_index, const uint8_t *name, uint16_t name_len,
    uint8_t name_present, uint8_t name_truncated
) {
    if (!server || !service || service->server != server ||
        segment_number > 63 || first_segment > 1 || last_segment > 1 ||
        name_present > 1 || name_truncated > 1 || (name_len && !name) ||
        (!first_segment && (name_present || name_truncated)) ||
        (name_truncated && !name_present) ||
        name_len > server->mtu - 4 - (first_segment ? 2 : 0) ||
        name_len > BLE_GATT_SERVER_VALUE_MAX - 3
    ) return 0;

    uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
    value[0] = (uint8_t)((segment_number << 2) |
        (first_segment ? 1 : 0) | (last_segment ? 2 : 0));
    uint16_t value_len = 1;

    if (first_segment) {
        value[value_len++] = (uint8_t)((name_present ? 1 : 0) |
            (name_truncated ? 2 : 0));
        value[value_len++] = user_index;
    }

    if (name_len)
        memcpy(value + value_len, name, name_len);
    value_len += name_len;
    return gatt_queue_event(server, service->registered_user_handle,
        value, value_len, 1);
}

// Application-facing API
// Update the Database Change Increment value and its readable database copy.
// UDS means User Data Service.
static inline int gatt_uds_set_db_change(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint32_t increment
) {
    if (!server || !service || service->server != server)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->database_change_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 4)
        return 0;

    value[0] = (uint8_t)increment;
    value[1] = (uint8_t)(increment >> 8);
    value[2] = (uint8_t)(increment >> 16);
    value[3] = (uint8_t)(increment >> 24);
    service->database_change_increment = increment;
    return 1;
}

// Application-facing API
// Update Database Change Increment and queue its notification.
// UDS means User Data Service.
static inline int gatt_uds_notify_db_change(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint32_t increment
) {
    if (!gatt_uds_set_db_change(
            server, service, increment)
    ) return 0;

    uint8_t value[4];
    value[0] = (uint8_t)increment;
    value[1] = (uint8_t)(increment >> 8);
    value[2] = (uint8_t)(increment >> 16);
    value[3] = (uint8_t)(increment >> 24);
    return gatt_queue_event(server, service->database_change_handle,
        value, sizeof(value), 0);
}

// Application-facing API
// Update the User Index value and its readable database copy.
// UDS means User Data Service.
static inline int gatt_uds_set_user_index(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint8_t user_index
) {
    if (!server || !service || service->server != server)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->user_index_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = user_index;
    service->user_index = user_index;
    return 1;
}

// Application-facing API
// Complete a User Control Point request with an indicated Response Code.
// Response values are 1 through 5; successful registration includes the
// assigned User Index as its one-byte response parameter.
// UDS means User Data Service.
static inline int gatt_uds_complete(
    ble_gatt_server *server, ble_gatt_uds_service *service,
    uint8_t response, const uint8_t *parameter, uint8_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->request_pending || response < 1 || response > 5 ||
        parameter_len > 13 || (parameter_len && !parameter) ||
        (response == 1 && service->request_opcode == 1 &&
         parameter_len != 1) ||
        (response == 1 && service->request_opcode == 4 &&
         parameter_len != 1)
    ) return 0;

    uint8_t value[16] = {
        GATT_UDS_OP_RESPONSE_CODE, service->request_opcode, response
    };

    if (parameter_len)
        memcpy(value + 3, parameter, parameter_len);
    int queued = gatt_queue_event(server, service->control_point_handle,
        value, (uint16_t)(3 + parameter_len), 1);

    if (queued > 0) {
        if (response == GATT_UDS_RESPONSE_SUCCESS &&
            service->request_opcode == GATT_UDS_OP_REGISTER_NEW_USER
        ) gatt_uds_set_user_index(server, service,
                parameter[0]);
        service->request_pending = 0;
    }

    return queued;
}

// Voice Assistant Service
typedef struct {
    uint16_t service_handle;
    uint16_t handles[7], cccd_handles[7];
    uint8_t characteristic_count;
} ble_gatt_vas_service;

// Application-facing API
// Add the common VAS/Generic VAS characteristic database. The first four
// values (name, UUID, service control point, session state) are required;
// session flag, languages, and features are optional. The application handles
// assistant sessions and service-control commands.
// VAS means Voice Assistant Service.
static inline int gatt_vas_add(
    ble_gatt_server *server, ble_gatt_vas_service *service,
    const ble_gatt_service_value values_in[7], uint8_t generic_service
) {
    if (!server || !service || !values_in || generic_service > 1)
        return 0;

    static const uint16_t uuids[7] = {
        0x2c31, 0x2c32, 0x2c33, 0x2c35, 0x2c36, 0x2c37, 0x2c38
    };
    ble_gatt_sig_characteristic chars[7];
    uint8_t indexes[7], count = 0;
    for (uint8_t i = 0; i < 7; i++) {
        const ble_gatt_service_value *value = &values_in[i];

        if (i < 4 && !value->data)
            return 0;

        if (!value->data) continue;

        if (value->len > value->capacity ||
            value->capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        uint8_t properties = BLE_GATT_PROP_READ, cccd = 0, permissions = 0;

        if (i == 2) {
            properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_NOTIFY;
            cccd = 1;
            permissions = BLE_GATT_PERM_WRITE_ENC;
        }
        else if (i == 3 || i == 4) {
            properties |= BLE_GATT_PROP_NOTIFY;
            cccd = 1;
        }

        if (properties & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;
        indexes[count] = i;
        chars[count++] = (ble_gatt_sig_characteristic){
            uuids[i], properties, cccd, permissions,
            value->data, value->len, value->capacity, 1
        };
    }
    uint16_t values[7], cccds[7];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, generic_service ? 0x185f : 0x185e,
            chars, count, &service->service_handle, values, cccds)
    ) return 0;

    service->characteristic_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];

        if (!cccds[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[i]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ_ENC |
                                      BLE_GATT_PERM_WRITE_ENC;
    }
    return 1;
}

// Hearing Access: HAS
typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t preset_control_point_handle, preset_control_point_cccd_handle;
    uint16_t active_preset_index_handle, active_preset_index_cccd_handle;
    ble_gatt_server *server;
    uint8_t features;
    uint8_t control_point_pending;
    uint16_t control_point_request_len;
    uint8_t control_point_request[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_hap_service;

enum {
    GATT_HAS_ATT_ERR_INVALID_OPCODE = 0x80,
    GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH = 0x81,
    GATT_HAS_ATT_ERR_PRESET_OPERATION_NOT_POSSIBLE = 0x82,
    GATT_HAS_ATT_ERR_PRESET_SYNCHRONIZATION_NOT_SUPPORTED = 0x83,
    GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS = 0x80,
    GATT_HAS_ATT_ERR_OUT_OF_RANGE = 0xff
};

// Internal helper
// HAP means Hearing Access Service.
static inline int gatt_hap_features_valid(uint8_t features) {
    uint8_t type = features & 0x03;
    uint8_t independent = (features & 0x08) != 0;
    uint8_t synchronized = (features & 0x04) != 0;

    if ((features & 0xc0) || type == 3)
        return 0;

    if (type != 0 && (independent || synchronized))
        return 0;

    if (independent && synchronized)
        return 0;

    if ((features & 0x20) && !(features & 0x10))
        return 0;

    return 1;
}

// Internal helper
// HAP means Hearing Access Service.
static inline uint8_t gatt_hap_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_hap_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

    uint8_t opcode = value[0];
    uint8_t required_len = 0;
    switch (opcode) {
    case 0x01: required_len = 3; break;
    case 0x04:
        if (!(service->features & 0x20))
            return GATT_HAS_ATT_ERR_INVALID_OPCODE;
        if (len < 3 || len > 42)
            return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

        break;
    case 0x05: case 0x08: required_len = 2; break;
    case 0x06: case 0x07: required_len = 1; break;
    case 0x09: case 0x0a:
        if (!(service->features & 0x04))
            return GATT_HAS_ATT_ERR_INVALID_OPCODE;

        required_len = 1;
        break;
    default:
        return GATT_HAS_ATT_ERR_INVALID_OPCODE;
    }

    if (required_len && len != required_len)
        return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

    if (len > sizeof(service->control_point_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if ((opcode == 0x01 && (!value[1] || !value[2])) ||
        ((opcode == 0x04 || opcode == 0x05 || opcode == 0x08) &&
         !value[1])
    ) return GATT_HAS_ATT_ERR_OUT_OF_RANGE;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->preset_control_point_handle);

    if ((opcode == 0x01 || opcode == 0x04 || opcode == 0x05 ||
         opcode == 0x08) && (!cccd || !(cccd->cccd & 2))
    ) return 0xfd;

    if (service->control_point_pending)
        return GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    if (gatt_dts_control_point_busy(service->server,
            service->preset_control_point_handle)
    ) return GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    memcpy(service->control_point_request, value, len);
    service->control_point_request_len = len;
    service->control_point_pending = 1;
    return 0;
}

// Application-facing API
// Add the mandatory HAS Hearing Aid Features characteristic without preset
// control support. `features` contains only the hearing-aid type bits.
// HAP means Hearing Access Service.
static inline int gatt_hap_add(
    ble_gatt_server *server, ble_gatt_hap_service *service,
    uint8_t features
) {
    if (!server || !service || features > 2)
        return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2bda, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        &features, 1
    };
    uint16_t value_handle;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1854, &characteristic, 1,
            &service->service_handle, &value_handle, NULL)
    ) return 0;

    service->features_handle = value_handle;
    return 1;
}

// Application-facing API
// Add HAS with preset control enabled. The application validates preset
// requests, updates the active index when appropriate, then completes each
// request with the encoded indication payload defined by HAS.
// HAP means Hearing Access Service.
static inline int gatt_hap_add_with_preset_control(
    ble_gatt_server *server, ble_gatt_hap_service *service,
    uint8_t features, uint8_t active_preset_index,
    uint16_t control_point_capacity
) {
    if (!server || !service || !gatt_hap_features_valid(features) ||
        server->local_mtu < 49 || control_point_capacity <
            ((features & 0x10) ? 46 : 44) ||
        control_point_capacity > BLE_GATT_ATT_VALUE_MAX
    ) return 0;

    ble_gatt_sig_characteristic characteristics[3] = {
        {0x2bda, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            &features, 1},
        {0x2bdb, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0,
            control_point_capacity, 1},
        {0x2bdc, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, &active_preset_index, 1}
    };
    uint16_t values[3], cccds[3];
    ble_gatt_sig_handlers handlers[3] = {0};
    memset(service, 0, sizeof(*service));
    handlers[1].write = gatt_hap_control_point_write;
    handlers[1].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1854,
            characteristics, 3, &service->service_handle, values, cccds,
            handlers)
    ) return 0;

    service->features_handle = values[0];
    service->preset_control_point_handle = values[1];
    service->preset_control_point_cccd_handle = cccds[1];
    service->active_preset_index_handle = values[2];
    service->active_preset_index_cccd_handle = cccds[2];
    service->server = server;
    service->features = features;
    return 1;
}

// Application-facing API
// Queue one encoded Preset Control Point response indication. Set is_last to
// zero when more response records follow, as for a multi-record read request.
// HAP means Hearing Access Service.
static inline int gatt_hap_control_point_indicate(
    ble_gatt_hap_service *service,
    const uint8_t *response, uint16_t response_len, uint8_t is_last
) {
    if (!service || !service->server || !service->control_point_pending ||
        !response || !response_len ||
        response_len > BLE_GATT_ATT_VALUE_MAX ||
        response_len > service->server->mtu - 3 || is_last > 1
    ) return 0;

    if (!gatt_queue_event(service->server,
            service->preset_control_point_handle, response, response_len, 1)
    ) return 0;

    if (is_last) {
        service->control_point_pending = 0;
        service->control_point_request_len = 0;
    }

    return 1;
}

// Application-facing API
// Queue a final response indication for the pending Preset Control Point
// request.
// HAP means Hearing Access Service.
static inline int gatt_hap_complete_control_point(
    ble_gatt_hap_service *service,
    const uint8_t *response, uint16_t response_len
) {
    return gatt_hap_control_point_indicate(service,
        response, response_len, 1);
}

// Application-facing API
// Indicate an unsolicited Preset Changed operation. Preset changes require
// Dynamic Presets support in the configured Hearing Aid Features value.
// HAP means Hearing Access Service.
static inline int gatt_hap_indicate_preset_change(
    ble_gatt_hap_service *service,
    const uint8_t *value, uint16_t value_len
) {
    if (!service || !service->server || !service->preset_control_point_handle ||
        !(service->features & 0x10) || !value || !value_len ||
        value_len > service->server->mtu - 3
    ) return 0;

    return gatt_queue_event(service->server,
        service->preset_control_point_handle, value, value_len, 1);
}

// Application-facing API
// Update the one-octet Active Preset Index value in the GATT database.
// HAP means Hearing Access Service.
static inline int gatt_hap_set_active_preset(
    ble_gatt_hap_service *service, uint8_t index
) {
    if (!service || !service->server || !service->active_preset_index_handle)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->active_preset_index_handle);
    uint8_t *value = attribute ? gatt_attribute_value(service->server,
                                                       attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = index;
    return 1;
}

// Application-facing API
// Update Active Preset Index and queue its notification.
// HAP means Hearing Access Service.
static inline int gatt_hap_notify_active_preset(
    ble_gatt_hap_service *service, uint8_t index
) {
    if (!service || !service->server ||
        !gatt_hap_set_active_preset(service, index)
    ) return 0;

    return gatt_queue_event(service->server,
        service->active_preset_index_handle, &index, 1, 0);
}

// Telephone Bearer Service (TBS, GTBS)
enum {
    GATT_TBS_PROVIDER_NAME,
    GATT_TBS_BEARER_UCI,
    GATT_TBS_BEARER_TECHNOLOGY,
    GATT_TBS_URI_SCHEMES,
    GATT_TBS_SIGNAL_STRENGTH,
    GATT_TBS_SIGNAL_STRENGTH_INTERVAL,
    GATT_TBS_CURRENT_CALLS,
    GATT_TBS_CONTENT_CONTROL_ID,
    GATT_TBS_STATUS_FLAGS,
    GATT_TBS_INCOMING_CALL_TARGET_URI,
    GATT_TBS_CALL_STATE,
    GATT_TBS_CALL_CONTROL_POINT,
    GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES,
    GATT_TBS_TERMINATION_REASON,
    GATT_TBS_INCOMING_CALL,
    GATT_TBS_CALL_FRIENDLY_NAME,
    GATT_TBS_CHARACTERISTIC_COUNT
};

#define GATT_TBS_OPTIONAL_SIGNAL_STRENGTH (1u << GATT_TBS_SIGNAL_STRENGTH)
#define GATT_TBS_OPTIONAL_SIGNAL_INTERVAL (1u << GATT_TBS_SIGNAL_STRENGTH_INTERVAL)
#define GATT_TBS_OPTIONAL_TARGET_URI (1u << GATT_TBS_INCOMING_CALL_TARGET_URI)
#define GATT_TBS_OPTIONAL_FRIENDLY_NAME (1u << GATT_TBS_CALL_FRIENDLY_NAME)
#define GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK \
    (GATT_TBS_OPTIONAL_SIGNAL_STRENGTH | GATT_TBS_OPTIONAL_SIGNAL_INTERVAL | \
     GATT_TBS_OPTIONAL_TARGET_URI | GATT_TBS_OPTIONAL_FRIENDLY_NAME)

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_tbs_value;

typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    ble_gatt_server *server;
    uint8_t pending, request_opcode;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
    uint16_t optional_characteristics;
    uint8_t signal_notification_pending, signal_notify_initialized;
    uint32_t signal_last_notify_ms;
} ble_gatt_tbs_service;

typedef struct {
    ble_gatt_tbs_value values[GATT_TBS_CHARACTERISTIC_COUNT];
    // Must fit the three-octet Control Point Result; increase for longer URIs.
    uint16_t call_control_point_capacity;
    uint8_t generic_service;
    uint16_t optional_characteristics;
} ble_gatt_tbs_config;

enum {
    GATT_TBS_RESULT_SUCCESS = 0x00,
    GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TBS_RESULT_OPERATION_NOT_POSSIBLE = 0x02,
    GATT_TBS_RESULT_INVALID_CALL_INDEX = 0x03,
    GATT_TBS_RESULT_STATE_MISMATCH = 0x04,
    GATT_TBS_RESULT_LACK_OF_RESOURCES = 0x05,
    GATT_TBS_RESULT_INVALID_OUTGOING_URI = 0x06
};

static int gatt_tbs_value_is_valid(
    uint8_t characteristic, const uint8_t *value, uint16_t len
) {
    if (len && !value) return 0;

    switch (characteristic) {
    case GATT_TBS_BEARER_TECHNOLOGY:
        return len == 1 && value[0] >= 1 && value[0] <= 9;
    case GATT_TBS_SIGNAL_STRENGTH:
        return len == 1 && (value[0] <= 100 || value[0] == 255);
    case GATT_TBS_SIGNAL_STRENGTH_INTERVAL:
        return len == 1;
    case GATT_TBS_CONTENT_CONTROL_ID:
        return len == 1;
    case GATT_TBS_STATUS_FLAGS:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_CALL_STATE:
        if (len % 3) return 0;

        for (uint16_t i = 0; i < len; i += 3) {
            if (!value[i] || value[i + 1] > 6 || (value[i + 2] & 0xf8))
                return 0;

            for (uint16_t j = 0; j < i; j += 3)
                if (value[i] == value[j]) return 0;
        }
        return 1;
    case GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_TERMINATION_REASON:
        return len == 2 && value[0] && value[1] <= 9;
    case GATT_TBS_CURRENT_CALLS:
        for (uint16_t i = 0; i < len;) {
            uint8_t item_len = value[i++];

            if (item_len < 3 || item_len > len - i || !value[i] ||
                value[i + 1] > 6 || (value[i + 2] & 0xf8)) return 0;

            for (uint16_t j = 0; j < i - 1;) {
                uint8_t previous_len = value[j++];

                if (value[j] == value[i]) return 0;

                j += previous_len;
            }
            i += item_len;
        }
        return 1;
    case GATT_TBS_INCOMING_CALL_TARGET_URI:
    case GATT_TBS_INCOMING_CALL:
    case GATT_TBS_CALL_FRIENDLY_NAME:
        return !len || (len >= 1 && value[0] != 0);
    case GATT_TBS_CALL_CONTROL_POINT:
        return len == 0;
    default:
        return 1;
    }
}

static int gatt_tbs_control_point_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication) return 1;

    return 0;
}

static int gatt_tbs_queue_result(
    ble_gatt_tbs_service *service, uint8_t opcode,
    uint8_t call_index, uint8_t result
) {
    uint8_t response[3] = {opcode, call_index, result};
    return service && service->server && service->server->mtu >= 6 &&
        gatt_queue_event(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_POINT],
            response, sizeof(response), 0);
}

static uint8_t gatt_tbs_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_tbs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (!len) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint16_t handle = service->value_handles[GATT_TBS_CALL_CONTROL_POINT];
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server, handle);

    if (!cccd || !(cccd->cccd & 1)) return 0xfd;

    if (service->pending || gatt_tbs_control_point_busy(service->server, handle))
        return 0xfe;

    uint8_t opcode = value[0];

    if (opcode > 5) {
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    if ((opcode == 2 || opcode == 3 || opcode == 5)) {
        ble_gatt_attribute *optional = gatt_server_find(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES]);
        uint8_t *bits = optional ?
            gatt_attribute_value(service->server, optional) : NULL;
        uint8_t required_bit = opcode == 5 ? 1 : 0;

        if (!bits || optional->value_len != 2 ||
            !(ble_get_le16(bits) & (uint16_t)(1u << required_bit))) {
            return gatt_tbs_queue_result(service, opcode, 0,
                GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
                0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    if ((opcode <= 3 && len != 2) || (opcode == 4 && len < 2) ||
        (opcode == 5 && len < 3))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (opcode <= 3 && value[1] == 0)
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;

    if (opcode == 5) {
        for (uint16_t i = 1; i < len; i++) {
            if (!value[i])
                return gatt_tbs_queue_result(service, opcode, 0,
                    GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                    0 : ATT_ERR_INSUFFICIENT_RESOURCES;

            for (uint16_t j = 1; j < i; j++)
                if (value[i] == value[j])
                    return gatt_tbs_queue_result(service, opcode, 0,
                        GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                        0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    service->request_opcode = opcode;
    service->parameter_len = len - 1;
    memcpy(service->parameters, value + 1, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application-facing API
// Add mandatory and selected optional TBS/GTBS characteristics. Use
// generic_service for GTBS.
// The builder validates and captures Call Control Point requests; the
// application performs telephony actions and updates call state.
// TBS means Telephone Bearer Service.
static inline int gatt_tbs_add(
    ble_gatt_server *server, ble_gatt_tbs_service *service,
    const ble_gatt_tbs_config *config
) {
    static const uint16_t uuids[GATT_TBS_CHARACTERISTIC_COUNT] = {
        0x2bb3, 0x2bb4, 0x2bb5, 0x2bb6, 0x2bb7, 0x2bb8,
        0x2bb9, 0x2bba, 0x2bbb, 0x2bbc, 0x2bbd, 0x2bbe,
        0x2bbf, 0x2bc0, 0x2bc1, 0x2bc2
    };
    static const uint8_t properties[GATT_TBS_CHARACTERISTIC_COUNT] = {
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
            BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY
    };

    if (!server || !service || !config || config->generic_service > 1 ||
        (config->optional_characteristics & ~GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK) ||
        (!!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) !=
         !!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_INTERVAL)) ||
        config->call_control_point_capacity < 3 ||
        config->call_control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        const ble_gatt_tbs_value *value = &config->values[i];

        if (value->value_len > value->value_capacity ||
            value->value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
            (value->value_len && !value->value)
        ) return 0;
    }

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++)
        if (!gatt_tbs_value_is_valid(i, config->values[i].value,
                config->values[i].value_len)) return 0;

    if (
        !config->values[GATT_TBS_BEARER_UCI].value_len ||
        !config->values[GATT_TBS_URI_SCHEMES].value_len ||
        ((config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) &&
         (config->values[GATT_TBS_SIGNAL_STRENGTH].value_len != 1 ||
          config->values[GATT_TBS_SIGNAL_STRENGTH_INTERVAL].value_len != 1))
    ) return 0;

    ble_gatt_sig_characteristic characteristics[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t indexes[GATT_TBS_CHARACTERISTIC_COUNT], count = 0;
    uint8_t empty_value = 0;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        uint16_t optional_bit = i == GATT_TBS_SIGNAL_STRENGTH ? GATT_TBS_OPTIONAL_SIGNAL_STRENGTH :
            i == GATT_TBS_SIGNAL_STRENGTH_INTERVAL ? GATT_TBS_OPTIONAL_SIGNAL_INTERVAL :
            i == GATT_TBS_INCOMING_CALL_TARGET_URI ? GATT_TBS_OPTIONAL_TARGET_URI :
            i == GATT_TBS_CALL_FRIENDLY_NAME ? GATT_TBS_OPTIONAL_FRIENDLY_NAME : 0;

        if (optional_bit && !(config->optional_characteristics & optional_bit))
            continue;

        const ble_gatt_tbs_value *value = &config->values[i];
        uint16_t permissions = 0;

        if (properties[i] & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;

        if (properties[i] & (BLE_GATT_PROP_WRITE |
                             BLE_GATT_PROP_WRITE_NO_RSP)
        ) permissions |= BLE_GATT_PERM_WRITE_ENC;
        characteristics[count] = (ble_gatt_sig_characteristic){
            .uuid = uuids[i], .properties = properties[i],
            .cccd = !!(properties[i] & BLE_GATT_PROP_NOTIFY),
            .permissions = permissions,
            .value = value->value_len ? value->value : &empty_value,
            .value_len = value->value_len,
            .value_capacity = i == GATT_TBS_CALL_CONTROL_POINT ?
                config->call_control_point_capacity : value->value_capacity,
            .variable_length = i == GATT_TBS_CALL_CONTROL_POINT
        };
        indexes[count++] = i;
    }
    memset(service, 0, sizeof(*service));
    service->server = server;
    service->optional_characteristics = config->optional_characteristics;
    ble_gatt_sig_handlers handlers[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t cp_index = 0;
    for (; cp_index < count && indexes[cp_index] != GATT_TBS_CALL_CONTROL_POINT; cp_index++);

    if (cp_index == count) return 0;

    handlers[cp_index].write =
        gatt_tbs_control_point_write;
    handlers[cp_index].context = service;

    if (!gatt_sig_service_add_with_handlers(server,
            config->generic_service ? 0x184c : 0x184b,
            characteristics, count,
            &service->service_handle, value_handles,
            cccd_handles, handlers)
    ) return 0;

    for (uint8_t n = 0; n < count; n++) {
        service->value_handles[indexes[n]] = value_handles[n];
        service->cccd_handles[indexes[n]] = cccd_handles[n];
    }
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        if (!service->cccd_handles[i])
            continue;

        ble_gatt_attribute *cccd =
            gatt_server_find(server, service->cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    return 1;
}

// Complete a captured TBS/GTBS call request and queue the required result
// notification. For non-Originate/Join success, the request's Call Index is
// reported automatically; Originate and Join require the assigned/first index.
// TBS means Telephone Bearer Service.
static inline int gatt_tbs_complete(
    ble_gatt_tbs_service *service, uint8_t result,
    uint8_t call_index
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TBS_RESULT_INVALID_OUTGOING_URI) return 0;

    uint8_t response_index = 0;

    if (result == GATT_TBS_RESULT_SUCCESS) {
        if (service->request_opcode == 4)
            response_index = call_index;
        else if (service->request_opcode == 5)
            response_index = service->parameters[0];
        else
            response_index = service->parameters[0];
        if (!response_index) return 0;
    }
    else if (call_index) {
        return 0;
    }

    if (!gatt_tbs_queue_result(service, service->request_opcode,
            response_index, result)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

// Replace one TBS/GTBS value and optionally queue its standard notification.
// TBS means Telephone Bearer Service.
static inline int gatt_tbs_set_value(
    ble_gatt_server *server, ble_gatt_tbs_service *service,
    uint8_t characteristic, const uint8_t *value, uint16_t len,
    uint8_t notify
) {
    if (!server || !service || service->server != server ||
        characteristic >= GATT_TBS_CHARACTERISTIC_COUNT ||
        characteristic == GATT_TBS_CALL_CONTROL_POINT ||
        characteristic == GATT_TBS_SIGNAL_STRENGTH ||
        characteristic == GATT_TBS_BEARER_UCI ||
        characteristic == GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES ||
        notify > 1 || (len && !value) || len > BLE_GATT_ATT_VALUE_MAX)
        return 0;

    uint16_t handle = service->value_handles[characteristic];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity ||
        (notify && !(attribute->properties & BLE_GATT_PROP_NOTIFY)))
        return 0;

    if (!gatt_tbs_value_is_valid(characteristic, value, len))
        return 0;

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;

    if (notify) return gatt_queue_event(server, handle, value, len, 0);

    return 1;
}

// Queue TBS signal strength changes at the configured minimum interval.
// TBS means Telephone Bearer Service.
static inline int gatt_tbs_set_signal_strength(
    ble_gatt_server *server, ble_gatt_tbs_service *service,
    uint8_t strength, uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !(service->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) ||
        (strength > 100 && strength != 255)) return 0;

    uint16_t handle = service->value_handles[GATT_TBS_SIGNAL_STRENGTH];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 1) return 0;
    if (attribute->value_len == 1 && stored[0] == strength) return 1;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, handle);

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 1;
    }

    stored[0] = strength;
    attribute->value_len = 1;
    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (!service->signal_notify_initialized || !delay ||
        (uint32_t)(now_ms - service->signal_last_notify_ms) >= delay) {
        if (!gatt_queue_event(server, handle, stored, 1, 0)) {
            service->signal_notification_pending = 1;
            return 0;
        }

        service->signal_notify_initialized = 1;
        service->signal_last_notify_ms = now_ms;
        service->signal_notification_pending = 0;
    }
    else service->signal_notification_pending = 1;
    return 1;
}

// TBS means Telephone Bearer Service.
static inline int gatt_tbs_poll_signal_strength(
    ble_gatt_server *server, ble_gatt_tbs_service *service,
    uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !service->signal_notification_pending) return 0;

    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    ble_gatt_attribute *strength_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    const uint8_t *strength = strength_attribute ?
        gatt_attribute_value(server, strength_attribute) : NULL;

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 0;
    }

    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (service->signal_notify_initialized && delay &&
        (uint32_t)(now_ms - service->signal_last_notify_ms) < delay) return 0;

    if (!strength || !gatt_queue_event(server,
            service->value_handles[GATT_TBS_SIGNAL_STRENGTH], strength, 1, 0))
        return 0;

    service->signal_notify_initialized = 1;
    service->signal_last_notify_ms = now_ms;
    service->signal_notification_pending = 0;
    return 1;
}

// Coordinated Set Identification Service
typedef struct {
    uint16_t service_handle, sirk_handle, sirk_cccd_handle;
    uint16_t set_size_handle, set_size_cccd_handle;
    uint16_t member_lock_handle, member_lock_cccd_handle;
    uint16_t member_rank_handle;
} ble_gatt_csis_service;

// Application-facing API
// Add CSIS with a 17-byte SIRK value and optional size, lock, and rank.
// Encrypted access is required; the application implements lock arbitration,
// timeout, persistence, SIRK encryption, and any resulting notifications.
// CSIS means Coordinated Set Identification Service.
static inline int gatt_csis_add(
    ble_gatt_server *server,
    ble_gatt_csis_service *service,
    const uint8_t sirk[17], uint8_t sirk_notify,
    uint8_t set_size, uint8_t size_notify,
    uint8_t member_lock, uint8_t member_rank
) {
    if (!server || !service || !sirk || sirk[0] > 1 ||
        sirk_notify > 1 || size_notify > 1 ||
        (set_size == 0 && size_notify) ||
        (member_lock && member_lock != 1 && member_lock != 2) ||
        (member_lock && !member_rank) || (member_rank && !member_lock) ||
        (set_size && member_rank > set_size)
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2b84, BLE_GATT_PROP_READ |
            (sirk_notify ? BLE_GATT_PROP_NOTIFY : 0), sirk_notify,
        BLE_GATT_PERM_READ_ENC, sirk, 17
    };

    if (set_size) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b85, BLE_GATT_PROP_READ |
                (size_notify ? BLE_GATT_PROP_NOTIFY : 0), size_notify,
            BLE_GATT_PERM_READ_ENC, &set_size, 1
        };
    }

    if (member_lock) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b86, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &member_lock, 1
        };
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b87, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_ENC, &member_rank, 1
        };
    }

    uint16_t values[4], cccds[4];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1846, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;
    service->sirk_handle = values[i];
    service->sirk_cccd_handle = cccds[i++];

    if (set_size) {
        service->set_size_handle = values[i];
        service->set_size_cccd_handle = cccds[i++];
    }

    if (member_lock) {
        service->member_lock_handle = values[i];
        service->member_lock_cccd_handle = cccds[i++];
        service->member_rank_handle = values[i];
    }

    return 1;
}

// Emergency Configuration Service
typedef struct {
    uint16_t service_handle, emergency_id_handle, emergency_text_handle;
} ble_gatt_ecs_service;

// Application-facing API
// Add the encrypted Emergency ID and optional encrypted Emergency Text.
// ECS means Emergency Configuration Service.
static inline int gatt_ecs_add(
    ble_gatt_server *server,
    ble_gatt_ecs_service *service,
    const uint8_t emergency_id[6], const uint8_t *emergency_text,
    uint16_t emergency_text_len, uint8_t text_writable
) {
    if (!server || !service || !emergency_id ||
        (emergency_text_len && !emergency_text) ||
        emergency_text_len > 20 || (emergency_text && !emergency_text_len)
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2b2d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
         emergency_id, 6},
        {0x2b2e,
         BLE_GATT_PROP_READ | (text_writable ? BLE_GATT_PROP_WRITE : 0),
         0,
         BLE_GATT_PERM_READ_ENC |
             (text_writable ? BLE_GATT_PERM_WRITE_ENC : 0),
         emergency_text, emergency_text_len,
         (uint16_t)(emergency_text_len ? 20 : 0), 1}
    };
    uint8_t count = emergency_text ? 2 : 1;
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x183c, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->emergency_id_handle = values[0];

    if (count == 2)
        service->emergency_text_handle = values[1];
    return 1;
}

#endif
