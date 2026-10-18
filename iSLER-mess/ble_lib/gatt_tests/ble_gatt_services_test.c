#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../gatt_services/ble_gatt_services.h"

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

static uint8_t ots_change_count, ots_last_change, ots_last_id[6];

static void ots_change_observer(
    void *context, uint8_t flags, const uint8_t object_id[6]
) {
    (void)context;
    ots_change_count++;
    ots_last_change = flags;
    memcpy(ots_last_id, object_id, sizeof(ots_last_id));
}

static uint8_t ots_select_for_test(void *context, const uint8_t object_id[6]) {
    ble_gatt_ots_service *service = context;
    for (uint16_t i = 0; i < service->object_count; i++) {
        ble_gatt_ots_object *object = &service->objects[i];
        if (memcmp(object->id, object_id, 6)) continue;
        uint8_t properties[4] = {
            (uint8_t)object->properties,
            (uint8_t)(object->properties >> 8),
            (uint8_t)(object->properties >> 16),
            (uint8_t)(object->properties >> 24)
        };
        return (uint8_t)gatt_ots_set_object_metadata(service,
            object->name, object->name_len, object->type, object->type_len,
            object->size, object->created, object->modified, object->id,
            properties);
    }
    return 0;
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_bas_service battery = {0};
    gatt_server_init(&server, 23);

    assert(gatt_bas_add(&server, &battery, 87));
    assert(battery.service_handle && battery.level_handle);
    uint8_t level = 0;
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 87);

    assert(gatt_bas_set_level(&battery, 42));
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);
    assert(!gatt_bas_set_level(&battery, 101));
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
           response[4] == ATT_ERR_WRITE_NOT_PERMITTED);
    assert(read_battery_level(&server, battery.level_handle, &level));
    assert(level == 42);

    gatt_server_init(&server, 23);
    ble_gatt_mesh_prov_service provisioning = {0};
    ble_gatt_mesh_proxy_service proxy = {0};
    assert(gatt_mesh_prov_add(&server, &provisioning));
    assert(gatt_mesh_proxy_add(&server, &proxy));
    assert(provisioning.service_handle && provisioning.data_in_handle &&
           provisioning.data_out_handle &&
           provisioning.data_out_cccd_handle);
    assert(proxy.service_handle && proxy.data_in_handle &&
           proxy.data_out_handle && proxy.data_out_cccd_handle);
    assert(server.attrs[provisioning.data_in_handle - 1].uuid.value[0] ==
           0xdb);
    assert(server.attrs[provisioning.data_out_handle - 1].properties ==
           BLE_GATT_PROP_NOTIFY);
    assert(server.attrs[proxy.data_in_handle - 1].uuid.value[0] ==
           0xdd);
    assert(server.attrs[proxy.data_out_handle - 1].properties ==
           BLE_GATT_PROP_NOTIFY);

    uint8_t digital_value = 1;
    uint8_t analog_value[2] = {0x34, 0x12};
    ble_gatt_sig_characteristic io_channels[] = {
        {0x2a56, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, &digital_value, 1},
        {0x2a58, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         analog_value, sizeof(analog_value)}
    };
    ble_gatt_aio_service automation_io = {0};
    assert(gatt_aio_add(&server, &automation_io,
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

    ble_gatt_bss_service binary_sensor = {0};
    assert(gatt_bss_add_full(&server, &binary_sensor, 20));
    assert(binary_sensor.service_handle && binary_sensor.control_point_handle &&
           binary_sensor.response_handle && binary_sensor.response_cccd_handle);
    assert(server.attrs[binary_sensor.control_point_handle - 1].uuid.value[0]
           == 0x2b);
    assert(server.attrs[binary_sensor.control_point_handle - 1].properties
           == BLE_GATT_PROP_WRITE);
    assert(server.attrs[binary_sensor.response_handle - 1].properties ==
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
           cgm.session_start_handle && cgm.session_run_handle &&
           cgm.record_access_handle && cgm.record_access_cccd_handle &&
           cgm.specific_ops_handle && cgm.specific_ops_cccd_handle);
    assert(gatt_server_find(&server, cgm.measurement_handle)->value_capacity ==
           cgm_config.measurement_capacity);
    assert(gatt_server_find(&server, cgm.record_access_cccd_handle)->permissions ==
           (BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE_AUTHN));

    // OTS returns Object Not Selected for metadata reads and writes when the
    // current object is invalid, and routes client changes to the app for
    // fan-out instead of indicating them back to the writing client.
    gatt_server_init(&server, 23);
    ble_gatt_ots_service ots = {0};
    const uint8_t ots_features[8] = {0xe3, 0x01, 0, 0, 0x01, 0, 0, 0};
    const uint8_t ots_name[] = {'a'};
    const uint8_t ots_type[] = {0x00, 0x18};
    const uint8_t ots_size[8] = {2, 0, 0, 0, 4, 0, 0, 0};
    const uint8_t ots_properties[4] = {0x79, 0, 0, 0};
    const uint8_t ots_id[6] = {0x00, 0x01, 0, 0, 0, 0};
    assert(gatt_ots_add_multi(&server, &ots, ots_features,
        ots_name, sizeof(ots_name), ots_type, sizeof(ots_type), ots_size,
        ots_properties, ots_id, NULL, 0, 32));
    server.encrypted = 1;
    gatt_ots_set_change_observer(&ots, ots_change_observer, NULL);
    uint8_t read_name[] = {
        ATT_OP_READ_REQ, (uint8_t)ots.object_name_handle,
        (uint8_t)(ots.object_name_handle >> 8)
    };
    assert(gatt_server_att(&server, read_name, sizeof(read_name), response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == ATT_OP_READ_RSP && response_len == 2 &&
           response[1] == 'a');
    ots.current_object_valid = 0;
    assert(gatt_server_att(&server, read_name, sizeof(read_name), response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == ATT_OP_ERROR_RSP && response[4] == 0x81);
    uint8_t write_name[] = {
        ATT_OP_WRITE_REQ, (uint8_t)ots.object_name_handle,
        (uint8_t)(ots.object_name_handle >> 8), 'b'
    };
    assert(gatt_server_att(&server, write_name, sizeof(write_name), response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == ATT_OP_ERROR_RSP && response[4] == 0x81 &&
           ots_change_count == 0);
    ots.current_object_valid = 1;
    assert(gatt_server_att(&server, write_name, sizeof(write_name), response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == ATT_OP_WRITE_RSP && response_len == 1 &&
           ots_change_count == 1 && ots_last_change ==
           (GATT_OTS_CHANGE_CLIENT | GATT_OTS_CHANGE_METADATA) &&
           !memcmp(ots_last_id, ots_id, sizeof(ots_id)) &&
           server.event_count == 0);
    ble_gatt_attribute *ots_oacp_cccd = gatt_server_find(&server,
        ots.action_control_point_cccd_handle);
    assert(ots_oacp_cccd);
    ots_oacp_cccd->cccd = 2;
    const uint8_t append_write[] = {
        0x06, 2, 0, 0, 0, 3, 0, 0, 0, 0
    };
    assert(gatt_ots_oacp_write(&ots, 0, append_write,
        sizeof(append_write), 0) == 0 && ots.request_pending);
    ots.request_pending = 0;
    ots.active_control_point = 0;
    ots.features[0] &= (uint8_t)~0x40;
    assert(gatt_ots_oacp_write(&ots, 0, append_write,
        sizeof(append_write), 0) == 0 && server.event_count == 1);
    assert(server.event_data[server.events[0].data_offset + 2] == 0x03);

    // A filter that excludes the current object invalidates it. Go To ignores
    // filters, selects the requested object, and resets every filter.
    gatt_server_init(&server, 23);
    memset(&ots, 0, sizeof(ots));
    const uint8_t ots_id_2[6] = {0x01, 0x01, 0, 0, 0, 0};
    assert(gatt_ots_add_multi(&server, &ots, ots_features,
        ots_name, sizeof(ots_name), ots_type, sizeof(ots_type), ots_size,
        ots_properties, ots_id, NULL, 0, 32));
    server.encrypted = 1;
    ble_gatt_ots_object objects[2] = {
        {
            .id = {0x00, 0x01, 0, 0, 0, 0}, .properties = 0x79,
            .name = (const uint8_t *)"a", .name_len = 1,
            .type = {0x00, 0x18}, .type_len = 2
        },
        {
            .id = {0x01, 0x01, 0, 0, 0, 0}, .properties = 0x79,
            .name = (const uint8_t *)"b", .name_len = 1,
            .type = {0x00, 0x18}, .type_len = 2
        }
    };
    assert(gatt_ots_set_objects(&ots, objects, 2,
        ots_select_for_test, NULL, NULL, &ots));
    const uint8_t name_filter[] = {0x04, 'b'};
    assert(gatt_ots_set_list_filter(&ots, 0, name_filter,
                                    sizeof(name_filter)));
    assert(!ots.current_object_valid);
    ble_gatt_attribute *oacp_cccd = gatt_server_find(&server,
        ots.action_control_point_cccd_handle);
    assert(oacp_cccd);
    oacp_cccd->cccd = 2;
    const uint8_t delete_request[] = {0x02};
    assert(gatt_ots_oacp_write(&ots, 0, delete_request,
        sizeof(delete_request), 0) == 0);
    assert(server.event_count == 1 &&
           server.event_data[server.events[0].data_offset + 2] == 0x05);
    ble_gatt_attribute *olcp_cccd = gatt_server_find(&server,
        ots.object_list_control_point_cccd_handle);
    assert(olcp_cccd);
    olcp_cccd->cccd = 2;
    const uint8_t go_to[] = {0x05, 0x01, 0x01, 0, 0, 0, 0};
    assert(gatt_ots_olcp_write(&ots, 0, go_to, sizeof(go_to), 0) == 0);
    assert(ots.current_object_valid &&
           !memcmp(ots.current_object_id, ots_id_2, sizeof(ots_id_2)));
    for (uint8_t i = 0; i < 3; i++) {
        ble_gatt_attribute *filter = gatt_server_find(&server,
            ots.object_list_filter_handles[i]);
        const uint8_t *filter_value = filter ?
            gatt_attribute_value(&server, filter) : NULL;
        assert(filter && filter_value && filter->value_len == 1 &&
               filter_value[0] == 0);
    }

    return 0;
}
