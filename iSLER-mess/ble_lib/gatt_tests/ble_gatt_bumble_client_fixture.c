#include <stdint.h>
#include <stdio.h>
#include "../ble_gatt.h"
#include "../ble_crypto.h"

static ble_gatt_client client;
static uint8_t active_command;

static int sign_with_test_csrk(
    void *context, const uint8_t *pdu,
                               uint16_t len, uint8_t signature[12]
) {
    (void)context;
    static const uint8_t csrk[16] = {
        0x61, 0x1b, 0x64, 0xeb, 0xfb, 0xcd, 0x1f, 0xd3,
        0x72, 0xec, 0x91, 0x96, 0xdf, 0x42, 0x5e, 0x50
    };
    const uint8_t counter[4] = {1, 0, 0, 0};
    uint8_t mac[16];
    ble_gatt_cmac cmac;
    gatt_cmac_init(&cmac, csrk);
    gatt_cmac_update(&cmac, pdu, len);
    gatt_cmac_update(&cmac, counter, sizeof(counter));
    gatt_cmac_final(&cmac, mac);
    memcpy(signature, counter, sizeof(counter));
    for (uint8_t i = 0; i < 8; i++) signature[4 + i] = mac[7 - i];
    return 1;
}

static int send_pdu(void *context, const uint8_t *pdu, uint16_t len) {
    (void)context;
    uint8_t header[3] = {1, (uint8_t)len, (uint8_t)(len >> 8)};
    if (fwrite(header, 1, sizeof(header), stdout) != sizeof(header) ||
        fwrite(pdu, 1, len, stdout) != len
    ) return 0;
    fflush(stdout);
    return 1;
}

static void on_result(
    void *context, uint8_t status,
                      const uint8_t *pdu, uint16_t len
) {
    (void)context;
    if (active_command == 10) {
        if (client.operation.value_len != 70)
            status = BLE_GATT_CLIENT_PROTOCOL_ERROR;
        else
            for (uint16_t i = 0; i < client.operation.value_len; i++)
                if (client.operation.value[i] != (uint8_t)i)
                    status = BLE_GATT_CLIENT_PROTOCOL_ERROR;
    }

    uint8_t header[4] = {2, status, (uint8_t)len, (uint8_t)(len >> 8)};
    if (fwrite(header, 1, sizeof(header), stdout) != sizeof(header) ||
        (len && fwrite(pdu, 1, len, stdout) != len)
    ) return;
    fflush(stdout);
}

static void on_event(
    void *context, uint16_t handle,
                     const uint8_t *value, uint16_t len
) {
    (void)context;
    uint8_t header[5] = {3, (uint8_t)handle, (uint8_t)(handle >> 8),
                         (uint8_t)len, (uint8_t)(len >> 8)};
    if (fwrite(header, 1, sizeof(header), stdout) != sizeof(header) ||
        (len && fwrite(value, 1, len, stdout) != len)
    ) return;
    fflush(stdout);
}

