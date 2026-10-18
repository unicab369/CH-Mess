#include <assert.h>
#include <stdint.h>

#include "../ble_gatt_services.h"

static uint8_t read_battery_level(
    ble_gatt_server *server, uint16_t handle, uint8_t *value
) {
    uint8_t request[] = {
        ATT_OP_READ_REQ, (uint8_t)handle, (uint8_t)(handle >> 8)
    };
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(server, request, sizeof(request), response,
                           sizeof(response), &response_len) == 1);
    assert(response_len == 2 && response[0] == ATT_OP_READ_RSP);
    *value = response[1];
    return response[0];
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_battery_service battery = {0};
    gatt_server_init(&server, 23);

    assert(gatt_battery_service_add(&server, &battery, 87));
    assert(battery.service_handle && battery.level_handle);
    uint8_t level = 0;
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 87);

    assert(gatt_battery_service_set_level(&battery, 42));
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);
    assert(!gatt_battery_service_set_level(&battery, 101));
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);

    uint8_t write_request[] = {
        ATT_OP_WRITE_REQ, (uint8_t)battery.level_handle,
        (uint8_t)(battery.level_handle >> 8), 50
    };
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(&server, write_request, sizeof(write_request),
        response, sizeof(response), &response_len) == 1);
    assert(response_len == 5 && response[0] == ATT_OP_ERROR_RSP &&
           response[1] == ATT_OP_WRITE_REQ &&
           response[4] == BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED);
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);

    gatt_server_init(&server, 23);
    ble_gatt_mesh_provisioning_service provisioning = {0};
    ble_gatt_mesh_proxy_service proxy = {0};
    assert(gatt_mesh_provisioning_service_add(&server, &provisioning));
    assert(gatt_mesh_proxy_service_add(&server, &proxy));
    assert(provisioning.service_handle && provisioning.data_in_handle &&
           provisioning.data_out_handle &&
           provisioning.data_out_cccd_handle);
    assert(proxy.service_handle && proxy.data_in_handle &&
           proxy.data_out_handle && proxy.data_out_cccd_handle);
    assert(server.attributes[provisioning.data_in_handle - 1].uuid.value[0] ==
           0xdb);
    assert(server.attributes[provisioning.data_out_handle - 1].properties ==
           BLE_GATT_PROP_NOTIFY);
    assert(server.attributes[proxy.data_in_handle - 1].uuid.value[0] ==
           0xdd);
    assert(server.attributes[proxy.data_out_handle - 1].properties ==
           BLE_GATT_PROP_NOTIFY);

    uint8_t digital_value = 1;
    uint8_t analog_value[2] = {0x34, 0x12};
    ble_gatt_sig_characteristic io_channels[] = {
        {0x2a56, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, &digital_value, 1},
        {0x2a58, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         analog_value, sizeof(analog_value)}
    };
    ble_gatt_automation_io_service automation_io = {0};
    assert(gatt_automation_io_service_add(&server, &automation_io,
        io_channels, 2, 1, BLE_GATT_PROP_READ));
    assert(automation_io.channel_count == 2 && automation_io.aggregate_handle);
    ble_gatt_attribute *aggregate = gatt_server_find(
        &server, automation_io.aggregate_handle);
    assert(aggregate && aggregate->value_len == 4);
    const uint8_t *aggregate_value = gatt_attribute_value(&server, aggregate);
    assert(aggregate_value[0] == (uint8_t)automation_io.channel_handles[0]);
    assert(aggregate_value[1] ==
           (uint8_t)(automation_io.channel_handles[0] >> 8));
    assert(aggregate_value[2] == (uint8_t)automation_io.channel_handles[1]);
    assert(aggregate_value[3] ==
           (uint8_t)(automation_io.channel_handles[1] >> 8));

    ble_gatt_binary_sensor_service binary_sensor = {0};
    assert(gatt_binary_sensor_service_add(&server, &binary_sensor));
    assert(binary_sensor.service_handle && binary_sensor.control_point_handle &&
           binary_sensor.response_handle && binary_sensor.response_cccd_handle);
    assert(server.attributes[binary_sensor.control_point_handle - 1].uuid.value[0]
           == 0x2b);
    assert(server.attributes[binary_sensor.control_point_handle - 1].properties
           == BLE_GATT_PROP_WRITE);
    assert(server.attributes[binary_sensor.response_handle - 1].properties ==
           BLE_GATT_PROP_INDICATE);

    const uint8_t cgm_feature[] = {0x01, 0x00};
    const uint8_t cgm_status[] = {0x00, 0x00, 0x00};
    const uint8_t cgm_start_time[] = {0};
    const uint8_t cgm_run_time[] = {0x2c, 0x01};
    ble_gatt_cgm_config cgm_config = {
        .feature = cgm_feature,
        .feature_len = sizeof(cgm_feature),
        .status = cgm_status,
        .status_len = sizeof(cgm_status),
        .session_start_time = cgm_start_time,
        .session_start_time_len = sizeof(cgm_start_time),
        .session_run_time = cgm_run_time,
        .session_run_time_len = sizeof(cgm_run_time),
        .measurement_capacity = 24,
        .control_point_capacity = 20,
        .feature_indicate = 1
    };
    ble_gatt_cgm_service cgm = {0};
    assert(gatt_cgm_service_add(&server, &cgm, &cgm_config));
    assert(cgm.measurement_handle && cgm.measurement_cccd_handle &&
           cgm.feature_handle && cgm.feature_cccd_handle && cgm.status_handle &&
           cgm.session_start_time_handle && cgm.session_run_time_handle &&
           cgm.record_access_handle && cgm.record_access_cccd_handle &&
           cgm.specific_ops_handle && cgm.specific_ops_cccd_handle);
    assert(gatt_server_find(&server, cgm.measurement_handle)->value_capacity ==
           cgm_config.measurement_capacity);
    assert(gatt_server_find(&server, cgm.record_access_cccd_handle)->permissions ==
           (BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE_AUTHN));

    return 0;
}
