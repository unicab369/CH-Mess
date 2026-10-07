#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../ble_gatt_server.h"

static uint8_t dynamic_value[] = {0x31, 0x32, 0x33};
static int read_calls, write_calls;

static uint8_t dynamic_read(void *context, uint16_t offset, uint8_t *out,
                            uint16_t *inout_len) {
    const uint8_t *value = context;
    if (offset > sizeof(dynamic_value)) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    uint16_t len = (uint16_t)(sizeof(dynamic_value) - offset);
    if (len > *inout_len) len = *inout_len;
    memcpy(out, value + offset, len);
    *inout_len = len;
    read_calls++;
    return 0;
}

static uint8_t dynamic_write(void *context, uint16_t offset,
                             const uint8_t *value, uint16_t len,
                             uint8_t command) {
    (void)context;
    assert(offset == 0 && !command);
    if (len != 1) return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    dynamic_value[0] = value[0];
    write_calls++;
    return 0;
}

static ble_gatt_uuid uuid16(uint16_t value) {
    ble_gatt_uuid uuid = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
    return uuid;
}

static void bluetooth_uuid128(uint16_t value, uint8_t out[16]) {
    static const uint8_t base[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(out, base, sizeof(base));
    out[14] = (uint8_t)value;
    out[15] = (uint8_t)(value >> 8);
}

static int att(ble_gatt_server *server, const uint8_t *request, uint16_t len,
               uint8_t *response, uint16_t *response_len) {
    return ble_gatt_server_att(server, request, len, response, 517,
                               response_len);
}

typedef struct {
    uint8_t visible[4], staged[4], touched[4];
    unsigned commits, cancels;
} transaction_state;

typedef struct {
    uint8_t value[22];
    uint8_t over_report;
} bounded_read_state;

static uint8_t bounded_read(void *context, uint16_t offset, uint8_t *out,
                            uint16_t *inout_len) {
    bounded_read_state *state = context;
    if (offset > sizeof(state->value)) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (state->over_report) {
        *inout_len = (uint16_t)(*inout_len + 1);
        return 0;
    }
    uint16_t available = (uint16_t)(sizeof(state->value) - offset);
    if (*inout_len > available) *inout_len = available;
    if (*inout_len) memcpy(out, state->value + offset, *inout_len);
    return 0;
}

static uint8_t oversized_dynamic_read(void *context, uint16_t offset,
                                      uint8_t *out, uint16_t *inout_len) {
    const uint8_t *value = context;
    const uint16_t value_len = BLE_GATT_ATT_VALUE_MAX + 1u;
    if (offset > value_len) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    uint16_t count = value_len - offset;
    if (count > *inout_len) count = *inout_len;
    if (count) memcpy(out, value + offset, count);
    *inout_len = count;
    return 0;
}

static uint8_t transaction_prepare(void *context, uint16_t offset,
                                  const uint8_t *value, uint16_t len) {
    transaction_state *state = context;
    if ((len && value[0] == 0xee) || offset > sizeof(state->staged) ||
        len > sizeof(state->staged) - offset)
        return BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED;
    for (uint16_t i = 0; i < len; i++) {
        state->staged[offset + i] = value[i];
        state->touched[offset + i] = 1;
    }
    return 0;
}

static void transaction_execute(void *context, uint8_t commit) {
    transaction_state *state = context;
    if (commit) {
        for (unsigned i = 0; i < sizeof(state->visible); i++)
            if (state->touched[i]) state->visible[i] = state->staged[i];
        state->commits++;
    } else {
        state->cancels++;
    }
    memset(state->touched, 0, sizeof(state->touched));
}

static void test_database_registration_and_handles(void) {
    ble_gatt_server server;
    assert(sizeof(server) <= 8192);
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid level_uuid = uuid16(0x2a19);
    ble_gatt_uuid custom_uuid = uuid16(0xfff1);
    uint16_t service, declaration, level, cccd, custom_decl, custom;
    const uint8_t level_value = 75;
    const uint8_t custom_value[] = {0x10};

    assert(!ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ, 0, NULL, 0, 0, NULL, NULL, NULL, NULL, NULL));
    assert(!ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_WRITE, NULL, 0, 0, NULL, NULL,
        NULL, NULL, NULL));
    assert(!ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE_SIGNED,
        NULL, 0, 0, NULL, NULL, NULL, NULL, NULL));
    assert(!ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_AUTH_SIGNED_WRITE,
        BLE_GATT_PERM_WRITE | BLE_GATT_PERM_WRITE_SIGNED,
        NULL, 0, 0, NULL, NULL, NULL, NULL, NULL));

    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(service == 1);
    assert(ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, BLE_GATT_PERM_READ,
        &level_value, 1, 1, NULL, NULL, NULL, &declaration, &level));
    assert(declaration == 2 && level == 3);
    ble_gatt_uuid cccd_uuid = uuid16(0x2902);
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, NULL, 0, 0,
        NULL, NULL, NULL, &cccd));
    assert(cccd == 4);
    assert(server.attributes[3].flags & BLE_GATT_ATTRIBUTE_CCCD);
    assert(server.attributes[3].properties & BLE_GATT_PROP_NOTIFY);
    assert(ble_gatt_server_add_characteristic(&server, &custom_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, custom_value, 1, 4,
        NULL, NULL, NULL, &custom_decl, &custom));
    assert(custom_decl == 5 && custom == 6);
    assert(server.count == 6 && server.next_handle == 7);

    ble_gatt_uuid wide_service = {16, {0}};
    wide_service.value[0] = 0x01;
    uint16_t wide_handle;
    assert(ble_gatt_server_add_service(&server, &wide_service, 1,
                                       &wide_handle));
    assert(wide_handle == 7);
    ble_gatt_server_seal_database(&server);
    assert(!ble_gatt_server_add_service(&server, &service_uuid, 1, NULL));
}

static void test_database_seal_requires_property_descriptors(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid characteristic_uuid = uuid16(0xfff9);
    ble_gatt_uuid cccd_uuid = uuid16(0x2902);
    uint16_t service, declaration, value, descriptor;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &characteristic_uuid,
        BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL,
        &declaration, &value));
    assert(!ble_gatt_server_seal_database(&server));
    assert(!server.database_sealed);
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &descriptor));
    assert(!ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, NULL));
    assert(ble_gatt_server_seal_database(&server));
    assert(server.database_sealed);

    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &characteristic_uuid,
        BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL,
        &declaration, &value));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid,
        BLE_GATT_PERM_READ_AUTHORIZED, NULL, 0, 0, NULL, NULL, NULL,
        &descriptor));
    assert(!ble_gatt_server_seal_database(&server));

    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid extended_uuid = uuid16(0xfffa);
    ble_gatt_uuid extended_descriptor_uuid = uuid16(0x2900);
    const uint8_t extended_properties[] = {1, 0};
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_EXTENDED, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(!ble_gatt_server_seal_database(&server));
    assert(ble_gatt_server_add_attribute(&server, &extended_descriptor_uuid,
        BLE_GATT_PERM_READ, extended_properties, sizeof(extended_properties),
        sizeof(extended_properties), NULL, NULL, NULL, &descriptor));
    assert(ble_gatt_server_seal_database(&server));

    const uint8_t short_extended_properties[] = {1};
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_EXTENDED, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_attribute(&server, &extended_descriptor_uuid,
        BLE_GATT_PERM_READ, short_extended_properties,
        sizeof(short_extended_properties), sizeof(short_extended_properties),
        NULL, NULL, NULL, &descriptor));
    assert(!ble_gatt_server_seal_database(&server));

    const uint8_t reserved_extended_properties[] = {4, 0};
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_EXTENDED, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_attribute(&server, &extended_descriptor_uuid,
        BLE_GATT_PERM_READ, reserved_extended_properties,
        sizeof(reserved_extended_properties), sizeof(reserved_extended_properties),
        NULL, NULL, NULL, &descriptor));
    assert(!ble_gatt_server_seal_database(&server));

    ble_gatt_uuid broadcast_descriptor_uuid = uuid16(0x2903);
    const uint8_t enabled_broadcast_configuration[] = {1, 0};
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_BROADCAST, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_attribute(&server, &broadcast_descriptor_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        enabled_broadcast_configuration, sizeof(enabled_broadcast_configuration),
        sizeof(enabled_broadcast_configuration), NULL, NULL, NULL, &descriptor));
    assert(ble_gatt_server_seal_database(&server));

    const uint8_t reserved_broadcast_configuration[] = {2, 0};
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_BROADCAST, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_attribute(&server, &broadcast_descriptor_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        reserved_broadcast_configuration,
        sizeof(reserved_broadcast_configuration),
        sizeof(reserved_broadcast_configuration), NULL, NULL, NULL,
        &descriptor));
    assert(!ble_gatt_server_seal_database(&server));

    const uint8_t short_broadcast_configuration[] = {0};
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_BROADCAST, BLE_GATT_PERM_READ,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_attribute(&server, &broadcast_descriptor_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        short_broadcast_configuration,
        sizeof(short_broadcast_configuration),
        sizeof(short_broadcast_configuration), NULL, NULL, NULL,
        &descriptor));
    assert(!ble_gatt_server_seal_database(&server));

    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &extended_uuid,
        BLE_GATT_PROP_AUTH_SIGNED_WRITE, BLE_GATT_PERM_WRITE_SIGNED,
        NULL, 0, 0, NULL, NULL, NULL, &declaration, &value));
    ble_gatt_server_find(&server, value)->permissions |= BLE_GATT_PERM_WRITE;
    assert(!ble_gatt_server_seal_database(&server));
}

