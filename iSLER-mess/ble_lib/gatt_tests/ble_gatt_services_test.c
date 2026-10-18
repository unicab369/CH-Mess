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

static uint8_t write_value(
    ble_gatt_server *server, uint16_t handle,
    const uint8_t *value, uint16_t len
) {
    uint8_t request[BLE_GATT_ATT_VALUE_MAX + 3];
    request[0] = ATT_OP_WRITE_REQ;
    request[1] = (uint8_t)handle;
    request[2] = (uint8_t)(handle >> 8);
    memcpy(request + 3, value, len);
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(server, request, len + 3, response,
        sizeof(response), &response_len) == 1);
    if (response[0] == ATT_OP_ERROR_RSP) {
        assert(response_len == 5);
        return response[4];
    }
    assert(response[0] == ATT_OP_WRITE_RSP && response_len == 1);
    return 0;
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

static uint8_t ftms_action_count;

static uint8_t ftms_action(
    void *context, uint8_t opcode, const uint8_t *parameters,
    uint8_t parameter_len, uint8_t *response_parameters,
    uint8_t *inout_response_parameter_len
) {
    (void)context;
    (void)opcode;
    (void)parameters;
    (void)parameter_len;
    (void)response_parameters;
    assert(*inout_response_parameter_len == 17);
    *inout_response_parameter_len = 0;
    ftms_action_count++;
    return GATT_FTMS_RESPONSE_SUCCESS;
}

static uint8_t ftms_defer(
    void *context, uint8_t opcode, const uint8_t *parameters,
    uint8_t parameter_len, uint8_t *response_parameters,
    uint8_t *inout_response_parameter_len
) {
    (void)context;
    (void)opcode;
    (void)parameters;
    (void)parameter_len;
    (void)response_parameters;
    (void)inout_response_parameter_len;
    return GATT_FTMS_CONTROL_DEFERRED;
}

