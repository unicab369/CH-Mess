#ifndef BLE_GATT_SERVICE_PERIPHERALS_H
#define BLE_GATT_SERVICE_PERIPHERALS_H

// Human Interface Device, HTTP Proxy, and Object Transfer services.
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

// Application facing
// Add the HID core characteristics and up to eight Report characteristics.
// Report type is 1 (input), 2 (output), or 3 (feature); report values and the
// HID Report Map are copied into the server database.
static inline int gatt_hid_service_add(
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

    if (!gatt_sig_service_add(server, 0x1812, characteristics,
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

// Application facing
// Add HID ISO Properties and LE HID Operation Mode. Properties are the
// caller-encoded Properties value; the application
// implements mode switching and ISO report transport.
static inline int gatt_hid_iso_service_add(
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

    if (!gatt_sig_service_add(server, 0x185c, chars, 2,
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
} ble_gatt_http_proxy_service;

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
    ble_gatt_http_proxy_service *service = context;
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
);

static uint8_t gatt_http_proxy_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_http_proxy_service *service = context;
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

// Application facing
// Add the HTTP Proxy attributes and validate/capture HTTP request opcodes.
static inline int gatt_http_proxy_service_add(
    ble_gatt_server *server, ble_gatt_http_proxy_service *service,
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
    handlers[4].write = gatt_http_proxy_control_write;
    handlers[4].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1823, chars, 6,
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
static inline int gatt_http_proxy_service_set_request_validator(
    ble_gatt_http_proxy_service *service,
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

static inline int gatt_http_proxy_service_take_request(
    ble_gatt_http_proxy_service *service, uint8_t *opcode
) {
    if (!service || !service->request_pending || service->request_taken ||
        !opcode) return 0;
    *opcode = service->request_opcode;
    service->request_taken = 1;
    return 1;
}

static inline int gatt_http_proxy_service_take_cancel(
    ble_gatt_http_proxy_service *service
) {
    if (!service || !service->cancel_requested) return 0;
    service->cancel_requested = 0;
    return 1;
}

// Complete one HTTP request. HPS sends the HTTP status code followed by the
// three data-status flags defined by the service specification.
static inline int gatt_http_proxy_service_complete_with_data_status(
    ble_gatt_http_proxy_service *service, uint16_t status_code,
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
static inline int gatt_http_proxy_service_complete(
    ble_gatt_http_proxy_service *service, uint16_t status_code
) {
    if (!service || !service->server) return 0;
    uint8_t data_status =
        (service->response_headers_received ?
            GATT_HTTP_PROXY_DATA_STATUS_HEADERS_RECEIVED : 0) |
        (service->response_body_received ?
            GATT_HTTP_PROXY_DATA_STATUS_BODY_RECEIVED : 0);
    return gatt_http_proxy_service_complete_with_data_status(
        service, status_code, data_status);
}

static inline int gatt_http_proxy_service_set_response_value(
    ble_gatt_http_proxy_service *service, uint16_t handle,
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
static inline void gatt_http_proxy_service_link_reset(
    ble_gatt_http_proxy_service *service
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
static inline int gatt_http_proxy_service_set_https_certificate_validity(
    ble_gatt_http_proxy_service *service, uint8_t valid
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
    uint16_t request_parameter_len;
    uint8_t request_parameters[BLE_GATT_ATT_VALUE_MAX - 1];
    uint8_t oacp_prepared[21], oacp_prepared_coverage[21];
    uint8_t oacp_prepare_active;
    uint8_t olcp_pending, olcp_taken, olcp_opcode, olcp_order;
    uint8_t olcp_parameter[6];
} ble_gatt_object_transfer_service;

static uint8_t gatt_ots_oacp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static uint8_t gatt_ots_olcp_write(
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
    ble_gatt_object_transfer_service *service = context;
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
    ble_gatt_object_transfer_service *service = context;
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
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint16_t handle
) {
    return gatt_set_prepare(server, handle,
                gatt_ots_oacp_prepare, gatt_ots_oacp_execute) &&
        gatt_set_validator(server, handle,
                gatt_ots_oacp_prepare_commit_validate);
}

static void gatt_ots_oacp_execute(void *context, uint8_t commit) {
    ble_gatt_object_transfer_service *service = context;
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
    ble_gatt_object_transfer_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->action_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;

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

    if (response_code != 0x01) {
        uint8_t response[] = {0x60, opcode, response_code};
        return gatt_queue_event(service->server,
            service->action_control_point_handle, response,
            sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    service->request_opcode = opcode;
    service->request_parameter_len = (uint16_t)(len - 1);
    if (len > 1)
        memcpy(service->request_parameters, value + 1, len - 1);
    service->request_pending = 1;
    service->active_control_point = 1;
    return 0;
}

static inline int gatt_object_transfer_service_take_oacp_request(
    ble_gatt_object_transfer_service *service, uint8_t *opcode,
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
static inline int gatt_object_transfer_service_oacp_complete(
    ble_gatt_object_transfer_service *service, uint8_t result_code,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->request_pending ||
        result_code < 1 || result_code > 0x0a ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 3 ||
        (result_code != 0x01 && response_parameter_len) ||
        (result_code == 0x01 &&
         service->request_opcode == 0x03 && response_parameter_len != 4) ||
        (result_code == 0x01 && service->request_opcode != 0x03 &&
         service->request_opcode != 0x04 && response_parameter_len)) return 0;
    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = 0x60;
    response[1] = service->request_opcode;
    response[2] = result_code;
    if (response_parameter_len)
        memcpy(response + 3, response_parameter, response_parameter_len);
    if (!gatt_queue_event(service->server,
            service->action_control_point_handle, response,
            (uint16_t)(response_parameter_len + 3), 1)) return 0;
    service->request_pending = 0;
    service->request_taken = 0;
    service->active_control_point = 0;
    service->request_opcode = 0;
    service->request_parameter_len = 0;
    return 1;
}

static uint8_t gatt_ots_olcp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_object_transfer_service *service = context;
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
    uint8_t response_code = 0x01;
    if (opcode < 0x01 || opcode > 0x08)
        response_code = 0x02;
    else if (opcode >= 0x05 &&
        !(service->features[4] & (uint8_t)(1u << (opcode - 0x05))))
        response_code = 0x02;
    else if (opcode == 0x06 && !(
        (value[1] >= 0x01 && value[1] <= 0x05) ||
        (value[1] >= 0x11 && value[1] <= 0x15))) response_code = 0x03;
    if (response_code != 0x01) {
        uint8_t response[] = {0x70, opcode, response_code};
        return gatt_queue_event(service->server,
            service->object_list_control_point_handle, response,
            sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    service->olcp_opcode = opcode;
    service->olcp_pending = 1;
    service->active_control_point = 2;
    if (opcode == 0x05) memcpy(service->olcp_parameter, value + 1, 6);
    else if (opcode == 0x06) service->olcp_order = value[1];
    return 0;
}

static inline int gatt_object_transfer_service_take_olcp_request(
    ble_gatt_object_transfer_service *service, uint8_t *opcode,
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

static inline int gatt_object_transfer_service_get_olcp_order(
    const ble_gatt_object_transfer_service *service, uint8_t *order
) {
    if (!service || !service->olcp_pending || !service->olcp_taken ||
        service->olcp_opcode != 0x06 || !order) return 0;
    *order = service->olcp_order;
    return 1;
}

static inline int gatt_object_transfer_service_olcp_complete(
    ble_gatt_object_transfer_service *service, uint8_t result_code,
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
    service->olcp_pending = 0;
    service->olcp_taken = 0;
    service->olcp_opcode = 0;
    service->active_control_point = 0;
    service->olcp_order = 0;
    memset(service->olcp_parameter, 0, sizeof(service->olcp_parameter));
    return 1;
}

// Application facing
// Add the single-object OTS layout. Multi-object servers also need Object ID,
// OLCP, and the Object List Filter characteristics. The application handles
// OACP procedures and the separate L2CAP object-transfer channel.
static inline int gatt_object_transfer_service_add_type_full(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
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
    handlers[7].write = gatt_ots_oacp_write;
    handlers[7].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_register_with_includes_and_handlers(server, 0x1825,
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
    memcpy(service->features, features, sizeof(service->features));
    return gatt_ots_bind_oacp_long_write(server, service,
        service->action_control_point_handle);
}

static inline int gatt_object_transfer_service_add_type(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_object_transfer_service_add_type_full(server, service,
        primary_service, features, object_name, object_name_len, object_type,
        object_type_len, object_size, unknown_date, unknown_date, 1,
        object_properties);
}

static inline int gatt_object_transfer_service_add(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_object_transfer_service_add_type(server, service, 1,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Add the secondary OTS instance required when an MCS references OTS objects.
static inline int gatt_object_transfer_service_add_secondary(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_object_transfer_service_add_type(server, service, 0,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Application facing
// Add the multi-object OTS attributes to the single-object OTS layout. The
// caller owns the Object List Filter state and implements OACP/OLCP procedures
// and object transfers.
static inline int gatt_object_transfer_service_add_multi_object_type_full(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
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
    handlers[8].write = gatt_ots_oacp_write;
    handlers[8].context = service;
    handlers[9].write = gatt_ots_olcp_write;
    handlers[9].context = service;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_register_with_includes_and_handlers(server,
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

static inline int gatt_object_transfer_service_add_multi_object_type(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_object_transfer_service_add_multi_object_type_full(server,
        service, primary_service, features, object_name, object_name_len,
        object_type, object_type_len, object_size, unknown_date, unknown_date,
        1, object_properties, object_id, object_list_filter, filter_len,
        filter_capacity);
}

static inline int gatt_object_transfer_service_set_object_list_filter(
    ble_gatt_object_transfer_service *service, uint8_t index,
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

static inline int gatt_object_transfer_service_set_current_object_metadata(
    ble_gatt_object_transfer_service *service,
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
    return 1;
}

static inline int gatt_object_transfer_service_add_multi_object(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_object_transfer_service_add_multi_object_type(server, service,
        1, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

// Add a secondary multi-object OTS to be included by MCS or another profile.
static inline int gatt_object_transfer_service_add_multi_object_secondary(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_object_transfer_service_add_multi_object_type(server, service,
        0, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

#endif
