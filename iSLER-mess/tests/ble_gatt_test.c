#include <assert.h>
#include <stdint.h>
#include <string.h>

// ble_gap_connection.h is included before ble_gap_security.h in the current GAP
// header, so declare these cross-header hooks before including the stack.
int mesh_gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type);
int mesh_gap_pair(void);

#include "../mesh_crypto.h"
#include "../ble_gatt.h"

static uint8_t received_type;
static uint8_t received_pdu[8];
static size_t received_len;
static uint32_t fake_now_ms;
static uint8_t provisioning_received[16];
static size_t provisioning_received_len;
static uint8_t provisioning_link_events[2];
static size_t provisioning_link_event_count;

uint32_t GET_MILLIS(void) {
    return fake_now_ms;
}

static int capture_proxy_pdu(uint8_t type, const uint8_t *pdu, size_t len,
                             void *context) {
    (void)context;
    received_type = type;
    received_len = len;
    assert(len <= sizeof(received_pdu));
    memcpy(received_pdu, pdu, len);
    return 1;
}

static int capture_provisioning_pdu(const uint8_t *pdu, size_t len,
                                    void *context) {
    (void)context;
    assert(len <= sizeof(provisioning_received));
    memcpy(provisioning_received, pdu, len);
    provisioning_received_len = len;
    return 1;
}

static void capture_provisioning_link(uint8_t open, void *context) {
    (void)context;
    assert(provisioning_link_event_count <
           sizeof(provisioning_link_events));
    provisioning_link_events[provisioning_link_event_count++] = open;
}

static void clear_tx(void) {
    mesh_gatt.tx_active = 0;
    mesh_gatt.tx_len = mesh_gatt.tx_offset = 0;
}

static void test_att_mtu_and_blob(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;

    const uint8_t exchange[] = {0x02, 100, 0};
    assert(mesh_gatt_att_request(exchange, sizeof(exchange)));
    assert(mesh_gatt.mtu == 100 && mesh_gatt.mtu_exchanged);
    assert(mesh_gatt.tx_l2cap[4] == 0x03);
    assert(mesh_gatt.tx_l2cap[5] == (uint8_t)MESH_GATT_ATT_MTU_MAX);
    assert(mesh_gatt.tx_l2cap[6] == (uint8_t)(MESH_GATT_ATT_MTU_MAX >> 8));

    clear_tx();
    assert(mesh_gatt_att_request(exchange, sizeof(exchange)));
    assert(mesh_gatt.tx_l2cap[4] == 0x01);
    assert(mesh_gatt.tx_l2cap[8] == 0x06); // Request Not Supported.

    clear_tx();
    const uint8_t read_cccd_tail[] = {0x0c, 6, 0, 1, 0};
    assert(mesh_gatt_att_request(read_cccd_tail, sizeof(read_cccd_tail)));
    assert(mesh_gatt.tx_l2cap[4] == 0x0d);
    assert(mesh_gatt.tx_l2cap[5] == 0);

    clear_tx();
    const uint8_t read_bad_offset[] = {0x0c, 1, 0, 3, 0};
    assert(mesh_gatt_att_request(read_bad_offset, sizeof(read_bad_offset)));
    assert(mesh_gatt.tx_l2cap[4] == 0x01);
    assert(mesh_gatt.tx_l2cap[8] == 0x07); // Invalid Offset.
}

