#include <assert.h>
#include <stdint.h>
#define BLE_GATT_EATT_MAX_BEARERS 2
#include "../ble_gatt_eatt.h"

typedef struct {
    uint16_t last_psm, last_mtu, last_cid, sent_len, closed;
    uint8_t received, ready, rejected;
    uint8_t last_pdu[128];
} fake_ecfc;

static int open_channel(void *ctx, uint16_t psm, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_psm = psm; f->last_mtu = mtu; return 1;
}
static int accept_channel(void *ctx, uint16_t cid, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->last_mtu = mtu; return 1;
}
static void reject_channel(void *ctx, uint16_t cid, uint16_t reason) {
    fake_ecfc *f = ctx; (void)cid; (void)reason; f->rejected++;
}
static int send_sdu(void *ctx, uint16_t cid, const uint8_t *pdu, uint16_t len) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->sent_len = len;
    assert(len <= sizeof(f->last_pdu));
    for (uint16_t i = 0; i < len; i++) f->last_pdu[i] = pdu[i];
    return 1;
}
static void close_channel(void *ctx, uint16_t cid) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->closed++;
}
static void ready(void *ctx, uint16_t cid, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->last_mtu = mtu; f->ready++;
}
static int receive_att(void *ctx, uint16_t cid, const uint8_t *pdu,
                       uint16_t len) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->received++;
    f->sent_len = len; f->last_pdu[0] = pdu[0]; return 1;
}

int main(void) {
    fake_ecfc fake = {0};
    ble_gatt_eatt_ops ops = {open_channel, accept_channel, reject_channel,
        send_sdu, close_channel, ready, NULL, receive_att, &fake};
    ble_gatt_eatt eatt;
    ble_gatt_eatt_init(&eatt, &ops, 100);
    assert(!ble_gatt_eatt_open(&eatt));
    ble_gatt_eatt_set_encrypted(&eatt, 1);
    assert(ble_gatt_eatt_open(&eatt));
    assert(fake.last_psm == BLE_GATT_EATT_PSM && fake.last_mtu == 100);
    assert(ble_gatt_eatt_open(&eatt));
    assert(!ble_gatt_eatt_open(&eatt));
    assert(ble_gatt_eatt_channel_opened(&eatt, 0x0040, 80, 1, 1));
    assert(ble_gatt_eatt_channel_opened(&eatt, 0x0041, 120, 1, 1));
    assert(fake.ready == 2 && fake.last_mtu == 100);
    assert(!ble_gatt_eatt_channel_opened(&eatt, 0x0042, 63, 1, 1));
    assert(!ble_gatt_eatt_channel_opened(&eatt, 0x0043, 90, 0, 1));
    assert(ble_gatt_eatt_find(&eatt, 0x0040) >= 0);
    uint8_t request[] = {0x0a, 1, 0};
    assert(ble_gatt_eatt_send(&eatt, 0x0040, request, sizeof(request)));
    assert(fake.last_cid == 0x0040 && fake.sent_len == sizeof(request));
    assert(ble_gatt_eatt_receive(&eatt, 0x0040, request, sizeof(request)));
    assert(fake.received == 1);
    uint8_t signed_write[] = {0xd2, 1, 0};
    assert(!ble_gatt_eatt_receive(&eatt, 0x0040, signed_write,
                                  sizeof(signed_write)));
    assert(!ble_gatt_eatt_send(&eatt, 0x0040, request, 101));
    assert(ble_gatt_eatt_reconfigure(&eatt, 0x0040, 70));
    assert(!ble_gatt_eatt_send(&eatt, 0x0040, request, 71));
    ble_gatt_eatt_channel_closed(&eatt, 0x0040, 7);
    assert(ble_gatt_eatt_find(&eatt, 0x0040) < 0);
    assert(ble_gatt_eatt_accept(&eatt, 0x0042));
    assert(fake.last_cid == 0x0042);
    ble_gatt_eatt_set_encrypted(&eatt, 0);
    assert(fake.closed >= 2);
    assert(!ble_gatt_eatt_send(&eatt, 0x0041, request, sizeof(request)));
    ble_gatt_eatt_disconnect(&eatt);
    return 0;
}