static void test_included_service_discovery(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 100);
    ble_gatt_uuid battery = uuid16(0x180f);
    ble_gatt_uuid aggregate = uuid16(0x1812);
    uint16_t battery_handle, declaration, value_handle, aggregate_handle;
    const uint8_t value = 42;
    assert(ble_gatt_server_add_service(&server, &battery, 1,
                                       &battery_handle));
    assert(ble_gatt_server_add_characteristic(&server, &battery,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, &value, 1, 1, NULL, NULL,
        NULL, &declaration, &value_handle));
    assert(ble_gatt_server_add_service(&server, &aggregate, 1,
                                       &aggregate_handle));
    uint16_t include_handle;
    assert(ble_gatt_server_add_included_service(&server, battery_handle,
                                               &include_handle));
    assert(include_handle == aggregate_handle + 1);

    const uint8_t request[] = {0x08, 1, 0, 0xff, 0xff, 0x02, 0x28};
    uint8_t response[64];
    uint16_t response_len;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 10 && response[0] == 0x09 && response[1] == 8);
    assert(ble_gatt_server_u16(response + 2) == include_handle);
    assert(ble_gatt_server_u16(response + 4) == battery_handle);
    assert(ble_gatt_server_u16(response + 6) == value_handle);
    assert(ble_gatt_server_u16(response + 8) == 0x180f);
}

typedef struct { uint32_t last_counter, accepted; } signed_state;

static int verify_signed(void *context, const uint8_t *pdu, uint16_t len,
                         const uint8_t signature[12]) {
    signed_state *state = context;
    uint32_t counter = (uint32_t)signature[8] |
        (uint32_t)signature[9] << 8 | (uint32_t)signature[10] << 16 |
        (uint32_t)signature[11] << 24;
    if (len != 4 || pdu[0] != 0xd2 || signature[0] != 0xa5 ||
        counter <= state->last_counter) return 0;
    state->last_counter = counter;
    state->accepted++;
    return 1;
}

static void test_signed_write_command(void) {
    ble_gatt_server server;
    signed_state state = {0};
    ble_gatt_server_init(&server, 23);
    ble_gatt_server_set_signed_write_verifier(&server, verify_signed, &state);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid value_uuid = uuid16(0xfff2);
    uint16_t service, declaration, handle;
    uint8_t initial = 0;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_AUTH_SIGNED_WRITE, BLE_GATT_PERM_WRITE_SIGNED,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &handle));

    uint8_t request[16] = {0xd2, (uint8_t)handle, (uint8_t)(handle >> 8), 0x77};
    request[4] = 0xa5;
    request[12] = 1;
    uint8_t response[23];
    uint16_t response_len = 0;
    assert(ble_gatt_server_att(&server, request, sizeof(request), response,
        sizeof(response), &response_len) == 0);
    assert(state.accepted == 1);

    uint8_t oversized_signed[24] = {0xd2, (uint8_t)handle,
                                     (uint8_t)(handle >> 8)};
    response_len = 0xffff;
    assert(ble_gatt_server_att(&server, oversized_signed,
        sizeof(oversized_signed), response, sizeof(response),
        &response_len) == 0);
    assert(response_len == 0 && state.accepted == 1);

    ble_gatt_attribute *value = ble_gatt_server_find(&server, handle);
    assert(value && ble_gatt_attribute_value(&server, value)[0] == 0x77);

    // A previously accepted sign counter cannot replay a write.
    assert(ble_gatt_server_att(&server, request, sizeof(request), response,
        sizeof(response), &response_len) == 0);
    assert(state.accepted == 1 &&
           ble_gatt_attribute_value(&server, value)[0] == 0x77);

    request[12] = 2;
    ble_gatt_server_set_security(&server, 1, 0);
    assert(ble_gatt_server_att(&server, request, sizeof(request), response,
        sizeof(response), &response_len) == 0);
    assert(state.accepted == 1);
}

static int authorize_access(void *context, uint16_t handle, uint8_t write) {
    (void)handle;
    (void)write;
    return *(uint8_t *)context;
}

static void test_application_authorization(void) {
    ble_gatt_server server;
    uint8_t allow = 0, initial = 0x21;
    ble_gatt_server_init(&server, 23);
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid value_uuid = uuid16(0xfff3);
    uint16_t service, declaration, handle;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_READ_AUTHORIZED | BLE_GATT_PERM_WRITE_AUTHORIZED,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &handle));

    uint8_t response[23];
    uint16_t response_len;
    uint8_t read[] = {0x0a, (uint8_t)handle, (uint8_t)(handle >> 8)};
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
    uint8_t write[] = {0x12, (uint8_t)handle, (uint8_t)(handle >> 8), 0x42};
    assert(att(&server, write, sizeof(write), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);

    allow = 1;
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[0] == 0x0b && response[1] == initial);
    assert(att(&server, write, sizeof(write), response, &response_len) == 1);
    assert(response[0] == 0x13);
    ble_gatt_attribute *attribute = ble_gatt_server_find(&server, handle);
    assert(attribute && ble_gatt_attribute_value(&server, attribute)[0] == 0x42);
}