static void test_att_discovery(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;

    const uint8_t find_service[] = {0x10, 1, 0, 0xff, 0xff, 0, 0x28};
    assert(mesh_gatt_att_request(find_service, sizeof(find_service)));
    assert(mesh_gatt.tx_l2cap[4] == 0x11);
    assert(mesh_gatt.tx_l2cap[5] == 6);
    assert(mesh_gatt.tx_l2cap[6] == 1 && mesh_gatt.tx_l2cap[7] == 0);
    assert(mesh_gatt.tx_l2cap[8] == 6 && mesh_gatt.tx_l2cap[9] == 0);
    assert(mesh_gatt.tx_l2cap[10] == 0x28 && mesh_gatt.tx_l2cap[11] == 0x18);

    clear_tx();
    const uint8_t find_service_by_value[] = {
        0x06, 1, 0, 0xff, 0xff, 0, 0x28, 0x28, 0x18
    };
    assert(mesh_gatt_att_request(find_service_by_value,
                                 sizeof(find_service_by_value)));
    assert(mesh_gatt.tx_l2cap[4] == 0x07);
    assert(mesh_gatt.tx_l2cap[5] == 1 && mesh_gatt.tx_l2cap[6] == 0);
    assert(mesh_gatt.tx_l2cap[7] == 6 && mesh_gatt.tx_l2cap[8] == 0);

    clear_tx();
    const uint8_t service_outside_range[] = {
        0x06, 2, 0, 0xff, 0xff, 0, 0x28, 0x28, 0x18
    };
    assert(mesh_gatt_att_request(service_outside_range,
                                 sizeof(service_outside_range)));
    assert(mesh_gatt.tx_l2cap[8] == 0x0a); // Attribute Not Found.

    clear_tx();
    const uint8_t find_characteristics[] = {0x08, 1, 0, 6, 0, 3, 0x28};
    assert(mesh_gatt_att_request(find_characteristics,
                                 sizeof(find_characteristics)));
    assert(mesh_gatt.tx_l2cap[4] == 0x09);
    assert(mesh_gatt.tx_l2cap[5] == 7);
    assert(mesh_gatt.tx_l2cap[6] == 2 && mesh_gatt.tx_l2cap[13] == 4);
    assert(mesh_gatt.tx_l2cap[11] == 0xdd && mesh_gatt.tx_l2cap[18] == 0xde);

    clear_tx();
    const uint8_t find_information[] = {0x04, 1, 0, 6, 0};
    assert(mesh_gatt_att_request(find_information, sizeof(find_information)));
    assert(mesh_gatt.tx_l2cap[4] == 0x05 && mesh_gatt.tx_l2cap[5] == 1);
    assert(mesh_gatt.tx_l2cap[0] == 22); // Five 4-byte entries fit at MTU 23.
    assert(mesh_gatt.tx_l2cap[6] == 1 && mesh_gatt.tx_l2cap[10] == 2);
    assert(mesh_gatt.tx_l2cap[14] == 3 && mesh_gatt.tx_l2cap[18] == 4);

    clear_tx();
    const uint8_t find_cccd[] = {0x04, 6, 0, 6, 0};
    assert(mesh_gatt_att_request(find_cccd, sizeof(find_cccd)));
    assert(mesh_gatt.tx_l2cap[4] == 0x05);
    assert(mesh_gatt.tx_l2cap[6] == 6 && mesh_gatt.tx_l2cap[8] == 0x02 &&
           mesh_gatt.tx_l2cap[9] == 0x29);
}

static void test_att_l2cap_fragment_reassembly(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.mtu = 23;
    const uint8_t first[] = {3, 0, 4, 0};
    const uint8_t continuation[] = {0x0a, 1, 0};
    mesh_gatt_receive_fragment(2, first, sizeof(first));
    assert(mesh_gatt.rx_active && !mesh_gatt.rx_att_pending);
    mesh_gatt_receive_fragment(1, continuation, sizeof(continuation));
    assert(!mesh_gatt.rx_active && mesh_gatt.rx_att_pending);
    assert(mesh_gatt.att_rx_len == 3);
    assert(!memcmp(mesh_gatt.att_rx, (uint8_t[]){0x0a, 1, 0}, 3));

    mesh_gatt.rx_att_pending = 0;
    const uint8_t invalid_cid[] = {3, 0, 6, 0, 0x0a, 1, 0};
    mesh_gatt_receive_fragment(2, invalid_cid, sizeof(invalid_cid));
    assert(!mesh_gatt.rx_att_pending);

    const uint8_t overrun[] = {2, 0, 4, 0, 0x0a, 1, 0};
    mesh_gatt_receive_fragment(2, overrun, sizeof(overrun));
    assert(!mesh_gatt.rx_active && !mesh_gatt.rx_att_pending);
}

