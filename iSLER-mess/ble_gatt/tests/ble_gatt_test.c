#include <assert.h>
#include <stdint.h>
#include <string.h>

// gap_connection.h is included before gap_security.h in GAP.
int gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type);
int gap_pair(void);

#include "../../ble_crypto.h"
#define MESH_GATT_EAD_SUPPORT 1
#define MESH_GATT_RPA_ONLY_SUPPORT 1
#include "../../ble_mesh/mesh_gatt.h"

static uint8_t received_type, received_pdu[64];
static size_t received_len;
static uint32_t fake_now_ms;
static uint8_t provisioning_received[MESH_GATT_PROVISIONING_PDU_MAX];
static size_t provisioning_received_len;
static uint8_t provisioning_link_events[4], provisioning_link_event_count;

static int authorize_gatt_access(
    void *context, uint16_t handle,
                                 uint8_t write
) {
    (void)handle;
    (void)write;
    return *(const uint8_t *)context;
}

uint32_t GET_MILLIS(void) { return fake_now_ms; }

static int capture_proxy_pdu(
    uint8_t type, const uint8_t *pdu, size_t len,
                             void *context
) {
    (void)context;
    assert(len <= sizeof(received_pdu));
    received_type = type;
    received_len = len;
    memcpy(received_pdu, pdu, len);
    return 1;
}

static int capture_provisioning_pdu(
    const uint8_t *pdu, size_t len,
                                    void *context
) {
    (void)context;
    assert(len <= sizeof(provisioning_received));
    memcpy(provisioning_received, pdu, len);
    provisioning_received_len = len;
    return 1;
}

static void capture_provisioning_link(uint8_t open, void *context) {
    (void)context;
    assert(provisioning_link_event_count < sizeof(provisioning_link_events));
    provisioning_link_events[provisioning_link_event_count++] = open;
}

static void mesh_link_setup(void) {
    assert(mesh_gatt_ensure_initialized());
    mesh_gatt_link_reset();
    ble_gatt_server_link_reset(&mesh_gatt.server);
    mesh_gatt.connected = 1;
}

static void set_cccd(uint16_t handle, uint16_t value) {
    ble_gatt_attribute *cccd = ble_gatt_server_find(&mesh_gatt.server, handle);
    assert(cccd && (cccd->flags & BLE_GATT_ATTRIBUTE_CCCD));
    cccd->cccd = value;
}

static void test_mesh_services_registered_in_generic_database(void) {
    assert(mesh_gatt_ensure_initialized());
    assert(mesh_gatt.server.count == 27);
    assert(mesh_gatt.server.attributes[0].handle ==
           MESH_GATT_HANDLE_PROXY_SERVICE);
    assert(ble_gatt_server_u16(ble_gatt_attribute_value(&mesh_gatt.server,
           &mesh_gatt.server.attributes[0])) == MESH_GATT_PROXY_SERVICE_UUID);
    assert(mesh_gatt.server.attributes[MESH_GATT_HANDLE_DATA_IN - 1].
           properties == BLE_GATT_PROP_WRITE_NO_RSP);
    assert(mesh_gatt.server.attributes[MESH_GATT_HANDLE_DATA_OUT - 1].
           properties == BLE_GATT_PROP_NOTIFY);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_DATA_OUT_CCCD - 1].flags &
           BLE_GATT_ATTRIBUTE_CCCD);
    assert(ble_gatt_server_u16(ble_gatt_attribute_value(&mesh_gatt.server,
           &mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_PROVISIONING_SERVICE - 1])) ==
           MESH_GATT_PROVISIONING_SERVICE_UUID);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_PROVISIONING_DATA_IN - 1].write ==
           mesh_gatt_data_in_write);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_SERVICE - 1].flags &
           BLE_GATT_ATTRIBUTE_PRIMARY_SERVICE);
    assert(ble_gatt_server_u16(ble_gatt_attribute_value(&mesh_gatt.server,
           &mesh_gatt.server.attributes[
               MESH_GATT_HANDLE_GAP_SERVICE - 1])) ==
           MESH_GATT_GAP_SERVICE_UUID);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_DEVICE_NAME - 1].permissions ==
           BLE_GATT_PERM_READ_AUTHENTICATED);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_CAR - 1].permissions ==
           BLE_GATT_PERM_READ);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_SECURITY_LEVELS - 1].permissions ==
           BLE_GATT_PERM_READ);
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_RPA_ONLY - 1].permissions == BLE_GATT_PERM_READ);
    assert(mesh_gatt.server.attributes[MESH_GATT_HANDLE_GAP_EDKM - 1].permissions ==
           (BLE_GATT_PERM_READ_AUTHENTICATED |
            BLE_GATT_PERM_READ_AUTHORIZED));
}

