#include "ch32fun.h"
#include "iSLER.h"
#include "ble_mesh_crypto.h"
#include "aes_cmm.h"
#include "ble_mesh_provisioning.h"
#include "ble_mesh_1network.h"
#include "ble_mesh_2transport.h"
#include "ble_mesh_3access.h"
#include "ble_mesh_4foundation.h"
#include "ble_mesh_4models.h"
#include "micro-ecc/uECC.h"
#include <stdio.h>
#include "ch5xx_flash.h"
#include "lib_rand.h"

#define SECTOR_SIZE         4096 // 4kB
#define BLE_MESH_DATA_ADDR  110 * SECTOR_SIZE // 0x6E000 = 440K of 448K
#define BLE_MESH_DATA_SIZE  2 * SECTOR_SIZE

// The radio sends a complete advertising PDU, while provisioning supplies an
// AD structure. Keep queued AD structures in order, including delayed ACKs.
#define BLE_ADV_ACCESS_ADDRESS  0x8E89BED6
#define ROM_CFG_MAC_ADDR		((const u32*)0x0007F018)

void mesh_advertise_bearer(uint8_t *wire, size_t wire_len);

static int flash_data_range_valid(uint32_t addr, int len) {
    return len > 0 && addr >= BLE_MESH_DATA_ADDR &&
           addr < BLE_MESH_DATA_ADDR + BLE_MESH_DATA_SIZE &&
           (uint32_t)len <= BLE_MESH_DATA_ADDR + BLE_MESH_DATA_SIZE - addr;
}

__HIGH_CODE
int flash_erase_data(uint32_t addr, int len) {
    if (!flash_data_range_valid(addr, len) ||
        (addr & (SECTOR_SIZE - 1))) return 0;
    return ch5xx_flash_cmd_erase(addr, len) == 0;
}

__HIGH_CODE
int flash_read_data(uint32_t addr, uint8_t *out, int len) {
    if (!out || !flash_data_range_valid(addr, len) ||
        (addr & 3) || (len & 3) || ((uintptr_t)out & 3)
    ) return 0;

    ch5xx_flash_cmd_read(addr, out, len);
    return 1;
}

__HIGH_CODE
int flash_write_data(uint32_t addr, uint8_t *data, int len) {
    if (!data || !flash_data_range_valid(addr, len) ||
        (addr & 3) || (len & 3) || ((uintptr_t)data & 3)
    ) return 0;

    return ch5xx_flash_cmd_write(addr, data, len) == 0;
}

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

// Supply a trusted monotonic second count that survives reboot. Until a clock
// is available, IV Update timing remains disabled rather than skipping its
// required minimum durations.
int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds) {
    (void)seconds;
    return 0;
}

int GET_RANDOM_BYTES(uint8_t *out, unsigned len) {
    if (!out && len) return 0;

    for (unsigned i = 0; i < len;) {
        uint32_t value = rand();
        for (unsigned j = 0; j < 4 && i < len; j++, i++) {
            out[i] = (uint8_t)(value >> (8 * j));
        }
    }
    return 1;
}

#define RADIO_QUEUE_SIZE 8
#define MESH_ADV_MAX_SIZE 31

// One slot holds the original AD data and all of its advertising repetitions.
static struct {
    uint8_t data[MESH_ADV_MAX_SIZE], len;
    uint32_t send_at_ms, order;
    uint16_t interval_ms;
    uint8_t remaining, started;
} radio_queue[RADIO_QUEUE_SIZE];
static uint32_t radio_order;
static int reset_slot = -1;

static int mesh_adv_queue_add(const uint8_t *ad, size_t len,
                              uint32_t send_at_ms, uint8_t transmit) {
    if (!ad || len < 2 || len > MESH_ADV_MAX_SIZE || (size_t)ad[0] + 1 != len)
        return -1;
    for (uint8_t i = 0; i < RADIO_QUEUE_SIZE; i++) {
        if (radio_queue[i].remaining) continue;
        memcpy(radio_queue[i].data, ad, len);
        radio_queue[i].len = (uint8_t)len;
        radio_queue[i].send_at_ms = send_at_ms;
        radio_queue[i].order = radio_order++;
        radio_queue[i].interval_ms = ((transmit >> 3) + 1u) * 10u;
        radio_queue[i].remaining = (transmit & 7) + 1;
        radio_queue[i].started = 0;
        return 0;
    }
    return -1;
}

