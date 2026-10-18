#ifndef BLE_GATT_SERVICE_DEVICE_H
#define BLE_GATT_SERVICE_DEVICE_H

// Contents: Ranging, Constant Tone Extension, Battery, Device Information,
// Generic Access, Tx Power, time, Reconnection Configuration, Bond Management,
// Scan Parameters, Transport Discovery, Internet Protocol Support, Elapsed
// Time, Authorization Control, and alert services.
// Ranging Service
typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t rt_data_handle, rt_data_cccd_handle;
    uint16_t od_data_handle, od_data_cccd_handle;
    uint16_t cp_handle, cp_cccd_handle;
    uint16_t ready_handle, ready_cccd_handle;
    uint16_t overwritten_handle, overwritten_cccd;
    ble_gatt_server *server;
    uint32_t features;
    uint16_t filters[4];
    uint16_t request_counter;
    uint16_t last_get_counter;
    uint8_t last_segment_index;
    uint8_t request[5], request_len;
    uint8_t prior_request[5], prior_request_len, prior_taken;
    uint16_t prior_counter;
    uint8_t cp_pending, cp_taken, cp_opcode, aborted_opcode;
    uint8_t get_completed;
    uint8_t data_mode_started;
} ble_gatt_ranging_service;

enum {
    GATT_RAS_GET_DATA = 0x00,
    GATT_RAS_ACK_DATA = 0x01,
    GATT_RAS_RETRIEVE_SEGMENTS = 0x02,
    GATT_RAS_ABORT = 0x03,
    GATT_RAS_SET_FILTER = 0x04,
    GATT_RAS_RSP_COMPLETE_DATA = 0x00,
    GATT_RAS_RSP_COMPLETE_SEGMENTS = 0x01,
    GATT_RAS_RSP_CODE = 0x02,
    GATT_RAS_SUCCESS = 0x01,
    GATT_RAS_OPCODE_UNSUPPORTED = 0x02,
    GATT_RAS_INVALID_PARAM = 0x03,
    GATT_RAS_SUCCESS_PERSISTED = 0x04,
    GATT_RAS_ABORT_FAILED = 0x05,
    GATT_RAS_PROCEDURE_FAILED = 0x06,
    GATT_RAS_BUSY = 0x07,
    GATT_RAS_NO_RECORD = 0x08
};

static int gatt_ras_cp_respond(
    ble_gatt_ranging_service *service, uint8_t result
) {
    uint8_t response[] = {GATT_RAS_RSP_CODE, result};
    return gatt_queue_event(service->server, service->cp_handle,
                            response, sizeof(response), 1);
}

static inline int gatt_ras_filter(
    const ble_gatt_ranging_service *service, uint8_t mode, uint8_t bit
) {
    return service && mode < 4 && bit < 14 &&
        (service->filters[mode] & (uint16_t)(1u << bit));
}

static inline int gatt_ras_data_event(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint16_t handle, uint16_t cccd_handle, uint16_t counter
) {
    if (!server || !service || service->server != server || counter > 0x0fff ||
        !handle || !cccd_handle) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;
    if (!attribute || !stored || attribute->value_capacity < 2 || !cccd)
        return 0;
    ble_put_le16(stored, counter);
    attribute->value_len = 2;
    return gatt_queue_event(server, handle, stored, 2, 1);
}

// Signal that a completed Channel Sounding procedure has stored ranging data.
static inline int gatt_ras_data_ready(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint16_t counter
) {
    if (!server || !service || service->server != server) return 0;
    ble_gatt_attribute *rt_cccd = service->rt_data_cccd_handle ?
        gatt_server_find(server, service->rt_data_cccd_handle) : NULL;
    ble_gatt_attribute *od_cccd = gatt_server_find(
        server, service->od_data_cccd_handle);
    if ((rt_cccd && rt_cccd->cccd) || !od_cccd || !od_cccd->cccd) return 0;
    return gatt_ras_data_event(server, service,
        service ? service->ready_handle : 0,
        service ? service->ready_cccd_handle : 0, counter);
}

// Signal that unread on-demand ranging data was overwritten.
static inline int gatt_ras_overwritten(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint16_t counter
) {
    if (!server || !service || service->server != server) return 0;
    ble_gatt_attribute *rt_cccd = service->rt_data_cccd_handle ?
        gatt_server_find(server, service->rt_data_cccd_handle) : NULL;
    ble_gatt_attribute *od_cccd = gatt_server_find(
        server, service->od_data_cccd_handle);
    if ((rt_cccd && rt_cccd->cccd) || !od_cccd || !od_cccd->cccd) return 0;
    return gatt_ras_data_event(server, service,
        service ? service->overwritten_handle : 0,
        service ? service->overwritten_cccd : 0, counter);
}

// Send one MTU-bounded RAS data segment. The caller supplies its segment
// index and whether it is the first or last segment in the ranging body.
static inline int gatt_ras_send_segment(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint8_t realtime, uint8_t index, uint8_t first, uint8_t last,
    const uint8_t *data, uint16_t len
) {
    if (!server || !service || service->server != server || realtime > 1 ||
        index > 63 || first > 1 || last > 1 || (len && !data) ||
        server->mtu < 5 ||
        len > server->mtu - 4) return 0;
    uint16_t handle = realtime ? service->rt_data_handle :
                                 service->od_data_handle;
    uint16_t cccd_handle = realtime ? service->rt_data_cccd_handle :
                                      service->od_data_cccd_handle;
    if (!handle || !cccd_handle) return 0;
    ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handle);
    ble_gatt_attribute *rt_cccd = service->rt_data_cccd_handle ?
        gatt_server_find(server, service->rt_data_cccd_handle) : NULL;
    ble_gatt_attribute *od_cccd = gatt_server_find(server,
        service->od_data_cccd_handle);
    if (realtime && (!rt_cccd || !rt_cccd->cccd)) return 0;
    if (realtime && od_cccd && od_cccd->cccd) return 0;
    if (!realtime && (rt_cccd && rt_cccd->cccd)) return 0;
    if (!realtime && (!service->cp_pending ||
        (service->cp_opcode != GATT_RAS_GET_DATA &&
         service->cp_opcode != GATT_RAS_RETRIEVE_SEGMENTS))) return 0;
    uint8_t indication;
    if (!cccd) return 0;
    if (cccd->cccd & 1u)
        indication = 0;
    else if (cccd->cccd & 2u)
        indication = 1;
    else
        return 0;
    uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
    packet[0] = (uint8_t)((first ? 1u : 0u) | (last ? 2u : 0u) |
                          (index << 2));
    if (len) memcpy(packet + 1, data, len);
    int queued = gatt_queue_event(server, handle, packet,
                                  (uint16_t)(len + 1), indication);
    if (queued > 0 && !realtime)
        service->last_segment_index = index;
    return queued;
}

static uint8_t gatt_ras_rt_cccd_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_ranging_service *service = context;
    if (!service || !service->server || !value || offset || len != 2)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (ble_get_le16(value)) {
        ble_gatt_attribute *od_cccd = gatt_server_find(service->server,
            service->od_data_cccd_handle);
        if (od_cccd && od_cccd->cccd) return 0xfd;
        service->data_mode_started = 1;
    }
    return 0;
}

static uint8_t gatt_ras_od_cccd_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_ranging_service *service = context;
    if (!service || !service->server || !value || offset || len != 2)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (ble_get_le16(value) && service->rt_data_cccd_handle) {
        ble_gatt_attribute *rt_cccd = gatt_server_find(service->server,
            service->rt_data_cccd_handle);
        if (rt_cccd && rt_cccd->cccd) return 0xfd;
    }
    if (ble_get_le16(value)) service->data_mode_started = 1;
    return 0;
}

