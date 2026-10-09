#ifndef MESH_GATT_H
#define MESH_GATT_H

// Bluetooth Mesh services layered on the generic GATT server and ATT/L2CAP
// transport. ATT procedures, attribute storage, CCCDs, and L2CAP framing live
// in the generic GATT module.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../ble_gap/ble_gap.h"
#include "../ble_gatt.h"

#ifndef MESH_GATT_ATT_MTU_MAX
#define MESH_GATT_ATT_MTU_MAX 247
#endif
#if MESH_GATT_ATT_MTU_MAX < 23 || MESH_GATT_ATT_MTU_MAX > BLE_GATT_SERVER_MTU_MAX
#error "MESH_GATT_ATT_MTU_MAX must be between 23 and BLE_GATT_SERVER_MTU_MAX"
#endif
#ifndef MESH_GATT_PROXY_PDU_MAX
#define MESH_GATT_PROXY_PDU_MAX 64
#endif
#ifndef MESH_GATT_PROVISIONING_PDU_MAX
#define MESH_GATT_PROVISIONING_PDU_MAX 65
#endif
#define MESH_GATT_BEARER_PDU_MAX \
    (MESH_GATT_PROXY_PDU_MAX > MESH_GATT_PROVISIONING_PDU_MAX ? \
     MESH_GATT_PROXY_PDU_MAX : MESH_GATT_PROVISIONING_PDU_MAX)
#ifndef MESH_GATT_PROXY_QUEUE_SIZE
#define MESH_GATT_PROXY_QUEUE_SIZE 4
#endif
#ifndef MESH_GATT_PROXY_FILTER_SIZE
#define MESH_GATT_PROXY_FILTER_SIZE 16
#endif
#ifndef MESH_GATT_PROXY_SAR_TIMEOUT_MS
#define MESH_GATT_PROXY_SAR_TIMEOUT_MS 20000u
#endif
#define MESH_GATT_PROXY_NETWORK_PDU_MIN 14
#define MESH_GATT_PROXY_NETWORK_PDU_MAX 29
#define MESH_GATT_PROXY_BEACON_PDU_LEN 22

enum {
    MESH_GATT_PROXY_NETWORK = 0,
    MESH_GATT_PROXY_BEACON = 1,
    MESH_GATT_PROXY_CONFIGURATION = 2,
    MESH_GATT_PROXY_PROVISIONING = 3
};

#ifndef MESH_GATT_EAD_SUPPORT
#define MESH_GATT_EAD_SUPPORT 0
#endif
#ifndef MESH_GATT_RPA_ONLY_SUPPORT
#define MESH_GATT_RPA_ONLY_SUPPORT 0
#endif

enum {
    MESH_GATT_HANDLE_PROXY_SERVICE = 1,
    MESH_GATT_HANDLE_DATA_IN_DECL = 2,
    MESH_GATT_HANDLE_DATA_IN = 3,
    MESH_GATT_HANDLE_DATA_OUT_DECL = 4,
    MESH_GATT_HANDLE_DATA_OUT = 5,
    MESH_GATT_HANDLE_DATA_OUT_CCCD = 6,
    MESH_GATT_HANDLE_PROVISIONING_SERVICE = 7,
    MESH_GATT_HANDLE_PROVISIONING_DATA_IN_DECL = 8,
    MESH_GATT_HANDLE_PROVISIONING_DATA_IN = 9,
    MESH_GATT_HANDLE_PROVISIONING_DATA_OUT_DECL = 10,
    MESH_GATT_HANDLE_PROVISIONING_DATA_OUT = 11,
    MESH_GATT_HANDLE_PROVISIONING_DATA_OUT_CCCD = 12,
    MESH_GATT_HANDLE_GAP_SERVICE = 13,
    MESH_GATT_HANDLE_GAP_DEVICE_NAME_DECL = 14,
    MESH_GATT_HANDLE_GAP_DEVICE_NAME = 15,
    MESH_GATT_HANDLE_GAP_APPEARANCE_DECL = 16,
    MESH_GATT_HANDLE_GAP_APPEARANCE = 17,
    MESH_GATT_HANDLE_GAP_PPCP_DECL = 18,
    MESH_GATT_HANDLE_GAP_PPCP = 19,
    MESH_GATT_HANDLE_GAP_CAR_DECL = 20,
    MESH_GATT_HANDLE_GAP_CAR = 21,
    MESH_GATT_HANDLE_GAP_SECURITY_LEVELS_DECL = 22,
    MESH_GATT_HANDLE_GAP_SECURITY_LEVELS = 23,
#if MESH_GATT_RPA_ONLY_SUPPORT
    MESH_GATT_HANDLE_GAP_RPA_ONLY_DECL = 24,
    MESH_GATT_HANDLE_GAP_RPA_ONLY = 25,
#endif
#if MESH_GATT_EAD_SUPPORT
    MESH_GATT_HANDLE_GAP_EDKM_DECL =
        24 + (MESH_GATT_RPA_ONLY_SUPPORT ? 2 : 0),
    MESH_GATT_HANDLE_GAP_EDKM = MESH_GATT_HANDLE_GAP_EDKM_DECL + 1
#endif
};

