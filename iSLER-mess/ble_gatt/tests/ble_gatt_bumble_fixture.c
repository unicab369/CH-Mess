#include <stdint.h>
#include <stdio.h>
#include "../ble_gatt_server.h"

static uint8_t application_authorized;
static uint32_t peer_sign_counter;

static int authorize_application_access(
    void *context, uint16_t handle,
                                        uint8_t write
) {
    (void)context;
    (void)handle;
    (void)write;
    return application_authorized;
}

static int verify_peer_signed_write(
    void *context, const uint8_t *pdu,
                                    uint16_t signed_len,
                                    const uint8_t signature[12]
) {
    (void)context;
    static const uint8_t csrk[16] = {
        0x61, 0x1b, 0x64, 0xeb, 0xfb, 0xcd, 0x1f, 0xd3,
        0x72, 0xec, 0x91, 0x96, 0xdf, 0x42, 0x5e, 0x50
    };
    uint32_t counter = (uint32_t)signature[0] |
        (uint32_t)signature[1] << 8 | (uint32_t)signature[2] << 16 |
        (uint32_t)signature[3] << 24;
    if (counter <= peer_sign_counter) return 0;
    uint8_t mac[16];
    ble_gatt_cmac cmac;
    ble_gatt_cmac_init(&cmac, csrk);
    ble_gatt_cmac_update(&cmac, pdu, signed_len);
    ble_gatt_cmac_update(&cmac, signature, 4);
    ble_gatt_cmac_final(&cmac, mac);
    for (uint8_t i = 0; i < 8; i++)
        if (signature[4 + i] != mac[7 - i]) return 0;
    peer_sign_counter = counter;
    return 1;
}

