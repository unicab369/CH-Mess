#include <assert.h>
#include <string.h>
#include "../ble_gatt_client.h"

static uint8_t sent[BLE_GATT_CLIENT_MTU_MAX];
static uint16_t sent_len, result_count, event_count;
static uint8_t result_status;

static int send_pdu(void *context, const uint8_t *pdu, uint16_t len) {
    (void)context;
    memcpy(sent, pdu, len);
    sent_len = len;
    return 1;
}

static void on_result(
    void *context, uint8_t status,
                      const uint8_t *pdu, uint16_t len
) {
    (void)context;
    (void)pdu;
    (void)len;
    result_status = status;
    result_count++;
}

static void on_event(
    void *context, uint16_t handle,
                     const uint8_t *value, uint16_t len
) {
    (void)context;
    assert(len == 1);
    if (handle == 0x1234) assert(value[0] == 0x5a);
    else if (handle == 0x5678) assert(value[0] == 0x6b);
    else assert(0);
    event_count++;
}

static int sign_pdu(
    void *context, const uint8_t *pdu, uint16_t len,
                    uint8_t signature[12]
) {
    (void)context;
    assert(len == 4 && pdu[0] == 0xd2);
    memset(signature, 0, 12);
    signature[0] = 0xa5;
    signature[8] = 1;
    return 1;
}

static void test_long_operation_deadlines_refresh(void) {
    ble_gatt_client client;
    uint8_t response[BLE_GATT_CLIENT_MTU_MAX];
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);

    assert(ble_gatt_client_read_long(&client, 0x0042, 1000));
    assert(ble_gatt_client_poll(&client, 20000) == 0);
    response[0] = 0x0b;
    memset(response + 1, 0x5a, 22);
    assert(ble_gatt_client_receive(&client, response, 23) == 1);
    assert(sent_len == 5 && sent[0] == 0x0c &&
           client.deadline_ms == 50000);

    ble_gatt_client_reset(&client);
    uint8_t value[40] = {0};
    assert(ble_gatt_client_write_long(&client, 0x0042, value,
                                      sizeof(value), 1000));
    assert(ble_gatt_client_poll(&client, 20000) == 0);
    memcpy(response, sent, sent_len);
    response[0] = 0x17;
    assert(ble_gatt_client_receive(&client, response, sent_len) == 1);
    assert(sent[0] == 0x16 && client.deadline_ms == 50000);
}

static void test_mtu_peer_below_default_keeps_default(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, NULL, NULL, NULL);
    assert(ble_gatt_client_exchange_mtu(&client, 100, 1));
    const uint8_t response[] = {0x03, 22, 0};
    assert(ble_gatt_client_receive(&client, response, sizeof(response)) == 1);
    assert(!client.pending && client.mtu_exchanged &&
           client.local_mtu == 100 && client.mtu == 23 && result_status == 0);
}