typedef int (*mesh_gatt_proxy_rx_fn)(uint8_t type, const uint8_t *pdu,
                                      size_t len, void *context);
typedef int (*mesh_gatt_provisioning_rx_fn)(const uint8_t *pdu, size_t len,
                                             void *context);
typedef void (*mesh_gatt_provisioning_link_fn)(uint8_t open, void *context);

#define MESH_GATT_PROXY_SERVICE_UUID 0x1828
#define MESH_GATT_PROXY_DATA_IN_UUID 0x2ADD
#define MESH_GATT_PROXY_DATA_OUT_UUID 0x2ADE
#define MESH_GATT_PROVISIONING_SERVICE_UUID 0x1827
#define MESH_GATT_PROVISIONING_DATA_IN_UUID 0x2ADB
#define MESH_GATT_PROVISIONING_DATA_OUT_UUID 0x2ADC
#define MESH_GATT_GAP_SERVICE_UUID 0x1800
#define MESH_GATT_GAP_DEVICE_NAME_UUID 0x2A00
#define MESH_GATT_GAP_APPEARANCE_UUID 0x2A01
#define MESH_GATT_GAP_PPCP_UUID 0x2A04
#define MESH_GATT_GAP_CAR_UUID 0x2AA6
#define MESH_GATT_GAP_RPA_ONLY_UUID 0x2AC9
#define MESH_GATT_GAP_SECURITY_LEVELS_UUID 0x2BF5
#define MESH_GATT_GAP_EDKM_UUID 0x2B88
#define MESH_GATT_DEVICE_NAME_MAX 248
#ifndef MESH_GATT_DEVICE_NAME
#define MESH_GATT_DEVICE_NAME "CH-Mess"
#endif
#ifndef MESH_GATT_APPEARANCE
#define MESH_GATT_APPEARANCE 0
#endif

static struct {
    ble_gatt_server server;
    ble_gatt_transport transport;
    uint8_t initialized, connected;
    uint8_t gap_device_name[MESH_GATT_DEVICE_NAME_MAX];
    uint16_t gap_device_name_len;
    uint8_t proxy_rx_active, proxy_rx_type, proxy_rx_service;
    uint8_t filter_type, filter_count;
    uint16_t filter[MESH_GATT_PROXY_FILTER_SIZE];
    uint32_t proxy_rx_started_ms, proxy_tx_started_ms;
    uint8_t proxy_tx_sar_active, proxy_sar_disconnect_pending;
    uint8_t proxy_rx[MESH_GATT_BEARER_PDU_MAX];
    uint16_t proxy_rx_len;
    struct {
        uint8_t type, len, offset;
        uint16_t destination;
        uint8_t data[MESH_GATT_BEARER_PDU_MAX];
    } proxy_tx[MESH_GATT_PROXY_QUEUE_SIZE];
    uint8_t proxy_tx_head, proxy_tx_count;
    mesh_gatt_proxy_rx_fn proxy_rx_callback;
    void *proxy_rx_context;
    mesh_gatt_provisioning_rx_fn provisioning_rx_callback;
    void *provisioning_rx_context;
    mesh_gatt_provisioning_link_fn provisioning_link_callback;
    void *provisioning_link_context;
} mesh_gatt;

static uint16_t mesh_gatt_u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static ble_gatt_uuid mesh_gatt_uuid16(uint16_t value) {
    ble_gatt_uuid uuid = {2, {(uint8_t)value, (uint8_t)(value >> 8)}};
    return uuid;
}

// Check UTF-8 without accepting overlong encodings, surrogate values, or
// codepoints above U+10FFFF. GAP Device Name is limited to 248 octets.
static int mesh_gatt_gap_device_name_valid(const uint8_t *name, size_t len) {
    if ((!name && len) || len > MESH_GATT_DEVICE_NAME_MAX) return 0;
    for (size_t i = 0; i < len;) {
        uint8_t first = name[i++];
        if (first <= 0x7f) continue;
        uint8_t continuation_count;
        uint8_t second_min = 0x80, second_max = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) continuation_count = 1;
        else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            if (first == 0xe0) second_min = 0xa0;
            if (first == 0xed) second_max = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            if (first == 0xf0) second_min = 0x90;
            if (first == 0xf4) second_max = 0x8f;
        } else return 0;
        if (continuation_count > len - i || name[i] < second_min ||
            name[i] > second_max)
            return 0;
        for (uint8_t j = 0; j < continuation_count; j++)
            if (name[i + j] < 0x80 || name[i + j] > 0xbf) return 0;
        i += continuation_count;
    }
    return 1;
}