static uint8_t gatt_ras_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ranging_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *cp_cccd = gatt_server_find(
        service->server, service->cp_cccd_handle);
    if (!cp_cccd || !(cp_cccd->cccd & 2u))
        return 0xfd; // CCCD improperly configured.

    uint8_t opcode = value[0];
    uint8_t expected = opcode == GATT_RAS_GET_DATA ||
        opcode == GATT_RAS_ACK_DATA ? 3 :
        opcode == GATT_RAS_RETRIEVE_SEGMENTS ? 5 :
        opcode == GATT_RAS_ABORT ? 1 :
        opcode == GATT_RAS_SET_FILTER ? 3 : 0;
    if (!expected || len != expected)
        return gatt_ras_cp_respond(service,
            expected ? GATT_RAS_INVALID_PARAM :
                       GATT_RAS_OPCODE_UNSUPPORTED) > 0 ? 0 :
                       ATT_ERR_UNLIKELY_ERROR;
    if ((opcode == GATT_RAS_GET_DATA || opcode == GATT_RAS_ACK_DATA ||
         opcode == GATT_RAS_RETRIEVE_SEGMENTS) &&
        (ble_get_le16(value + 1) & 0xf000u))
        return gatt_ras_cp_respond(service,
            GATT_RAS_INVALID_PARAM) > 0 ? 0 : ATT_ERR_UNLIKELY_ERROR;

    uint32_t feature = opcode == GATT_RAS_RETRIEVE_SEGMENTS ? 2u :
        opcode == GATT_RAS_ABORT ? 4u :
        opcode == GATT_RAS_SET_FILTER ? 8u : 0u;
    if (feature && !(service->features & feature))
        return gatt_ras_cp_respond(service,
            GATT_RAS_OPCODE_UNSUPPORTED) > 0 ? 0 : ATT_ERR_UNLIKELY_ERROR;

    if (service->cp_pending && opcode != GATT_RAS_ABORT)
        return gatt_ras_cp_respond(service, GATT_RAS_BUSY) > 0 ?
            0 : ATT_ERR_UNLIKELY_ERROR;

    if (opcode == GATT_RAS_SET_FILTER) {
        ble_gatt_attribute *rt_cccd = service->rt_data_cccd_handle ?
            gatt_server_find(service->server,
                             service->rt_data_cccd_handle) : NULL;
        ble_gatt_attribute *od_cccd = gatt_server_find(
            service->server, service->od_data_cccd_handle);
        uint16_t config = ble_get_le16(value + 1);
        uint8_t mode = (uint8_t)(config & 3u);
        if (service->data_mode_started ||
            (rt_cccd && rt_cccd->cccd) || (od_cccd && od_cccd->cccd))
            return gatt_ras_cp_respond(service,
                GATT_RAS_INVALID_PARAM) > 0 ? 0 : ATT_ERR_UNLIKELY_ERROR;
        service->filters[mode] = (uint16_t)(config >> 2);
        return gatt_ras_cp_respond(service, GATT_RAS_SUCCESS) > 0 ?
            0 : ATT_ERR_UNLIKELY_ERROR;
    }

    if (opcode == GATT_RAS_ABORT && !service->cp_pending)
        return gatt_ras_cp_respond(service, GATT_RAS_ABORT_FAILED) > 0 ?
            0 : ATT_ERR_UNLIKELY_ERROR;

    if (opcode != GATT_RAS_ABORT) {
        ble_gatt_attribute *rt_cccd = service->rt_data_cccd_handle ?
            gatt_server_find(service->server,
                             service->rt_data_cccd_handle) : NULL;
        if ((rt_cccd && rt_cccd->cccd) ||
            (opcode != GATT_RAS_ACK_DATA && opcode != GATT_RAS_GET_DATA &&
             opcode != GATT_RAS_RETRIEVE_SEGMENTS))
            return gatt_ras_cp_respond(service,
                GATT_RAS_INVALID_PARAM) > 0 ? 0 : ATT_ERR_UNLIKELY_ERROR;
        if (opcode == GATT_RAS_GET_DATA || opcode == GATT_RAS_ACK_DATA ||
            opcode == GATT_RAS_RETRIEVE_SEGMENTS) {
            ble_gatt_attribute *od_cccd = gatt_server_find(
                service->server, service->od_data_cccd_handle);
            if (!od_cccd || !od_cccd->cccd)
                return gatt_ras_cp_respond(service,
                    GATT_RAS_INVALID_PARAM) > 0 ? 0 :
                           ATT_ERR_UNLIKELY_ERROR;
        }
    }

    if (opcode == GATT_RAS_RETRIEVE_SEGMENTS &&
        ((value[4] != 0xff && value[4] < value[3]) ||
         value[3] > 63 || (value[4] != 0xff && value[4] > 63) ||
         !service->get_completed ||
         ble_get_le16(value + 1) != service->last_get_counter))
        return gatt_ras_cp_respond(service,
            GATT_RAS_INVALID_PARAM) > 0 ? 0 : ATT_ERR_UNLIKELY_ERROR;

    if (service->cp_pending) {
        service->aborted_opcode = service->cp_opcode;
        memcpy(service->prior_request, service->request,
               service->request_len);
        service->prior_request_len = service->request_len;
        service->prior_counter = service->request_counter;
        service->prior_taken = service->cp_taken;
    }
    else {
        service->aborted_opcode = 0;
        service->prior_request_len = 0;
    }
    memcpy(service->request, value, len);
    service->request_len = (uint8_t)len;
    service->request_counter = len >= 3 ? ble_get_le16(value + 1) : 0;
    service->cp_opcode = opcode;
    service->cp_pending = 1;
    service->cp_taken = 0;
    return 0;
}

// Return a validated RAS command once; the application supplies the ranging
// records and performs record storage operations.
static inline int gatt_ras_take_request(
    ble_gatt_ranging_service *service, uint8_t *opcode,
    uint8_t *parameters, uint8_t *inout_len
) {
    if (!service || !service->cp_pending || service->cp_taken || !opcode ||
        !inout_len || (*inout_len && !parameters) ||
        *inout_len < service->request_len - 1) return 0;
    *opcode = service->cp_opcode;
    *inout_len = (uint8_t)(service->request_len - 1);
    if (*inout_len)
        memcpy(parameters, service->request + 1, *inout_len);
    service->cp_taken = 1;
    return 1;
}

// Clear per-connection procedures and restore the default all-enabled filters.
static inline void gatt_ras_reset(ble_gatt_ranging_service *service) {
    if (!service) return;
    service->cp_pending = 0;
    service->cp_taken = 0;
    service->cp_opcode = 0;
    service->aborted_opcode = 0;
    service->request_len = 0;
    service->prior_request_len = 0;
    service->get_completed = 0;
    service->data_mode_started = 0;
    service->last_get_counter = 0;
    service->last_segment_index = 0;
    for (uint8_t mode = 0; mode < 4; mode++)
        service->filters[mode] = 0x3fff;
}

// Complete a captured RAS command with a SIG response code. Successful Get
// and segment-retry requests use their dedicated response formats.
static inline int gatt_ras_complete(
    ble_gatt_ranging_service *service, uint8_t result
) {
    if (!service || !service->server || !service->cp_pending ||
        result < GATT_RAS_SUCCESS || result > GATT_RAS_NO_RECORD)
        return 0;
    uint8_t response[5];
    uint16_t len = 2;
    if (result == GATT_RAS_SUCCESS &&
        service->cp_opcode == GATT_RAS_GET_DATA) {
        response[0] = GATT_RAS_RSP_COMPLETE_DATA;
        ble_put_le16(response + 1, service->request_counter);
        len = 3;
    }
    else if (result == GATT_RAS_SUCCESS &&
             service->cp_opcode == GATT_RAS_RETRIEVE_SEGMENTS) {
        response[0] = GATT_RAS_RSP_COMPLETE_SEGMENTS;
        ble_put_le16(response + 1, service->request_counter);
        response[3] = service->request[3];
        response[4] = service->request[4] == 0xff ?
            service->last_segment_index : service->request[4];
        len = 5;
    }
    else {
        response[0] = GATT_RAS_RSP_CODE;
        response[1] = result;
    }
    int queued = gatt_queue_event(service->server, service->cp_handle,
                                  response, len, 1);
    if (queued > 0) {
        if (service->cp_opcode == GATT_RAS_ABORT &&
            result == GATT_RAS_ABORT_FAILED) {
            service->cp_opcode = service->aborted_opcode;
            memcpy(service->request, service->prior_request,
                   service->prior_request_len);
            service->request_len = service->prior_request_len;
            service->request_counter = service->prior_counter;
            service->cp_taken = service->prior_taken;
            service->aborted_opcode = 0;
            service->prior_request_len = 0;
            return queued;
        }
        if (result == GATT_RAS_SUCCESS &&
            service->cp_opcode == GATT_RAS_GET_DATA) {
            service->last_get_counter = service->request_counter;
            service->get_completed = 1;
        }
        if (result == GATT_RAS_SUCCESS &&
            service->cp_opcode == GATT_RAS_ACK_DATA)
            service->get_completed = 0;
        if (result == GATT_RAS_SUCCESS &&
            service->cp_opcode == GATT_RAS_ABORT)
            service->get_completed = 0;
        service->cp_pending = 0;
        service->cp_taken = 0;
        service->cp_opcode = 0;
        service->aborted_opcode = 0;
        service->request_len = 0;
        service->prior_request_len = 0;
    }
    return queued;
}