static void test_gap_service_characteristics(void) {
    assert(mesh_gatt_ensure_initialized());
    const uint8_t flags[] = {2, 0x01, 0x06};
    assert(gap_adv_start_connectable(flags, sizeof(flags),
                                                   NULL, 0, 100));
    mesh_gatt_gap_policy_update();
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_DEVICE_NAME - 1].permissions ==
           BLE_GATT_PERM_READ);
    uint8_t response[64];
    uint16_t response_len;
    const uint8_t read_name[] = {
        0x0a, MESH_GATT_HANDLE_GAP_DEVICE_NAME, 0
    };
    assert(ble_gatt_server_att(&mesh_gatt.server, read_name,
        sizeof(read_name), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b);
    assert(response_len == 1 + sizeof(MESH_GATT_DEVICE_NAME) - 1);
    assert(!memcmp(response + 1, MESH_GATT_DEVICE_NAME,
                   sizeof(MESH_GATT_DEVICE_NAME) - 1));

    const uint8_t custom_name[] = "Desk";
    assert(mesh_gatt_gap_device_name_set(custom_name,
                                         sizeof(custom_name) - 1));
    const uint8_t invalid_utf8[] = {0xc0, 0xaf};
    assert(!mesh_gatt_gap_device_name_set(invalid_utf8, sizeof(invalid_utf8)));
    uint8_t oversized_name[MESH_GATT_DEVICE_NAME_MAX + 1] = {0};
    assert(!mesh_gatt_gap_device_name_set(oversized_name,
                                           sizeof(oversized_name)));
    assert(ble_gatt_server_att(&mesh_gatt.server, read_name,
        sizeof(read_name), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 1 + sizeof(custom_name) - 1);
    assert(!memcmp(response + 1, custom_name, sizeof(custom_name) - 1));
    assert(mesh_gatt_gap_device_name_set(NULL, 0)); // GAP permits an empty name.
    assert(mesh_gatt_gap_device_name_set((const uint8_t *)MESH_GATT_DEVICE_NAME,
                                         sizeof(MESH_GATT_DEVICE_NAME) - 1));

    const uint8_t read_appearance[] = {
        0x0a, MESH_GATT_HANDLE_GAP_APPEARANCE, 0
    };
    assert(ble_gatt_server_att(&mesh_gatt.server, read_appearance,
        sizeof(read_appearance), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 3);
    assert(ble_gatt_server_u16(response + 1) == MESH_GATT_APPEARANCE);

    const uint8_t read_ppcp[] = {0x0a, MESH_GATT_HANDLE_GAP_PPCP, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, read_ppcp,
        sizeof(read_ppcp), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 9);
    for (uint8_t i = 1; i < response_len; i++) assert(response[i] == 0xff);

    const uint8_t read_car[] = {0x0a, MESH_GATT_HANDLE_GAP_CAR, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, read_car,
        sizeof(read_car), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 2 && response[1] == 1);

    const uint8_t read_security_levels[] = {
        0x0a, MESH_GATT_HANDLE_GAP_SECURITY_LEVELS, 0
    };
    assert(ble_gatt_server_att(&mesh_gatt.server, read_security_levels,
        sizeof(read_security_levels), response, sizeof(response),
        &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 3 &&
           response[1] == 1 && response[2] == 3);

    const uint8_t read_rpa_only[] = {
        0x0a, MESH_GATT_HANDLE_GAP_RPA_ONLY, 0
    };
    assert(ble_gatt_server_att(&mesh_gatt.server, read_rpa_only,
        sizeof(read_rpa_only), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 2 && response[1] == 0);

    uint8_t session_key[16], iv[8], material[24];
    for (uint8_t i = 0; i < sizeof(session_key); i++) session_key[i] = i + 1;
    for (uint8_t i = 0; i < sizeof(iv); i++) iv[i] = i + 0x20;
    memcpy(material, session_key, sizeof(session_key));
    memcpy(material + sizeof(session_key), iv, sizeof(iv));
    assert(gap_ead_key_set(session_key, iv));
    uint8_t allow_authorization = 1;
    assert(mesh_gatt_set_authorizer(authorize_gatt_access,
                                    &allow_authorization));
    ble_gatt_server_set_security(&mesh_gatt.server, 1, 1);
    const uint8_t read_edkm[] = {0x0a, MESH_GATT_HANDLE_GAP_EDKM, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, read_edkm,
        sizeof(read_edkm), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0b && response_len == 23 &&
           !memcmp(response + 1, material, response_len - 1));
    const uint8_t read_edkm_tail[] = {
        0x0c, MESH_GATT_HANDLE_GAP_EDKM, 0, 22, 0
    };
    assert(ble_gatt_server_att(&mesh_gatt.server, read_edkm_tail,
        sizeof(read_edkm_tail), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x0d && response_len == 3 &&
           !memcmp(response + 1, material + 22, 2));
    allow_authorization = 0;
    assert(ble_gatt_server_att(&mesh_gatt.server, read_edkm,
        sizeof(read_edkm), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x01 && response[4] ==
           BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHORIZATION);
    assert(mesh_gatt_set_authorizer(NULL, NULL));
    ble_gatt_server_set_security(&mesh_gatt.server, 0, 0);
    gap_ead_key_clear();

    gap_adv_stop();
    mesh_gatt_gap_policy_update();
    assert(mesh_gatt.server.attributes[
           MESH_GATT_HANDLE_GAP_DEVICE_NAME - 1].permissions ==
           BLE_GATT_PERM_READ_AUTHENTICATED);
    assert(ble_gatt_server_att(&mesh_gatt.server, read_name,
        sizeof(read_name), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x01 && response[4] ==
           BLE_GATT_ATT_ERR_INSUFFICIENT_AUTHENTICATION);
}

static void test_gap_adv_data_helpers(void) {
    uint8_t data[31];
    gap_ad_builder builder;
    assert(gap_ad_builder_init(&builder, data, sizeof(data)));
    assert(gap_ad_add_flags(&builder, 0x06));
    assert(!gap_ad_add_flags(&builder, 0x80));
    const uint8_t name[] = "Sensor";
    assert(gap_ad_add_local_name(&builder, name, sizeof(name) - 1, 1));
    const uint16_t services[] = {0x1800, 0x1801};
    assert(gap_ad_add_uuid16_list(&builder, services, 2, 1));
    assert(gap_ad_add_tx_power(&builder, -8));
    const uint8_t service_payload[] = {0xaa, 0xbb, 0xcc};
    const uint8_t service_uuid[] = {0x0f, 0x18};
    assert(gap_ad_add_service_data(&builder, service_uuid,
        sizeof(service_uuid), service_payload, sizeof(service_payload)));
    assert(builder.len <= sizeof(data));

    size_t offset = 0, value_len;
    uint8_t type;
    const uint8_t *value;
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 1);
    assert(type == GAP_AD_FLAGS && value_len == 1 && value[0] == 0x06);
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 1);
    assert(type == GAP_AD_NAME_COMPLETE && value_len == 6 &&
           !memcmp(value, name, value_len));
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 1);
    assert(type == GAP_AD_UUID16_COMPLETE && value_len == 4 &&
           value[0] == 0x00 && value[1] == 0x18 &&
           value[2] == 0x01 && value[3] == 0x18);
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 1);
    assert(type == GAP_AD_TX_POWER && value_len == 1 && value[0] == 0xf8);
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 1);
    assert(type == GAP_AD_SERVICE_DATA16 && value_len == 5 &&
           value[0] == 0x0f && value[1] == 0x18 &&
           !memcmp(value + 2, service_payload, sizeof(service_payload)));
    assert(gap_ad_parse_next(data, builder.len, &offset, &type, &value,
                            &value_len) == 0);

    uint8_t small[3];
    assert(gap_ad_builder_init(&builder, small, sizeof(small)));
    assert(gap_ad_add_flags(&builder, 0x06));
    assert(!gap_ad_add_tx_power(&builder, 0));
    assert(builder.len == sizeof(small));
    const uint8_t malformed[] = {3, GAP_AD_FLAGS, 0x06};
    offset = 0;
    assert(gap_ad_parse_next(malformed, sizeof(malformed), &offset, &type,
                            &value, &value_len) == -1);
    assert(offset == 0);
}

static void test_generic_att_handles_mesh_attributes(void) {
    mesh_link_setup();
    uint8_t response[517];
    uint16_t response_len;
    const uint8_t exchange[] = {0x02, 100, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, exchange, sizeof(exchange),
        response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x03 && ble_gatt_server_u16(response + 1) ==
           MESH_GATT_ATT_MTU_MAX);
    assert(mesh_gatt.server.mtu == 100);

    const uint8_t discovery[] = {0x10, 1, 0, 21, 0, 0, 0x28};
    assert(ble_gatt_server_att(&mesh_gatt.server, discovery, sizeof(discovery),
        response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x11);
    assert(ble_gatt_server_u16(response + 2) == 1);
    assert(ble_gatt_server_u16(response + 8) == 7);

    const uint8_t enable_proxy_cccd[] = {0x12, 6, 0, 1, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, enable_proxy_cccd,
        sizeof(enable_proxy_cccd), response, sizeof(response), &response_len) == 1);
    assert(response[0] == 0x13 && mesh_gatt_proxy_cccd());

    const uint8_t enable_pb_cccd[] = {0x12, 12, 0, 1, 0};
    assert(ble_gatt_server_att(&mesh_gatt.server, enable_pb_cccd,
        sizeof(enable_pb_cccd), response, sizeof(response), &response_len) == 1);
    assert(mesh_gatt_provisioning_cccd() && mesh_gatt_proxy_cccd());
}

static void test_proxy_sar_and_configuration(void) {
    mesh_link_setup();
    mesh_gatt_proxy_set_rx_callback(capture_proxy_pdu, NULL);
    const uint8_t first[] = {0x40, 0xaa, 0xbb};
    const uint8_t last[] = {0xc0, 0xcc, 0xdd};
    mesh_gatt_proxy_input(first, sizeof(first), 0);
    assert(mesh_gatt.proxy_rx_active && !received_len);
    mesh_gatt_proxy_input(last, sizeof(last), 0);
    assert(!mesh_gatt.proxy_rx_active);
    assert(received_type == MESH_GATT_PROXY_NETWORK && received_len == 4);
    assert(!memcmp(received_pdu, (uint8_t[]){0xaa, 0xbb, 0xcc, 0xdd}, 4));

    const uint8_t set_blacklist[] = {MESH_GATT_PROXY_CONFIGURATION, 0, 1};
    mesh_gatt_proxy_input(set_blacklist, sizeof(set_blacklist), 0);
    assert(mesh_gatt.filter_type == 1 && mesh_gatt.filter_count == 0);
    assert(mesh_gatt.proxy_tx_count == 1);
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;

    const uint8_t network[MESH_GATT_PROXY_NETWORK_PDU_MIN] = {0};
    set_cccd(MESH_GATT_HANDLE_DATA_OUT_CCCD, 1);
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network,
                                 sizeof(network), 0x1201));
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;

    const uint8_t add[] = {MESH_GATT_PROXY_CONFIGURATION, 1, 0x01, 0x12};
    mesh_gatt_proxy_input(add, sizeof(add), 0);
    assert(mesh_gatt.filter_count == 1 && mesh_gatt.filter[0] == 0x1201);
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
    assert(!mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network,
                                  sizeof(network), 0x1201));
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network,
                                 sizeof(network), 0x1202));
}