static uint8_t mesh_gatt_gap_device_name_read(void *context, uint16_t offset,
    uint8_t *out, uint16_t *inout_len) {
    (void)context;
    if (!inout_len || (out == NULL && *inout_len))
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset > mesh_gatt.gap_device_name_len)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    uint16_t len = (uint16_t)(mesh_gatt.gap_device_name_len - offset);
    if (len > *inout_len) len = *inout_len;
    if (len) memcpy(out, mesh_gatt.gap_device_name + offset, len);
    *inout_len = len;
    return 0;
}

#if MESH_GATT_EAD_SUPPORT
static uint8_t mesh_gatt_gap_edkm_read(void *context, uint16_t offset,
    uint8_t *out, uint16_t *inout_len) {
    (void)context;
    uint8_t material[24];
    if (!inout_len || (out == NULL && *inout_len))
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (!gap_ead_key_material_get(material))
        return BLE_GATT_ATT_ERR_INSUFFICIENT_RESOURCES;
    if (offset > sizeof(material)) {
        memset(material, 0, sizeof(material));
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    }
    uint16_t len = (uint16_t)(sizeof(material) - offset);
    if (len > *inout_len) len = *inout_len;
    if (len) memcpy(out, material + offset, len);
    *inout_len = len;
    volatile uint8_t *wipe = material;
    for (size_t i = 0; i < sizeof(material); i++) wipe[i] = 0;
    return 0;
}
#endif

// Bind the generic L2CAP/ATT transport to this application's GAP API. The
// generic transport itself depends only on the operations supplied here.
static int mesh_gatt_gap_connected(void *context) {
    (void)context;
    return gap_connected();
}

static int mesh_gatt_gap_receive(void *context, uint8_t *llid,
                                 uint8_t *data, size_t *len) {
    (void)context;
    return gap_receive_data(llid, data, len);
}

static int mesh_gatt_gap_send(void *context, uint8_t llid,
                              const uint8_t *data, size_t len) {
    (void)context;
    return gap_send_data(llid, data, len);
}

static uint16_t mesh_gatt_gap_max_payload(void *context) {
    (void)context;
    return gap_data_length_get().tx_octets;
}

static void mesh_gatt_gap_security_state(void *context, uint8_t *encrypted,
                                        uint8_t *authenticated) {
    (void)context;
    if (encrypted) *encrypted = gap_encrypted() != 0;
    if (authenticated) *authenticated = gap_authenticated() != 0;
}

static int mesh_gatt_transport_init(ble_gatt_transport *transport,
                                    ble_gatt_server *server) {
    const ble_gatt_transport_ops ops = {
        mesh_gatt_gap_connected,
        mesh_gatt_gap_receive,
        mesh_gatt_gap_send,
        mesh_gatt_gap_max_payload,
        NULL,
        mesh_gatt_gap_security_state
    };
    return ble_gatt_transport_init(transport, server, &ops);
}

static uint8_t mesh_gatt_proxy_cccd(void) {
    ble_gatt_attribute *cccd = ble_gatt_server_find(&mesh_gatt.server,
        MESH_GATT_HANDLE_DATA_OUT_CCCD);
    return cccd ? (uint8_t)cccd->cccd : 0;
}

static uint8_t mesh_gatt_provisioning_cccd(void) {
    ble_gatt_attribute *cccd = ble_gatt_server_find(&mesh_gatt.server,
        MESH_GATT_HANDLE_PROVISIONING_DATA_OUT_CCCD);
    return cccd ? (uint8_t)cccd->cccd : 0;
}

static void mesh_gatt_proxy_sar_cancel(void) {
    mesh_gatt.proxy_rx_active = 0;
    mesh_gatt.proxy_rx_len = 0;
    mesh_gatt.proxy_rx_started_ms = 0;
    mesh_gatt.proxy_tx_sar_active = 0;
    mesh_gatt.proxy_tx_started_ms = 0;
    mesh_gatt.proxy_sar_disconnect_pending = 0;
}

// A Proxy SAR timeout terminates the Mesh Proxy bearer as required by its
// bearer behavior; this is separate from generic ATT indication timeouts.
static int mesh_gatt_proxy_sar_timeout_poll(void) {
    if (mesh_gatt.proxy_sar_disconnect_pending) {
        gap_disconnect(0x13);
        return 1;
    }
    uint32_t now = GET_MILLIS();
    if ((mesh_gatt.proxy_rx_active &&
         (uint32_t)(now - mesh_gatt.proxy_rx_started_ms) >=
             MESH_GATT_PROXY_SAR_TIMEOUT_MS) ||
        (mesh_gatt.proxy_tx_sar_active &&
         (uint32_t)(now - mesh_gatt.proxy_tx_started_ms) >=
             MESH_GATT_PROXY_SAR_TIMEOUT_MS)) {
        mesh_gatt_proxy_sar_cancel();
        mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
        mesh_gatt.proxy_sar_disconnect_pending = 1;
        gap_disconnect(0x13);
        return 1;
    }
    return 0;
}