int main(void) {
    gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    gatt_client_set_signer(&client, sign_with_test_csrk);
    uint32_t now_ms = 0;
    for (;;) {
        int command = fgetc(stdin);
        if (command == EOF || command == 0)
            break;
        active_command = (uint8_t)command;
        uint16_t requested_handle = 0;
        if (command == 23 || command == 24) {
            uint8_t handle_bytes[2];
            if (fread(handle_bytes, 1, sizeof(handle_bytes), stdin) !=
                sizeof(handle_bytes)
            ) return 11;
            requested_handle = (uint16_t)handle_bytes[0] |
                (uint16_t)handle_bytes[1] << 8;
        }

        int started = 0;
        switch (command) {
        case 1:
            started = gatt_client_mtu(&client, 64, now_ms);
            break;
        case 2:
            started = gatt_client_services(&client, 1, 0xffff,
                                                        NULL, 0, now_ms);
            break;
        case 3: {
            const uint8_t uuid[] = {0x0f, 0x18};
            started = gatt_client_services(&client, 1, 0xffff,
                                                        uuid, 2, now_ms);
            break;
        }
        case 4:
            started = gatt_client_characteristics(&client,
                1, 4, NULL, 0, now_ms);
            break;
        case 5:
            started = gatt_client_discover_descriptors(&client, 4, 4,
                                                            now_ms);
            break;
        case 6:
            started = gatt_client_read(&client, 3, now_ms);
            break;
        case 7: {
            const uint8_t value = 0x77;
            started = gatt_client_write(&client, 3, &value, 1, now_ms);
            break;
        }
        case 8: {
            const uint8_t value = 0x55;
            started = gatt_client_prepare_write(&client, 3, 0, &value,
                                                      1, now_ms);
            break;
        }
        case 9:
            started = gatt_client_execute_write(&client, 1, now_ms);
            break;
        case 10:
            started = gatt_client_read_long(&client, 3, now_ms);
            break;
        case 11: {
            uint8_t value[70];
            for (uint8_t i = 0; i < sizeof(value); i++)
                value[i] = (uint8_t)(0x80 + i);
            started = gatt_client_write_long(&client, 3, value,
                sizeof(value), now_ms);
            break;
        }
        case 12: {
            const uint8_t value = 0x44;
            started = gatt_client_write_command(&client, 3, &value, 1);
            break;
        }
        case 13:
            started = gatt_client_set_cccd(&client, 5, 1, 1, now_ms);
            break;
        case 14: case 15:
            started = 1;
            break;
        case 16:
            started = gatt_client_read(&client, 7, now_ms);
            break;
        case 17:
            started = gatt_client_read(&client, 9, now_ms);
            break;
        case 18: {
            const uint16_t handles[] = {3, 4};
            started = gatt_client_read_multiple(&client, handles, 2, 0,
                                                     now_ms);
            break;
        }
        case 19: {
            const uint8_t uuid[] = {0x01, 0x29};
            started = gatt_client_read_by_uuid(&client, 1, 0xffff,
                uuid, sizeof(uuid), now_ms);
            break;
        }
        case 20:
            started = gatt_client_included(&client,
                11, 11, now_ms);
            break;
        case 21: {
            const uint16_t handles[] = {3, 4};
            started = gatt_client_read_multiple(&client, handles, 2, 1,
                                                     now_ms);
            break;
        }
        case 22: {
            const uint8_t value = 0x37;
            started = gatt_client_write_signed(&client, 3, &value, 1);
            break;
        }
        case 23: case 24:
            started = gatt_client_read(&client, requested_handle, now_ms);
            break;
        default:
            return 2;
        }

        if (!started) return 3;

        if (command == 12 || command == 22) {
            const uint8_t result[4] = {2, 0, 0, 0};
            if (fwrite(result, 1, sizeof(result), stdout) != sizeof(result))
                return 7;
            fflush(stdout);
            continue;
        }

        if (command == 14 || command == 15) {
            uint8_t size_bytes[2], pdu[BLE_GATT_CLIENT_MTU_MAX];
            if (fread(size_bytes, 1, sizeof(size_bytes), stdin) !=
                sizeof(size_bytes)
            ) return 8;
            uint16_t pdu_len = (uint16_t)size_bytes[0] |
                               (uint16_t)size_bytes[1] << 8;
            if (!pdu_len || pdu_len > sizeof(pdu) ||
                fread(pdu, 1, pdu_len, stdin) != pdu_len
            ) return 9;
            if (gatt_client_receive(&client, pdu, pdu_len) < 0)
                return 10;
            continue;
        }

        do {
            uint8_t size_bytes[2], response[BLE_GATT_CLIENT_MTU_MAX];
            if (fread(size_bytes, 1, 2, stdin) != 2)
                return 4;
            uint16_t response_len = (uint16_t)size_bytes[0] |
                                    (uint16_t)size_bytes[1] << 8;
            if (response_len > sizeof(response) ||
                fread(response, 1, response_len, stdin) != response_len
            ) return 5;
            if (gatt_client_receive(&client, response, response_len) < 0)
                return 6;
        } while (client.request.pending);
        now_ms += 1;
    }
    return 0;
}
