#include <stdint.h>
#include <stdio.h>
#include "../ble_gatt_server.h"

static int deny_application_access(void *context, uint16_t handle,
                                   uint8_t write) {
    (void)context;
    (void)handle;
    (void)write;
    return 0;
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 64);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid aggregate_uuid = {2, {0x12, 0x18}};
    ble_gatt_uuid characteristic_uuid = {2, {0x19, 0x2a}};
    ble_gatt_uuid descriptor_uuid = {2, {0x01, 0x29}};
    ble_gatt_uuid protected_uuid = {2, {0x1a, 0x2a}};
    ble_gatt_uuid authenticated_uuid = {2, {0x1b, 0x2a}};
    ble_gatt_uuid authorized_uuid = {2, {0x1c, 0x2a}};
    uint8_t initial[70];
    for (uint8_t i = 0; i < sizeof(initial); i++) initial[i] = i;
    uint16_t service, declaration, value, descriptor, aggregate;
    uint16_t protected_value, authenticated_value, authorized_value;
    if (!ble_gatt_server_add_service(&server, &service_uuid, 1, &service) ||
        !ble_gatt_server_add_characteristic(&server, &characteristic_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, initial,
            sizeof(initial), 100, NULL, NULL, NULL, &declaration, &value) ||
        !ble_gatt_server_add_descriptor(&server, &descriptor_uuid,
            BLE_GATT_PERM_READ, (const uint8_t *)"x", 1, 1,
            NULL, NULL, NULL, &descriptor) ||
        !ble_gatt_server_add_service(&server, &aggregate_uuid, 1, &aggregate) ||
        !ble_gatt_server_add_included_service(&server, service, NULL) ||
        !ble_gatt_server_add_attribute(&server, &protected_uuid,
            BLE_GATT_PERM_READ_ENCRYPTED, (const uint8_t *)"protected",
            9, 9, NULL, NULL, NULL, &protected_value) ||
        !ble_gatt_server_add_attribute(&server, &authenticated_uuid,
            BLE_GATT_PERM_READ_AUTHENTICATED,
            (const uint8_t *)"authenticated", 13, 13, NULL, NULL, NULL,
            &authenticated_value) ||
        !ble_gatt_server_add_attribute(&server, &authorized_uuid,
            BLE_GATT_PERM_READ_AUTHORIZED, (const uint8_t *)"authorized",
            10, 10, NULL, NULL, NULL, &authorized_value)) return 2;
    ble_gatt_server_set_authorizer(&server, deny_application_access, NULL);

    uint8_t request[BLE_GATT_SERVER_MTU_MAX];
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    for (;;) {
        uint8_t size_bytes[2];
        if (fread(size_bytes, 1, 2, stdin) != 2) break;
        uint16_t request_len = (uint16_t)size_bytes[0] |
                               (uint16_t)size_bytes[1] << 8;
        if (!request_len || request_len > sizeof(request) ||
            fread(request, 1, request_len, stdin) != request_len) return 3;
        uint16_t response_len = 0;
        int has_response = ble_gatt_server_att(&server, request, request_len,
            response, sizeof(response), &response_len);
        uint16_t output_len = has_response > 0 ? response_len : 0;
        uint8_t output_size[2] = {(uint8_t)output_len,
                                  (uint8_t)(output_len >> 8)};
        if (fwrite(output_size, 1, 2, stdout) != 2 ||
            (output_len && fwrite(response, 1, output_len, stdout) != output_len))
            return 4;
        fflush(stdout);
    }
    return 0;
}