static void test_mesh_notification_sar_uses_generic_queue(void) {
    mesh_link_setup();
    mesh_gatt.server.mtu = 23;
    set_cccd(MESH_GATT_HANDLE_DATA_OUT_CCCD, 1);
    uint8_t pdu[25];
    for (uint8_t i = 0; i < sizeof(pdu); i++) pdu[i] = i;
    assert(mesh_gatt_proxy_queue(MESH_GATT_PROXY_CONFIGURATION, pdu,
                                  sizeof(pdu)));
    mesh_gatt_notify_poll();
    assert(mesh_gatt.server.event_count == 1);
    assert(mesh_gatt.server.events[0].handle == MESH_GATT_HANDLE_DATA_OUT);
    assert(mesh_gatt.server.event_data[0] == 0x42); // SAR Start + Config.
    assert(mesh_gatt.proxy_tx[0].offset == 19);

    uint8_t att[517];
    uint16_t att_len;
    assert(ble_gatt_server_poll_event(&mesh_gatt.server, 0, att, sizeof(att),
                                      &att_len) == 1);
    assert(att[0] == 0x1b && att[3] == 0x42);
    mesh_gatt_notify_poll();
    assert(mesh_gatt.server.event_count == 1);
    assert(mesh_gatt.server.event_data[0] == 0xc2); // SAR Complete.
    assert(mesh_gatt.server.event_data[1] == 19);
    assert(mesh_gatt.proxy_tx_count == 0);
}

