#include "ch32fun.h"

void gap_hw_received(void);
void gap_hw_radio_transmitted(void);

#define ISLER_CALLBACK_TX gap_hw_radio_transmitted
#define ISLER_CALLBACK_RX gap_hw_received
#include "iSLER.h"

#include "../../aes_cmm.h"
#include "lib_rand.h"
#ifndef ROM_CFG_MAC_ADDR
#define ROM_CFG_MAC_ADDR ((const uint32_t *)0x0007F018)
#endif
#define GAP_RADIO_BUFFER_ATTR ISLER_BUF_ATTR
#include "../../ble_lib/ble_gap.h"
#include "../../ble_lib/gap_ports/ch58x_isler.h"
#include "../../ble_lib/gatt_services/ble_gatt_services.h"

void L2CAP_INTERFACE_CHANNEL_OPENED(
    void *context, uint16_t psm, uint16_t local, uint16_t remote, uint16_t mtu
) {
    gatt_transport_channel_opened(context, psm, local, remote, mtu);
}

void L2CAP_INTERFACE_CHANNEL_CLOSED(
    void *context, uint16_t psm, uint16_t local, uint16_t remote,
    uint16_t reason
) {
    gatt_transport_channel_closed(context, psm, local, remote, reason);
}

int L2CAP_INTERFACE_CHANNEL_DATA(
    void *context, uint16_t local, const uint8_t *sdu, uint16_t len
) {
    return gatt_transport_channel_data(context, local, sdu, len);
}

int L2CAP_INTERFACE_CONNECTION_UPDATE(
    void *context, uint16_t min, uint16_t max, uint16_t latency,
    uint16_t timeout
) {
    (void)context; (void)min; (void)max; (void)latency; (void)timeout;
    return 0;
}

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

static ble_gatt_server gatt_server;
static ble_gatt_transport gatt_transport;
static ble_gatt_bas_service battery_service;
static ble_gap_gatt gatt_links;

static int gatt_start(void) {
    gatt_server_init(&gatt_server, 64);

    if (!gatt_bas_add(&gatt_server, &battery_service, 50))
        return 0;

    ble_gap_gatt_init(&gatt_links);
    return ble_gap_gatt_bind_slot(&gatt_links, 0, &gatt_transport,
                                  &gatt_server, NULL);
}

int main(void) {
    static const uint8_t local_name[] = "CH582 GATT";
    const uint16_t service_uuid = 0x180f;
    uint8_t advertising_data[31];
    gap_ad_builder builder;

    SystemInit();
    seed((uint32_t)funSysTick64());
    gap_hw_init();

    if (!gatt_start() ||
        !gap_ad_builder_init(&builder, advertising_data,
                             sizeof(advertising_data)) ||
        !gap_ad_flags(&builder, 0x06) ||
        !gap_ad_uuid16_list(&builder, &service_uuid, 1, 1) ||
        !gap_ad_local_name(&builder, local_name,
                           sizeof(local_name) - 1, 1) ||
        !gap_adv_start_connectable(advertising_data, builder.len,
                                   NULL, 0, 100)
    ) {
        for (;;) {}
    }

    uint8_t level = 50;
    uint32_t next_update = GET_MILLIS() + 1000;

    for (;;) {
        uint32_t now = GET_MILLIS();

        if (gap_conn_count())
            gap_conn_poll_all();

        gap_hw_scan_poll();
        (void)ble_gap_gatt_poll(&gatt_links);

        if ((int32_t)(now - next_update) >= 0) {
            next_update = now + 1000;
            level = level >= 100 ? 0 : (uint8_t)(level + 10);
            (void)gatt_bas_notify_level(&gatt_server,
                                                     &battery_service, level);
        }

        (void)gap_send_due(NULL, 0, now, NULL, NULL);
    }
}