static void test_attribute_write_permissions(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;
    const uint8_t write_data_in[] = {0x12, 3, 0, 0, 0};
    assert(mesh_gatt_att_request(write_data_in, sizeof(write_data_in)));
    assert(mesh_gatt.tx_l2cap[4] == 0x01);
    assert(mesh_gatt.tx_l2cap[8] == 0x03); // Write Not Permitted.

    clear_tx();
    const uint8_t short_cccd_write[] = {0x12, 6, 0, 1};
    assert(mesh_gatt_att_request(short_cccd_write, sizeof(short_cccd_write)));
    assert(mesh_gatt.tx_l2cap[8] == 0x0d); // Invalid Attribute Value Length.

    clear_tx();
    const uint8_t invalid_cccd_value[] = {0x12, 6, 0, 2, 0};
    assert(mesh_gatt_att_request(invalid_cccd_value, sizeof(invalid_cccd_value)));
    assert(mesh_gatt.tx_l2cap[8] == 0x13); // Value Not Allowed.

    clear_tx();
    const uint8_t enable_cccd[] = {0x12, 6, 0, 1, 0};
    assert(mesh_gatt_att_request(enable_cccd, sizeof(enable_cccd)));
    assert(mesh_gatt.cccd == 1 && mesh_gatt.tx_l2cap[4] == 0x13);
}

static void test_proxy_sar(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt_proxy_set_rx_callback(capture_proxy_pdu, NULL);
    const uint8_t first[] = {0x40, 0xaa, 0xbb};
    const uint8_t last[] = {0xc0, 0xcc, 0xdd};
    mesh_gatt_proxy_input(first, sizeof(first), 0);
    assert(mesh_gatt.proxy_rx_active && !received_len);
    mesh_gatt_proxy_input(last, sizeof(last), 0);
    assert(!mesh_gatt.proxy_rx_active);
    assert(received_type == MESH_GATT_PROXY_NETWORK && received_len == 4);
    assert(!memcmp(received_pdu, (uint8_t[]){0xaa, 0xbb, 0xcc, 0xdd}, 4));

    uint8_t oversized[MESH_GATT_PROXY_PDU_MAX + 2] = {0x40};
    mesh_gatt_proxy_input(oversized, sizeof(oversized), 0);
    assert(!mesh_gatt.proxy_rx_active && !mesh_gatt.proxy_rx_len);
}

static void send_proxy_configuration(const uint8_t *configuration,
                                     size_t len) {
    uint8_t message[MESH_GATT_PROXY_PDU_MAX + 1];
    assert(len <= MESH_GATT_PROXY_PDU_MAX);
    message[0] = MESH_GATT_PROXY_CONFIGURATION;
    memcpy(message + 1, configuration, len);
    mesh_gatt_proxy_input(message, len + 1, 0);
}

static void test_proxy_configuration_and_filter(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;

    const uint8_t set_blacklist[] = {0, 1};
    send_proxy_configuration(set_blacklist, sizeof(set_blacklist));
    assert(mesh_gatt.filter_type == 1 && mesh_gatt.filter_count == 0);
    assert(mesh_gatt.proxy_tx_count == 1);
    assert(mesh_gatt.proxy_tx[0].type == MESH_GATT_PROXY_CONFIGURATION);
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;

    const uint8_t network_pdu[MESH_GATT_PROXY_NETWORK_PDU_MIN] = {0};
    mesh_gatt.cccd = 1;
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                 sizeof(network_pdu), 0x1201));
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;

    const uint8_t add_address[] = {1, 0x01, 0x12};
    send_proxy_configuration(add_address, sizeof(add_address));
    assert(mesh_gatt.filter_count == 1 && mesh_gatt.filter[0] == 0x1201);
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
    assert(!mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                  sizeof(network_pdu), 0x1201));
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                 sizeof(network_pdu), 0x1202));
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;

    const uint8_t remove_address[] = {2, 0x01, 0x12};
    send_proxy_configuration(remove_address, sizeof(remove_address));
    assert(mesh_gatt.filter_count == 0);
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                 sizeof(network_pdu), 0x1201));
}

static void test_proxy_data_out_sar(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = mesh_gatt.cccd = 1;
    mesh_gatt.mtu = 23;
    uint8_t pdu[25];
    for (uint8_t i = 0; i < sizeof(pdu); i++) pdu[i] = i;
    assert(mesh_gatt_proxy_queue(MESH_GATT_PROXY_CONFIGURATION, pdu,
                                 sizeof(pdu)));

    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_active);
    assert(mesh_gatt.tx_l2cap[4] == 0x1b);
    assert(mesh_gatt.tx_l2cap[7] == 0x42); // SAR Start, Configuration.
    assert(mesh_gatt.proxy_tx[0].offset == 19);

    clear_tx(); // The connection layer completed the first notification.
    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_active);
    assert(mesh_gatt.tx_l2cap[7] == 0xc2); // SAR Complete, Configuration.
    assert(mesh_gatt.tx_l2cap[8] == 19);
    assert(mesh_gatt.proxy_tx_count == 0);
}