static void test_security_and_authorization_check_order(void) {
    ble_gatt_server server;
    uint8_t allow = 0, initial = 0x33;
    ble_gatt_server_init(&server, 23);
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid value_uuid = uuid16(0xfff4);
    uint16_t service, declaration, handle;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_READ,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHENTICATED |
            BLE_GATT_PERM_READ_AUTHORIZED,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &handle));
    uint8_t response[23];
    uint16_t response_len;
    const uint8_t read[] = {0x0a, (uint8_t)handle, (uint8_t)(handle >> 8)};

    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
    ble_gatt_server_set_security(&server, 1, 0);
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
    ble_gatt_server_set_security(&server, 1, 1);
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
    allow = 1;
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[0] == 0x0b && response[1] == initial);

    ble_gatt_server_init(&server, 23);
    allow = 0;
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);
    ble_gatt_uuid authz_uuid = uuid16(0xfff5);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &authz_uuid,
        BLE_GATT_PROP_READ,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHORIZED,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &handle));
    const uint8_t authz_read[] = {0x0a, (uint8_t)handle,
                                  (uint8_t)(handle >> 8)};
    assert(att(&server, authz_read, sizeof(authz_read), response,
               &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
    allow = 1;
    assert(att(&server, authz_read, sizeof(authz_read), response,
               &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION);
    ble_gatt_server_set_security(&server, 1, 0);
    assert(att(&server, authz_read, sizeof(authz_read), response,
               &response_len) == 1);
    assert(response[0] == 0x0b && response[1] == initial);

    // Prepare Write applies the same security checks before property and
    // value validation, including for a structure changed after registration.
    ble_gatt_server_init(&server, 23);
    allow = 0;
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);
    ble_gatt_uuid secure_write_uuid = uuid16(0xfff7);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &secure_write_uuid,
        BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_WRITE_AUTHENTICATED | BLE_GATT_PERM_WRITE_AUTHORIZED,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &handle));
    ble_gatt_attribute *secure_write = ble_gatt_server_find(&server, handle);
    assert(secure_write);
    secure_write->properties = BLE_GATT_PROP_READ;
    const uint8_t prepare[] = {0x16, (uint8_t)handle,
        (uint8_t)(handle >> 8), 0, 0, 0x55};
    assert(att(&server, prepare, sizeof(prepare), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
    ble_gatt_server_set_security(&server, 1, 1);
    assert(att(&server, prepare, sizeof(prepare), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
}

static void test_minimum_encryption_key_size(void) {
    ble_gatt_server server;
    uint8_t value = 0x39;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid uuid = uuid16(0xfff6);
    uint16_t handle;
    assert(ble_gatt_server_add_attribute(&server, &uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, &value, 1, 1,
        NULL, NULL, NULL, &handle));
    assert(ble_gatt_server_set_min_key_size(&server, handle, 12));
    assert(!ble_gatt_server_set_min_key_size(&server, handle, 6));

    uint8_t response[32];
    uint16_t response_len;
    uint8_t read[] = {0x0a, (uint8_t)handle, (uint8_t)(handle >> 8)};
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION);

    ble_gatt_server_set_security(&server, 1, 0);
    ble_gatt_server_set_encryption_key_size(&server, 10);
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT);
    uint8_t write[] = {0x12, (uint8_t)handle, (uint8_t)(handle >> 8), 0x44};
    assert(att(&server, write, sizeof(write), response, &response_len) == 1);
    assert(response[4] == BLE_GATT_ATT_ERR_ENCRYPTION_KEY_SIZE_TOO_SHORT);

    ble_gatt_server_set_encryption_key_size(&server, 12);
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[0] == 0x0b && response[1] == value);
    assert(att(&server, write, sizeof(write), response, &response_len) == 1);
    ble_gatt_attribute *attribute = ble_gatt_server_find(&server, handle);
    assert(response[0] == 0x13 && attribute &&
           ble_gatt_attribute_value(&server, attribute)[0] == 0x44);

    ble_gatt_server_seal_database(&server);
    assert(!ble_gatt_server_set_min_key_size(&server, handle, 13));
}

static void test_fixed_and_variable_length_writes(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid fixed_uuid = uuid16(0xfff7);
    ble_gatt_uuid variable_uuid = uuid16(0xfff8);
    const uint8_t fixed_initial[] = {1, 2, 3, 4};
    const uint8_t variable_initial[] = {5, 6, 7, 8};
    uint16_t fixed_handle, variable_handle;
    assert(ble_gatt_server_add_attribute(&server, &fixed_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, fixed_initial, 4, 6,
        NULL, NULL, NULL, &fixed_handle));
    assert(ble_gatt_server_add_attribute(&server, &variable_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, variable_initial, 4, 8,
        NULL, NULL, NULL, &variable_handle));
    assert(ble_gatt_server_set_variable_length(&server, fixed_handle, 0));
    assert(ble_gatt_server_set_variable_length(&server, variable_handle, 1));

    uint8_t response[32];
    uint16_t response_len;
    uint8_t fixed_write[] = {0x12, (uint8_t)fixed_handle,
        (uint8_t)(fixed_handle >> 8), 0xa1, 0xa2};
    assert(att(&server, fixed_write, sizeof(fixed_write), response,
               &response_len) == 1);
    ble_gatt_attribute *fixed = ble_gatt_server_find(&server, fixed_handle);
    const uint8_t *fixed_value = ble_gatt_attribute_value(&server, fixed);
    assert(response[0] == 0x13 && fixed->value_len == 4 &&
           fixed_value[0] == 0xa1 && fixed_value[1] == 0xa2 &&
           fixed_value[2] == 3 && fixed_value[3] == 4);
    uint8_t fixed_too_long[] = {0x12, (uint8_t)fixed_handle,
        (uint8_t)(fixed_handle >> 8), 1, 2, 3, 4, 5};
    assert(att(&server, fixed_too_long, sizeof(fixed_too_long), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH);

    // ATT accepts Prepare Write fragments first and validates static value
    // bounds when Execute Write is requested.
    uint8_t prepared_offset[] = {0x16, (uint8_t)fixed_handle,
        (uint8_t)(fixed_handle >> 8), 5, 0, 0xaa};
    assert(att(&server, prepared_offset, sizeof(prepared_offset), response,
               &response_len) == 1 && response[0] == 0x17);
    const uint8_t execute[] = {0x18, 1};
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           ble_gatt_server_u16(response + 2) == fixed_handle &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_OFFSET);

    uint8_t prepared_too_long[] = {0x16, (uint8_t)fixed_handle,
        (uint8_t)(fixed_handle >> 8), 3, 0, 0xaa, 0xbb};
    assert(att(&server, prepared_too_long, sizeof(prepared_too_long), response,
               &response_len) == 1 && response[0] == 0x17);
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           ble_gatt_server_u16(response + 2) == fixed_handle &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH);
    assert(!server.prepare_count);

    uint8_t variable_write[] = {0x12, (uint8_t)variable_handle,
        (uint8_t)(variable_handle >> 8), 0xb1, 0xb2};
    assert(att(&server, variable_write, sizeof(variable_write), response,
               &response_len) == 1);
    ble_gatt_attribute *variable = ble_gatt_server_find(&server,
                                                        variable_handle);
    const uint8_t *variable_value = ble_gatt_attribute_value(&server, variable);
    assert(response[0] == 0x13 && variable->value_len == 2 &&
           variable_value[0] == 0xb1 && variable_value[1] == 0xb2);
    uint8_t truncate[] = {0x12, (uint8_t)variable_handle,
        (uint8_t)(variable_handle >> 8)};
    assert(att(&server, truncate, sizeof(truncate), response,
               &response_len) == 1);
    assert(response[0] == 0x13 && variable->value_len == 0);
}

static void test_maximum_att_attribute_value(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, BLE_GATT_SERVER_MTU_MAX);
    ble_gatt_uuid uuid = uuid16(0xfff9);
    uint8_t value[512];
    for (uint16_t i = 0; i < sizeof(value); i++) value[i] = (uint8_t)i;
    uint16_t handle;
    assert(ble_gatt_server_add_attribute(&server, &uuid, BLE_GATT_PERM_READ,
        value, sizeof(value), sizeof(value), NULL, NULL, NULL, &handle));
    const uint8_t exchange_mtu[] = {0x02, 0x05, 0x02};
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len;
    assert(att(&server, exchange_mtu, sizeof(exchange_mtu), response,
               &response_len) == 1);
    assert(response[0] == 0x03 && ble_gatt_server_u16(response + 1) == 517);
    uint8_t read[] = {0x0a, (uint8_t)handle, (uint8_t)(handle >> 8)};
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response_len == 513 && response[0] == 0x0b &&
           !memcmp(response + 1, value, sizeof(value)));
    server.mtu = 23;
    uint8_t read_multiple[] = {0x20, (uint8_t)handle,
        (uint8_t)(handle >> 8), (uint8_t)handle, (uint8_t)(handle >> 8)};
    assert(att(&server, read_multiple, sizeof(read_multiple), response,
               &response_len) == 1);
    assert(response_len == 23 && response[0] == 0x21 &&
           ble_gatt_server_u16(response + 1) == sizeof(value) &&
           !memcmp(response + 3, value, response_len - 3));
}

static void test_read_multiple_checks_all_permissions(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid first_uuid = uuid16(0xfffa), second_uuid = uuid16(0xfffb);
    uint8_t first_value[40] = {0}, second_value = 0x42, allow = 0;
    uint16_t first_handle, second_handle;
    assert(ble_gatt_server_add_attribute(&server, &first_uuid,
        BLE_GATT_PERM_READ, first_value, sizeof(first_value),
        sizeof(first_value), NULL, NULL, NULL, &first_handle));
    assert(ble_gatt_server_add_attribute(&server, &second_uuid,
        BLE_GATT_PERM_READ_AUTHORIZED, &second_value, 1, 1,
        NULL, NULL, NULL, &second_handle));
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);

    uint8_t request[] = {0x20, (uint8_t)first_handle,
        (uint8_t)(first_handle >> 8), (uint8_t)second_handle,
        (uint8_t)(second_handle >> 8)};
    uint8_t response[23];
    uint16_t response_len;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01 &&
           ble_gatt_server_u16(response + 2) == second_handle &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
}