static void ftms_write(
    ble_gatt_server *server, ble_gatt_ftms_controlled_service *service,
    const uint8_t *value, uint8_t len, uint8_t expected_result
) {
    uint8_t request[BLE_GATT_ATT_VALUE_MAX + 3];
    request[0] = ATT_OP_WRITE_REQ;
    request[1] = (uint8_t)service->control_point_handle;
    request[2] = (uint8_t)(service->control_point_handle >> 8);
    memcpy(request + 3, value, len);
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    assert(gatt_server_att(server, request, (uint16_t)(len + 3), response,
        sizeof(response), &response_len) == 1);
    assert(response_len == 1 && response[0] == ATT_OP_WRITE_RSP);
    if (expected_result == GATT_FTMS_CONTROL_DEFERRED) {
        assert(service->control_point_pending && !server->event_count);
        return;
    }
    assert(server->event_count >= 1 && server->event_count <= 3);
    ble_gatt_server_event *event = NULL;
    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == service->control_point_handle)
            event = &server->events[i];
    assert(event && event->indication);
    const uint8_t *indication = server->event_data + event->data_offset;
    assert(event->len == 3 && indication[0] == GATT_FTMS_OP_RESPONSE_CODE &&
           indication[1] == value[0] && indication[2] == expected_result);
    if (service->fitness_machine_status_handle &&
        expected_result == GATT_FTMS_RESPONSE_SUCCESS && value[0] !=
        GATT_FTMS_OP_REQUEST_CONTROL) {
        uint8_t expected_status = value[0] == GATT_FTMS_OP_RESET ? 0x01 :
            value[0] == GATT_FTMS_OP_START_RESUME ? 0x04 :
            value[0] == GATT_FTMS_OP_STOP_PAUSE ? 0x02 :
            value[0] == GATT_FTMS_OP_SET_POWER ? 0x08 : 0;
        uint8_t found_status = 0;
        for (uint16_t i = 0; i < server->event_count; i++)
            if (server->events[i].handle ==
                service->fitness_machine_status_handle) {
                const uint8_t *status = server->event_data +
                    server->events[i].data_offset;
                found_status = status[0] == expected_status &&
                    server->events[i].len == len &&
                    (len == 1 || !memcmp(status + 1, value + 1, len - 1));
            }
        assert(found_status);
    }
    server->event_count = 0;
    server->event_used = 0;
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

    // ESS accepts valid trigger/configuration descriptor writes and rejects
    // unsupported logic, condition codes, and malformed time operands.
    gatt_server_init(&server, 23);
    ble_gatt_ess_service ess = {0};
    uint8_t temperature[2] = {0, 0};
    uint8_t descriptor_value_changed[4] = {0};
    uint8_t logic = 0, trigger[8] = {0}, interval[8] = {0};
    uint8_t minimum[8] = {0};
    uint8_t measurement_descriptor[3] = {0};
    const uint8_t extended_properties[] = {2, 0};
    const uint8_t user_description[] = {'T', 'e', 'm', 'p'};
    const uint8_t presentation[] = {0x06, 0, 0, 0, 0, 0, 0};
    const ble_gatt_sig_descriptor ess_descs[] = {
        {0x290b, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         &logic, 1, 1},
        {0x290d, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         trigger, 1, sizeof(trigger)},
        {0x290d, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         interval, 1, sizeof(interval)},
        {0x290d, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         minimum, 1, sizeof(minimum)},
        {0x2904, BLE_GATT_PERM_READ, presentation,
         sizeof(presentation), sizeof(presentation)},
        {0x290c, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         measurement_descriptor, sizeof(measurement_descriptor),
         sizeof(measurement_descriptor)},
        {0x2900, BLE_GATT_PERM_READ, extended_properties,
         sizeof(extended_properties), sizeof(extended_properties)},
        {0x2901, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         user_description, sizeof(user_description), sizeof(user_description)}
    };
    const ble_gatt_sig_characteristic ess_chars[] = {
        {0x2a6e, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ,
         temperature, sizeof(temperature), sizeof(temperature), 0,
         ess_descs, 8},
        {0x2a7d, BLE_GATT_PROP_INDICATE, 1, 0,
         descriptor_value_changed, sizeof(descriptor_value_changed),
         sizeof(descriptor_value_changed), 0, NULL, 0}
    };
    assert(gatt_ess_add(&server, &ess, ess_chars, 2));
    uint16_t config_handle = 0, trigger_handle = 0;
    uint16_t triggers[3] = {0};
    uint16_t measurement_descriptor_handle = 0;
    uint16_t user_description_handle = 0;
    uint16_t descriptor_value_changed_handle = 0;
    uint8_t found_triggers = 0;
    for (uint16_t i = 0; i < server.count; i++) {
        uint16_t uuid = gatt_uuid_assigned16(&server.attrs[i].uuid);
        if (uuid == 0x290b) config_handle = server.attrs[i].handle;
        if (uuid == 0x290c)
            measurement_descriptor_handle = server.attrs[i].handle;
        if (uuid == 0x2901)
            user_description_handle = server.attrs[i].handle;
        if (uuid == 0x2a7d)
            descriptor_value_changed_handle = server.attrs[i].handle;
        if (uuid == 0x290d && found_triggers < 3) {
            if (!trigger_handle) trigger_handle = server.attrs[i].handle;
            triggers[found_triggers++] = server.attrs[i].handle;
        }
    }
    assert(config_handle && trigger_handle && found_triggers == 3 &&
           measurement_descriptor_handle && user_description_handle &&
           descriptor_value_changed_handle ==
               ess.descriptor_value_changed_handle &&
           ess.descriptor_value_changed_cccd_handle);
    const uint8_t use_or[] = {1};
    const uint8_t bad_logic[] = {2};
    const uint8_t less_than[] = {4, 30, 0};
    const uint8_t time_trigger[] = {1, 5, 0, 0};
    const uint8_t min_trigger[] = {2, 5, 0, 0};
    const uint8_t bad_time_trigger[] = {1, 5, 0};
    const uint8_t unsupported_trigger[] = {10, 0, 0};
    // Descriptor Value Changed is optional and remains silent until its CCCD
    // enables indications.
    assert(!write_value(&server, config_handle, use_or, sizeof(use_or)));
    assert(!server.event_count);
    assert(gatt_server_set_cccd(&server, descriptor_value_changed_handle, 2));
    const uint8_t use_and[] = {0};
    assert(!write_value(&server, config_handle, use_and, sizeof(use_and)));
    assert(server.event_count == 1 && server.events[0].indication &&
           server.events[0].handle == descriptor_value_changed_handle);
    const uint8_t *changed = server.event_data + server.events[0].data_offset;
    assert(server.events[0].len == 4 && changed[0] == 0x05 &&
           changed[1] == 0 && changed[2] == 0x6e && changed[3] == 0x2a);
    server.event_count = server.event_used = 0;
    assert(gatt_ess_set_descriptor(&ess, 0, 0x290b, use_or,
        sizeof(use_or)));
    assert(server.event_count == 1 && server.events[0].indication);
    changed = server.event_data + server.events[0].data_offset;
    assert(server.events[0].len == 4 && changed[0] == 0x04 &&
           changed[1] == 0 && changed[2] == 0x6e && changed[3] == 0x2a);
    server.event_count = server.event_used = 0;

    // Check the remaining standardized descriptor flag mappings.
    assert(!write_value(&server, triggers[0], time_trigger,
        sizeof(time_trigger)));
    assert(server.event_count == 1);
    changed = server.event_data + server.events[0].data_offset;
    assert(changed[0] == 0x03 && changed[1] == 0);
    server.event_count = server.event_used = 0;
    const uint8_t measurement_descriptor_update[] = {1, 2, 3};
    assert(!write_value(&server, measurement_descriptor_handle,
        measurement_descriptor_update, sizeof(measurement_descriptor_update)));
    assert(server.event_count == 1);
    changed = server.event_data + server.events[0].data_offset;
    assert(changed[0] == 0x09 && changed[1] == 0);
    server.event_count = server.event_used = 0;
    const uint8_t description_update[] = {'R', 'o', 'o', 'm'};
    assert(!write_value(&server, user_description_handle,
        description_update, sizeof(description_update)));
    assert(server.event_count == 1);
    changed = server.event_data + server.events[0].data_offset;
    assert(changed[0] == 0x11 && changed[1] == 0);
    server.event_count = server.event_used = 0;

    // A server-side batch reports several descriptor types with one flags
    // field and one indication.
    const ble_gatt_ess_desc_update combined_updates[] = {
        {0x290b, use_or, sizeof(use_or)},
        {0x290d, time_trigger, sizeof(time_trigger)},
        {0x290c, measurement_descriptor_update,
         sizeof(measurement_descriptor_update)},
        {0x2901, description_update, sizeof(description_update)}
    };
    assert(gatt_ess_set_descriptors(&ess, 0, combined_updates,
        sizeof(combined_updates) / sizeof(combined_updates[0])));
    assert(server.event_count == 1 && server.events[0].indication);
    changed = server.event_data + server.events[0].data_offset;
    assert(changed[0] == 0x1e && changed[1] == 0 &&
           changed[2] == 0x6e && changed[3] == 0x2a);
    server.event_count = server.event_used = 0;

    assert(gatt_server_set_cccd(&server, descriptor_value_changed_handle, 0));
    assert(gatt_ess_set_descriptor(&ess, 0, 0x290b, use_and,
        sizeof(use_and)));
    assert(!server.event_count);
    assert(write_value(&server, config_handle, bad_logic,
        sizeof(bad_logic)) == ATT_ERR_VALUE_NOT_ALLOWED);
    assert(!write_value(&server, trigger_handle, time_trigger,
        sizeof(time_trigger)));
    assert(write_value(&server, trigger_handle, bad_time_trigger,
        sizeof(bad_time_trigger)) == ATT_ERR_INVALID_ATTRIBUTE_LENGTH);
    assert(write_value(&server, trigger_handle, unsupported_trigger,
        sizeof(unsupported_trigger)) == 0x81);

    // Trigger evaluation applies characteristic presentation formats, Boolean
    // configuration, fixed intervals, and the minimum interval gate.
    assert(gatt_server_set_cccd(&server, ess.measurement_handles[0], 1));
    const uint8_t temp_low[] = {20, 0}, temp_high[] = {40, 0};
    assert(!write_value(&server, config_handle, (const uint8_t[]){0}, 1));
    assert(!write_value(&server, triggers[0], less_than, sizeof(less_than)));
    assert(!write_value(&server, triggers[1], time_trigger,
        sizeof(time_trigger)));
    assert(!write_value(&server, triggers[2], min_trigger,
        sizeof(min_trigger)));
    assert(gatt_ess_send_at(&server, &ess, 0, temp_low, sizeof(temp_low),
        0, 100));
    server.event_count = server.event_used = 0;
    assert(!gatt_ess_send_at(&server, &ess, 0, temp_high, sizeof(temp_high),
        0, 104));
    assert(!server.event_count);
    assert(!write_value(&server, config_handle, use_or, sizeof(use_or)));
    assert(!gatt_ess_send_at(&server, &ess, 0, temp_low, sizeof(temp_low),
        0, 104));
    assert(!server.event_count); // The minimum interval still blocks it.
    assert(gatt_ess_send_at(&server, &ess, 0, temp_high, sizeof(temp_high),
        0, 105));
    assert(server.event_count == 1);

    // FTMS applies its standardized start/stop state rules and completes
    // synchronous machine actions through the application callback.
    gatt_server_init(&server, 23);
    ble_gatt_ftms_controlled_service ftms = {0};
    const uint8_t ftms_features[8] = {0, 0, 0, 0, 0x08, 0, 0, 0};
    const uint8_t power_range[6] = {0, 0, 100, 0, 1, 0};
    ble_gatt_ftms_options ftms_options = {
        .supported_ranges = {[4] = power_range},
        .training_status = (const uint8_t[]){0x02},
        .training_status_len = 1,
        .training_status_capacity = 1,
        .fitness_machine_status_capacity = 20,
        .control_point_capacity = 20
    };
    assert(gatt_ftms_add_full(&server, &ftms, 0x2ad2,
        ftms_features, 20, &ftms_options));
    ble_gatt_attribute *ftms_cccd = gatt_server_find(&server,
        ftms.control_point_cccd_handle);
    assert(ftms_cccd);
    ftms_cccd->cccd = 2;
    ble_gatt_attribute *ftms_status_cccd = gatt_server_find(&server,
        ftms.fitness_machine_status_cccd_handle);
    assert(ftms_status_cccd);
    ftms_status_cccd->cccd = 1;
    ble_gatt_attribute *ftms_training_cccd = gatt_server_find(&server,
        ftms.training_status_cccd_handle);
    assert(ftms_training_cccd);
    ftms_training_cccd->cccd = 1;
    ftms_action_count = 0;
    gatt_ftms_set_control_handler(&ftms, ftms_action, NULL);

    const uint8_t request_control[] = {GATT_FTMS_OP_REQUEST_CONTROL};
    const uint8_t set_power[] = {GATT_FTMS_OP_SET_POWER, 50, 0};
    const uint8_t start[] = {GATT_FTMS_OP_START_RESUME};
    const uint8_t stop[] = {GATT_FTMS_OP_STOP_PAUSE, 1};
    const uint8_t pause[] = {GATT_FTMS_OP_STOP_PAUSE, 2};
    const uint8_t reset[] = {GATT_FTMS_OP_RESET};
    ftms_write(&server, &ftms, request_control, sizeof(request_control),
        GATT_FTMS_RESPONSE_SUCCESS);
    assert(ftms.control_granted);
    ftms_write(&server, &ftms, set_power, sizeof(set_power),
        GATT_FTMS_RESPONSE_SUCCESS);
    ftms_write(&server, &ftms, start, sizeof(start), GATT_FTMS_RESPONSE_SUCCESS);
    assert(ftms.activity_state == GATT_FTMS_STATE_RUNNING);
    uint8_t actions = ftms_action_count;
    ftms_write(&server, &ftms, start, sizeof(start), GATT_FTMS_RESPONSE_FAILED);
    assert(ftms_action_count == actions);
    ftms_write(&server, &ftms, pause, sizeof(pause), GATT_FTMS_RESPONSE_SUCCESS);
    assert(ftms.activity_state == GATT_FTMS_STATE_PAUSED);
    ftms_write(&server, &ftms, pause, sizeof(pause), GATT_FTMS_RESPONSE_FAILED);
    ftms_write(&server, &ftms, start, sizeof(start), GATT_FTMS_RESPONSE_SUCCESS);
    ftms_write(&server, &ftms, stop, sizeof(stop), GATT_FTMS_RESPONSE_SUCCESS);
    assert(ftms.activity_state == GATT_FTMS_STATE_STOPPED);
    ftms_write(&server, &ftms, stop, sizeof(stop), GATT_FTMS_RESPONSE_FAILED);
    ftms_write(&server, &ftms, reset, sizeof(reset), GATT_FTMS_RESPONSE_SUCCESS);
    assert(!ftms.control_granted &&
           ftms.activity_state == GATT_FTMS_STATE_STOPPED);
    ble_gatt_attribute *ftms_training = gatt_server_find(&server,
        ftms.training_status_handle);
    assert(ftms_training && ftms_training->value_len == 1 &&
           *gatt_attribute_value(&server, ftms_training) == 0x01);
    gatt_ftms_set_control_handler(&ftms, ftms_defer, NULL);
    ftms_write(&server, &ftms, request_control, sizeof(request_control),
        GATT_FTMS_CONTROL_DEFERRED);
    assert(gatt_ftms_complete_control_point(&server, &ftms,
        GATT_FTMS_RESPONSE_SUCCESS, NULL, 0));
    assert(ftms.control_granted && !ftms.control_point_pending);
    server.event_count = 0;
    server.event_used = 0;

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