static void mesh_gatt_proxy_deliver(uint8_t type, const uint8_t *pdu,
                                    size_t len);
static void mesh_gatt_proxy_input(const uint8_t *p, size_t len,
                                  uint8_t provisioning_service);

static uint8_t mesh_gatt_data_in_write(void *context, uint16_t offset,
    const uint8_t *value, uint16_t len, uint8_t command) {
    uint8_t provisioning = *(const uint8_t *)context;
    if (!command) return BLE_GATT_ATT_ERR_WRITE_NOT_PERMITTED;
    if (offset) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (!len) return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t header = value[0];
    if ((header & 0x30) || (provisioning &&
        (header & 0x0f) != MESH_GATT_PROXY_PROVISIONING))
        return 0;
    mesh_gatt_proxy_input(value, len, provisioning);
    return 0;
}

static uint8_t mesh_gatt_proxy_write_context;
static uint8_t mesh_gatt_provisioning_write_context = 1;

static int mesh_gatt_register_services(void) {
    ble_gatt_server_init(&mesh_gatt.server, MESH_GATT_ATT_MTU_MAX);
    size_t device_name_len = sizeof(MESH_GATT_DEVICE_NAME) - 1;
    if (!mesh_gatt_gap_device_name_valid(
            (const uint8_t *)MESH_GATT_DEVICE_NAME, device_name_len))
        return 0;
    memcpy(mesh_gatt.gap_device_name, MESH_GATT_DEVICE_NAME, device_name_len);
    mesh_gatt.gap_device_name_len = (uint16_t)device_name_len;
    ble_gatt_uuid proxy_service = mesh_gatt_uuid16(MESH_GATT_PROXY_SERVICE_UUID);
    ble_gatt_uuid provisioning_service =
        mesh_gatt_uuid16(MESH_GATT_PROVISIONING_SERVICE_UUID);
    ble_gatt_uuid proxy_in = mesh_gatt_uuid16(MESH_GATT_PROXY_DATA_IN_UUID);
    ble_gatt_uuid proxy_out = mesh_gatt_uuid16(MESH_GATT_PROXY_DATA_OUT_UUID);
    ble_gatt_uuid provisioning_in =
        mesh_gatt_uuid16(MESH_GATT_PROVISIONING_DATA_IN_UUID);
    ble_gatt_uuid provisioning_out =
        mesh_gatt_uuid16(MESH_GATT_PROVISIONING_DATA_OUT_UUID);
    ble_gatt_uuid gap_service = mesh_gatt_uuid16(MESH_GATT_GAP_SERVICE_UUID);
    ble_gatt_uuid gap_device_name =
        mesh_gatt_uuid16(MESH_GATT_GAP_DEVICE_NAME_UUID);
    ble_gatt_uuid gap_appearance =
        mesh_gatt_uuid16(MESH_GATT_GAP_APPEARANCE_UUID);
    ble_gatt_uuid gap_ppcp = mesh_gatt_uuid16(MESH_GATT_GAP_PPCP_UUID);
    ble_gatt_uuid gap_car = mesh_gatt_uuid16(MESH_GATT_GAP_CAR_UUID);
    ble_gatt_uuid gap_security_levels =
        mesh_gatt_uuid16(MESH_GATT_GAP_SECURITY_LEVELS_UUID);
#if MESH_GATT_RPA_ONLY_SUPPORT
    ble_gatt_uuid gap_rpa_only_uuid =
        mesh_gatt_uuid16(MESH_GATT_GAP_RPA_ONLY_UUID);
#endif
#if MESH_GATT_EAD_SUPPORT
    ble_gatt_uuid gap_edkm = mesh_gatt_uuid16(MESH_GATT_GAP_EDKM_UUID);
#endif
    ble_gatt_uuid cccd = mesh_gatt_uuid16(0x2902);
    uint16_t service, decl, value, descriptor;
    if (!ble_gatt_server_add_service(&mesh_gatt.server, &proxy_service, 1,
            &service) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server, &proxy_in,
            BLE_GATT_PROP_WRITE_NO_RSP, BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, mesh_gatt_data_in_write,
            &mesh_gatt_proxy_write_context, &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server, &proxy_out,
            BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL,
            &decl, &value) ||
        !ble_gatt_server_add_descriptor(&mesh_gatt.server, &cccd,
            0, NULL, 0, 0, NULL, NULL, NULL, &descriptor) ||
        !ble_gatt_server_add_service(&mesh_gatt.server,
            &provisioning_service, 1, &service) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &provisioning_in, BLE_GATT_PROP_WRITE_NO_RSP,
            BLE_GATT_PERM_WRITE, NULL, 0, 0, NULL, mesh_gatt_data_in_write,
            &mesh_gatt_provisioning_write_context, &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &provisioning_out, BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0,
            NULL, NULL, NULL, &decl, &value) ||
        !ble_gatt_server_add_descriptor(&mesh_gatt.server, &cccd,
            0, NULL, 0, 0, NULL, NULL, NULL, &descriptor))
        return 0;
    uint8_t appearance[2] = {
        (uint8_t)MESH_GATT_APPEARANCE,
        (uint8_t)(MESH_GATT_APPEARANCE >> 8)
    };
    // 0xffff in each field means the device has no preferred connection value.
    const uint8_t ppcp[8] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    const uint8_t central_address_resolution = 1;
    // The server's strongest attribute requirement is authenticated LE
    // security mode 1, level 3 (Device Name when not discoverable).
    const uint8_t gap_security_level_requirements[2] = {1, 3};