static int mesh_adv_queue_next(uint32_t now) {
    int first = -1, next = -1;
    for (uint8_t i = 0; i < RADIO_QUEUE_SIZE; i++) {
        if (!radio_queue[i].remaining) continue;
        if (!radio_queue[i].started && (first < 0 ||
            (int32_t)(radio_queue[i].order - radio_queue[first].order) < 0)) first = i;
        if (radio_queue[i].started && (int32_t)(now - radio_queue[i].send_at_ms) >= 0 &&
            (next < 0 || (int32_t)(radio_queue[i].order - radio_queue[next].order) < 0)) next = i;
    }
    // Keep initial packets in order; a waiting retry does not block new packets.
    if (first >= 0 && (int32_t)(now - radio_queue[first].send_at_ms) >= 0 &&
        (next < 0 || (int32_t)(radio_queue[first].order - radio_queue[next].order) < 0)) next = first;
    return next;
}

// Call after a complete advertising event; jitter is 0..10 milliseconds.
static void mesh_adv_queue_sent(uint8_t slot, uint32_t now, uint8_t jitter) {
    if (!radio_queue[slot].remaining) return;
    if (--radio_queue[slot].remaining) {
        radio_queue[slot].started = 1;
        radio_queue[slot].order = radio_order++;
        radio_queue[slot].send_at_ms = now + radio_queue[slot].interval_ms + jitter % 11;
    }
}

static void mesh_adv_queue_clear(uint8_t ad_type) {
    for (uint8_t i = 0; i < RADIO_QUEUE_SIZE; i++)
        if (radio_queue[i].remaining && radio_queue[i].data[1] == ad_type)
            radio_queue[i].remaining = 0;
}


static uint8_t rx_armed, rx_channel_index;
static uint32_t rx_started_ms;
static ISLER_BUF_ATTR uint8_t adv_frame[8 + PB_MAX_AD_SIZE];

static void mesh_radio_init(void) {
    iSLERInit(LL_TX_POWER_0_DBM);
    uint32_t value = (uint32_t)funSysTick64();
    seed(value ? value : 0x747AA32F);
}

int BLE_MESH_QUEUE_TX(const uint8_t *adv_data, size_t len) {
    if (reset_slot >= 0) return -1;
    uint8_t transmit = len >= 2 && adv_data && adv_data[1] == MESH_NETWORK_AD_TYPE ?
        mesh_network.state.network_transmit : 0;
    return mesh_adv_queue_add(adv_data, len, GET_MILLIS(), transmit);
}

int BLE_MESH_QUEUE_RELAY_TX(const uint8_t *adv_data, size_t len,
                            uint8_t relay_retransmit) {
    if (reset_slot >= 0) return -1;
    return mesh_adv_queue_add(adv_data, len, GET_MILLIS(), relay_retransmit);
}

int BLE_MESH_QUEUE_TX_DELAYED(
    const uint8_t *adv_data, size_t len,
    uint16_t min_delay_ms, uint16_t max_delay_ms
) {
    if (reset_slot >= 0 || min_delay_ms > max_delay_ms) return -1;

    uint32_t range = (uint32_t)max_delay_ms - min_delay_ms + 1;
    uint32_t delay_ms = min_delay_ms + rand() % range;
    return mesh_adv_queue_add(adv_data, len, GET_MILLIS() + delay_ms, 0);
}


int GET_LOCAL_UUID(uint8_t device_uuid[16]) {
    if (!device_uuid) return -1;

    // Use the factory MAC as the stable, device-specific part of the UUID.
    memset(device_uuid, 0, 16);
    for (int i = 0; i < 6; i++) {
        device_uuid[i] = ((const uint8_t *)ROM_CFG_MAC_ADDR)[i];
    }
    return 0;
}