// Application-facing API
// RAS means Ranging Service.
// Add mandatory RAS characteristics and optional real-time data. Control
// Point commands are validated and captured for application record handling.
static inline int gatt_ras_add(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint32_t features, uint16_t ranging_data_capacity,
    uint16_t cp_capacity
) {
    if (!server || !service || (features & ~0x0fu) ||
        !ranging_data_capacity ||
        ranging_data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !cp_capacity ||
        cp_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t feature_value[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t counter[2] = {0};
    uint8_t control_point_initial = 0;
    uint8_t data_initial = 0;
    uint8_t include_realtime = (features & 1u) != 0;
    uint8_t count = include_realtime ? 6 : 5;
    ble_gatt_sig_characteristic characteristics[6] = {0};
    uint8_t i = 0;
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        feature_value, sizeof(feature_value)
    };

    if (include_realtime)
        characteristics[i++] = (ble_gatt_sig_characteristic){
            0x2c15, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
            ranging_data_capacity, 1
        };

    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c16, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
        ranging_data_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c17, BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE_ENC, &control_point_initial, 0,
        cp_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c18, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c19, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };

    uint16_t values[6], cccds[6];
    uint16_t service_handle;
    ble_gatt_sig_handlers handlers[6] = {0};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));
    service->server = server;
    service->features = features;
    for (uint8_t mode = 0; mode < 4; mode++)
        service->filters[mode] = 0x3fff;
    handlers[include_realtime ? 3 : 2].write_request = gatt_ras_cp_write;
    handlers[include_realtime ? 3 : 2].context = service;

    if (!gatt_sig_add_handlers(server, 0x185b, characteristics, count,
            &service_handle, values, cccds, handlers)
    ) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->service_handle = service_handle;
    i = 0;
    service->features_handle = values[i++];

    if (include_realtime) {
        service->rt_data_handle = values[i];
        service->rt_data_cccd_handle = cccds[i++];
    }

    service->od_data_handle = values[i];
    service->od_data_cccd_handle = cccds[i++];
    service->cp_handle = values[i];
    service->cp_cccd_handle = cccds[i++];
    service->ready_handle = values[i];
    service->ready_cccd_handle = cccds[i++];
    service->overwritten_handle = values[i];
    service->overwritten_cccd = cccds[i];
    if (service->rt_data_cccd_handle) {
        ble_gatt_attribute *cccd = gatt_server_find(server,
            service->rt_data_cccd_handle);
        if (!cccd) {
            gatt_service_rollback(server, old_count, old_next_handle,
                                  old_value_used);
            memset(service, 0, sizeof(*service));
            return 0;
        }
        cccd->write_request = gatt_ras_rt_cccd_write;
        cccd->context = service;
    }
    ble_gatt_attribute *cccd = gatt_server_find(server,
        service->od_data_cccd_handle);
    if (!cccd) {
        gatt_service_rollback(server, old_count, old_next_handle,
                              old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }
    cccd->write_request = gatt_ras_od_cccd_write;
    cccd->context = service;
    return 1;
}

// Constant Tone Extension Service
typedef struct {
    uint16_t service_handle, enable_handle;
    uint16_t min_len_handle, min_tx_count_handle;
    uint16_t tx_duration_handle, interval_handle, phy_handle;
} ble_gatt_ctes_service;

// Application-facing API
// Add CTES, including advertising-channel controls when supported. The
// application applies accepted writes to the Link Layer/advertising setup.
// CTES means Constant Tone Extension Service.
static inline int gatt_ctes_add(
    ble_gatt_server *server, ble_gatt_ctes_service *service,
    uint8_t enable, uint8_t advertising_cte,
    uint8_t minimum_length, uint8_t minimum_transmit_count,
    uint8_t transmit_duration, uint16_t interval, uint8_t phy
) {
    if (!server || !service || (enable & 0xfc) || advertising_cte > 1 ||
        (advertising_cte && (minimum_length < 2 || minimum_length > 20 ||
            !minimum_transmit_count || minimum_transmit_count > 15 ||
            interval < 6 || phy < 1 || phy > 3))
    ) return 0;

    uint8_t interval_value[2] = {
        (uint8_t)interval, (uint8_t)(interval >> 8)
    };
    ble_gatt_sig_characteristic characteristics[6] = {
        {0x2bad, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &enable, 1},
        {0x2bae, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_length, 1},
        {0x2baf, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_transmit_count, 1},
        {0x2bb0, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &transmit_duration, 1},
        {0x2bb1, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            interval_value, 2},
        {0x2bb2, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &phy, 1}
    };
    uint8_t count = advertising_cte ? 6 : 1;
    uint16_t values[6];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x184a, characteristics, count,
            &service->service_handle, values, NULL)
    ) return 0;

    service->enable_handle = values[0];

    if (advertising_cte) {
        service->min_len_handle = values[1];
        service->min_tx_count_handle = values[2];
        service->tx_duration_handle = values[3];
        service->interval_handle = values[4];
        service->phy_handle = values[5];
    }

    return 1;
}

// Battery Service
typedef struct {
    uint16_t service_handle, level_handle, level_cccd_handle;
    uint8_t level;
} ble_gatt_bas_service;

static uint8_t gatt_bas_level_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1)
        return ATT_ERR_INVALID_OFFSET;

    if (!offset && *inout_len) {
        out[0] = ((ble_gatt_bas_service *)context)->level;
        *inout_len = 1;
    }
    else {
        *inout_len = 0;
    }
    return 0;
}

// Application-facing API
// Add the standard Battery Service with a readable, notifiable Battery Level.
// Keep one service state per GATT server/connection and keep it alive while
// that server may read the characteristic.
// BAS means Battery Service.
static inline int gatt_bas_add(
    ble_gatt_server *server,
    ble_gatt_bas_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid level_uuid = {2, {0x19, 0x2a}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    battery->level = level;

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &battery->service_handle) &&
        gatt_add_characteristic(server, &level_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, BLE_GATT_PERM_READ,
            &battery->level, 1, 1, gatt_bas_level_read, NULL, battery,
            NULL, &battery->level_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &battery->level_cccd_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(battery, 0, sizeof(*battery));
        return 0;
    }

    return 1;
}

// Application-facing API
// BAS means Battery Service.
static inline int gatt_bas_set_level(
    ble_gatt_bas_service *battery, uint8_t level
) {
    if (!battery || !battery->level_handle || level > 100)
        return 0;

    battery->level = level;
    return 1;
}

// Application-facing API
// Update Battery Level and queue its notification.
// BAS means Battery Service.
static inline int gatt_bas_notify_level(
    ble_gatt_server *server, ble_gatt_bas_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100 ||
        !gatt_bas_set_level(battery, level)
    ) return 0;

    return gatt_queue_event(server, battery->level_handle,
                                   &battery->level, 1, 0);
}

// Device Information and Generic Access services
typedef struct {
    uint16_t service_handle;
    uint16_t mfr_name_handle;
    uint16_t model_handle;
    uint16_t serial_handle;
    uint16_t hw_rev_handle;
    uint16_t fw_rev_handle;
    uint16_t sw_rev_handle;
    uint16_t sys_id_handle;
    uint16_t pnp_id_handle;
} ble_gatt_dis_service;

typedef struct {
    const char *mfr_name;
    const char *model;
    const char *serial;
    const char *hw_rev;
    const char *fw_rev;
    const char *sw_rev;
    const uint8_t *sys_id;
    const uint8_t *pnp_id;
} ble_gatt_dis_info;

static int gatt_dis_add_str(
    ble_gatt_server *server, uint16_t uuid16, const char *value,
    uint16_t *handle
) {
    if (!value) {
        *handle = 0;
        return 1;
    }

    uint16_t len = 0;
    while (len < BLE_GATT_SERVER_VALUE_MAX && value[len]) len++;

    if (!len) {
        *handle = 0;
        return 1;
    }

    ble_gatt_uuid uuid = {2, {(uint8_t)uuid16, (uint8_t)(uuid16 >> 8)}};
    return gatt_add_characteristic(server, &uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
        (const uint8_t *)value, len, len, NULL, NULL, NULL, NULL, handle);
}