static void test_fixed_read_multiple_checks_after_mtu_boundary(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid first_uuid = uuid16(0xfffc), second_uuid = uuid16(0xfffd);
    uint8_t first_value[22] = {0}, second_value = 0x42, allow = 0;
    uint16_t first_handle, second_handle;
    assert(ble_gatt_server_add_attribute(&server, &first_uuid,
        BLE_GATT_PERM_READ, first_value, sizeof(first_value),
        sizeof(first_value), NULL, NULL, NULL, &first_handle));
    assert(ble_gatt_server_add_attribute(&server, &second_uuid,
        BLE_GATT_PERM_READ_AUTHORIZED, &second_value, 1, 1,
        NULL, NULL, NULL, &second_handle));
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow);

    uint8_t request[] = {0x0e, (uint8_t)first_handle,
        (uint8_t)(first_handle >> 8), (uint8_t)second_handle,
        (uint8_t)(second_handle >> 8)};
    uint8_t response[23];
    uint16_t response_len;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01 &&
           response[1] == 0x0e &&
           ble_gatt_server_u16(response + 2) == second_handle &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);

    allow = 1;
    request[3] = 0xff;
    request[4] = 0x7f;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01 &&
           response[1] == 0x0e && ble_gatt_server_u16(response + 2) == 0x7fff &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_HANDLE);
}

static void test_response_capacity_and_large_find_information(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, BLE_GATT_SERVER_MTU_MAX);
    uint8_t guard[] = {0xa5, 0x5a};
    uint16_t response_len;
    const uint8_t exchange[] = {0x02, 64, 0};
    assert(ble_gatt_server_att(&server, exchange, sizeof(exchange), guard + 1,
        2, &response_len) == 0);
    assert(!server.mtu_exchanged && server.mtu == 23 && guard[1] == 0x5a);

    ble_gatt_uuid uuid = {16, {0}};
    for (uint8_t i = 0; i < BLE_GATT_SERVER_MAX_ATTRIBUTES; i++)
        assert(ble_gatt_server_add_attribute(&server, &uuid, BLE_GATT_PERM_READ,
            NULL, 0, 0, NULL, NULL, NULL, NULL));
    const uint8_t find[] = {0x04, 1, 0, 0xff, 0xff};
    server.mtu = BLE_GATT_SERVER_MTU_MAX;
    assert(ble_gatt_server_att(&server, find, sizeof(find), guard + 1, 0,
        &response_len) == 0);
    assert(guard[1] == 0x5a);
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    assert(att(&server, find, sizeof(find), response, &response_len) == 1);
    assert(response[0] == 0x05 && response[1] == 2 && response_len == 506);
    for (uint16_t i = 0; i < 28; i++)
        assert(ble_gatt_server_u16(response + 2 + i * 18) == i + 1);

    ble_gatt_server write_server;
    ble_gatt_server_init(&write_server, 23);
    ble_gatt_uuid write_uuid = uuid16(0xfffc);
    uint8_t initial = 0x11;
    uint16_t handle;
    assert(ble_gatt_server_add_attribute(&write_server, &write_uuid,
        BLE_GATT_PERM_WRITE, &initial, 1, 1, NULL, NULL, NULL, &handle));
    uint8_t write[] = {0x12, (uint8_t)handle, (uint8_t)(handle >> 8), 0x22};
    assert(ble_gatt_server_att(&write_server, write, sizeof(write), guard + 1,
        0, &response_len) == 0);
    assert(guard[1] == 0x5a && ble_gatt_attribute_value(&write_server,
        ble_gatt_server_find(&write_server, handle))[0] == initial);
}

static void test_att_mtu_and_reads(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 100);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid level_uuid = uuid16(0x2a19);
    uint16_t service, declaration, level;
    const uint8_t level_value = 75;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, &level_value, 1, 1,
        NULL, NULL, NULL, &declaration, &level));

    uint8_t response[517];
    uint16_t response_len;
    const uint8_t exchange[] = {0x02, 80, 0};
    assert(att(&server, exchange, sizeof(exchange), response, &response_len) == 1);
    assert(response_len == 3 && response[0] == 0x03);
    assert(ble_gatt_server_u16(response + 1) == 100);
    assert(server.mtu == 80);

    const uint8_t read_level[] = {0x0a, 3, 0};
    assert(att(&server, read_level, sizeof(read_level), response, &response_len) == 1);
    assert(response_len == 2 && response[0] == 0x0b && response[1] == 75);

    const uint8_t write_read_only[] = {0x12, 3, 0, 1};
    assert(att(&server, write_read_only, sizeof(write_read_only), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED);
    const uint8_t command_read_only[] = {0x52, 3, 0, 1};
    assert(att(&server, command_read_only, sizeof(command_read_only), response,
               &response_len) == 0);

    const uint8_t read_blob[] = {0x0c, 3, 0, 1, 0};
    assert(att(&server, read_blob, sizeof(read_blob), response, &response_len) == 1);
    assert(response_len == 1 && response[0] == 0x0d);

    const uint8_t read_missing[] = {0x0a, 0xff, 0};
    assert(att(&server, read_missing, sizeof(read_missing), response,
               &response_len) == 1);
    assert(response[0] == 0x01 && response[4] == BLE_GATT_ATT_ERR_INVALID_HANDLE);
}