void PROV_ATTENTION_START(uint8_t seconds) {
    (void)seconds;
}

void PROV_ATTENTION_STOP(void) {
}

#define MESH_STATE_MAGIC 0x4d53
#define MESH_STATE_VERSION (15 + MESH_MAX_SUBNETS)
#define PROVISIONER_MAX_NODES 8

typedef struct {
    uint8_t device_key[16];
    uint16_t unicast_address;
    uint8_t num_elements;
} mesh_node_record;

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t generation; // wraps after 255 saves
    mesh_net_state state;
    uint16_t next_unicast_address;
    uint8_t node_count;
    mesh_node_record nodes[PROVISIONER_MAX_NODES];
    mesh_models_state models;
    uint32_t checksum;
} mesh_state_record;

// FNV-1a over the stored record bytes before the checksum field.
static uint32_t mesh_state_checksum(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        hash = (hash ^ bytes[i]) * 16777619;
    }
    return hash;
}

static int mesh_state_read(uint32_t addr, mesh_state_record *record) {
    return flash_read_data(addr, (uint8_t *)record, sizeof(*record)) &&
           record->magic == MESH_STATE_MAGIC &&
           record->version == MESH_STATE_VERSION &&
           record->checksum == mesh_state_checksum(
               record, offsetof(mesh_state_record, checksum));
}

static int mesh_state_load_record(mesh_state_record *record) {
    mesh_state_record first, second;
    int has_first = mesh_state_read(BLE_MESH_DATA_ADDR, &first);
    int has_second = mesh_state_read(BLE_MESH_DATA_ADDR + SECTOR_SIZE, &second);
    if (!has_first && !has_second) return 0;

    if (!has_first || (has_second &&
         (uint8_t)(second.generation - first.generation) < 0x80)) {
        *record = second;
    } else {
        *record = first;
    }
    return 1;
}

// The two sectors alternate so an interrupted save leaves the prior copy.
static int mesh_state_save_record(mesh_state_record *record) {
    mesh_state_record first, second;
    int has_first = mesh_state_read(BLE_MESH_DATA_ADDR, &first);
    int has_second = mesh_state_read(BLE_MESH_DATA_ADDR + SECTOR_SIZE, &second);
    uint32_t addr = BLE_MESH_DATA_ADDR;
    uint8_t generation = 1;

    if (has_second &&
        (!has_first || (uint8_t)(second.generation - first.generation) < 0x80)
    ) {
        generation = second.generation + 1;
    } else if (has_first) {
        addr += SECTOR_SIZE;
        generation = first.generation + 1;
    }

    mesh_state_record check;
    record->magic = MESH_STATE_MAGIC;
    record->version = MESH_STATE_VERSION;
    record->generation = generation;
    record->checksum = mesh_state_checksum(
        record, offsetof(mesh_state_record, checksum));

    if (!flash_erase_data(addr, SECTOR_SIZE) ||
        !flash_write_data(addr, (uint8_t *)record, sizeof(*record)) ||
        !flash_read_data(addr, (uint8_t *)&check, sizeof(check))) return 0;
    return memcmp(record, &check, sizeof(*record)) == 0;
}

