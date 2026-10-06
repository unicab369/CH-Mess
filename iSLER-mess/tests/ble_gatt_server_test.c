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

static int att(ble_gatt_server *server, const uint8_t *request, uint16_t len,
               uint8_t *response, uint16_t *response_len) {
    return ble_gatt_server_att(server, request, len, response, 517,
                               response_len);
}

typedef struct {
    uint8_t visible[4], staged[4], touched[4];
    unsigned commits, cancels;
} transaction_state;

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

    const uint8_t read_blob[] = {0x0c, 3, 0, 1, 0};
    assert(att(&server, read_blob, sizeof(read_blob), response, &response_len) == 1);
    assert(response_len == 1 && response[0] == 0x0d);

    const uint8_t read_missing[] = {0x0a, 0xff, 0};
    assert(att(&server, read_missing, sizeof(read_missing), response,
               &response_len) == 1);
    assert(response[0] == 0x01 && response[4] == BLE_GATT_ATT_ERR_INVALID_HANDLE);
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

    uint8_t type_req[] = {0x08, 1, 0, 3, 0, 3, 0x28};
    assert(att(&server, type_req, sizeof(type_req), response,
               &response_len) == 1);
    assert(response_len == 23 && response[1] == 21);
    assert(ble_gatt_server_u16(response + 2) == declaration);
    assert(ble_gatt_server_u16(response + 5) == value);
    assert(!memcmp(response + 7, char_uuid.value, 16));

    uint8_t find_req[23] = {0x06, 1, 0, 3, 0, 0, 0x28};
    memcpy(find_req + 7, service_uuid.value, 16);
    assert(att(&server, find_req, sizeof(find_req), response,
               &response_len) == 1);
    assert(response_len == 5 && response[0] == 0x07);
    assert(ble_gatt_server_u16(response + 1) == service);
    assert(ble_gatt_server_u16(response + 3) == value);
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
           response[4] == BLE_GATT_ATT_ERR_INSUFFICIENT_ENCRYPTION);
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

    subscribe[1] = (uint8_t)indicate_cccd;
    subscribe[3] = 2;
    assert(att(&server, subscribe, sizeof(subscribe), response, &response_len) == 1);
    assert(ble_gatt_server_indicate(&server, indicate, event, sizeof(event),
        response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x1d && server.indication_pending);
    assert(ble_gatt_server_indicate(&server, indicate, event, sizeof(event),
        response, sizeof(response), &response_len) == 0);
    const uint8_t confirmation[] = {0x1e};
    assert(att(&server, confirmation, sizeof(confirmation), response,
               &response_len) == 0);
    assert(!server.indication_pending);

    assert(ble_gatt_server_queue_event(&server, notify, event, sizeof(event), 0));
    assert(ble_gatt_server_poll_event(&server, 100, response, sizeof(response),
        &response_len) == 1);
    assert(response[0] == 0x1b && server.event_count == 0);
    assert(ble_gatt_server_queue_event(&server, indicate, event,
        sizeof(event), 1));
    assert(ble_gatt_server_poll_event(&server, 0xfffffff0u, response,
        sizeof(response), &response_len) == 1);
    assert(response[0] == 0x1d && server.indication_pending);
    assert(ble_gatt_server_poll_event(&server, 0xfffffff0u + 1000u, response,
        sizeof(response), &response_len) == 0);
    assert(server.indication_pending);
    assert(ble_gatt_server_poll_event(&server,
        0xfffffff0u + BLE_GATT_SERVER_INDICATION_TIMEOUT_MS, response,
        sizeof(response), &response_len) == -1);
    assert(!server.indication_pending);
    assert(ble_gatt_server_queue_event(&server, indicate, event,
        sizeof(event), 1));
    assert(ble_gatt_server_poll_event(&server, 5000, response, sizeof(response),
        &response_len) == 1);
    assert(att(&server, confirmation, sizeof(confirmation), response,
               &response_len) == 0);
    assert(!server.indication_pending && server.indication_started_ms == 0);
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

    // A rejected fragment cancels earlier staged callbacks and its own state.
    uint8_t prep_reject[] = {0x16, (uint8_t)dynamic_handle, 0, 0, 0, 0xee};
    assert(att(&server, prep_dynamic, sizeof(prep_dynamic), response,
               &response_len) == 1);
    assert(att(&server, prep_reject, sizeof(prep_reject), response,
               &response_len) == 1);
    assert(response[0] == 0x01 &&
           response[4] == BLE_GATT_ATT_ERR_VALUE_NOT_ALLOWED);
    assert(state.visible[1] == 0x44 && state.cancels == 3);
    assert(server.prepare_count == 0);
}

int main(void) {
    test_database_registration_and_handles();
    test_att_mtu_and_reads();
    test_discovery();
    test_128_bit_uuids();
    test_writes_cccd_and_permissions();
    test_callbacks_and_invalid_requests();
    test_read_by_type_mtu_limit();
    test_write_command();
    test_read_multiple();
    test_notifications_and_indications();
    test_prepare_execute_writes();
    test_transactional_prepare_callbacks();
    return 0;
}