static void test_data_in_write_and_provisioning(void) {
    mesh_link_setup();
    mesh_gatt_proxy_set_rx_callback(capture_proxy_pdu, NULL);
    mesh_gatt_provisioning_set_rx_callback(capture_provisioning_pdu, NULL);
    uint8_t response[517];
    uint16_t response_len;
    const uint8_t proxy_write[] = {0x52, 3, 0, 0x00, 0x11, 0x22};
    assert(ble_gatt_server_att(&mesh_gatt.server, proxy_write,
        sizeof(proxy_write), response, sizeof(response), &response_len) == 0);
    assert(received_type == MESH_GATT_PROXY_NETWORK && received_len == 2);

    const uint8_t start[] = {0x52, 9, 0, 0x43, 0xaa, 0xbb};
    const uint8_t complete[] = {0x52, 9, 0, 0xc3, 0xcc};
    assert(ble_gatt_server_att(&mesh_gatt.server, start, sizeof(start),
        response, sizeof(response), &response_len) == 0);
    assert(!provisioning_received_len && mesh_gatt.proxy_rx_active);
    assert(ble_gatt_server_att(&mesh_gatt.server, complete, sizeof(complete),
        response, sizeof(response), &response_len) == 0);
    assert(provisioning_received_len == 3);
    assert(!memcmp(provisioning_received, (uint8_t[]){0xaa,0xbb,0xcc}, 3));

    set_cccd(MESH_GATT_HANDLE_PROVISIONING_DATA_OUT_CCCD, 1);
    const uint8_t pdu[] = {1, 2, 3};
    assert(mesh_gatt_provisioning_offer(pdu, sizeof(pdu)));
    mesh_gatt_notify_poll();
    assert(mesh_gatt.server.events[0].handle ==
           MESH_GATT_HANDLE_PROVISIONING_DATA_OUT);
    assert(mesh_gatt.server.event_data[0] == MESH_GATT_PROXY_PROVISIONING);
}