static void test_read_callback_boundaries(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = uuid16(0x180f), value_uuid = uuid16(0xfff3);
    uint16_t service, value_handle;
    bounded_read_state state = {{0}, 0};
    for (uint8_t i = 0; i < sizeof(state.value); i++) state.value[i] = i;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_attribute(&server, &value_uuid,
        BLE_GATT_PERM_READ, NULL, 0, 0, bounded_read, NULL, &state,
        &value_handle));
    uint8_t response[517];
    uint16_t response_len;

    const uint8_t read[] = {0x0a, (uint8_t)value_handle, 0};
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response_len == 23 && response[0] == 0x0b);
    assert(!memcmp(response + 1, state.value, sizeof(state.value)));

    uint8_t at_end[] = {0x0c, (uint8_t)value_handle, 0, 22, 0};
    assert(att(&server, at_end, sizeof(at_end), response, &response_len) == 1);
    assert(response_len == 1 && response[0] == 0x0d);
    uint8_t past_end[] = {0x0c, (uint8_t)value_handle, 0, 23, 0};
    assert(att(&server, past_end, sizeof(past_end), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_OFFSET);
    const uint8_t malformed_blob[] = {0x0c, (uint8_t)value_handle, 0, 0};
    assert(att(&server, malformed_blob, sizeof(malformed_blob), response,
               &response_len) == 1);
    assert(response[0] == 0x01 && response[4] == BLE_GATT_ATT_ERR_INVALID_PDU);

    state.over_report = 1;
    assert(att(&server, read, sizeof(read), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH);
}

static void test_dynamic_read_enforces_att_value_limit(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, BLE_GATT_SERVER_MTU_MAX);
    ble_gatt_uuid uuid = uuid16(0xfff2);
    uint8_t oversized_value[BLE_GATT_ATT_VALUE_MAX + 1u] = {0};
    uint16_t handle, response_len;
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    assert(ble_gatt_server_add_attribute(&server, &uuid, BLE_GATT_PERM_READ,
        NULL, 0, 0, oversized_dynamic_read, NULL, oversized_value, &handle));

    const uint8_t exchange_mtu[] = {0x02, 0x05, 0x02};
    assert(att(&server, exchange_mtu, sizeof(exchange_mtu), response,
               &response_len) == 1);
    uint8_t read_blob[] = {0x0c, (uint8_t)handle,
                           (uint8_t)(handle >> 8), 1, 0};
    assert(att(&server, read_blob, sizeof(read_blob), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01 &&
           response[1] == 0x0c && ble_gatt_server_u16(response + 2) == handle &&
           response[4] == BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH);
}

static void test_discovery(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid level_uuid = uuid16(0x2a19);
    uint16_t service, declaration, level;
    const uint8_t level_value = 42;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &level_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, &level_value, 1, 1,
        NULL, NULL, NULL, &declaration, &level));
    uint8_t response[517];
    uint16_t response_len;

    const uint8_t groups[] = {0x10, 1, 0, 0xff, 0xff, 0, 0x28};
    assert(att(&server, groups, sizeof(groups), response, &response_len) == 1);
    assert(response_len == 8 && response[0] == 0x11 && response[1] == 6);
    assert(ble_gatt_server_u16(response + 2) == 1);
    assert(ble_gatt_server_u16(response + 4) == 3);
    assert(ble_gatt_server_u16(response + 6) == 0x180f);

    const uint8_t characteristics[] = {0x08, 1, 0, 3, 0, 3, 0x28};
    assert(att(&server, characteristics, sizeof(characteristics), response,
               &response_len) == 1);
    assert(response_len == 9 && response[0] == 0x09 && response[1] == 7);
    assert(ble_gatt_server_u16(response + 2) == declaration);
    assert(response[4] == BLE_GATT_PROP_READ);
    assert(ble_gatt_server_u16(response + 5) == level);
    assert(ble_gatt_server_u16(response + 7) == 0x2a19);

    const uint8_t read_by_uuid[] = {0x08, (uint8_t)level,
        (uint8_t)(level >> 8), (uint8_t)level, (uint8_t)(level >> 8),
        0x19, 0x2a};
    assert(att(&server, read_by_uuid, sizeof(read_by_uuid), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x09 && response[1] == 3 &&
           ble_gatt_server_u16(response + 2) == level && response[4] == 42);

    const uint8_t information[] = {0x04, 1, 0, 3, 0};
    assert(att(&server, information, sizeof(information), response,
               &response_len) == 1);
    assert(response[0] == 0x05 && response[1] == 1);
    assert(response_len == 14);

    const uint8_t find_service[] = {0x06, 1, 0, 3, 0, 0, 0x28, 0x0f, 0x18};
    assert(att(&server, find_service, sizeof(find_service), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x07);
    assert(ble_gatt_server_u16(response + 1) == 1);
    assert(ble_gatt_server_u16(response + 3) == 3);
}

static void test_128_bit_uuids(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = {16, {0}};
    ble_gatt_uuid char_uuid = {16, {0}};
    for (uint8_t i = 0; i < 16; i++) {
        service_uuid.value[i] = (uint8_t)(i + 1);
        char_uuid.value[i] = (uint8_t)(0xa0 + i);
    }
    uint16_t service, declaration, value;
    const uint8_t initial = 0x5a;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &char_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, &initial, 1, 1,
        NULL, NULL, NULL, &declaration, &value));
    uint8_t response[517];
    uint16_t response_len;

    uint8_t group_req[] = {0x10, 1, 0, 3, 0, 0, 0x28};
    assert(att(&server, group_req, sizeof(group_req), response,
               &response_len) == 1);
    assert(response_len == 22 && response[1] == 20);
    assert(!memcmp(response + 6, service_uuid.value, 16));

    uint8_t primary_uuid128[16];
    bluetooth_uuid128(0x2800, primary_uuid128);
    uint8_t group_req128[21] = {0x10, 1, 0, 3, 0};
    memcpy(group_req128 + 5, primary_uuid128, sizeof(primary_uuid128));
    assert(att(&server, group_req128, sizeof(group_req128), response,
               &response_len) == 1);
    assert(response_len == 22 && response[1] == 20);
    assert(!memcmp(response + 6, service_uuid.value, 16));

    uint8_t type_req[] = {0x08, 1, 0, 3, 0, 3, 0x28};
    assert(att(&server, type_req, sizeof(type_req), response,
               &response_len) == 1);
    assert(response_len == 23 && response[1] == 21);
    assert(ble_gatt_server_u16(response + 2) == declaration);
    assert(ble_gatt_server_u16(response + 5) == value);
    assert(!memcmp(response + 7, char_uuid.value, 16));

    uint8_t characteristic_uuid128[16];
    bluetooth_uuid128(0x2803, characteristic_uuid128);
    uint8_t type_req128[21] = {0x08, 1, 0, 3, 0};
    memcpy(type_req128 + 5, characteristic_uuid128,
           sizeof(characteristic_uuid128));
    assert(att(&server, type_req128, sizeof(type_req128), response,
               &response_len) == 1);
    assert(response_len == 23 && response[1] == 21);
    assert(ble_gatt_server_u16(response + 2) == declaration);

    uint8_t find_req[23] = {0x06, 1, 0, 3, 0, 0, 0x28};
    memcpy(find_req + 7, service_uuid.value, 16);
    assert(att(&server, find_req, sizeof(find_req), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x07);
    assert(ble_gatt_server_u16(response + 1) == service);
    assert(ble_gatt_server_u16(response + 3) == value);

    uint8_t assigned_uuid_bytes[16];
    bluetooth_uuid128(0xfff9, assigned_uuid_bytes);
    ble_gatt_uuid assigned_uuid128 = {16, {0}};
    memcpy(assigned_uuid128.value, assigned_uuid_bytes,
           sizeof(assigned_uuid_bytes));
    const uint8_t assigned_value = 0x6c;
    uint16_t assigned_handle;
    assert(ble_gatt_server_add_attribute(&server, &assigned_uuid128,
        BLE_GATT_PERM_READ, &assigned_value, 1, 1, NULL, NULL, NULL,
        &assigned_handle));
    uint8_t type_req16[] = {0x08, (uint8_t)assigned_handle,
        (uint8_t)(assigned_handle >> 8), (uint8_t)assigned_handle,
        (uint8_t)(assigned_handle >> 8), 0xf9, 0xff};
    assert(att(&server, type_req16, sizeof(type_req16), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x09 &&
           ble_gatt_server_u16(response + 2) == assigned_handle &&
           response[4] == assigned_value);
}

static void test_writes_cccd_and_permissions(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid notify_uuid = uuid16(0x2a19);
    ble_gatt_uuid secure_uuid = uuid16(0xfff2);
    uint16_t service, declaration, notify, cccd, secure_decl, secure;
    const uint8_t initial = 1;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &notify_uuid,
        BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL,
        &declaration, &notify));
    ble_gatt_uuid cccd_uuid = uuid16(0x2902);
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &cccd));
    assert(ble_gatt_server_add_characteristic(&server, &secure_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_WRITE_ENCRYPTED |
            BLE_GATT_PERM_READ_AUTHENTICATED,
        &initial, 1, 1, NULL, NULL, NULL, &secure_decl, &secure));

    uint8_t response[517];
    uint16_t response_len;
    const uint8_t enable_cccd[] = {0x12, (uint8_t)cccd, 0, 1, 0};
    assert(att(&server, enable_cccd, sizeof(enable_cccd), response,
               &response_len) == 1);
    assert(response[0] == 0x13);
    assert(server.attributes[cccd - 1].cccd == 1);

    const uint8_t invalid_cccd[] = {0x12, (uint8_t)cccd, 0, 2, 0};
    assert(att(&server, invalid_cccd, sizeof(invalid_cccd), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED);

    const uint8_t read_secure[] = {0x0a, (uint8_t)secure, 0};
    assert(att(&server, read_secure, sizeof(read_secure), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
    ble_gatt_server_set_security(&server, 1, 0);
    assert(att(&server, read_secure, sizeof(read_secure), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
    ble_gatt_server_set_security(&server, 1, 1);
    assert(att(&server, read_secure, sizeof(read_secure), response,
               &response_len) == 1);
    assert(response[0] == 0x0b && response[1] == initial);

    ble_gatt_server_link_reset(&server);
    assert(server.attributes[cccd - 1].cccd == 0);
    assert(server.mtu == 23 && !server.encrypted);
}

static void test_callbacks_and_invalid_requests(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid dynamic_uuid = uuid16(0xfff3);
    uint16_t service, value;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_attribute(&server, &dynamic_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, NULL, 0, 8,
        dynamic_read, dynamic_write, dynamic_value, &value));
    uint8_t response[517];
    uint16_t response_len;
    uint8_t read_req[] = {0x0a, (uint8_t)value, 0};
    assert(att(&server, read_req, sizeof(read_req), response, &response_len) == 1);
    assert(response_len == 4 && response[1] == 0x31 && read_calls == 1);
    uint8_t blob_req[] = {0x0c, (uint8_t)value, 0, 2, 0};
    assert(att(&server, blob_req, sizeof(blob_req), response, &response_len) == 1);
    assert(response_len == 2 && response[0] == 0x0d && response[1] == 0x33);

    uint8_t write_req[] = {0x12, (uint8_t)value, 0, 0x55};
    assert(att(&server, write_req, sizeof(write_req), response, &response_len) == 1);
    assert(response[0] == 0x13 && write_calls == 1 && dynamic_value[0] == 0x55);

    const uint8_t malformed[] = {0x0a, 1};
    assert(att(&server, malformed, sizeof(malformed), response, &response_len) == 1);
    assert(response[0] == 0x01 && response[4] == BLE_GATT_ATT_ERR_INVALID_PDU);

    const uint8_t unsupported[] = {0xd2, 1, 0, 0};
    assert(att(&server, unsupported, sizeof(unsupported), response,
               &response_len) == 0);

    const uint8_t unsupported_request[] = {0x22, 0x34, 0x12};
    assert(att(&server, unsupported_request, sizeof(unsupported_request),
               response, &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01 &&
           response[1] == unsupported_request[0] && response[2] == 0 &&
           response[3] == 0 &&
           response[4] == BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED);

    const uint8_t unsupported_command[] = {0x62, 0x34, 0x12};
    assert(att(&server, unsupported_command, sizeof(unsupported_command),
               response, &response_len) == 0);
    assert(response_len == 0);
}

static void test_read_by_type_mtu_limit(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid uuid = uuid16(0xfff8);
    uint8_t value[40];
    for (uint8_t i = 0; i < sizeof(value); i++) value[i] = i;
    uint16_t handle;
    assert(ble_gatt_server_add_attribute(&server, &uuid, BLE_GATT_PERM_READ,
        value, sizeof(value), sizeof(value), NULL, NULL, NULL, &handle));
    uint8_t request[] = {0x08, 1, 0, 1, 0, 0xf8, 0xff};
    uint8_t response[517];
    uint16_t response_len;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 23 && response[1] == 21);
    assert(ble_gatt_server_u16(response + 2) == handle);
    assert(response[4] == 0 && response[22] == 18);
}

static void test_find_by_type_value_and_unsupported_group(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid type = uuid16(0xfff5);
    const uint8_t stored[] = {0xa1, 0xb2, 0xc3};
    uint16_t handle;
    assert(ble_gatt_server_add_attribute(&server, &type, BLE_GATT_PERM_READ,
        stored, sizeof(stored), sizeof(stored), NULL, NULL, NULL, &handle));

    uint8_t find[] = {0x06, 1, 0, 0xff, 0xff, 0xf5, 0xff,
                      0xa1, 0xb2, 0xc3};
    uint8_t response[64];
    uint16_t response_len;
    assert(att(&server, find, sizeof(find), response, &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x07);
    assert(ble_gatt_server_u16(response + 1) == handle);
    assert(ble_gatt_server_u16(response + 3) == handle);

    const uint8_t unsupported_group[] = {0x10, 1, 0, 0xff, 0xff,
                                         0x02, 0x29};
    assert(att(&server, unsupported_group, sizeof(unsupported_group), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x01);
    assert(response[4] == BLE_GATT_ATT_ERR_UNSUPPORTED_GROUP_TYPE);

    uint8_t custom_2800[16];
    bluetooth_uuid128(0x2800, custom_2800);
    custom_2800[0] ^= 1;
    uint8_t custom_group[21] = {0x10, 1, 0, 0xff, 0xff};
    memcpy(custom_group + 5, custom_2800, sizeof(custom_2800));
    assert(att(&server, custom_group, sizeof(custom_group), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_UNSUPPORTED_GROUP_TYPE);
}

static void test_write_command(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid value_uuid = uuid16(0xfff4);
    uint16_t service, declaration, handle;
    const uint8_t initial = 0;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_WRITE_NO_RSP, BLE_GATT_PERM_WRITE, &initial, 1, 1,
        NULL, NULL, NULL, &declaration, &handle));
    uint8_t request[] = {0x52, (uint8_t)handle, 0, 0xa5};
    uint8_t response[517];
    uint16_t response_len;
    assert(att(&server, request, sizeof(request), response, &response_len) == 0);
    assert(ble_gatt_attribute_value(&server,
        &server.attributes[handle - 1])[0] == 0xa5);

    server.mtu = 23;
    uint8_t oversized_command[24] = {0x52, (uint8_t)handle,
                                     (uint8_t)(handle >> 8)};
    memset(oversized_command + 3, 0x5a, sizeof(oversized_command) - 3);
    response_len = 0xffff;
    assert(ble_gatt_server_att(&server, oversized_command,
        sizeof(oversized_command), response, sizeof(response),
        &response_len) == 0);
    assert(response_len == 0 && ble_gatt_attribute_value(&server,
        &server.attributes[handle - 1])[0] == 0xa5);
}

static void test_read_multiple(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x181a);
    ble_gatt_uuid one_uuid = uuid16(0xffe8), two_uuid = uuid16(0xffe9);
    uint16_t service, one_decl, one, two_decl, two;
    const uint8_t one_value[] = {0x10, 0x11};
    const uint8_t two_value[] = {0x20, 0x21, 0x22};
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &one_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, one_value, 2, 2,
        NULL, NULL, NULL, &one_decl, &one));
    assert(ble_gatt_server_add_characteristic(&server, &two_uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, two_value, 3, 3,
        NULL, NULL, NULL, &two_decl, &two));
    uint8_t response[517];
    uint16_t response_len;
    uint8_t request[] = {0x0e, (uint8_t)one, 0, (uint8_t)two, 0};
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 6 && response[0] == 0x0f);
    assert(!memcmp(response + 1, one_value, 2));
    assert(!memcmp(response + 3, two_value, 3));

    request[0] = 0x20;
    assert(att(&server, request, sizeof(request), response, &response_len) == 1);
    assert(response_len == 10 && response[0] == 0x21);
    assert(ble_gatt_server_u16(response + 1) == 2);
    assert(!memcmp(response + 3, one_value, 2));
    assert(ble_gatt_server_u16(response + 5) == 3);
    assert(!memcmp(response + 7, two_value, 3));
}