int BLE_MESH_ADV_POLL(uint8_t *adv_data, size_t *len, int8_t *rssi) {
    if (!adv_data || !len) return -1;
    if (rssi) *rssi = 127;
    uint32_t now = GET_MILLIS();
    int received = 0;

    if (rx_ready) {
        const uint8_t *frame = (const uint8_t *)LLE_BUF;
        uint8_t payload_len = frame[1];
        rx_armed = 0;
        rx_ready = 0;

        // An ADV_NONCONN_IND payload is AdvA (6 bytes) followed by AD data.
        if ((frame[0] & 0x0F) == 0x02 && payload_len >= 8 &&
            payload_len <= 37
        ) {
            size_t end = (size_t)payload_len + 2;
            for (size_t offset = 8; offset < end;) {
                uint8_t ad_len = frame[offset];
                if (ad_len == 0 || offset + ad_len + 1 > end) break;
                if (frame[offset + 1] == MESH_PROV_AD_TYPE ||
                    frame[offset + 1] == MESH_BEACON_AD_TYPE ||
                    frame[offset + 1] == MESH_NETWORK_AD_TYPE
                ) {
                    if ((size_t)ad_len + 1 > *len) return -1;
                    memcpy(adv_data, frame + offset, (size_t)ad_len + 1);
                    *len = (size_t)ad_len + 1;
                    if (rssi) *rssi = (int8_t)iSLERRSSI();
                    received = 1;
                    break;
                }
                offset += (size_t)ad_len + 1;
            }
        }
    }

    int slot = mesh_adv_queue_next(now);
    if (slot >= 0) {
        // The factory MAC is stored most-significant byte first in ROM.
        const uint8_t *mac = (const uint8_t *)ROM_CFG_MAC_ADDR;
        adv_frame[0] = 0x02;
        adv_frame[1] = 0;

        for (uint8_t i = 0; i < 6; i++) {
            adv_frame[7 - i] = mac[i];
        }
        memcpy(adv_frame + 8, radio_queue[slot].data, radio_queue[slot].len);
        size_t frame_len = 8 + radio_queue[slot].len;
        rx_armed = 0;

        for (uint8_t channel = 37; channel <= 39; channel++) {
            iSLERTX(BLE_ADV_ACCESS_ADDRESS, adv_frame, frame_len, channel, PHY_1M);
            if (!tx_done) return -1;
        }
        mesh_adv_queue_sent((uint8_t)slot, GET_MILLIS(), (uint8_t)(rand() % 11));
    }
    // Finish Node Reset after its reply is sent; retry failed flash operations.
    if (reset_slot >= 0 && !radio_queue[reset_slot].remaining) {
        mesh_state_record empty = {0};
        // Write an empty record first so a reboot cannot restore the old copy.
        if (mesh_state_save_record(&empty)) {
            mesh_state_record first;
            uint32_t old_addr = mesh_state_read(BLE_MESH_DATA_ADDR, &first) &&
                first.generation == empty.generation ? BLE_MESH_DATA_ADDR + SECTOR_SIZE : BLE_MESH_DATA_ADDR;
            if (flash_erase_data(old_addr, SECTOR_SIZE)) {
                memset(radio_queue, 0, sizeof(radio_queue));
                for (uint8_t i = 0; i < MESH_MAX_ELEMENTS; i++)
                    if (mesh_models.health_server[i].attention)
                        BLE_MESH_HEALTH_ATTENTION(mesh_network.state.unicast_address + i, 0);
                memset(&mesh_network, 0, sizeof(mesh_network));
                memset(&mesh_models, 0, sizeof(mesh_models));
                memset(&transport_tx, 0, sizeof(transport_tx));
                memset(segmented_tx_queue, 0, sizeof(segmented_tx_queue));
                segmented_tx_queue_head = segmented_tx_queue_count = 0;
                memset(&transport_rx, 0, sizeof(transport_rx));
                memset(transport_labels, 0, sizeof(transport_labels));
                mesh_transport_clear_labels();
                memset(&session, 0, sizeof(session));
                memset(&provisioner, 0, sizeof(provisioner));
                memset(&provisionee, 0, sizeof(provisionee));
                memset(&prov_rx, 0, sizeof(prov_rx));
                memset(&bearer, 0, sizeof(bearer));
                memset(&tx, 0, sizeof(tx));
                reset_slot = -1;
            }
        }
    }

    // Rotate reception through advertising channels 37, 38, and 39 every 20 ms.
    if (!rx_armed || (uint32_t)(now - rx_started_ms) >= 20) {
        uint8_t channel = 37 + rx_channel_index;
        rx_channel_index = (rx_channel_index + 1) % 3;
        iSLERRX(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M);
        rx_started_ms = GET_MILLIS();
        rx_armed = 1;
    }
    return received;
}

int BLE_MESH_NETWORK_LOAD_STATE(mesh_net_state *state) {
    if (!state) return 0;
    mesh_state_record record;
    if (!mesh_state_load_record(&record)) return 0;
    *state = record.state;
    return 1;
}