static void test_disconnect_reset(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt_proxy_set_rx_callback(capture_proxy_pdu, NULL);
    mesh_gatt.connected = mesh_gatt.cccd = mesh_gatt.mtu_exchanged = 1;
    mesh_gatt.mtu = 100;
    mesh_gatt.rx_active = mesh_gatt.rx_att_pending = 1;
    mesh_gatt.proxy_rx_active = 1;
    mesh_gatt.proxy_rx_len = 7;
    mesh_gatt.l2cap_expected = 20;
    mesh_gatt.l2cap_used = 9;
    mesh_gatt.att_rx_len = 5;
    mesh_gatt.tx_active = 1;
    mesh_gatt.tx_len = 12;
    mesh_gatt.tx_offset = 4;
    mesh_gatt.filter_type = 1;
    mesh_gatt.filter_count = 2;
    mesh_gatt.proxy_tx_head = 1;
    mesh_gatt.proxy_tx_count = 3;

    mesh_gatt_link_reset(); // Called by the poller after GAP reports disconnect.

    assert(!mesh_gatt.connected && !mesh_gatt.cccd && !mesh_gatt.mtu_exchanged);
    assert(mesh_gatt.mtu == 23);
    assert(!mesh_gatt.rx_active && !mesh_gatt.rx_att_pending);
    assert(!mesh_gatt.proxy_rx_active && !mesh_gatt.proxy_rx_len);
    assert(!mesh_gatt.l2cap_expected && !mesh_gatt.l2cap_used && !mesh_gatt.att_rx_len);
    assert(!mesh_gatt.tx_active && !mesh_gatt.tx_len && !mesh_gatt.tx_offset);
    assert(!mesh_gatt.filter_type && !mesh_gatt.filter_count);
    assert(!mesh_gatt.proxy_tx_head && !mesh_gatt.proxy_tx_count);
    assert(mesh_gatt.proxy_rx_callback == capture_proxy_pdu);
}

static void test_proxy_sar_timeout_disconnect(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    memset(&gap_conn, 0, sizeof(gap_conn));
    gap_conn.active = 1;
    gap_conn.first_event = 0;
    mesh_gatt.proxy_rx_active = 1;
    mesh_gatt.proxy_rx_started_ms = 100;
    fake_now_ms = 100 + MESH_GATT_PROXY_SAR_TIMEOUT_MS - 1;
    assert(!mesh_gatt_proxy_sar_timeout_poll());
    assert(mesh_gatt.proxy_rx_active);

    fake_now_ms++;
    assert(mesh_gatt_proxy_sar_timeout_poll());
    assert(!mesh_gatt.proxy_rx_active && mesh_gatt.proxy_sar_disconnect_pending);
    assert(gap_conn.local_terminate_queued &&
           gap_conn.local_terminate_reason == 0x13);

    mesh_gatt_link_reset();
    memset(&gap_conn, 0, sizeof(gap_conn));
    gap_conn.active = 1;
    mesh_gatt.proxy_tx_sar_active = 1;
    mesh_gatt.proxy_tx_started_ms = fake_now_ms;
    fake_now_ms += MESH_GATT_PROXY_SAR_TIMEOUT_MS;
    assert(mesh_gatt_proxy_sar_timeout_poll());
    assert(!mesh_gatt.proxy_tx_sar_active && mesh_gatt.proxy_sar_disconnect_pending);
    assert(gap_conn.local_terminate_queued &&
           gap_conn.local_terminate_reason == 0x13);
}