#if MESH_GATT_RPA_ONLY_SUPPORT
    // Opting in declares that the product uses only RPAs as its local address
    // after bonding; zero is the sole defined value for this characteristic.
    const uint8_t gap_rpa_only_value = 0;
#endif
    if (!ble_gatt_server_add_service(&mesh_gatt.server, &gap_service, 1,
            &service) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &gap_device_name, BLE_GATT_PROP_READ,
            BLE_GATT_PERM_READ_AUTHENTICATED,
            NULL, 0, 0, mesh_gatt_gap_device_name_read, NULL, NULL,
            &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &gap_appearance, BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            appearance, sizeof(appearance), sizeof(appearance), NULL, NULL,
            NULL, &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &gap_ppcp, BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            ppcp, sizeof(ppcp), sizeof(ppcp), NULL, NULL, NULL,
            &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server, &gap_car,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &central_address_resolution, 1, 1, NULL, NULL, NULL,
            &decl, &value) ||
        !ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &gap_security_levels, BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            gap_security_level_requirements,
            sizeof(gap_security_level_requirements),
            sizeof(gap_security_level_requirements), NULL, NULL, NULL,
            &decl, &value))
        return 0;
#if MESH_GATT_RPA_ONLY_SUPPORT
    if (!ble_gatt_server_add_characteristic(&mesh_gatt.server,
            &gap_rpa_only_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &gap_rpa_only_value, 1, 1, NULL, NULL, NULL,
            &decl, &value))
        return 0;
#endif
#if MESH_GATT_EAD_SUPPORT
    if (!ble_gatt_server_add_characteristic(&mesh_gatt.server, &gap_edkm,
            BLE_GATT_PROP_READ,
            BLE_GATT_PERM_READ_AUTHENTICATED | BLE_GATT_PERM_READ_AUTHORIZED,
            NULL, 0, 24, mesh_gatt_gap_edkm_read, NULL, NULL,
            &decl, &value))
        return 0;
#endif
    if (!mesh_gatt_transport_init(&mesh_gatt.transport,
                                  &mesh_gatt.server))
        return 0;
    return mesh_gatt.server.next_handle ==
        24 + (MESH_GATT_RPA_ONLY_SUPPORT ? 2 : 0) +
        (MESH_GATT_EAD_SUPPORT ? 2 : 0);
}

static int mesh_gatt_ensure_initialized(void) {
    if (mesh_gatt.initialized) return 1;
    if (!mesh_gatt_register_services()) return 0;
    mesh_gatt.initialized = 1;
    return 1;
}

// Configure application authorization for secured GAP characteristics such as
// Encrypted Data Key Material. Without an authorizer, authorized reads fail.
int mesh_gatt_set_authorizer(ble_gatt_authorize_fn authorize, void *context) {
    if (!mesh_gatt_ensure_initialized()) return 0;
    ble_gatt_server_set_authorizer(&mesh_gatt.server, authorize, context);
    return 1;
}

// Set the UTF-8 Device Name value. Empty names are allowed by GAP.
int mesh_gatt_gap_device_name_set(const uint8_t *name, size_t len) {
    if (!mesh_gatt_gap_device_name_valid(name, len) ||
        !mesh_gatt_ensure_initialized())
        return 0;
    if (len) memcpy(mesh_gatt.gap_device_name, name, len);
    if (len < sizeof(mesh_gatt.gap_device_name))
        memset(mesh_gatt.gap_device_name + len, 0,
               sizeof(mesh_gatt.gap_device_name) - len);
    mesh_gatt.gap_device_name_len = (uint16_t)len;
    return 1;
}

// Device Name access follows the discoverable bits in the active advertising
// data. This policy belongs to the Mesh GATT adapter, which consumes that data.
static int mesh_gatt_gap_data_discoverable(const uint8_t *data, size_t len) {
    for (size_t offset = 0; offset < len;) {
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        int result = gap_ad_parse_next(data, len, &offset, &type, &value,
                                 &value_len);
        if (result <= 0) return 0;
        if (type == GAP_AD_FLAGS && value_len >= 1 && (value[0] & 0x03))
            return 1;
    }
    return 0;
}

