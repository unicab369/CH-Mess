#ifndef BLE_GATT_SERVICE_TPMS_H
#define BLE_GATT_SERVICE_TPMS_H

// Tire Pressure Monitoring Service (TPMS).
typedef struct {
    uint16_t service_handle;
    uint16_t handles[7];
    uint16_t cccd_handles[7];
} ble_gatt_tpms_service;

// Application facing
// Add the mandatory TPMS Properties and Tire Pressure characteristics with
// optional temperature, acceleration, duty-cycle, position, and signing key.
// The application encodes each present value and handles TPMS signing.
static inline int gatt_tpms_service_add(
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

    if (!gatt_sig_service_add(server, 0x1860, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    for (uint8_t i = 0; i < n; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];
    }
    return 1;
}

#endif