static void test_proxy_client_flow(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;

    const uint8_t discover_service[] = {0x10, 1, 0, 6, 0, 0, 0x28};
    assert(mesh_gatt_att_request(discover_service, sizeof(discover_service)));
    assert(mesh_gatt.tx_l2cap[4] == 0x11);
    clear_tx();

    const uint8_t enable_notifications[] = {0x12, 6, 0, 1, 0};
    assert(mesh_gatt_att_request(enable_notifications,
                                 sizeof(enable_notifications)));
    assert(mesh_gatt.cccd && mesh_gatt.tx_l2cap[4] == 0x13);
    clear_tx();

    const uint8_t set_whitelist[] = {0, 0};
    send_proxy_configuration(set_whitelist, sizeof(set_whitelist));
    assert(mesh_gatt.filter_type == 0 && mesh_gatt.filter_count == 0);
    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_l2cap[4] == 0x1b);
    assert(mesh_gatt.tx_l2cap[7] == MESH_GATT_PROXY_CONFIGURATION);
    assert(mesh_gatt.tx_l2cap[8] == 3); // Filter Status opcode.
    clear_tx();

    const uint8_t add_client_address[] = {1, 0x01, 0x12};
    send_proxy_configuration(add_client_address, sizeof(add_client_address));
    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_l2cap[7] == MESH_GATT_PROXY_CONFIGURATION);
    assert(mesh_gatt.tx_l2cap[8] == 3);
    clear_tx();

    const uint8_t network_pdu[MESH_GATT_PROXY_NETWORK_PDU_MIN] = {0x5a};
    assert(mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                 sizeof(network_pdu), 0x1201));
    assert(!mesh_gatt_proxy_offer(MESH_GATT_PROXY_NETWORK, network_pdu,
                                  sizeof(network_pdu), 0x1202));
    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_l2cap[4] == 0x1b);
    assert(mesh_gatt.tx_l2cap[7] == MESH_GATT_PROXY_NETWORK);
    assert(mesh_gatt.tx_l2cap[8] == 0x5a);
}

static void test_pb_gatt_service(void) {
    memset(&mesh_gatt, 0, sizeof(mesh_gatt));
    mesh_gatt.connected = 1;
    mesh_gatt.mtu = 23;
    mesh_gatt_provisioning_set_rx_callback(capture_provisioning_pdu, NULL);
    mesh_gatt_provisioning_set_link_callback(capture_provisioning_link, NULL);

    const uint8_t discover[] = {0x10, 1, 0, 12, 0, 0, 0x28};
    assert(mesh_gatt_att_request(discover, sizeof(discover)));
    assert(mesh_gatt.tx_l2cap[4] == 0x11 && mesh_gatt.tx_l2cap[5] == 6);
    assert(mesh_gatt.tx_l2cap[6] == 1 && mesh_gatt.tx_l2cap[12] == 7);
    clear_tx();

    const uint8_t enable_pb_notifications[] = {0x12, 12, 0, 1, 0};
    assert(mesh_gatt_att_request(enable_pb_notifications,
                                 sizeof(enable_pb_notifications)));
    assert(mesh_gatt.provisioning_cccd && !mesh_gatt.cccd);
    clear_tx();

    const uint8_t start[] = {0x43, 0xaa, 0xbb};
    const uint8_t complete[] = {0xc3, 0xcc};
    mesh_gatt_proxy_input(start, sizeof(start), 1);
    assert(!provisioning_received_len && mesh_gatt.proxy_rx_active);
    mesh_gatt_proxy_input(complete, sizeof(complete), 1);
    assert(provisioning_received_len == 3);
    assert(!memcmp(provisioning_received, (uint8_t[]){0xaa,0xbb,0xcc}, 3));

    const uint8_t pdu[] = {1, 2, 3};
    assert(mesh_gatt_provisioning_offer(pdu, sizeof(pdu)));
    mesh_gatt_notify_poll();
    assert(mesh_gatt.tx_l2cap[4] == 0x1b);
    assert(mesh_gatt_u16(mesh_gatt.tx_l2cap + 5) ==
           MESH_GATT_HANDLE_PROVISIONING_DATA_OUT);
    assert(mesh_gatt.tx_l2cap[7] == MESH_GATT_PROXY_PROVISIONING);

    clear_tx();
    mesh_gatt.provisioning_cccd = 0;
    assert(!mesh_gatt_provisioning_offer(pdu, sizeof(pdu)));

    provisioning_link_event_count = 0;
    mesh_gatt_provisioning_link_notify(1);
    mesh_gatt_provisioning_link_notify(0);
    assert(provisioning_link_event_count == 2 &&
           provisioning_link_events[0] == 1 &&
           provisioning_link_events[1] == 0);
}

int main(void) {
    test_att_mtu_and_blob();
    test_att_discovery();
    test_att_l2cap_fragment_reassembly();
    test_attribute_write_permissions();
    test_proxy_sar();
    test_proxy_configuration_and_filter();
    test_proxy_data_out_sar();
    test_disconnect_reset();
    test_proxy_sar_timeout_disconnect();
    test_proxy_client_flow();
    test_pb_gatt_service();
    return 0;
}