static void test_notifications_and_indications(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid notify_uuid = uuid16(0xffea), indicate_uuid = uuid16(0xffeb);
    ble_gatt_uuid cccd_uuid = uuid16(0x2902);
    uint16_t service, decl, notify, cccd, indicate_decl, indicate, indicate_cccd;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &notify_uuid,
        BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL, &decl, &notify));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &cccd));
    assert(ble_gatt_server_add_characteristic(&server, &indicate_uuid,
        BLE_GATT_PROP_INDICATE, 0, NULL, 0, 0, NULL, NULL, NULL,
        &indicate_decl, &indicate));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &indicate_cccd));
    uint8_t response[517], event[] = {0xa1, 0xb2};
    uint16_t response_len;

    uint8_t subscribe[] = {0x12, (uint8_t)cccd, 0, 1, 0};
    assert(att(&server, subscribe, sizeof(subscribe), response, &response_len) == 1);
    assert(ble_gatt_server_notify(&server, notify, event, sizeof(event),
        response, sizeof(response), &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x1b);
    assert(ble_gatt_server_u16(response + 1) == notify);
    assert(!memcmp(response + 3, event, sizeof(event)));

    uint8_t oversized_event[21] = {0};
    assert(ble_gatt_server_queue_event(&server, notify, oversized_event,
                                        sizeof(oversized_event), 0));
    assert(server.event_count == 1 && server.events[0].len == 21);
    assert(ble_gatt_server_poll_event(&server, 99, response,
        sizeof(response), &response_len) == 1);
    assert(response_len == 23 && server.event_count == 0 &&
           server.event_used == 0);

    subscribe[1] = (uint8_t)indicate_cccd;
    subscribe[3] = 2;
    assert(att(&server, subscribe, sizeof(subscribe), response, &response_len) == 1);
    assert(ble_gatt_server_indicate(&server, indicate, event, sizeof(event),
        response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x1d && server.indication_pending);
    assert(ble_gatt_server_indicate(&server, indicate, event, sizeof(event),
        response, sizeof(response), &response_len) == 0);
    assert(ble_gatt_server_poll_event(&server, 100000, response,
        sizeof(response), &response_len) == 0);
    assert(server.indication_timeout_armed &&
           server.indication_started_ms == 100000);
    assert(ble_gatt_server_poll_event(&server,
        100000 + BLE_GATT_SERVER_INDICATION_TIMEOUT_MS - 1, response,
        sizeof(response), &response_len) == 0);
    const uint8_t confirmation[] = {0x1e};
    assert(att(&server, confirmation, sizeof(confirmation), response,
               &response_len) == 0);
    assert(!server.indication_pending && !server.indication_timeout_armed);

    assert(ble_gatt_server_queue_event(&server, notify, event, sizeof(event), 0));
    assert(ble_gatt_server_poll_event(&server, 100, response, sizeof(response),
        &response_len) == 1);
    assert(response[0] == 0x1b && server.event_count == 0);
    assert(ble_gatt_server_queue_event(&server, notify, event, sizeof(event), 0));
    subscribe[1] = (uint8_t)cccd;
    subscribe[3] = 0;
    assert(att(&server, subscribe, sizeof(subscribe), response,
               &response_len) == 1);
    assert(response[0] == 0x13 && server.event_count == 0 &&
           server.event_used == 0);
    subscribe[1] = (uint8_t)indicate_cccd;
    subscribe[3] = 2;
    assert(ble_gatt_server_queue_event(&server, indicate, event,
        sizeof(event), 1));
    assert(ble_gatt_server_poll_event(&server, 0xfffffff0u, response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == 0x1d && server.indication_pending &&
           !server.indication_timeout_armed);
    uint32_t indication_sent_ms = 0xfffffff0u + 1000u;
    assert(ble_gatt_server_poll_event(&server, indication_sent_ms, response,
        sizeof(response), &response_len) == 0);
    assert(server.indication_timeout_armed &&
           server.indication_started_ms == indication_sent_ms);
    assert(ble_gatt_server_poll_event(&server, indication_sent_ms + 1000u,
        response, sizeof(response), &response_len) == 0);
    assert(server.indication_pending);
    assert(ble_gatt_server_poll_event(&server,
        indication_sent_ms + BLE_GATT_SERVER_INDICATION_TIMEOUT_MS,
        response, sizeof(response), &response_len) == -1);
    assert(!server.indication_pending);
    assert(ble_gatt_server_queue_event(&server, indicate, event,
        sizeof(event), 1));
    assert(ble_gatt_server_poll_event(&server, 5000, response, sizeof(response),
        &response_len) == 1);
    assert(att(&server, confirmation, sizeof(confirmation), response,
               &response_len) == 0);
    assert(!server.indication_pending && server.indication_started_ms == 0);
    assert(!server.indication_timeout_armed);
}

static void test_attribute_value_limit_and_notification_truncation(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, BLE_GATT_SERVER_MTU_MAX);
    server.mtu = BLE_GATT_SERVER_MTU_MAX;
    ble_gatt_uuid service_uuid = uuid16(0x180f);
    ble_gatt_uuid notify_uuid = uuid16(0xffec), indicate_uuid = uuid16(0xffed);
    ble_gatt_uuid cccd_uuid = uuid16(0x2902), writable_uuid = uuid16(0xffee);
    uint16_t service, notify_decl, notify, notify_cccd;
    uint16_t indicate_decl, indicate, indicate_cccd, writable;
    uint8_t initial = 0;
    uint8_t allow_notify = 0;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &notify_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHENTICATED |
            BLE_GATT_PERM_READ_AUTHORIZED,
        NULL, 0, 0, NULL, NULL, NULL,
        &notify_decl, &notify));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &notify_cccd));
    assert(ble_gatt_server_add_characteristic(&server, &indicate_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_READ_AUTHENTICATED |
            BLE_GATT_PERM_READ_AUTHORIZED,
        NULL, 0, 0, NULL, NULL, NULL,
        &indicate_decl, &indicate));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &indicate_cccd));
    assert(ble_gatt_server_add_attribute(&server, &writable_uuid,
        BLE_GATT_PERM_WRITE, &initial, 1, 1,
        NULL, NULL, NULL, &writable));
    assert(ble_gatt_server_set_min_key_size(&server, notify, 12));
    assert(ble_gatt_server_set_min_key_size(&server, indicate, 12));
    ble_gatt_server_set_authorizer(&server, authorize_access, &allow_notify);
    assert(ble_gatt_server_set_cccd(&server, notify, 1));
    assert(ble_gatt_server_set_cccd(&server, indicate, 2));

    uint8_t value[BLE_GATT_ATT_VALUE_MAX + 1];
    memset(value, 0x6c, sizeof(value));
    uint8_t att_pdu[BLE_GATT_SERVER_MTU_MAX];
    uint16_t att_len;
    assert(!ble_gatt_server_notify(&server, notify, value, sizeof(value),
        att_pdu, sizeof(att_pdu), &att_len));
    assert(!ble_gatt_server_notify(&server, notify, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    ble_gatt_server_set_security(&server, 1, 0);
    ble_gatt_server_set_encryption_key_size(&server, 10);
    assert(!ble_gatt_server_notify(&server, notify, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    ble_gatt_server_set_encryption_key_size(&server, 12);
    assert(!ble_gatt_server_notify(&server, notify, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    ble_gatt_server_set_security(&server, 1, 1);
    assert(!ble_gatt_server_notify(&server, notify, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    allow_notify = 1;
    assert(ble_gatt_server_notify(&server, notify, value,
        BLE_GATT_ATT_VALUE_MAX, att_pdu, sizeof(att_pdu), &att_len));
    assert(att_len == BLE_GATT_ATT_VALUE_MAX + 3);

    uint8_t write_request[BLE_GATT_ATT_VALUE_MAX + 4] = {0x12,
        (uint8_t)writable, (uint8_t)(writable >> 8)};
    memcpy(write_request + 3, value, BLE_GATT_ATT_VALUE_MAX + 1);
    uint16_t response_len;
    assert(att(&server, write_request, sizeof(write_request), att_pdu,
        &response_len) == 1);
    assert(att_pdu[0] == 0x01 &&
           att_pdu[4] == BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH);

    write_request[0] = 0x52;
    response_len = 0xffff;
    assert(ble_gatt_server_att(&server, write_request, sizeof(write_request),
        att_pdu, sizeof(att_pdu), &response_len) == 0);
    assert(response_len == 0 && *ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, writable)) == 0);

    server.mtu = 23;
    assert(ble_gatt_server_notify(&server, notify, value,
        BLE_GATT_ATT_VALUE_MAX, att_pdu, sizeof(att_pdu), &att_len));
    assert(att_len == 23 && !memcmp(att_pdu + 3, value, 20));
    assert(!ble_gatt_server_indicate(&server, indicate, value,
        BLE_GATT_ATT_VALUE_MAX + 1, att_pdu, sizeof(att_pdu), &att_len));
    ble_gatt_server_set_encryption_key_size(&server, 10);
    assert(!ble_gatt_server_indicate(&server, indicate, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    ble_gatt_server_set_encryption_key_size(&server, 12);
    allow_notify = 0;
    assert(!ble_gatt_server_indicate(&server, indicate, value, 1,
        att_pdu, sizeof(att_pdu), &att_len));
    allow_notify = 1;
    assert(ble_gatt_server_indicate(&server, indicate, value,
        BLE_GATT_ATT_VALUE_MAX, att_pdu, sizeof(att_pdu), &att_len));
    assert(att_len == 23 && !memcmp(att_pdu + 3, value, 20));
    const uint8_t confirmation[] = {0x1e};
    assert(att(&server, confirmation, sizeof(confirmation), att_pdu,
               &response_len) == 0);

    ble_gatt_server_set_encryption_key_size(&server, 10);
    assert(!ble_gatt_server_queue_event(&server, notify, value, 1, 0));
    ble_gatt_server_set_encryption_key_size(&server, 12);
    allow_notify = 0;
    assert(!ble_gatt_server_queue_event(&server, notify, value, 1, 0));
    allow_notify = 1;
    assert(!ble_gatt_server_queue_event(&server, notify, value,
        BLE_GATT_ATT_VALUE_MAX + 1, 0));
    assert(ble_gatt_server_queue_event(&server, notify, value,
        BLE_GATT_ATT_VALUE_MAX, 0));
    assert(server.events[0].len == BLE_GATT_ATT_VALUE_MAX);
    assert(ble_gatt_server_poll_event(&server, 1, att_pdu, sizeof(att_pdu),
                                      &att_len) == 1);
    assert(att_len == 23 && !memcmp(att_pdu + 3, value, 20));
    assert(ble_gatt_server_queue_event(&server, notify, value, 1, 0));
    allow_notify = 0;
    assert(ble_gatt_server_poll_event(&server, 2, att_pdu, sizeof(att_pdu),
                                      &att_len) == 0);
    assert(server.event_count == 0 && server.event_used == 0);
}

static void test_prepare_execute_writes(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x181a);
    ble_gatt_uuid one_uuid = uuid16(0xffec), two_uuid = uuid16(0xffed);
    uint16_t service, one_decl, one, two_decl, two, callback_handle;
    uint8_t one_initial[] = {0x10, 0x11}, two_initial[] = {0x20};
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &one_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, one_initial, 2, 4,
        NULL, NULL, NULL, &one_decl, &one));
    assert(ble_gatt_server_add_characteristic(&server, &two_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, two_initial, 1, 4,
        NULL, NULL, NULL, &two_decl, &two));
    assert(ble_gatt_server_set_variable_length(&server, one, 1));
    assert(ble_gatt_server_set_variable_length(&server, two, 1));
    ble_gatt_uuid callback_uuid = uuid16(0xffee);
    assert(ble_gatt_server_add_attribute(&server, &callback_uuid,
        BLE_GATT_PERM_WRITE, dynamic_value, 3, 3, NULL, dynamic_write,
        NULL, &callback_handle));
    uint8_t response[517];
    uint16_t response_len;
    uint8_t prep_one[] = {0x16, (uint8_t)one, 0, 2, 0, 0x12, 0x13};
    uint8_t prep_two[] = {0x16, (uint8_t)two, 0, 1, 0, 0x21};
    assert(att(&server, prep_one, sizeof(prep_one), response, &response_len) == 1);
    assert(response_len == sizeof(prep_one) && response[0] == 0x17);
    assert(!memcmp(response + 1, prep_one + 1, sizeof(prep_one) - 1));
    assert(att(&server, prep_two, sizeof(prep_two), response, &response_len) == 1);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, one))[0] == 0x10);
    const uint8_t execute[] = {0x18, 1};
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x19 && server.prepare_count == 0);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, one))[2] == 0x12);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, one))[3] == 0x13);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, two))[1] == 0x21);

    uint8_t cancel_prep[] = {0x16, (uint8_t)one, 0, 0, 0, 0xaa};
    const uint8_t cancel[] = {0x18, 0};
    assert(att(&server, cancel_prep, sizeof(cancel_prep), response,
               &response_len) == 1);
    assert(att(&server, cancel, sizeof(cancel), response, &response_len) == 1);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, one))[0] == 0x10);

    uint8_t hole[] = {0x16, (uint8_t)two, 0, 3, 0, 0xbb};
    assert(att(&server, hole, sizeof(hole), response, &response_len) == 1);
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x01 && response[4] == BLE_GATT_ATT_ERR_INVALID_OFFSET);
    assert(ble_gatt_server_find(&server, two)->value_len == 2);

    uint8_t prep_callback[] = {0x16, (uint8_t)callback_handle, 0, 0, 0, 0xcc};
    assert(att(&server, prep_callback, sizeof(prep_callback), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED);

    assert(att(&server, prep_one, sizeof(prep_one), response, &response_len) == 1);
    assert(att(&server, prep_two, sizeof(prep_two), response, &response_len) == 1);
    ble_gatt_server_find(&server, two)->permissions = 0;
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, one))[2] == 0x12);
    assert(server.prepare_count == 0);
}