static void test_full_65_byte_provisioning_pdu(void) {
    mesh_link_setup();
    mesh_gatt_provisioning_set_rx_callback(capture_provisioning_pdu, NULL);
    uint8_t part[20];
    for (uint8_t i = 0; i < 19; i++) part[i + 1] = i;
    part[0] = 0x43; // Provisioning SAR Start.
    mesh_gatt_proxy_input(part, sizeof(part), 1);
    part[0] = 0x83;
    for (uint8_t i = 0; i < 19; i++) part[i + 1] = (uint8_t)(i + 19);
    mesh_gatt_proxy_input(part, sizeof(part), 1);
    for (uint8_t i = 0; i < 19; i++) part[i + 1] = (uint8_t)(i + 38);
    mesh_gatt_proxy_input(part, sizeof(part), 1);
    part[0] = 0xc3; // Complete with the final eight bytes.
    for (uint8_t i = 0; i < 8; i++) part[i + 1] = (uint8_t)(i + 57);
    mesh_gatt_proxy_input(part, 9, 1);
    assert(provisioning_received_len == 65);
    for (uint8_t i = 0; i < 65; i++) assert(provisioning_received[i] == i);

    set_cccd(MESH_GATT_HANDLE_PROVISIONING_DATA_OUT_CCCD, 1);
    uint8_t public_key[MESH_GATT_PROVISIONING_PDU_MAX];
    memset(public_key, 0x5a, sizeof(public_key));
    assert(mesh_gatt_provisioning_offer(public_key, sizeof(public_key)));
}