static int mesh_gatt_gap_discoverable(void) {
#if GAP_EXT_ADV_SUPPORT
    for (uint8_t set = 0; set < GAP_EXT_ADV_SET_COUNT; set++) {
        if (gap_ext_adv[set].enabled &&
            mesh_gatt_gap_data_discoverable(gap_ext_adv[set].data,
                gap_ext_adv[set].data_len))
            return 1;
    }
#endif
    return gap_advertising.enabled && mesh_gatt_gap_data_discoverable(
        gap_advertising.data, gap_advertising.data_len);
}

// Refresh Device Name access permissions from the active Flags AD structure.
static void mesh_gatt_gap_policy_update(void) {
    ble_gatt_attribute *name = ble_gatt_server_find(&mesh_gatt.server,
        MESH_GATT_HANDLE_GAP_DEVICE_NAME);
    if (!name) return;
    name->permissions = mesh_gatt_gap_discoverable() ? BLE_GATT_PERM_READ :
        BLE_GATT_PERM_READ_AUTHENTICATED;
}

void mesh_gatt_proxy_set_rx_callback(mesh_gatt_proxy_rx_fn callback,
                                      void *context) {
    mesh_gatt.proxy_rx_callback = callback;
    mesh_gatt.proxy_rx_context = context;
}

void mesh_gatt_provisioning_set_rx_callback(
    mesh_gatt_provisioning_rx_fn callback, void *context) {
    mesh_gatt.provisioning_rx_callback = callback;
    mesh_gatt.provisioning_rx_context = context;
}

void mesh_gatt_provisioning_set_link_callback(
    mesh_gatt_provisioning_link_fn callback, void *context) {
    mesh_gatt.provisioning_link_callback = callback;
    mesh_gatt.provisioning_link_context = context;
}

static void mesh_gatt_provisioning_link_notify(uint8_t open) {
    if (mesh_gatt.provisioning_link_callback)
        mesh_gatt.provisioning_link_callback(open,
            mesh_gatt.provisioning_link_context);
}

int mesh_gatt_provisioning_advertising_start(const uint8_t device_uuid[16],
                                             uint16_t oob_info,
                                             uint16_t interval_ms) {
    if (!device_uuid) return 0;
    uint8_t data[25] = {2, 0x01, 0x06, 21, 0x16, 0x27, 0x18};
    memcpy(data + 7, device_uuid, 16);
    data[23] = (uint8_t)oob_info;
    data[24] = (uint8_t)(oob_info >> 8);
    return gap_connectable_advertising_start(data, sizeof(data),
                                                   NULL, 0, interval_ms);
}

int mesh_gatt_proxy_advertising_start(uint16_t interval_ms) {
    static const uint8_t data[] = {
        2, 0x01, 0x06, 3, 0x03, 0x28, 0x18
    };
    return gap_connectable_advertising_start(data, sizeof(data),
                                                   NULL, 0, interval_ms);
}

int mesh_gatt_proxy_offer(uint8_t type, const uint8_t *pdu, size_t len,
                          uint16_t destination) {
    if (!mesh_gatt_ensure_initialized() || !pdu || !len ||
        len > MESH_GATT_PROXY_PDU_MAX ||
        type > MESH_GATT_PROXY_CONFIGURATION || !mesh_gatt.connected ||
        !mesh_gatt_proxy_cccd() ||
        mesh_gatt.proxy_tx_count >= MESH_GATT_PROXY_QUEUE_SIZE)
        return 0;
    if ((type == MESH_GATT_PROXY_NETWORK &&
         (len < MESH_GATT_PROXY_NETWORK_PDU_MIN ||
          len > MESH_GATT_PROXY_NETWORK_PDU_MAX)) ||
        (type == MESH_GATT_PROXY_BEACON &&
         len != MESH_GATT_PROXY_BEACON_PDU_LEN))
        return 0;
    if (type == MESH_GATT_PROXY_NETWORK) {
        uint8_t listed = 0;
        for (uint8_t i = 0; i < mesh_gatt.filter_count; i++)
            if (mesh_gatt.filter[i] == destination) listed = 1;
        if ((!mesh_gatt.filter_type && !listed) ||
            (mesh_gatt.filter_type && listed))
            return 0;
    }
    uint8_t slot = (mesh_gatt.proxy_tx_head + mesh_gatt.proxy_tx_count) %
                   MESH_GATT_PROXY_QUEUE_SIZE;
    mesh_gatt.proxy_tx[slot].type = type;
    mesh_gatt.proxy_tx[slot].len = (uint8_t)len;
    mesh_gatt.proxy_tx[slot].offset = 0;
    mesh_gatt.proxy_tx[slot].destination = destination;
    memcpy(mesh_gatt.proxy_tx[slot].data, pdu, len);
    mesh_gatt.proxy_tx_count++;
    return 1;
}