static void test_transactional_prepare_callbacks(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 247);
    ble_gatt_uuid service_uuid = uuid16(0x181a);
    ble_gatt_uuid static_uuid = uuid16(0xffef), dynamic_uuid = uuid16(0xfff0);
    uint16_t service, static_handle, dynamic_handle;
    uint8_t static_initial = 0x11;
    transaction_state state = {{0x20, 0x21, 0x22, 0x23}, {0}, {0}, 0, 0};
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_attribute(&server, &static_uuid,
        BLE_GATT_PERM_WRITE, &static_initial, 1, 2, NULL, NULL, NULL,
        &static_handle));
    assert(ble_gatt_server_add_attribute(&server, &dynamic_uuid,
        BLE_GATT_PERM_WRITE, NULL, 0, 0, NULL, NULL, &state, &dynamic_handle));
    assert(ble_gatt_server_set_transaction_callbacks(&server, dynamic_handle,
        transaction_prepare, transaction_execute));

    uint8_t response[517];
    uint16_t response_len;
    uint8_t prep_dynamic[] = {0x16, (uint8_t)dynamic_handle, 0, 1, 0, 0x44};
    assert(att(&server, prep_dynamic, sizeof(prep_dynamic), response,
               &response_len) == 1 && response[0] == 0x17);
    assert(state.visible[1] == 0x21 && state.commits == 0);
    const uint8_t execute[] = {0x18, 1};
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x19 && state.visible[1] == 0x44);
    assert(state.commits == 1 && state.cancels == 0);

    uint8_t prep_cancel[] = {0x16, (uint8_t)dynamic_handle, 0, 2, 0, 0x55};
    const uint8_t cancel[] = {0x18, 0};
    assert(att(&server, prep_cancel, sizeof(prep_cancel), response,
               &response_len) == 1);
    assert(att(&server, cancel, sizeof(cancel), response, &response_len) == 1);
    assert(state.visible[2] == 0x22 && state.cancels == 1);

    // If any attribute fails final validation, neither static nor dynamic
    // values are published and the dynamic staging area is canceled.
    uint8_t prep_static[] = {0x16, (uint8_t)static_handle, 0, 1, 0, 0x33};
    assert(att(&server, prep_static, sizeof(prep_static), response,
               &response_len) == 1);
    assert(att(&server, prep_dynamic, sizeof(prep_dynamic), response,
               &response_len) == 1);
    ble_gatt_server_find(&server, static_handle)->permissions = 0;
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, static_handle))[1] == 0);
    assert(state.visible[1] == 0x44 && state.cancels == 2);
    ble_gatt_server_find(&server, static_handle)->permissions = BLE_GATT_PERM_WRITE;

    // A rejected fragment is treated as if it was never queued, preserving
    // the earlier accepted fragment for a later Execute Write.
    uint8_t prep_reject[] = {0x16, (uint8_t)dynamic_handle, 0, 0, 0, 0xee};
    prep_dynamic[5] = 0x66;
    assert(att(&server, prep_dynamic, sizeof(prep_dynamic), response,
               &response_len) == 1);
    assert(att(&server, prep_reject, sizeof(prep_reject), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED);
    assert(state.visible[1] == 0x44 && state.staged[1] == 0x66 &&
           state.cancels == 2);
    assert(server.prepare_count == 1);
    assert(att(&server, execute, sizeof(execute), response, &response_len) == 1);
    assert(response[0] == 0x19 && state.visible[1] == 0x66 &&
           state.commits == 2 && state.cancels == 2);
}

int main(void) {
    test_database_registration_and_handles();
    test_database_seal_requires_property_descriptors();
    test_included_service_discovery();
    test_signed_write_command();
    test_application_authorization();
    test_security_and_authorization_check_order();
    test_minimum_encryption_key_size();
    test_fixed_and_variable_length_writes();
    test_maximum_att_attribute_value();
    test_read_multiple_checks_all_permissions();
    test_fixed_read_multiple_checks_after_mtu_boundary();
    test_response_capacity_and_large_find_information();
    test_att_mtu_and_reads();
    test_read_callback_boundaries();
    test_dynamic_read_enforces_att_value_limit();
    test_discovery();
    test_128_bit_uuids();
    test_writes_cccd_and_permissions();
    test_callbacks_and_invalid_requests();
    test_read_by_type_mtu_limit();
    test_find_by_type_value_and_unsupported_group();
    test_write_command();
    test_read_multiple();
    test_notifications_and_indications();
    test_attribute_value_limit_and_notification_truncation();
    test_prepare_execute_writes();
    test_transactional_prepare_callbacks();
    return 0;
}