// Application-facing API
// Add the standard Device Information Service. Optional strings and IDs are
// copied into the server database; omitted fields have a zero handle. IDs use
// the Bluetooth-defined 8-byte System ID and 7-byte PnP ID formats.
// DIS means Device Information Service.
static inline int gatt_dis_add(
    ble_gatt_server *server,
    ble_gatt_dis_service *service,
    const ble_gatt_dis_info *information
) {
    if (!server || !service || !information || server->database_sealed)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0a, 0x18}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_dis_add_str(server, 0x2a29,
            information->mfr_name,
            &service->mfr_name_handle) &&
        gatt_dis_add_str(server, 0x2a24,
            information->model, &service->model_handle) &&
        gatt_dis_add_str(server, 0x2a25,
            information->serial, &service->serial_handle) &&
        gatt_dis_add_str(server, 0x2a27,
            information->hw_rev,
            &service->hw_rev_handle) &&
        gatt_dis_add_str(server, 0x2a26,
            information->fw_rev,
            &service->fw_rev_handle) &&
        gatt_dis_add_str(server, 0x2a28,
            information->sw_rev,
            &service->sw_rev_handle);

    if (added && information->sys_id) {
        ble_gatt_uuid uuid = {2, {0x23, 0x2a}};
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->sys_id, 8, 8, NULL, NULL, NULL, NULL,
            &service->sys_id_handle);
    }

    if (added && information->pnp_id) {
        ble_gatt_uuid uuid = {2, {0x50, 0x2a}};
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->pnp_id, 7, 7, NULL, NULL, NULL, NULL,
            &service->pnp_id_handle);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t device_name_handle;
    uint16_t appearance_handle;
} ble_gatt_gap_service;

// Application-facing API
// Add the Generic Access Service with read-only Device Name and Appearance.
// The name is UTF-8 and follows GAP's 248-octet maximum; values are copied to
// the server database during registration.
// GAP means Generic Access Profile.
static inline int gatt_gap_add(
    ble_gatt_server *server, ble_gatt_gap_service *service,
    const char *device_name, uint16_t appearance
) {
    if (!server || !service || !device_name || server->database_sealed)
        return 0;

    uint16_t name_len = 0;
    while (name_len < BLE_GATT_SERVER_VALUE_MAX && device_name[name_len])
        name_len++;

    if (!name_len || name_len > 248)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x00, 0x18}};
    ble_gatt_uuid name_uuid = {2, {0x00, 0x2a}};
    ble_gatt_uuid appearance_uuid = {2, {0x01, 0x2a}};
    uint8_t appearance_value[2] = {
        (uint8_t)appearance, (uint8_t)(appearance >> 8)
    };
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_add_characteristic(server, &name_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            (const uint8_t *)device_name, name_len, name_len,
            NULL, NULL, NULL, NULL, &service->device_name_handle) &&
        gatt_add_characteristic(server, &appearance_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            appearance_value, sizeof(appearance_value),
            sizeof(appearance_value), NULL, NULL, NULL, NULL,
            &service->appearance_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

// Tx Power, Current Time, Reference Time Update, and Next DST Change
typedef struct {
    uint16_t service_handle, level_handle;
} ble_gatt_tx_power_service;

typedef struct {
    uint16_t service_handle, time_handle, time_cccd_handle;
    uint16_t local_info_handle, ref_info_handle;
} ble_gatt_cts_service;

typedef struct {
    uint16_t service_handle, cp_handle, state_handle;
    ble_gatt_server *server;
    uint8_t last_command, request_count;
} ble_gatt_rtus_service;

typedef struct {
    uint16_t service_handle, time_dst_handle;
} ble_gatt_ndcs_service;

// Application-facing API
// TX means Tx Power Service.
static inline int gatt_tx_add(
    ble_gatt_server *server, ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t value = (uint8_t)tx_power_dbm;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a07, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &value, 1}
    };
    uint16_t values[1];

    if (!gatt_sig_add(server, 0x1804, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->level_handle = values[0];
    return 1;
}

// Application-facing API
// Update the signed Tx Power Level stored in the server database.
static inline int gatt_tx_set_level(
    ble_gatt_server *server, const ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service || !service->level_handle)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->level_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = (uint8_t)tx_power_dbm;
    return 1;
}

// Application-facing API
// CTS means Current Time Service.
static inline int gatt_cts_add(
    ble_gatt_server *server, ble_gatt_cts_service *service,
    const uint8_t time[10], const uint8_t local_info[2],
    const uint8_t ref_info[4]
) {
    if (!service || !time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    uint8_t local_time_index = 0xff, reference_time_index = 0xff;
    chars[count++] = (ble_gatt_sig_characteristic)
        {0x2a2b, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, time, 10};

    if (local_info) {
        local_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a0f, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
             BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, local_info, 2};
    }

    if (ref_info) {
        reference_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
             ref_info, 4};
    }

    uint16_t values[3], cccds[3];

    if (!gatt_sig_add(server, 0x1805, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->time_handle = values[0];
    service->time_cccd_handle = cccds[0];

    if (local_time_index != 0xff)
        service->local_info_handle = values[local_time_index];
    if (reference_time_index != 0xff)
        service->ref_info_handle = values[reference_time_index];
    return 1;
}

// Application-facing API
// Update the readable Current Time value and queue its notification.
// CTS means Current Time Service.
static inline int gatt_cts_notify(
    ble_gatt_server *server, const ble_gatt_cts_service *service,
    const uint8_t time[10]
) {
    if (!server || !service || !time)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->time_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_len != 10 ||
        attribute->value_capacity < 10
    ) return 0;

    memcpy(stored, time, 10);
    return gatt_queue_event(server, service->time_handle,
                                   time, 10, 0);
}

// Internal helper
static uint8_t gatt_rtus_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_rtus_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] != 0x01 && value[0] != 0x02)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(service->server, service->state_handle);
    uint8_t *state = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!state || attribute->value_len != 2 ||
        attribute->value_capacity < 2
    ) return ATT_ERR_UNLIKELY_ERROR;

    if (value[0] == 0x01) {
        state[0] = 1;
    }
    else if (state[0] == 1) {
        state[0] = 0;
        state[1] = 1;
    }

    service->last_command = value[0];
    service->request_count++;
    return 0;
}

// Application-facing API
// RTUS means Reference Time Update Service.
static inline int gatt_rtus_add(
    ble_gatt_server *server, ble_gatt_rtus_service *service,
    const uint8_t state[2]
) {
    if (!server || !service || !state || state[0] > 1 || state[1] > 5)
        return 0;

    memset(service, 0, sizeof(*service));
    service->server = server;
    static const uint8_t control_point = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a16, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &control_point, 1},
        {0x2a17, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, state, 2}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write_request = gatt_rtus_write;
    handlers[0].context = service;

    if (!gatt_sig_add_handlers(server, 0x1806, chars, 2,
            &service->service_handle, values, NULL, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->cp_handle = values[0];
    service->state_handle = values[1];
    return 1;
}

// Application-facing API
// Finish a pending update with its Reference Time Update result code.
// RTUS means Reference Time Update Service.
static inline int gatt_rtus_complete(
    ble_gatt_server *server,
    const ble_gatt_rtus_service *service,
    uint8_t result
) {
    if (!server || !service || service->server != server || result > 5)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->state_handle);
    uint8_t *state = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!state || attribute->value_len != 2 || state[0] != 1)
        return 0;

    state[0] = 0;
    state[1] = result;
    return 1;
}

// Application-facing API
// NDCS means Next DST Change Service.
static inline int gatt_ndcs_add(
    ble_gatt_server *server, ble_gatt_ndcs_service *service,
    const uint8_t time_dst[7]
) {
    if (!service || !time_dst)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a11, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         time_dst, 7}
    };
    uint16_t values[1];

    if (!gatt_sig_add(server, 0x1807, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->time_dst_handle = values[0];
    return 1;
}

// Application-facing API
// Update the seven-octet Time with DST value in the server database.
// NDCS means Next DST Change Service.
static inline int gatt_ndcs_set_time(
    ble_gatt_server *server,
    const ble_gatt_ndcs_service *service,
    const uint8_t time_dst[7]
) {
    if (!server || !service || !service->time_dst_handle ||
        !time_dst
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->time_dst_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 7)
        return 0;

    memcpy(value, time_dst, 7);
    return 1;
}

// Device Time Service (DTS)
typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t feature_cccd_handle;
    uint16_t params_handle, params_cccd_handle;
    uint16_t time_handle, time_cccd_handle;
    uint16_t cp_handle, cp_cccd_handle;
    ble_gatt_server *server;
    uint8_t cp_pending_opcode, cp_req_len;
    uint8_t cp_req[11];
} ble_gatt_dts_service;