static int mesh_gatt_proxy_queue(uint8_t type, const uint8_t *data, size_t len) {
    size_t max_len = type == MESH_GATT_PROXY_PROVISIONING ?
        MESH_GATT_PROVISIONING_PDU_MAX : MESH_GATT_PROXY_PDU_MAX;
    if (!mesh_gatt_ensure_initialized() || !data || !len || len > max_len ||
        mesh_gatt.proxy_tx_count >= MESH_GATT_PROXY_QUEUE_SIZE)
        return 0;
    uint8_t slot = (mesh_gatt.proxy_tx_head + mesh_gatt.proxy_tx_count) %
                   MESH_GATT_PROXY_QUEUE_SIZE;
    mesh_gatt.proxy_tx[slot].type = type;
    mesh_gatt.proxy_tx[slot].len = (uint8_t)len;
    mesh_gatt.proxy_tx[slot].offset = 0;
    mesh_gatt.proxy_tx[slot].destination = 0;
    memcpy(mesh_gatt.proxy_tx[slot].data, data, len);
    mesh_gatt.proxy_tx_count++;
    return 1;
}

int mesh_gatt_provisioning_offer(const uint8_t *pdu, size_t len) {
    if (!mesh_gatt.connected || !mesh_gatt_provisioning_cccd()) return 0;
    return mesh_gatt_proxy_queue(MESH_GATT_PROXY_PROVISIONING, pdu, len);
}

static void mesh_gatt_proxy_configuration(const uint8_t *p, size_t len) {
    if (!len) return;
    uint8_t opcode = p[0];
    if (opcode == 0 && len == 2 && p[1] <= 1) {
        mesh_gatt.filter_type = p[1];
        mesh_gatt.filter_count = 0;
    } else if ((opcode == 1 || opcode == 2) && len >= 3 && !((len - 1) & 1)) {
        for (size_t off = 1; off + 1 < len; off += 2) {
            uint16_t address = mesh_gatt_u16(p + off);
            uint8_t i = 0;
            while (i < mesh_gatt.filter_count && mesh_gatt.filter[i] != address) i++;
            if (opcode == 1 && i == mesh_gatt.filter_count &&
                i < MESH_GATT_PROXY_FILTER_SIZE)
                mesh_gatt.filter[mesh_gatt.filter_count++] = address;
            if (opcode == 2 && i < mesh_gatt.filter_count)
                mesh_gatt.filter[i] = mesh_gatt.filter[--mesh_gatt.filter_count];
        }
    } else return;
    uint8_t status[4] = {3, mesh_gatt.filter_type,
        (uint8_t)mesh_gatt.filter_count,
        (uint8_t)(mesh_gatt.filter_count >> 8)};
    mesh_gatt_proxy_queue(MESH_GATT_PROXY_CONFIGURATION, status, sizeof(status));
}

static void mesh_gatt_proxy_deliver(uint8_t type, const uint8_t *pdu,
                                    size_t len) {
    if (type == MESH_GATT_PROXY_CONFIGURATION)
        mesh_gatt_proxy_configuration(pdu, len);
    else if (type != MESH_GATT_PROXY_PROVISIONING && mesh_gatt.proxy_rx_callback)
        mesh_gatt.proxy_rx_callback(type, pdu, len, mesh_gatt.proxy_rx_context);
}

static void mesh_gatt_proxy_input(const uint8_t *p, size_t len,
                                  uint8_t provisioning_service) {
    if (!len) return;
    if (mesh_gatt_proxy_sar_timeout_poll()) return;
    uint8_t header = p[0], sar = header >> 6, type = header & 0x0f;
    if ((header & 0x30) ||
        (provisioning_service ? type != MESH_GATT_PROXY_PROVISIONING :
                                type > MESH_GATT_PROXY_CONFIGURATION) ||
        (mesh_gatt.proxy_rx_active &&
         mesh_gatt.proxy_rx_service != provisioning_service)) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        return;
    }
    p++; len--;
    if (!sar) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        if (len) {
            if (provisioning_service) {
                if (mesh_gatt.provisioning_rx_callback)
                    mesh_gatt.provisioning_rx_callback(p, len,
                        mesh_gatt.provisioning_rx_context);
            } else mesh_gatt_proxy_deliver(type, p, len);
        }
    } else if (sar == 1) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
        if (!len || len > sizeof(mesh_gatt.proxy_rx)) return;
        memcpy(mesh_gatt.proxy_rx, p, len);
        mesh_gatt.proxy_rx_len = (uint16_t)len;
        mesh_gatt.proxy_rx_type = type;
        mesh_gatt.proxy_rx_service = provisioning_service;
        mesh_gatt.proxy_rx_active = 1;
        mesh_gatt.proxy_rx_started_ms = GET_MILLIS();
    } else if (!len || !mesh_gatt.proxy_rx_active ||
               type != mesh_gatt.proxy_rx_type ||
               len > sizeof(mesh_gatt.proxy_rx) - mesh_gatt.proxy_rx_len) {
        mesh_gatt.proxy_rx_active = 0;
        mesh_gatt.proxy_rx_len = 0;
        mesh_gatt.proxy_rx_started_ms = 0;
    } else {
        memcpy(mesh_gatt.proxy_rx + mesh_gatt.proxy_rx_len, p, len);
        mesh_gatt.proxy_rx_len += (uint16_t)len;
        if (sar == 3) {
            if (provisioning_service) {
                if (mesh_gatt.provisioning_rx_callback)
                    mesh_gatt.provisioning_rx_callback(mesh_gatt.proxy_rx,
                        mesh_gatt.proxy_rx_len, mesh_gatt.provisioning_rx_context);
            } else mesh_gatt_proxy_deliver(type, mesh_gatt.proxy_rx,
                                            mesh_gatt.proxy_rx_len);
            mesh_gatt.proxy_rx_active = 0;
            mesh_gatt.proxy_rx_len = 0;
            mesh_gatt.proxy_rx_started_ms = 0;
        }
    }
}