static void test_mesh_sar_timeout_and_link_cleanup(void) {
    mesh_link_setup();
    memset(&gap_conn, 0, sizeof(gap_conn));
    gap_conn.active = 1;
    mesh_gatt.proxy_rx_active = 1;
    mesh_gatt.proxy_rx_started_ms = 100;
    fake_now_ms = 100 + MESH_GATT_PROXY_SAR_TIMEOUT_MS;
    assert(mesh_gatt_proxy_sar_timeout_poll());
    assert(!mesh_gatt.proxy_rx_active && mesh_gatt.proxy_sar_disconnect_pending);
    assert(gap_conn.local_terminate_queued &&
           gap_conn.local_terminate_reason == 0x13);

    mesh_gatt.filter_type = 1;
    mesh_gatt.filter_count = 1;
    mesh_gatt.proxy_tx_count = 1;
    mesh_gatt_link_reset();
    ble_gatt_server_link_reset(&mesh_gatt.server);
    assert(!mesh_gatt.connected && !mesh_gatt.filter_type &&
           !mesh_gatt.filter_count && !mesh_gatt.proxy_tx_count);
    assert(!mesh_gatt_proxy_cccd() && !mesh_gatt_provisioning_cccd());
}

static void test_provisioning_link_callbacks(void) {
    provisioning_link_event_count = 0;
    mesh_gatt_provisioning_set_link_callback(capture_provisioning_link, NULL);
    mesh_gatt_provisioning_link_notify(1);
    mesh_gatt_provisioning_link_notify(0);
    assert(provisioning_link_event_count == 2);
    assert(provisioning_link_events[0] == 1 && provisioning_link_events[1] == 0);
}

int main(void) {
    test_mesh_services_registered_in_generic_database();
    test_gap_service_characteristics();
    test_gap_adv_data_helpers();
    test_generic_att_handles_mesh_attributes();
    test_proxy_sar_and_configuration();
    test_mesh_notification_sar_uses_generic_queue();
    test_data_in_write_and_provisioning();
    test_full_65_byte_provisioning_pdu();
    test_mesh_sar_timeout_and_link_cleanup();
    test_provisioning_link_callbacks();
    return 0;
}