int main(void) {
    ble_gatt_server server;
    ble_gatt_server_init(&server, 64);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid aggregate_uuid = {2, {0x12, 0x18}};
    ble_gatt_uuid characteristic_uuid = {2, {0x19, 0x2a}};
    ble_gatt_uuid descriptor_uuid = {2, {0x01, 0x29}};
    ble_gatt_uuid extended_properties_uuid = {2, {0x00, 0x29}};
    ble_gatt_uuid protected_uuid = {2, {0x1a, 0x2a}};
    ble_gatt_uuid authenticated_uuid = {2, {0x1b, 0x2a}};
    ble_gatt_uuid authorized_uuid = {2, {0x1c, 0x2a}};
    ble_gatt_uuid gap_uuid = {2, {0x00, 0x18}};
    ble_gatt_uuid device_name_uuid = {2, {0x00, 0x2a}};
    ble_gatt_uuid appearance_uuid = {2, {0x01, 0x2a}};
    ble_gatt_uuid ppcp_uuid = {2, {0x04, 0x2a}};
    ble_gatt_uuid car_uuid = {2, {0xa6, 0x2a}};
    ble_gatt_uuid security_levels_uuid = {2, {0xf5, 0x2b}};
    ble_gatt_uuid edkm_uuid = {2, {0x88, 0x2b}};
    ble_gatt_uuid signed_uuid = {2, {0xf0, 0xff}};
    uint8_t initial[70];
    const uint8_t writable_auxiliaries[] = {2, 0};
    for (uint8_t i = 0; i < sizeof(initial); i++) initial[i] = i;
    uint16_t service, declaration, value, descriptor, aggregate;
    uint16_t protected_value, authenticated_value, authorized_value;
    uint16_t signed_declaration, signed_value;
    uint8_t appearance[2] = {0, 0};
    const uint8_t ppcp[8] = {0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff};
    const uint8_t central_address_resolution = 1;
    const uint8_t security_level_requirements[2] = {1, 3};
    const uint8_t edkm[24] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24
    };
    ble_gatt_standard_service_handles gatt_service;
    if (!ble_gatt_server_add_service(&server, &service_uuid, 1, &service) ||
        !ble_gatt_server_add_characteristic(&server, &characteristic_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_EXTENDED,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, initial,
            sizeof(initial), 100, NULL, NULL, NULL, &declaration, &value) ||
        !ble_gatt_server_add_attribute(&server, &extended_properties_uuid,
            BLE_GATT_PERM_READ, writable_auxiliaries,
            sizeof(writable_auxiliaries), sizeof(writable_auxiliaries),
            NULL, NULL, NULL, NULL) ||
        !ble_gatt_server_add_descriptor(&server, &descriptor_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            (const uint8_t *)"x", 1, 32,
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
            10, 10, NULL, NULL, NULL, &authorized_value) ||
        !ble_gatt_server_add_standard_gatt_service(&server, &gatt_service) ||
        !ble_gatt_server_add_service(&server, &gap_uuid, 1, &service) ||
        !ble_gatt_server_add_characteristic(&server, &device_name_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            (const uint8_t *)"CH-Mess", 7, 7, NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &appearance_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, appearance,
            sizeof(appearance), sizeof(appearance), NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &ppcp_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ, ppcp,
            sizeof(ppcp), sizeof(ppcp), NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &car_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &central_address_resolution, 1, 1, NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &security_levels_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            security_level_requirements, sizeof(security_level_requirements),
            sizeof(security_level_requirements), NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &edkm_uuid,
            BLE_GATT_PROP_READ,
            BLE_GATT_PERM_READ_AUTHENTICATED | BLE_GATT_PERM_READ_AUTHORIZED,
            edkm, sizeof(edkm), sizeof(edkm), NULL, NULL, NULL,
            &declaration, &value) ||
        !ble_gatt_server_add_characteristic(&server, &signed_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_AUTH_SIGNED_WRITE,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE_SIGNED,
            (const uint8_t *)"\0", 1, 1, NULL, NULL, NULL,
            &signed_declaration, &signed_value) ||
        !ble_gatt_server_set_min_key_size(&server, authorized_value, 12) ||
        !ble_gatt_server_seal_database(&server))
        return 2;
    (void)signed_declaration;
    ble_gatt_server_set_authorizer(&server,
        authorize_application_access, NULL);
    ble_gatt_server_set_signed_write_verifier(&server,
        verify_peer_signed_write, NULL);

    uint8_t request[BLE_GATT_SERVER_MTU_MAX];
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    for (;;) {
        uint8_t size_bytes[2];
        if (fread(size_bytes, 1, 2, stdin) != 2) break;
        uint16_t request_len = (uint16_t)size_bytes[0] |
                               (uint16_t)size_bytes[1] << 8;
        if (!request_len || request_len > sizeof(request) ||
            fread(request, 1, request_len, stdin) != request_len)
            return 3;
        uint16_t response_len = 0;
        int has_response;
        // Harness-only link state control; this is not an ATT PDU sent by a
        // peer. It lets interop tests exercise security error selection.
        if (request_len == 4 && request[0] == 0xf0) {
            server.encrypted = request[1] != 0;
            server.encryption_key_size = request[2];
            application_authorized = request[3] != 0;
            has_response = 0;
        } else {
            has_response = ble_gatt_server_att(&server, request,
                request_len, response, sizeof(response), &response_len);
        }
        uint8_t event[BLE_GATT_SERVER_MTU_MAX];
        uint16_t event_len = 0;
        if (request[0] == 0x12 && request_len == 5 &&
            ble_gatt_server_u16(request + 1) ==
                gatt_service.service_changed_cccd_handle &&
            ble_gatt_server_u16(request + 3) == 2) {
            (void)ble_gatt_server_service_changed(&server, 1, 0xffff);
            (void)ble_gatt_server_poll_event(&server, 1, event,
                                             sizeof(event), &event_len);
        }
        uint16_t output_len = has_response > 0 ? response_len : 0;
        uint8_t output_size[2] = {(uint8_t)output_len,
                                  (uint8_t)(output_len >> 8)};
        if (fwrite(output_size, 1, 2, stdout) != 2 ||
            (output_len && fwrite(response, 1, output_len, stdout) != output_len))
            return 4;
        uint8_t event_size[2] = {(uint8_t)event_len,
                                 (uint8_t)(event_len >> 8)};
        if (fwrite(event_size, 1, 2, stdout) != 2 ||
            (event_len && fwrite(event, 1, event_len, stdout) != event_len))
            return 5;
        fflush(stdout);
    }
    return 0;
}