static void mesh_gatt_notify_poll(void) {
    if (!mesh_gatt.proxy_tx_count) return;
    uint8_t slot = mesh_gatt.proxy_tx_head;
    uint8_t provisioning =
        mesh_gatt.proxy_tx[slot].type == MESH_GATT_PROXY_PROVISIONING;
    if (provisioning ? !mesh_gatt_provisioning_cccd() : !mesh_gatt_proxy_cccd())
        return;
    uint16_t mtu = mesh_gatt.server.mtu;
    if (mtu < 23) return;
    uint16_t max_payload = mtu - 3;
    uint16_t remaining = mesh_gatt.proxy_tx[slot].len -
                         mesh_gatt.proxy_tx[slot].offset;
    uint16_t chunk;
    uint8_t sar;
    if (!mesh_gatt.proxy_tx[slot].offset && remaining + 1 <= max_payload) {
        sar = 0; chunk = remaining;
    } else if (!mesh_gatt.proxy_tx[slot].offset) {
        sar = 1; chunk = max_payload - 1;
    } else if (remaining + 1 <= max_payload) {
        sar = 3; chunk = remaining;
    } else {
        sar = 2; chunk = max_payload - 1;
    }
    uint8_t value[BLE_GATT_SERVER_MTU_MAX];
    value[0] = (uint8_t)((sar << 6) | mesh_gatt.proxy_tx[slot].type);
    memcpy(value + 1, mesh_gatt.proxy_tx[slot].data +
           mesh_gatt.proxy_tx[slot].offset, chunk);
    uint16_t handle = provisioning ? MESH_GATT_HANDLE_PROVISIONING_DATA_OUT :
                                     MESH_GATT_HANDLE_DATA_OUT;
    if (!ble_gatt_server_queue_event(&mesh_gatt.server, handle, value,
                                      chunk + 1, 0))
        return;
    if (sar == 1) {
        mesh_gatt.proxy_tx_sar_active = 1;
        mesh_gatt.proxy_tx_started_ms = GET_MILLIS();
    }
    mesh_gatt.proxy_tx[slot].offset += (uint8_t)chunk;
    if (!sar || sar == 3) {
        mesh_gatt.proxy_tx_sar_active = 0;
        mesh_gatt.proxy_tx_started_ms = 0;
        mesh_gatt.proxy_tx_head = (slot + 1) % MESH_GATT_PROXY_QUEUE_SIZE;
        mesh_gatt.proxy_tx_count--;
    }
}

static void mesh_gatt_link_reset(void) {
    mesh_gatt.connected = 0;
    mesh_gatt_proxy_sar_cancel();
    mesh_gatt.filter_type = mesh_gatt.filter_count = 0;
    mesh_gatt.proxy_tx_head = mesh_gatt.proxy_tx_count = 0;
}

// Poll from the application's connection loop. Generic ATT/L2CAP work flows
// through the transport/server; this adapter only queues Mesh bearer values.
void mesh_gatt_poll(void) {
    if (!mesh_gatt_ensure_initialized()) return;
    mesh_gatt_gap_policy_update();
    uint8_t connected = gap_connected() != 0;
    if (!connected) {
        if (mesh_gatt.connected) {
            mesh_gatt_provisioning_link_notify(0);
            mesh_gatt_link_reset();
        }
        ble_gatt_transport_poll(&mesh_gatt.transport, GET_MILLIS());
        return;
    }
    if (!mesh_gatt.connected) {
        mesh_gatt_link_reset();
        mesh_gatt.connected = 1;
        mesh_gatt_provisioning_link_notify(1);
    }
    if (mesh_gatt_proxy_sar_timeout_poll()) return;
    mesh_gatt_notify_poll();
    ble_gatt_transport_poll(&mesh_gatt.transport, GET_MILLIS());
}

#endif