int BLE_MESH_NETWORK_SAVE_STATE(const mesh_net_state *state) {
    if (!state) return 0;
    mesh_state_record record = {0};
    if (!mesh_state_load_record(&record)) record.models.default_ttl = MODEL_TTL;
    record.state = *state;
    if (!mesh_state_save_record(&record)) return 0;
    // Cached packets cannot outlive a change in the transmitting credentials.
    if (mesh_network.ready &&
        (memcmp(state->net_key, mesh_network.state.net_key, 16) ||
         (state->key_refresh_phase == 2) != (mesh_network.state.key_refresh_phase == 2) ||
         state->iv_index != mesh_network.state.iv_index ||
         state->iv_update != mesh_network.state.iv_update)) {
        mesh_adv_queue_clear(MESH_NETWORK_AD_TYPE);
        mesh_adv_queue_clear(MESH_NETWORK_BEACON_AD_TYPE);
    }
    if (!state->beacon) mesh_adv_queue_clear(MESH_NETWORK_BEACON_AD_TYPE);
    return 1;
}

int BLE_MESH_MODELS_LOAD_STATE(mesh_models_state *state) {
    if (!state) return 0;
    mesh_state_record record;
    if (!mesh_state_load_record(&record)) return 0;
    *state = record.models;
    return 1;
}

int BLE_MESH_MODELS_SAVE_STATE(const mesh_models_state *state) {
    if (!state) return 0;
    mesh_state_record record;
    if (!mesh_state_load_record(&record)) return 0;
    record.models = *state;
    return mesh_state_save_record(&record);
}

int BLE_MESH_NODE_RESET(uint16_t dst) {
    mesh_state_record record;
    if (reset_slot >= 0 || !mesh_state_load_record(&record) || record.node_count ||
        bearer.role != PB_ROLE_NONE) return 0;
    if (!mesh_access_queue(mesh_network.state.unicast_address, dst,
        mesh_models.state.default_ttl, DEVICE_KEY_LOCAL,
        OP_CONFIG_NODE_RESET_STATUS, NULL, 0, 0)) return 0;
    // The immediately preceding queue operation added the Reset Status packet.
    for (uint8_t i = 0; i < RADIO_QUEUE_SIZE; i++)
        if (radio_queue[i].remaining && radio_queue[i].order == radio_order - 1u)
            reset_slot = i;
    if (reset_slot < 0) return 0;
    for (uint8_t i = 0; i < RADIO_QUEUE_SIZE; i++)
        if (i != reset_slot) radio_queue[i].remaining = 0;
    mesh_network.ready = 0;
    return 1;
}


int PROVISIONEE_STORE_DATA(const prov_data *data, const uint8_t device_key[16],
                           uint8_t num_elements) {
    if (!data || !device_key) return -1;
    if (!num_elements || num_elements > MESH_MAX_ELEMENTS ||
        (uint32_t)data->unicast_address + num_elements - 1 > 0x7fff) return -1;
    mesh_net_state state = {0};
    memcpy(state.net_key, data->net_key, 16);
    state.net_key_index = data->net_key_index;
    memcpy(state.dev_key, device_key, 16);
    state.iv_index = data->iv_index;
    state.phase2_provisioned = (data->flags & 1) != 0;
    state.iv_update = (data->flags & 2) != 0;
    state.iv_skip_min_time = state.iv_update;
    state.unicast_address = data->unicast_address;
    state.element_count = num_elements;
    state.beacon = 1;

    uint64_t seconds;
    if (BLE_MESH_NETWORK_TIME_SECONDS(&seconds) == 1) {
        state.iv_time_valid = 1;
        state.iv_state_start_time = seconds;
    }

    // New provisioning replaces this device's network and clears old node keys.
    mesh_state_record record = {0};
    record.state = state;
    record.models.default_ttl = MODEL_TTL;
    record.models.sar_transmitter = MESH_SAR_TRANSMITTER_DEFAULT;
    record.models.sar_receiver = MESH_SAR_RX_DEFAULT;
    if (!mesh_state_save_record(&record)) return -1;
    if (!mesh_network_init(&state)) return -1;
    memset(&mesh_models, 0, sizeof(mesh_models));
    return mesh_models_init() ? 0 : -1;
}

