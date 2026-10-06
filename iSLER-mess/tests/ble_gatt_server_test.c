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

int main(void) {
    test_database_registration_and_handles();
    test_att_mtu_and_reads();
    test_discovery();
    test_128_bit_uuids();
    test_writes_cccd_and_permissions();
    test_callbacks_and_invalid_requests();
    test_read_by_type_mtu_limit();
    test_write_command();
    return 0;
}