// Internal helper
static inline uint8_t gatt_dts_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_dts_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        server, service->cp_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->cp_pending_opcode ||
        gatt_dts_cp_busy(server, service->cp_handle)
    ) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t response[3] = {0x09, opcode, 0x02};
    uint16_t response_len = 3;

    if (opcode == 2 || opcode == 3) {
        ble_gatt_attribute *feature = gatt_server_find(
            server, service->feature_handle);
        if (!feature || feature->value_len != 4)
            return ATT_ERR_UNLIKELY_ERROR;

        uint8_t *feature_value = gatt_attribute_value(server, feature);

        if (!feature_value)
            return ATT_ERR_UNLIKELY_ERROR;

        uint16_t features = ble_get_le16(feature_value + 2);

        if (len != 11) {
            response[2] = 0x03;
        }
        else {
            uint16_t flags = ble_get_le16(value + 1);
            uint8_t epoch_2000 = (uint8_t)((flags >> 6) & 1);
            uint8_t epoch_feature = (uint8_t)(features &
                (epoch_2000 ? (1u << 10) : (1u << 9)));
            if ((flags & 0xff00) || !epoch_feature) {
                response[2] = 0x03;
            }
            else {
                service->cp_pending_opcode = opcode;
                service->cp_req_len = (uint8_t)len;
                memcpy(service->cp_req, value, len);
                return 0;
            }
        }
    }

    if (!gatt_queue_event(server, service->cp_handle,
            response, response_len, 1)
    ) return ATT_ERR_INSUFFICIENT_RESOURCES;

    return 0;
}

// Application-facing API
// Add DTS mandatory characteristics. Feature is the four-byte CRC/feature
// value; device_time is the fixed eight-byte base time and status value.
// The application supplies Device Time Parameters and applies accepted time
// updates before completing the DTCP procedure.
// DTS means Device Time Service.
static inline int gatt_dts_add(
    ble_gatt_server *server, ble_gatt_dts_service *service,
    const uint8_t feature[4], const uint8_t *parameters,
    uint16_t parameters_len, const uint8_t device_time[8],
    uint8_t feature_changeable, uint8_t parameters_changeable,
    uint16_t cp_capacity
) {
    if (!server || !service || !feature || !parameters ||
        !parameters_len || !device_time ||
        cp_capacity < 11 || cp_capacity > 20
    ) return 0;

    uint16_t feature_bits = ble_get_le16(feature + 2);

    if ((feature_bits != 0x0200 && feature_bits != 0x0400) ||
        ble_get_le16(feature) != 0xffff
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4] = {
        {0x2b8e, BLE_GATT_PROP_READ |
            (feature_changeable ? BLE_GATT_PROP_INDICATE : 0),
            feature_changeable, BLE_GATT_PERM_READ, feature, 4},
        {0x2b8f, BLE_GATT_PROP_READ |
            (parameters_changeable ? BLE_GATT_PROP_INDICATE : 0),
            parameters_changeable, BLE_GATT_PERM_READ,
            parameters, parameters_len},
        {0x2b90, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, device_time, 8},
        {0x2b91, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, NULL, 0, cp_capacity, 1}
    };
    uint16_t values[4], cccds[4];
    ble_gatt_sig_handlers handlers[4] = {0};
    memset(service, 0, sizeof(*service));
    handlers[3].write_request = gatt_dts_cp_write;
    handlers[3].context = service;

    if (!gatt_sig_add_handlers(server, 0x1847,
            characteristics, 4, &service->service_handle, values, cccds,
            handlers)
    ) return 0;

    service->feature_handle = values[0];
    service->feature_cccd_handle = cccds[0];
    service->params_handle = values[1];
    service->params_cccd_handle = cccds[1];
    service->time_handle = values[2];
    service->time_cccd_handle = cccds[2];
    service->cp_handle = values[3];
    service->cp_cccd_handle = cccds[3];
    service->server = server;
    return 1;
}

// Application-facing API
// Complete a pending Propose/Force Time Update with a DTCP response.
// Rejection flags are required only when response_value is 0x05.
// DTS means Device Time Service.
static inline int gatt_dts_complete(
    ble_gatt_dts_service *service, uint8_t response_value,
    uint16_t rejection_flags
) {
    if (!service || !service->server ||
        (service->cp_pending_opcode != 2 &&
         service->cp_pending_opcode != 3) ||
        !response_value ||
        (response_value > 5 && response_value != 7) ||
        (response_value != 5 && rejection_flags)
    ) return 0;

    uint8_t response[5] = {
        0x09, service->cp_pending_opcode, response_value,
        (uint8_t)rejection_flags, (uint8_t)(rejection_flags >> 8)
    };
    uint16_t len = response_value == 5 ? 5 : 3;

    if (!gatt_queue_event(service->server,
            service->cp_handle, response, len, 1)
    ) return 0;

    service->cp_pending_opcode = 0;
    service->cp_req_len = 0;
    memset(service->cp_req, 0,
           sizeof(service->cp_req));
    return 1;
}

// Reconnection Configuration Service (RCS)
typedef struct {
    uint16_t service_handle, feature_handle;
} ble_gatt_rcs_service;

// Application-facing API
// Add the base RCS instance with no optional features. Its five-byte input is
// the 0xffff CRC followed by a zero three-byte feature bit field.
// RCS means Reconnection Configuration Service.
static inline int gatt_rcs_add(
    ble_gatt_server *server,
    ble_gatt_rcs_service *service,
    const uint8_t feature[5]
) {
    if (!server || !service || !feature || ble_get_le16(feature) != 0xffff ||
        feature[2] || feature[3] || feature[4]
    ) return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2b1d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 5
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x1829, &characteristic, 1,
            &service->service_handle, values, NULL)
    ) return 0;

    service->feature_handle = values[0];
    return 1;
}
// Bond Management Service (BMS)
typedef struct {
    uint16_t service_handle, cp_handle, feature_handle;
    ble_gatt_server *server;
    uint8_t feature[3], feature_len;
    uint8_t request_pending, request_opcode, request_count;
    uint16_t request_len;
    uint8_t request_parameter[BLE_GATT_SERVER_VALUE_MAX - 1];
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len);
    void *authorize_context;
} ble_gatt_bms_service;

// BMS means Bond Management Service.
static uint8_t gatt_bms_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_bms_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > BLE_GATT_SERVER_VALUE_MAX ||
        len - 1 > sizeof(service->request_parameter))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *control_point = gatt_server_find(
        service->server, service->cp_handle);
    if (!control_point || len > control_point->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (service->request_pending)
        return 0x81; // Operation failed while the previous request is pending.

    uint8_t opcode = value[0];
    if (opcode < 1 || opcode > 9)
        return 0x80; // Op Code not supported.
    uint8_t feature_bit = (uint8_t)((opcode - 1) * 2);
    uint16_t code_len = (uint16_t)(len - 1);
    uint8_t auth_bit = (uint8_t)(feature_bit + 1);
    uint8_t auth_supported =
        (service->feature[auth_bit / 8] & (uint8_t)(1u << (auth_bit % 8))) != 0;
    uint8_t base_supported =
        (service->feature[feature_bit / 8] &
         (uint8_t)(1u << (feature_bit % 8))) != 0;
    if (!auth_supported && !base_supported)
        return 0x80;
    if ((code_len && !auth_supported) || (!code_len && !base_supported))
        return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    for (uint16_t i = 0; i < code_len; i++)
        if (value[i + 1] < '0' || value[i + 1] > '9')
            return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    if (service->authorize) {
        uint8_t error = service->authorize(service->authorize_context,
            opcode, code_len ? value + 1 : NULL, code_len);
        if (error) return error;
    }

    service->request_opcode = opcode;
    service->request_len = code_len;
    if (code_len)
        memcpy(service->request_parameter, value + 1, code_len);
    service->request_pending = 1;
    service->request_count++;
    return 0;
}

