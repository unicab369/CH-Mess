#ifndef BLE_GATT_SERVICE_HID_H
#define BLE_GATT_SERVICE_HID_H

// Human Interface Device and HTTP Proxy services.
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

#endif