int PROVISIONER_GET_DATA(prov_data *data, uint8_t num_elements) {
    if (!data || num_elements == 0) return -1;
    mesh_state_record record;
    if (!mesh_state_load_record(&record) ||
        record.state.unicast_address == 0 ||
        record.state.unicast_address > 0x7FFF ||
        record.state.element_count == 0 ||
        record.state.element_count > MESH_MAX_ELEMENTS ||
        record.state.net_key_index > 0x0FFF ||
        record.node_count >= PROVISIONER_MAX_NODES ||
        (record.state.key_refresh_phase == 2 && !record.state.has_new_key)
    ) return -1;

    uint16_t next_address = record.next_unicast_address;
    if (!next_address) {
        next_address = record.state.unicast_address + record.state.element_count;
    }
    if (next_address > 0x7FFF ||
        (uint32_t)next_address + num_elements - 1 > 0x7FFF) return -1;

    // Reserve the full range before sending Provisioning Data.
    record.next_unicast_address = next_address + num_elements;
    if (!mesh_state_save_record(&record)) return -1;

    const mesh_net_state *state = &record.state;
    int use_new_key = state->key_refresh_phase == 2;
    memcpy(data->net_key, use_new_key ? state->new_net_key : state->net_key, 16);
    data->net_key_index = state->net_key_index;
    data->flags = (use_new_key || state->phase2_provisioned ? 1 : 0) |
                  (state->iv_update ? 2 : 0);
    data->iv_index = state->iv_index;
    data->unicast_address = next_address;
    return 0;
}

int PROVISIONER_STORE_NODE_DEVKEY(
    const uint8_t device_key[16], uint16_t unicast_address,
    uint8_t num_elements
) {
    if (!device_key || num_elements == 0 || unicast_address == 0 ||
        (uint32_t)unicast_address + num_elements - 1 > 0x7FFF) return -1;

    mesh_state_record record;
    if (!mesh_state_load_record(&record) ||
        record.node_count >= PROVISIONER_MAX_NODES) return -1;

    if ((uint32_t)unicast_address + num_elements !=
        record.next_unicast_address) return -1;

    mesh_node_record *node = &record.nodes[record.node_count];
    memcpy(node->device_key, device_key, 16);
    node->unicast_address = unicast_address;
    node->num_elements = num_elements;
    record.node_count++;
    return mesh_state_save_record(&record) ? 0 : -1;
}

int BLE_MESH_TRANSPORT_GET_DEVICE_KEY(uint16_t address, uint8_t key[16]) {
    if (!key || address == 0 || address > 0x7fff) return 0;
    mesh_state_record record;
    if (!mesh_state_load_record(&record) ||
        record.node_count > PROVISIONER_MAX_NODES) return 0;

    if (address >= record.state.unicast_address &&
        (uint32_t)address < (uint32_t)record.state.unicast_address +
                            record.state.element_count) {
        memcpy(key, record.state.dev_key, 16);
        return 1;
    }
    for (uint8_t i = 0; i < record.node_count; i++) {
        const mesh_node_record *node = &record.nodes[i];
        if (address >= node->unicast_address &&
            (uint32_t)address < (uint32_t)node->unicast_address + node->num_elements) {
            memcpy(key, node->device_key, 16);
            return 1;
        }
    }
    return 0;
}

void BLE_MESH_ONOFF_CHANGED(uint16_t element, uint8_t on) {
    (void)element;
    (void)on;
}

void BLE_MESH_ONOFF_STATUS(uint16_t element, uint16_t src, uint8_t present) {
    (void)element;
    (void)src;
    (void)present;
}

void BLE_MESH_CONFIG_STATUS(uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len) {
    (void)src;
    (void)opcode;
    (void)params;
    (void)len;
}