// Add Bond Management attributes and bind the request validator. The optional
// authorizer checks authorization-code writes before the ATT Write Response;
// the application then performs the captured bond operation.
// BMS means Bond Management Service.
static inline int gatt_bms_add_auth(
    ble_gatt_server *server, ble_gatt_bms_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t cp_capacity,
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len),
    void *authorize_context
) {
    if (!server || !service || !features || !feature_len || feature_len > 3 ||
        !features[feature_len - 1] ||
        (feature_len == 3 && (features[2] & 0xfcu)) ||
        !cp_capacity ||
        cp_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        // This service binds a normal Write Request callback. Keep its
        // maximum value within one request at the configured local MTU;
        // larger values require prepare/execute support for the same
        // authorization and request-capture semantics.
        cp_capacity > server->local_mtu - 3 ||
        (!authorize &&
         ((features[0] & 0xaa) ||
          (feature_len > 1 && (features[1] & 0xaa)) ||
          (feature_len > 2 && (features[2] & 0x02))))
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa4, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_AUTHN, NULL, 0,
            cp_capacity, 1},
        {0x2aa5, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_AUTHN, features, feature_len}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write_request = gatt_bms_control_write;
    handlers[0].context = service;

    if (!gatt_sig_add_handlers(server, 0x181e, chars, 2,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->cp_handle = values[0];
    service->feature_handle = values[1];
    service->server = server;
    service->feature_len = (uint8_t)feature_len;
    memcpy(service->feature, features, feature_len);
    service->authorize = authorize;
    service->authorize_context = authorize_context;
    return 1;
}

// Copy one validated request for application-owned bond-store processing.
// The operation is cleared only when the caller passes a sufficient buffer.
// BMS means Bond Management Service.
static inline int gatt_bms_take_request(
    ble_gatt_bms_service *service, uint8_t *opcode,
    uint8_t *authorization_code, uint16_t *inout_code_len
) {
    if (!service || !service->server || !service->request_pending || !opcode ||
        !inout_code_len ||
        (service->request_len && !authorization_code) ||
        *inout_code_len < service->request_len)
        return 0;
    *opcode = service->request_opcode;
    if (service->request_len)
        memcpy(authorization_code, service->request_parameter,
               service->request_len);
    *inout_code_len = service->request_len;
    service->request_pending = 0;
    service->request_opcode = 0;
    service->request_len = 0;
    return 1;
}

// Scan Parameters Service
typedef struct {
    uint16_t service_handle, scan_window_handle;
    uint16_t refresh_handle, refresh_cccd_handle;
    uint16_t scan_interval, scan_window;
} ble_gatt_sps_service;

// Scan Parameters Service protocol behavior. The application applies valid
// parameters to its scanner and decides when to request a client refresh.
// Internal helper
static uint8_t gatt_sps_window_write(
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

    uint16_t interval = ble_get_le16(value);
    uint16_t window = ble_get_le16(value + 2);

    if (interval < 4 || interval > 0x4000 || window < 4 ||
        window > interval
    ) return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_sps_service *service = context;
    service->scan_interval = interval;
    service->scan_window = window;
    return 0;
}

// Application-facing API
// Add Scan Parameters Service with a validated initial interval and window.
// Valid client writes update service state; the application applies the new
// parameters to its scanner.
// SPS means Scan Parameters Service.
static inline int gatt_sps_add(
    ble_gatt_server *server, ble_gatt_sps_service *service,
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
        {.write_request = gatt_sps_window_write, .context = service},
        {0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_add_handlers(server, 0x1813, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->scan_window_handle = values[0];
    service->refresh_handle = values[1];
    service->refresh_cccd_handle = cccds[1];
    return 1;
}

// Transport Discovery, Internet Protocol Support, and Elapsed Time
typedef struct {
    uint16_t service_handle, cp_handle, cp_cccd_handle;
    ble_gatt_server *server;
    uint8_t pending, request_opcode, organization_id;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_tds_service;

enum {
    GATT_TDS_RESULT_SUCCESS = 0x00,
    GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TDS_RESULT_INVALID_PARAMETER = 0x02,
    GATT_TDS_RESULT_UNSUPPORTED_ORGANIZATION_ID = 0x03,
    GATT_TDS_RESULT_OPERATION_FAILED = 0x04
};

static uint8_t gatt_tds_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_tds_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (command) return ATT_ERR_WRITE_NOT_PERMITTED;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (len < 2) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->cp_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->cp_handle);

    if (!cccd || !(cccd->cccd & 2)) return 0xfd;

    if (service->pending || gatt_dts_cp_busy(service->server,
            service->cp_handle)) return 0xfe;

    if (value[0] != 1) {
        uint8_t response[2] = {value[0], GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED};

        if (service->server->mtu < 5 || !gatt_queue_event(
                service->server, service->cp_handle,
                response, sizeof(response), 1))
            return ATT_ERR_INSUFFICIENT_RESOURCES;

        return 0;
    }

    service->request_opcode = value[0];
    service->organization_id = value[1];
    service->parameter_len = len - 2;

    if (service->parameter_len)
        memcpy(service->parameters, value + 2, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application-facing API
// Add TDS with its optional Control Point. Set capacity to zero to omit it.
// The builder captures Activate Transport requests; the application performs
// organization-specific activation and completes each request with an indication.
// TDS means Transport Discovery Service.
static inline int gatt_tds_add(
    ble_gatt_server *server, ble_gatt_tds_service *service,
    uint16_t cp_capacity
) {
    if (!server || !service ||
        cp_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (cp_capacity && cp_capacity < 2)
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!cp_capacity) {
        ble_gatt_uuid uuid = {2, {0x24, 0x18}};
        return gatt_server_add_service(server, &uuid, 1,
                                       &service->service_handle);
    }

    uint8_t initial_value = 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2abc, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE, &initial_value, 0, cp_capacity, 1
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write_request = gatt_tds_control_point_write, .context = service}
    };
    uint16_t value_handle, cccd_handle;
    service->server = server;

    if (!gatt_sig_add_handlers(server, 0x1824, &characteristic,
            1, &service->service_handle, &value_handle, &cccd_handle,
            handlers)
    ) return 0;

    service->cp_handle = value_handle;
    service->cp_cccd_handle = cccd_handle;
    return 1;
}

// Complete an Activate Transport request and indicate its result. A response
// parameter, when present, begins with the request Organization ID.
// TDS means Transport Discovery Service.
static inline int gatt_tds_complete(
    ble_gatt_tds_service *service, uint8_t result,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TDS_RESULT_OPERATION_FAILED ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 2 ||
        (response_parameter_len &&
         response_parameter[0] != service->organization_id) ||
        service->server->mtu < 5 ||
        response_parameter_len + 2 > service->server->mtu - 3)
        return 0;

    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = service->request_opcode;
    response[1] = result;

    if (response_parameter_len)
        memcpy(response + 2, response_parameter, response_parameter_len);

    if (!gatt_queue_event(service->server,
            service->cp_handle, response,
            response_parameter_len + 2, 1)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle;
} ble_gatt_ipsp_service;

// Application-facing API
// Add the Internet Protocol Support service. IPSP defines no characteristics.
// IPSP means Internet Protocol Support Profile.
static inline int gatt_ipsp_add(
    ble_gatt_server *server,
    ble_gatt_ipsp_service *service
) {
    if (!server || !service || server->database_sealed)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_uuid uuid = {2, {0x20, 0x18}};
    return gatt_server_add_service(server, &uuid, 1, &service->service_handle);
}

typedef struct {
    uint16_t service_handle, elapsed_handle;
    uint16_t elapsed_cccd_handle;
} ble_gatt_elapsed_service;

// Application-facing API
// Add Current Elapsed Time as a readable, indicatable 11-byte value. The value
// contains the 9-byte elapsed-time field followed by clock status/capabilities.
static inline int gatt_elapsed_add(
    ble_gatt_server *server, ble_gatt_elapsed_service *service,
    const uint8_t current_elapsed_time[11]
) {
    if (!server || !service || !current_elapsed_time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristic = {
        0x2bf2, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ, current_elapsed_time, 11
    };
    uint16_t values[1], cccds[1];

    if (!gatt_sig_add(server, 0x183f, &characteristic, 1,
            &service->service_handle, values, cccds)
    ) return 0;

    service->elapsed_handle = values[0];
    service->elapsed_cccd_handle = cccds[0];
    return 1;
}

// Authorization Control Service
typedef struct {
    uint16_t service_handle, status_handle, status_cccd_handle;
    uint16_t data_in_handle, notify_handle;
    uint16_t notify_cccd_handle, indicate_handle;
    uint16_t indicate_cccd, cp_handle;
    uint16_t cp_cccd_handle;
    ble_gatt_server *server;
    uint16_t data_in_capacity, cp_capacity;
    uint8_t in_assembling, in_next_counter;
    uint16_t in_assem_len;
    uint8_t in_assem[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t data_in_pending;
    uint16_t in_msg_len;
    uint8_t in_msg[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t cp_assembling, cp_next_counter;
    uint16_t cp_assem_len;
    uint8_t cp_assem[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t cp_pending, cp_taken;
    uint16_t cp_msg_len;
    uint8_t cp_msg[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_acs_service;

// ACS means Authorization Control Service.
static inline uint8_t gatt_acs_receive_segment(
    uint8_t *active, uint8_t *next_counter, uint16_t *assembly_len,
    uint8_t *assembly, const uint8_t *value, uint16_t len,
    uint8_t **complete, uint16_t *complete_len
) {
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t header = value[0];
    uint8_t first = header & 1;
    uint8_t last = (header >> 1) & 1;
    uint8_t counter = header >> 2;
    if (first) {
        *active = 1;
        *assembly_len = 0;
        *next_counter = (uint8_t)((counter + 1) & 0x3f);
    }
    else if (!*active || counter != *next_counter) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    else *next_counter = (uint8_t)((counter + 1) & 0x3f);

    uint16_t payload_len = (uint16_t)(len - 1);
    if (payload_len > BLE_GATT_SERVER_VALUE_MAX - *assembly_len) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    if (payload_len) memcpy(assembly + *assembly_len, value + 1, payload_len);
    *assembly_len = (uint16_t)(*assembly_len + payload_len);
    if (last) {
        *active = 0;
        *complete = assembly;
        *complete_len = *assembly_len;
        return 1;
    }
    return 0;
}

// ACS means Authorization Control Service.
static uint8_t gatt_acs_data_in_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_acs_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (service->data_in_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->data_in_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_acs_receive_segment(
        &service->in_assembling, &service->in_next_counter,
        &service->in_assem_len, service->in_assem, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->in_msg, complete, complete_len);
        service->in_msg_len = complete_len;
        service->data_in_pending = 1;
    }
    return 0;
}

// ACS means Authorization Control Service.
static uint8_t gatt_acs_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_acs_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->cp_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->cp_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->cp_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_acs_receive_segment(
        &service->cp_assembling,
        &service->cp_next_counter,
        &service->cp_assem_len,
        service->cp_assem, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->cp_msg, complete, complete_len);
        service->cp_msg_len = complete_len;
        service->cp_pending = 1;
    }
    return 0;
}

// Application-facing API
// Add ACS attributes and bind segmented Data In/Control Point reassembly.
// ACS means Authorization Control Service.
static inline int gatt_acs_add(
    ble_gatt_server *server,
    ble_gatt_acs_service *service,
    const uint8_t status[3], uint16_t data_capacity,
    uint16_t cp_capacity
) {
    if (!server || !service || !status || !data_capacity ||
        data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !cp_capacity ||
        cp_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t data_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[5] = {
        {0x2b2f, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, status, 3},
        {0x2b30, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &data_initial, 0, data_capacity, 1},
        {0x2b31, BLE_GATT_PROP_NOTIFY, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b32, BLE_GATT_PROP_INDICATE, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b33, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 0,
            cp_capacity, 1}
    };
    uint16_t values[5], cccds[5];
    ble_gatt_sig_handlers handlers[5] = {0};
    handlers[1].write_request = gatt_acs_data_in_write;
    handlers[1].context = service;
    handlers[4].write_request = gatt_acs_control_point_write;
    handlers[4].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add_handlers(server, 0x183d, characteristics,
            5, &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->status_handle = values[0];
    service->status_cccd_handle = cccds[0];
    service->data_in_handle = values[1];
    service->notify_handle = values[2];
    service->notify_cccd_handle = cccds[2];
    service->indicate_handle = values[3];
    service->indicate_cccd = cccds[3];
    service->cp_handle = values[4];
    service->cp_cccd_handle = cccds[4];
    service->server = server;
    service->data_in_capacity = data_capacity;
    service->cp_capacity = cp_capacity;
    return 1;
}

// ACS means Authorization Control Service.
static inline int gatt_acs_take_data_in(
    ble_gatt_acs_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->data_in_pending || !payload ||
        !inout_payload_len || *inout_payload_len < service->in_msg_len)
        return 0;
    memcpy(payload, service->in_msg, service->in_msg_len);
    *inout_payload_len = service->in_msg_len;
    service->data_in_pending = 0;
    service->in_msg_len = 0;
    return 1;
}

// ACS means Authorization Control Service.
static inline int gatt_acs_take_cp(
    ble_gatt_acs_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->cp_pending ||
        service->cp_taken || !payload ||
        !inout_payload_len ||
        *inout_payload_len < service->cp_msg_len) return 0;
    memcpy(payload, service->cp_msg,
           service->cp_msg_len);
    *inout_payload_len = service->cp_msg_len;
    service->cp_taken = 1;
    return 1;
}

// ACS means Authorization Control Service.
static inline int gatt_acs_queue_payload(
    ble_gatt_acs_service *service, uint16_t handle,
    const uint8_t *payload, uint16_t payload_len, uint8_t indication
) {
    if (!service || !service->server || !handle || !payload || !payload_len ||
        payload_len > BLE_GATT_SERVER_VALUE_MAX) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);
    if (!attribute || !(attribute->properties &
            (indication ? BLE_GATT_PROP_INDICATE : BLE_GATT_PROP_NOTIFY)) ||
        !attribute->value_capacity) return 0;
    uint16_t chunk_limit = (uint16_t)(service->server->mtu - 4);
    if (chunk_limit > attribute->value_capacity - 1)
        chunk_limit = (uint16_t)(attribute->value_capacity - 1);
    if (!chunk_limit) return 0;
    uint16_t fragments = (uint16_t)((payload_len + chunk_limit - 1) /
                                     chunk_limit);
    if (fragments > BLE_GATT_SERVER_EVENT_QUEUE_SIZE -
                    service->server->event_count ||
        (uint32_t)payload_len + fragments >
            BLE_GATT_SERVER_EVENT_BYTES - service->server->event_used) return 0;
    uint16_t offset = 0;
    uint8_t counter = 0;
    do {
        uint16_t chunk = (uint16_t)(payload_len - offset);
        if (chunk > chunk_limit) chunk = chunk_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t first = offset == 0;
        uint8_t last = chunk == payload_len - offset;
        packet[0] = (uint8_t)((counter << 2) | (first ? 1 : 0) |
                              (last ? 2 : 0));
        memcpy(packet + 1, payload + offset, chunk);
        if (!gatt_queue_event(service->server, handle, packet,
                (uint16_t)(chunk + 1), indication)) return 0;
        offset = (uint16_t)(offset + chunk);
        counter = (uint8_t)((counter + 1) & 0x3f);
    } while (offset < payload_len);
    return 1;
}

// ACS means Authorization Control Service.
static inline int gatt_acs_set_status(
    ble_gatt_acs_service *service, const uint8_t status[3],
    uint8_t indicate
) {
    if (!service || !service->server || !status || indicate > 1) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      service->status_handle);
    if (!attribute || attribute->value_capacity != 3) return 0;
    uint8_t *stored = gatt_attribute_value(service->server, attribute);
    if (!stored) return 0;
    memcpy(stored, status, 3);
    if (!indicate) return 1;
    return gatt_queue_event(service->server, service->status_handle,
                                    status, 3, 1) == 1;
}

// ACS means Authorization Control Service.
static inline int gatt_acs_complete_cp(
    ble_gatt_acs_service *service,
    const uint8_t *response, uint16_t response_len
) {
    if (!service || !service->cp_pending || !response ||
        !response_len) return 0;
    if (!gatt_acs_queue_payload(service,
            service->cp_handle, response, response_len, 1)) return 0;
    service->cp_pending = 0;
    service->cp_taken = 0;
    service->cp_msg_len = 0;
    return 1;
}

// Alert services
typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level, request_count;
} ble_gatt_ias_service;

typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level;
} ble_gatt_lls_service;

typedef struct {
    uint16_t service_handle, alert_status_handle, status_cccd_handle;
    uint16_t ringer_handle, ringer_cp_handle;
    uint16_t ringer_cccd_handle;
    uint8_t alert_status, ringer, last_ringer_cmd;
    uint8_t ringer_count;
} ble_gatt_pass_service;

typedef struct {
    uint16_t service_handle, new_sup_handle;
    uint16_t unread_sup_handle, new_alert_handle;
    uint16_t new_cccd_handle, unread_handle;
    uint16_t unread_cccd_handle, cp_handle;
    uint16_t supported_new, supported_unread;
    uint16_t enabled_new, enabled_unread;
    uint8_t last_command, last_category, control_count;
} ble_gatt_ans_service;

// Protocol behavior for Immediate Alert, Link Loss, and Phone Alert Status.
// The application observes the state fields to perform device-specific
// actions.

// Internal helper
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

// Internal helper
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

// Internal helper
static uint8_t gatt_ias_write(
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

    ble_gatt_ias_service *service = context;
    service->alert_level = value[0];
    service->request_count++;
    return 0;
}

// Internal helper
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

    ble_gatt_pass_service *service = context;
    service->last_ringer_cmd = value[0];
    service->ringer_count++;

    if (value[0] == 1)
        service->ringer = 0;
    if (value[0] == 3)
        service->ringer = 1;
    return 0;
}

// Application-facing API
// Add Immediate Alert. Valid writes update alert_level and increment
// request_count so the application can detect repeated requests.
// IAS means Immediate Alert Service.
static inline int gatt_ias_add(
    ble_gatt_server *server, ble_gatt_ias_service *service,
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
        {.write_request = gatt_ias_write, .context = service}
    };
    uint16_t values[1];

    if (!gatt_sig_add_handlers(server, 0x1802, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application-facing API
// Add Link Loss Alert Level with readable state and validated writes.
// LLS means Link Loss Service.
static inline int gatt_lls_add(
    ble_gatt_server *server, ble_gatt_lls_service *service,
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
        {.read = gatt_alert_u8_read, .write_request = gatt_alert_level_write,
         .context = &service->alert_level}
    };
    uint16_t values[1];

    if (!gatt_sig_add_handlers(server, 0x1803, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application-facing API
// Add Phone Alert Status. Valid Ringer Control Point writes update the
// exposed state; the application performs the corresponding phone behavior
// and sends notifications when the values change.
// PASS means Phone Alert Status Service.
static inline int gatt_pass_add(
    ble_gatt_server *server, ble_gatt_pass_service *service,
    const uint8_t alert_status[1], uint8_t ringer
) {
    if (!service || !alert_status || (*alert_status & (uint8_t)~0x07) ||
        ringer > 1
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->alert_status = *alert_status;
    service->ringer = ringer;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a3f, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->alert_status, 1},
        {0x2a41, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->ringer, 1},
        {0x2a40, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         NULL, 0, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.read = gatt_alert_u8_read, .context = &service->alert_status},
        {.read = gatt_alert_u8_read, .context = &service->ringer},
        {.write_request = gatt_phone_ringer_control_write, .context = service}
    };
    uint16_t values[3], cccds[3];

    if (!gatt_sig_add_handlers(server, 0x180e, chars, 3,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->alert_status_handle = values[0];
    service->status_cccd_handle = cccds[0];
    service->ringer_handle = values[1];
    service->ringer_cccd_handle = cccds[1];
    service->ringer_cp_handle = values[2];
    return 1;
}

// Application-facing API
// PASS means Phone Alert Status Service.
static inline int gatt_pass_set_status(
    ble_gatt_pass_service *service, uint8_t alert_status
) {
    if (!service || !service->alert_status_handle ||
        (alert_status & (uint8_t)~0x07)
    ) return 0;

    service->alert_status = alert_status;
    return 1;
}

// Application-facing API
// Update Alert Status and queue its notification.
// PASS means Phone Alert Status Service.
static inline int gatt_pass_notify_status(
    ble_gatt_server *server, ble_gatt_pass_service *service,
    uint8_t alert_status
) {
    if (!server ||
        !gatt_pass_set_status(service,
            alert_status)
    ) return 0;

    return gatt_queue_event(server, service->alert_status_handle,
        &service->alert_status, 1, 0);
}

// Application-facing API
// PASS means Phone Alert Status Service.
static inline int gatt_pass_set_ringer(
    ble_gatt_pass_service *service, uint8_t ringer
) {
    if (!service || !service->ringer_handle || ringer > 1)
        return 0;

    service->ringer = ringer;
    return 1;
}

// Application-facing API
// Update Ringer Setting and queue its notification.
// PASS means Phone Alert Status Service.
static inline int gatt_pass_notify_ringer(
    ble_gatt_server *server, ble_gatt_pass_service *service,
    uint8_t ringer
) {
    if (!server ||
        !gatt_pass_set_ringer(service,
            ringer)
    ) return 0;

    return gatt_queue_event(server, service->ringer_handle,
        &service->ringer, 1, 0);
}

// Alert Notification Profile control point protocol. The application owns
// alert records and sends any requested notifications.
#define GATT_ALERT_CATEGORY_MASK 0x03ffu
#define GATT_ALERT_CATEGORY_ALL  0xffu

// Internal helper
static uint8_t gatt_ans_control_write(
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

    ble_gatt_ans_service *service = context;
    uint16_t supported = (cmd == 1 || cmd == 3 || cmd == 5) ?
        service->supported_unread : service->supported_new;
    uint16_t category_bit = category == GATT_ALERT_CATEGORY_ALL ?
        supported : (uint16_t)(1u << category);

    if (category_bit & (uint16_t)~supported)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    if (cmd == 0 || cmd == 1) {
        uint16_t *enabled = cmd == 0 ? &service->enabled_new :
                                      &service->enabled_unread;
        *enabled |= category_bit;
    }
    else if (cmd == 2 || cmd == 3) {
        uint16_t *enabled = cmd == 2 ? &service->enabled_new :
                                      &service->enabled_unread;
        *enabled &= (uint16_t)~category_bit;
    }

    service->last_command = cmd;
    service->last_category = category;
    service->control_count++;
    return 0;
}

// Application-facing API
// Add Alert Notification Service with supported categories. Control point
// writes update enable masks and the last-command fields; the application
// handles alert data and sends requested notifications.
// ANS means Alert Notification Service.
static inline int gatt_ans_add(
    ble_gatt_server *server, ble_gatt_ans_service *service,
    uint16_t supported_new, uint16_t supported_unread
) {
    if (!service ||
        (supported_new & (uint16_t)~GATT_ALERT_CATEGORY_MASK) ||
        (supported_unread & (uint16_t)~GATT_ALERT_CATEGORY_MASK)
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->supported_new = supported_new;
    service->supported_unread = supported_unread;
    uint8_t new_categories[2];
    uint8_t unread_categories[2];
    ble_put_le16(new_categories, supported_new);
    ble_put_le16(unread_categories, supported_unread);
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
    handlers[4].write_request = gatt_ans_control_write;
    handlers[4].context = service;
    uint16_t values[5], cccds[5];

    if (!gatt_sig_add_handlers(server, 0x1811, chars, 5,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->new_sup_handle = values[0];
    service->unread_sup_handle = values[1];
    service->new_alert_handle = values[2];
    service->new_cccd_handle = cccds[2];
    service->unread_handle = values[3];
    service->unread_cccd_handle = cccds[3];
    service->cp_handle = values[4];
    return 1;
}

// Application-facing API
// Queue a New Alert notification for an enabled, supported category. Text is
// optional and limited to the 18 octets allowed by the characteristic format.
// ANS means Alert Notification Service.
static inline int gatt_ans_notify_new(
    ble_gatt_server *server,
    const ble_gatt_ans_service *service,
    uint8_t category, uint8_t count, const uint8_t *text, uint16_t text_len
) {
    if (!server || !service || category > 9 || text_len > 18 ||
        (text_len && !text)
    ) return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_new & category_bit) ||
        !(service->enabled_new & category_bit)
    ) return 0;

    uint8_t value[20] = {category, count};

    if (text_len)
        memcpy(value + 2, text, text_len);
    return gatt_queue_event(server, service->new_alert_handle,
        value, (uint16_t)(2 + text_len), 0);
}

// Application-facing API
// Queue an Unread Alert Status notification for an enabled, supported
// category.
// ANS means Alert Notification Service.
static inline int gatt_ans_notify_unread(
    ble_gatt_server *server,
    const ble_gatt_ans_service *service,
    uint8_t category, uint8_t count
) {
    if (!server || !service || category > 9)
        return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_unread & category_bit) ||
        !(service->enabled_unread & category_bit)
    ) return 0;

    uint8_t value[2] = {category, count};
    return gatt_queue_event(server, service->unread_handle,
        value, sizeof(value), 0);
}

#endif