static void test_attribute_value_limit(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    client.mtu = BLE_GATT_CLIENT_MTU_MAX;
    event_count = 0;

    uint8_t pdu[BLE_GATT_CLIENT_MTU_MAX] = {0x1b, 0x34, 0x12};
    memset(pdu + 3, 0x5a, BLE_GATT_ATT_VALUE_MAX + 1);
    assert(ble_gatt_client_receive(&client, pdu,
        BLE_GATT_ATT_VALUE_MAX + 4) == 1);
    assert(event_count == 0);
    pdu[0] = 0x1d;
    sent_len = 0;
    assert(ble_gatt_client_receive(&client, pdu,
        BLE_GATT_ATT_VALUE_MAX + 4) == 1);
    assert(event_count == 0 && sent_len == 1 && sent[0] == 0x1e);

    assert(ble_gatt_client_read(&client, 1, 1));
    uint8_t oversized_read[BLE_GATT_ATT_VALUE_MAX + 2] = {0x0b};
    assert(ble_gatt_client_receive(&client, oversized_read,
                                   sizeof(oversized_read)) == -1);
    assert(result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_read(&client, 1, 2));
    uint8_t maximum_read[BLE_GATT_ATT_VALUE_MAX + 1] = {0x0b};
    assert(ble_gatt_client_receive(&client, maximum_read,
                                   sizeof(maximum_read)) == 1);
    assert(result_status == 0);

    assert(ble_gatt_client_read_blob(&client, 1, 511, 3));
    const uint8_t overrun_blob[] = {0x0d, 0xaa, 0xbb};
    assert(ble_gatt_client_receive(&client, overrun_blob,
                                   sizeof(overrun_blob)) == -1);
    assert(result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    const uint16_t handles[] = {1, 2};
    assert(ble_gatt_client_read_multiple(&client, handles, 2, 1, 4));
    uint8_t oversized_variable[BLE_GATT_CLIENT_MTU_MAX] = {0x21, 1, 2};
    memset(oversized_variable + 3, 0x6b, sizeof(oversized_variable) - 3);
    assert(ble_gatt_client_receive(&client, oversized_variable,
        sizeof(oversized_variable)) == -1);
    assert(result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_read_multiple(&client, handles, 2, 1, 5));
    uint8_t maximum_variable[BLE_GATT_CLIENT_MTU_MAX] = {0x21, 0, 0, 0, 2};
    memset(maximum_variable + 5, 0x6b, sizeof(maximum_variable) - 5);
    assert(ble_gatt_client_receive(&client, maximum_variable,
        sizeof(maximum_variable)) == 1);
    assert(result_status == 0);

    assert(!ble_gatt_client_write_command(&client, 1, pdu,
                                          BLE_GATT_ATT_VALUE_MAX + 1));
    assert(!ble_gatt_client_read_blob(&client, 1,
                                      BLE_GATT_ATT_VALUE_MAX + 1, 3));
    assert(!ble_gatt_client_prepare_write(&client, 1,
        BLE_GATT_ATT_VALUE_MAX, pdu, 1, 4));
}

static void test_error_response_handle_matching(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    result_count = 0;

    assert(ble_gatt_client_read(&client, 1, 1));
    const uint8_t wrong_handle[] = {0x01, 0x0a, 2, 0, 0x01};
    assert(ble_gatt_client_receive(&client, wrong_handle,
        sizeof(wrong_handle)) == -1);
    assert(!client.pending && result_count == 1 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    const uint16_t handles[] = {1, 2};
    assert(ble_gatt_client_read_multiple(&client, handles, 2, 0, 2));
    const uint8_t matching_list_handle[] = {0x01, 0x0e, 2, 0, 0x02};
    assert(ble_gatt_client_receive(&client, matching_list_handle,
        sizeof(matching_list_handle)) == 1);
    assert(!client.pending && result_count == 2 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);

    assert(ble_gatt_client_read_multiple(&client, handles, 2, 0, 3));
    const uint8_t wrong_list_handle[] = {0x01, 0x0e, 3, 0, 0x02};
    assert(ble_gatt_client_receive(&client, wrong_list_handle,
        sizeof(wrong_list_handle)) == -1);
    assert(!client.pending && result_count == 3 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_exchange_mtu(&client, 23, 4));
    const uint8_t unsupported_mtu[] = {0x01, 0x02, 0, 0, 0x06};
    assert(ble_gatt_client_receive(&client, unsupported_mtu,
        sizeof(unsupported_mtu)) == 1);
    assert(!client.pending && result_count == 4 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);

    const uint8_t invalid_handle_read[] = {0x0a, 0, 0};
    assert(ble_gatt_client_request(&client, invalid_handle_read,
        sizeof(invalid_handle_read), 0x0b, 5));
    const uint8_t invalid_handle_error[] = {0x01, 0x0a, 0, 0, 0x01};
    assert(ble_gatt_client_receive(&client, invalid_handle_error,
        sizeof(invalid_handle_error)) == 1);
    assert(!client.pending && result_count == 5 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);

    assert(ble_gatt_client_read(&client, 1, 6));
    const uint8_t truncated_error[] = {0x01, 0x0a, 1, 0};
    assert(ble_gatt_client_receive(&client, truncated_error,
        sizeof(truncated_error)) == -1);
    assert(!client.pending && result_count == 6 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_execute_write(&client, 1, 7));
    const uint8_t execute_application_error[] = {
        0x01, 0x18, 7, 0, 0x80
    };
    assert(ble_gatt_client_receive(&client, execute_application_error,
        sizeof(execute_application_error)) == 1);
    assert(!client.pending && result_count == 7 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);
}

static void test_unexpected_response_fails_transaction(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    result_count = 0;
    assert(ble_gatt_client_read(&client, 0x1234, 1));
    const uint8_t wrong_response[] = {0x13}; // Write Response to a Read.
    assert(ble_gatt_client_receive(&client, wrong_response,
        sizeof(wrong_response)) == -1);
    assert(!client.pending && !client.request_len && !client.operation &&
           result_count == 1 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);
}

static void test_unsolicited_response_is_rejected(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    result_count = 0;
    const uint8_t response_without_request[] = {0x0b, 0x5a};
    assert(ble_gatt_client_receive(&client, response_without_request,
        sizeof(response_without_request)) == -1);
    assert(result_count == 0);
}

int main(void) {
    ble_gatt_client client;
    ble_gatt_client_init(&client, send_pdu, on_result, on_event, on_event,
                         NULL);
    const uint8_t request[] = {0x0a, 0x01, 0x00};
    assert(ble_gatt_client_request(&client, request, sizeof(request),
                                   0x0b, 100));
    assert(client.pending && sent_len == sizeof(request));
    assert(!ble_gatt_client_request(&client, request, sizeof(request),
                                    0x0b, 100));
    const uint8_t response[] = {0x0b, 0x42};
    assert(ble_gatt_client_receive(&client, response, sizeof(response)) == 1);
    assert(!client.pending && result_count == 1 && result_status == 0);

    assert(ble_gatt_client_request(&client, request, sizeof(request),
                                   0x0b, 200));
    const uint8_t error[] = {0x01, 0x0a, 0x01, 0x00, 0x02};
    assert(ble_gatt_client_receive(&client, error, sizeof(error)) == 1);
    assert(result_count == 2 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);

    assert(ble_gatt_client_request(&client, request, sizeof(request),
                                   0x0b, 300));
    assert(!ble_gatt_client_poll(&client, 30299));
    assert(ble_gatt_client_poll(&client, 30300));
    assert(result_count == 3 && result_status == BLE_GATT_CLIENT_TIMEOUT);
    assert(client.bearer_failed);
    assert(!ble_gatt_client_request(&client, request, sizeof(request),
                                    0x0b, 30301));

    const uint8_t notification[] = {0x1b, 0x34, 0x12, 0x5a};
    assert(ble_gatt_client_receive(&client, notification,
                                   sizeof(notification)) == -1);
    ble_gatt_client_reset(&client);
    const uint8_t invalid_notification[] = {0x1b, 0, 0, 0x5a};
    assert(ble_gatt_client_receive(&client, invalid_notification,
                                   sizeof(invalid_notification)) == 1);
    assert(event_count == 0);
    assert(ble_gatt_client_receive(&client, notification,
                                   sizeof(notification)) == 1);
    const uint8_t multiple_notification[] = {
        0x23, 0x34, 0x12, 1, 0, 0x5a, 0x78, 0x56, 1, 0, 0x6b
    };
    assert(ble_gatt_client_receive(&client, multiple_notification,
                                   sizeof(multiple_notification)) == 1);
    assert(event_count == 3);
    const uint8_t multiple_with_invalid_handle[] = {
        0x23, 0, 0, 1, 0, 0x5a, 0x78, 0x56, 1, 0, 0x6b
    };
    assert(ble_gatt_client_receive(&client, multiple_with_invalid_handle,
                                   sizeof(multiple_with_invalid_handle)) == 1);
    assert(event_count == 4);
    const uint8_t malformed_multiple_notification[] = {
        0x23, 0x34, 0x12, 2, 0, 0x5a, 0x78
    };
    assert(ble_gatt_client_receive(&client, malformed_multiple_notification,
                                   sizeof(malformed_multiple_notification)) == -1);
    assert(event_count == 4);
    const uint8_t invalid_indication[] = {0x1d, 0, 0, 0x5a};
    assert(ble_gatt_client_receive(&client, invalid_indication,
                                   sizeof(invalid_indication)) == 1);
    assert(event_count == 4 && sent_len == 1 && sent[0] == 0x1e);
    const uint8_t indication[] = {0x1d, 0x34, 0x12, 0x5a};
    assert(ble_gatt_client_receive(&client, indication,
                                   sizeof(indication)) == 1);
    assert(event_count == 5 && sent_len == 1 && sent[0] == 0x1e);

    assert(ble_gatt_client_exchange_mtu(&client, 100, 40000));
    assert(sent_len == 3 && sent[0] == 0x02 && sent[1] == 100);
    const uint8_t mtu_response[] = {0x03, 80, 0};
    assert(ble_gatt_client_receive(&client, mtu_response,
                                   sizeof(mtu_response)) == 1);
    assert(client.mtu == 80);
    assert(!ble_gatt_client_exchange_mtu(&client, 90, 40500));
    assert(ble_gatt_client_set_cccd(&client, 0x0004, 1, 1, 40500));
    assert(sent_len == 5 && sent[0] == 0x12 && sent[1] == 4 &&
           sent[3] == 3 && sent[4] == 0);
    const uint8_t cccd_write_response[] = {0x13};
    assert(ble_gatt_client_receive(&client, cccd_write_response,
                                   sizeof(cccd_write_response)) == 1);

    const uint8_t service_uuid[] = {0x0f, 0x18};
    assert(ble_gatt_client_discover_services(&client, 1, 0xffff,
        service_uuid, sizeof(service_uuid), 41000));
    assert(sent_len == 9 && sent[0] == 0x06 && sent[5] == 0x00 &&
           sent[6] == 0x28 && sent[7] == 0x0f && sent[8] == 0x18);
    const uint8_t service_response[] = {0x07, 1, 0, 5, 0};
    assert(ble_gatt_client_receive(&client, service_response,
                                   sizeof(service_response)) == 1);

    assert(ble_gatt_client_read(&client, 0x0004, 42000));
    assert(sent_len == 3 && sent[0] == 0x0a && sent[1] == 4);
    const uint8_t read_response[] = {0x0b, 0x33};
    assert(ble_gatt_client_receive(&client, read_response,
                                   sizeof(read_response)) == 1);
    assert(ble_gatt_client_write_command(&client, 0x0004,
                                         read_response + 1, 1));
    assert(sent_len == 4 && sent[0] == 0x52 && sent[3] == 0x33);
    assert(ble_gatt_client_prepare_write(&client, 0x0004, 3,
        read_response + 1, 1, 43000));
    assert(sent_len == 6 && sent[0] == 0x16 && sent[3] == 3);
    const uint8_t prepare_response[] = {0x17, 4, 0, 3, 0, 0x33};
    assert(ble_gatt_client_receive(&client, prepare_response,
                                   sizeof(prepare_response)) == 1);
    assert(ble_gatt_client_execute_write(&client, 1, 44000));
    assert(sent_len == 2 && sent[0] == 0x18 && sent[1] == 1);
    const uint8_t execute_response[] = {0x19};
    assert(ble_gatt_client_receive(&client, execute_response,
                                   sizeof(execute_response)) == 1);

    ble_gatt_client_set_signer(&client, sign_pdu);
    assert(ble_gatt_client_write_signed(&client, 0x0004,
                                        read_response + 1, 1));
    assert(sent_len == 16 && sent[0] == 0xd2 && sent[1] == 4 &&
           sent[3] == 0x33 && sent[4] == 0xa5 && sent[12] == 1);

    assert(ble_gatt_client_read_long(&client, 0x0004, 44500));
    uint8_t first_long_part[80] = {0x0b};
    for (uint8_t i = 1; i < sizeof(first_long_part); i++)
        first_long_part[i] = i;
    assert(ble_gatt_client_receive(&client, first_long_part,
                                   sizeof(first_long_part)) == 1);
    assert(client.pending && sent_len == 5 && sent[0] == 0x0c &&
           sent[3] == 79);
    const uint8_t last_long_part[] = {0x0d, 0xa1, 0xa2};
    assert(ble_gatt_client_receive(&client, last_long_part,
                                   sizeof(last_long_part)) == 1);
    assert(client.long_value_len == 81 && client.long_value[78] == 79 &&
           client.long_value[79] == 0xa1 && client.long_value[80] == 0xa2 &&
           result_count == 10);

    uint8_t long_write[100];
    for (uint8_t i = 0; i < sizeof(long_write); i++) long_write[i] = i;
    assert(ble_gatt_client_write_long(&client, 0x0004, long_write,
                                      sizeof(long_write), 44600));
    assert(sent_len == 80 && sent[0] == 0x16 && sent[5] == 0);
    uint8_t prepare_echo[BLE_GATT_CLIENT_MTU_MAX];
    memcpy(prepare_echo, sent, sent_len);
    prepare_echo[0] = 0x17;
    assert(ble_gatt_client_receive(&client, prepare_echo, sent_len) == 1);
    assert(client.pending && sent_len == 30 && sent[0] == 0x16 &&
           sent[3] == 75 && sent[5] == 75);
    memcpy(prepare_echo, sent, sent_len);
    prepare_echo[0] = 0x17;
    assert(ble_gatt_client_receive(&client, prepare_echo, sent_len) == 1);
    assert(client.pending && sent_len == 2 && sent[0] == 0x18 && sent[1] == 1);
    assert(ble_gatt_client_receive(&client, execute_response,
                                   sizeof(execute_response)) == 1);
    assert(result_count == 11 && result_status == 0);

    assert(ble_gatt_client_write_long(&client, 0x0004, long_write,
                                      sizeof(long_write), 44700));
    const uint8_t prepare_error[] = {0x01, 0x16, 4, 0, 0x0d};
    assert(ble_gatt_client_receive(&client, prepare_error,
                                   sizeof(prepare_error)) == 1);
    assert(client.pending && sent_len == 2 && sent[0] == 0x18 && sent[1] == 0);
    assert(ble_gatt_client_receive(&client, execute_response,
                                   sizeof(execute_response)) == 1);
    assert(result_count == 12 &&
           result_status == BLE_GATT_CLIENT_REMOTE_ERROR);

    assert(ble_gatt_client_discover_descriptors(&client, 1, 2, 45000));
    const uint8_t malformed_info[] = {0x05, 0x01, 0x01, 0x00};
    assert(ble_gatt_client_receive(&client, malformed_info,
                                   sizeof(malformed_info)) == -1);
    assert(!client.pending && result_count == 13 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_discover_descriptors(&client, 1, 2, 45100));
    const uint8_t descending_info[] = {
        0x05, 0x01, 0x02, 0x00, 0x02, 0x29, 0x01, 0x00, 0x01, 0x29
    };
    assert(ble_gatt_client_receive(&client, descending_info,
        sizeof(descending_info)) == -1);
    assert(!client.pending && result_count == 14 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_discover_services(&client, 1, 0xffff, NULL, 0,
                                              45200));
    const uint8_t invalid_group_range[] = {
        0x11, 0x06, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x18
    };
    assert(ble_gatt_client_receive(&client, invalid_group_range,
        sizeof(invalid_group_range)) == -1);
    assert(!client.pending && result_count == 15 &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    ble_gatt_client_reset(&client);
    assert(client.mtu == 23 && !client.pending);
    assert(ble_gatt_client_read(&client, 1, 50000));
    ble_gatt_client_reset(&client);
    assert(result_count == 16 &&
           result_status == BLE_GATT_CLIENT_DISCONNECTED);

    const uint16_t multiple_handles[] = {1, 2};
    assert(ble_gatt_client_read_multiple(&client, multiple_handles, 2, 1,
                                         51000));
    uint8_t truncated_multiple[23] = {0x21, 30, 0};
    memset(truncated_multiple + 3, 0x5a, sizeof(truncated_multiple) - 3);
    assert(ble_gatt_client_receive(&client, truncated_multiple,
                                   sizeof(truncated_multiple)) == 1);
    assert(!client.pending && result_status == 0);

    assert(ble_gatt_client_read_multiple(&client, multiple_handles, 2, 1,
                                         51100));
    const uint8_t short_truncated_multiple[] = {0x21, 30, 0, 0x5a};
    assert(ble_gatt_client_receive(&client, short_truncated_multiple,
        sizeof(short_truncated_multiple)) == -1);
    assert(!client.pending &&
           result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    const uint8_t empty_value_uuid[] = {0x34, 0x12};
    assert(ble_gatt_client_read_by_uuid(&client, 1, 0xffff,
        empty_value_uuid, sizeof(empty_value_uuid), 51200));
    const uint8_t empty_value_by_type[] = {0x09, 2, 1, 0};
    assert(ble_gatt_client_receive(&client, empty_value_by_type,
        sizeof(empty_value_by_type)) == 1);
    assert(result_status == 0);

    assert(ble_gatt_client_read_by_uuid(&client, 1, 0xffff,
        empty_value_uuid, sizeof(empty_value_uuid), 51300));
    const uint8_t malformed_empty_by_type[] = {0x09, 2, 1};
    assert(ble_gatt_client_receive(&client, malformed_empty_by_type,
        sizeof(malformed_empty_by_type)) == -1);
    assert(result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_discover_characteristics(&client, 1, 0xffff,
                                                     NULL, 0, 51400));
    const uint8_t characteristic16[] = {
        0x09, 7, 2, 0, 0x12, 3, 0, 0x19, 0x2a
    };
    assert(ble_gatt_client_receive(&client, characteristic16,
        sizeof(characteristic16)) == 1);
    assert(result_status == 0);

    // A partial discovery range may end at the declaration handle while the
    // characteristic value attribute follows just outside that range.
    assert(ble_gatt_client_discover_characteristics(&client, 2, 2,
        NULL, 0, 51425));
    assert(ble_gatt_client_receive(&client, characteristic16,
        sizeof(characteristic16)) == 1);
    assert(result_status == 0);

    const uint8_t characteristic_uuid128[] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0, 0, 0x80,
        0, 0x10, 0, 0, 0, 0, 0x03, 0x28
    };
    assert(ble_gatt_client_discover_characteristics(&client, 1, 0xffff,
        characteristic_uuid128, sizeof(characteristic_uuid128), 51450));
    assert(ble_gatt_client_receive(&client, characteristic16,
        sizeof(characteristic16)) == 1);
    assert(result_status == 0);

    assert(ble_gatt_client_discover_characteristics(&client, 1, 0xffff,
                                                     NULL, 0, 51500));
    const uint8_t characteristic128[] = {
        0x09, 21, 2, 0, 0x12, 3, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    assert(ble_gatt_client_receive(&client, characteristic128,
        sizeof(characteristic128)) == 1);
    assert(result_status == 0);

    assert(ble_gatt_client_discover_characteristics(&client, 1, 0xffff,
                                                     NULL, 0, 51600));
    const uint8_t wrong_characteristic_size[] = {
        0x09, 6, 2, 0, 0x12, 3, 0
    };
    assert(ble_gatt_client_receive(&client, wrong_characteristic_size,
        sizeof(wrong_characteristic_size)) == -1);
    assert(result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR);

    assert(ble_gatt_client_discover_included_services(&client, 1, 0xffff,
                                                       51700));
    const uint8_t included16[] = {
        0x09, 8, 2, 0, 4, 0, 8, 0, 0x0f, 0x18
    };
    assert(ble_gatt_client_receive(&client, included16,
        sizeof(included16)) == 1);
    assert(result_status == 0);

    assert(ble_gatt_client_discover_included_services(&client, 1, 0xffff,
                                                       51800));
    const uint8_t included128[] = {0x09, 6, 2, 0, 4, 0, 8, 0};
    assert(ble_gatt_client_receive(&client, included128,
        sizeof(included128)) == 1);
    assert(result_status == 0);
    test_attribute_value_limit();
    test_error_response_handle_matching();
    test_unexpected_response_fails_transaction();
    test_unsolicited_response_is_rejected();
    test_long_operation_deadlines_refresh();
    test_mtu_peer_below_default_keeps_default();
    return 0;
}