void BLE_MESH_HEALTH_STATUS(uint16_t element, uint16_t src, uint32_t opcode,
                            const uint8_t *params, size_t len) {
    (void)element;
    (void)src;
    (void)opcode;
    (void)params;
    (void)len;
}

void BLE_MESH_HEALTH_ATTENTION(uint16_t element, uint8_t seconds) {
    (void)element;
    (void)seconds;
}

int BLE_MESH_HEALTH_TEST(uint16_t element, uint8_t test_id, uint8_t *faults, size_t *len) {
    int index = mesh_element_index(element);
    if (test_id || index < 0 || !faults || !len ||
        *len < mesh_models.health_server[index].current_count) return 0;
    // The default standard test returns the latest application-reported faults.
    *len = mesh_models.health_server[index].current_count;
    memcpy(faults, mesh_models.health_server[index].current, *len);
    return 1;
}

// Temporary RAM-only sequence storage; this resets after a reboot.
static volatile uint32_t ram_next_seq;

int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq) {
    ram_next_seq = next_seq;
    return 1;
}


int ECDH_GENERATE_KPAIR(uint8_t private_key[32], uint8_t public_key[64]) {
    // micro-ecc needs an RNG callback before it can generate a private key.
    uECC_set_rng(GET_RANDOM_BYTES);
    return uECC_make_key(public_key, private_key, uECC_secp256r1()) ? 0 : -1;
}

int ECDH_COMPUTE_DHKEY(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
) {
    uECC_Curve curve = uECC_secp256r1();
    // This validates the peer's key; it does not verify a signature.
    if (!uECC_valid_public_key(peer_public_key, curve)) return -1;
    return uECC_shared_secret(peer_public_key, private_key, dhkey, curve) ? 0 : -1;
}

int AUTH_COMPUTE_CONFIRMATION(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32], uint8_t confirmation_salt[16],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t confirmation_key[16], input[32], t[16];

    // s1(confirm_inputs) is AES-CMAC with an all-zero key.
    aes_cmac(zero, confirm_inputs, PROV_CONFIRM_INPUTS_LEN, confirmation_salt);

    // k1 derives the confirmation key from the DHKey and confirmation salt.
    aes_cmac(confirmation_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prck", 4, confirmation_key);
    memcpy(input, random, 16);
    memcpy(input + 16, auth_value, 16);
    aes_cmac(confirmation_key, input, sizeof(input), confirmation);
    return 0;
}

int AUTH_DERIVE_SESSION(
    const uint8_t dhkey[32], const uint8_t confirmation_salt[16],
    const uint8_t provisioner_random[16],
    const uint8_t provisionee_random[16],
    uint8_t session_key[16], uint8_t session_nonce[13],
    uint8_t device_key[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t input[48], provisioning_salt[16], nonce_key[16], t[16];

    memcpy(input, confirmation_salt, 16);
    memcpy(input + 16, provisioner_random, 16);
    memcpy(input + 32, provisionee_random, 16);

    // s1(confirmation_salt || both random values) uses an all-zero key.
    aes_cmac(zero, input, sizeof(input), provisioning_salt);

    // k1 uses the same first CMAC result for all three derived keys.
    aes_cmac(provisioning_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prsk", 4, session_key);
    aes_cmac(t, (const uint8_t *)"prsn", 4, nonce_key);
    memcpy(session_nonce, nonce_key + 3, 13);
    aes_cmac(t, (const uint8_t *)"prdk", 4, device_key);
    return 0;
}

// The provisioning state machine supplies a 25-byte data PDU and an 8-byte MIC.
// AES-CCM is already available here, so these two crypto interfaces can be wired now.
int AUTH_ENCRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t plain[25], uint8_t encrypted[25], uint8_t mic[8]
) {
    return ccm_encrypt_and_tag(session_key, session_nonce, 13, NULL, 0,
                               plain, 25, encrypted, mic, 8);
}

int AUTH_DECRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t encrypted[25], const uint8_t mic[8], uint8_t plain[25]
) {
    return ccm_auth_decrypt(session_key, session_nonce, 13, NULL, 0,
                            encrypted, 25, mic, 8, plain);
}

// Transport and access
// Message states
