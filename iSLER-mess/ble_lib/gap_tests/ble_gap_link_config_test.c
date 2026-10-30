#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint16_t radio_data_max;
#define RADIO_DATA_MAX radio_data_max
#define GAP_CENTRAL_SUPPORT 1
#define GAP_PERIPHERAL_SUPPORT 1
#define GAP_SECURITY_SUPPORT 0
#define GAP_EXT_ADV_SUPPORT 0
#include "../ble_crypto.h"
#include "../ble_gap.h"

static uint64_t radio_ticks;
static uint8_t *configured_frame;
static uint8_t configured_tx;
static uint8_t tx_started, rx_started;

uint32_t GET_MILLIS(void) { return 1000; }
uint32_t GAP_CRITICAL_ENTER(void) { return 0; }
void GAP_CRITICAL_EXIT(uint32_t state) { (void)state; }
const uint8_t *RADIO_RX_FRAME(void) { return NULL; }
int8_t RADIO_RSSI(void) { return -40; }
void RADIO_INIT(void) {}
void RADIO_STOP(void) {}
int RADIO_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel) {
    (void)frame; (void)len; (void)channel; return 1;
}
void RADIO_TX_TIMER_BEGIN(void) {}
int RADIO_CONNECT_TX_TIMER_ARM(uint8_t *frame, uint8_t len, uint8_t channel) {
    (void)frame; (void)len; (void)channel; return 1;
}
void RADIO_CONNECT_TX_TIMER_CANCEL(void) {}
int RADIO_ADV_TX_PHY(uint8_t *frame, uint8_t len, uint8_t channel, uint8_t phy) {
    (void)frame; (void)len; (void)channel; (void)phy; return 1;
}
void RADIO_LINK_CONFIG(uint32_t aa, uint8_t channel, uint8_t *frame,
                        uint8_t tx, uint8_t tx_phy, uint8_t rx_phy,
                        uint32_t crc_init) {
    (void)aa; (void)channel; (void)tx_phy; (void)rx_phy;
    (void)crc_init;
    configured_frame = frame;
    configured_tx = tx;
}
uint8_t RADIO_PHY_MASK(void) { return GAP_PHY_1M; }
void RADIO_LINK_TX(void) { tx_started++; }
int RADIO_LINK_TX_AT(uint64_t start_ticks) {
    (void)start_ticks; RADIO_LINK_TX(); return 1;
}
int RADIO_IFS_TX_TIMER_ARM(void) { RADIO_LINK_TX(); return 1; }
void RADIO_LINK_RX(void) { rx_started++; }
void RADIO_SCAN_RX(uint8_t channel) { (void)channel; }
void RADIO_TX_BUFFER(const uint8_t *frame) { (void)frame; }
int RADIO_TX_DONE(void) { return 1; }
void RADIO_TX_CLEAR_DONE(void) {}
uint64_t RADIO_TICKS(void) { return radio_ticks++; }
uint64_t HW_TICKS_FROM_US(uint32_t us) { return us; }
void RADIO_PUBLIC_ADDRESS(uint8_t address[6]) { memset(address, 0, 6); }
int GAP_RANDOM_BYTES(uint8_t *out, size_t len) {
    memset(out, 0, len); return 1;
}
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
    (void)key; memcpy(out, in, 16);
}

int main(void) {
    gap_conn_context *conn = &gap_conn_ctx[0];
    radio_data_max = GAP_CONN_DATA_MAX;
    memset(conn, 0, sizeof(*conn));
    gap_conn_slot = 0;
    gap_conn_generations[0] = 1;
    gap_conn_slot_mask = GAP_CONN_SLOT_BIT(0);
    radio_data_max = 27;

    conn->is_central = 1;
    conn->link_layer.next_ticks = 10000;
    conn->link_layer.last_rx_ms = 1000;
    conn->link_layer.access_address = 0x12345678;
    conn->link_layer.hop = 5;
    conn->link_layer.used_count = 37;
    memset(conn->link_layer.channel_map, 0xff, sizeof(conn->link_layer.channel_map));
    conn->link_layer.channel_map[4] &= 0x1f;
    conn->schedule.interval_125us = 240;
    conn->schedule.factor = 1;
    conn->schedule.timeout_10ms = 200;
    conn->data_length.tx_octets = 27;
    conn->data_length.tx_time = 328;
    conn->central.central_anchor_set = 1;

    radio_ticks = 10000;
    gap_conn_poll();
    assert(configured_frame == conn->tx_frame && configured_tx);
    assert(tx_started == 1 && rx_started == 0);

    gap_poll.slot_valid = 0;
    conn->link_state.rx_armed = 0;
    conn->link_state.channel_selected = 0;
    conn->link_layer.tx_pending = 0;
    conn->is_central = 0;
    radio_ticks = conn->link_layer.next_ticks;
    gap_conn_poll();
    assert(configured_frame == NULL && !configured_tx);
    assert(tx_started == 1 && rx_started == 1);
    return 0;
}
