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
#include "../../ble_lib/ble_gatt_transport.h"
#include "../../ble_lib/gap_ports/ch58x_isler.h"
#include "../../ble_lib/gatt_services/ble_gatt_services.h"

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

// Watch these in the debugger to see how long the main-loop cycle takes.
volatile uint32_t basic_gatt_loop_us;
volatile uint32_t basic_gatt_loop_max_us;

static ble_gatt_server gatt_server;
static ble_gatt_transport gatt_transport;
static ble_gatt_bas_service battery_service;
static ble_gap_gatt_peer gatt_links[GAP_CONNECTION_COUNT] = {0};

static int gatt_start(void) {
    gatt_server_init(&gatt_server, 64);

    if (!gatt_bas_add(&gatt_server, &battery_service, 50))
        return 0;

    return ble_gap_gatt_bind(&gatt_links[0], &gatt_transport,
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
        !gap_ad_builder_init(&builder, advertising_data, sizeof(advertising_data)) ||
        !gap_ad_flags(&builder, 0x06) ||
        !gap_ad_uuid16_list(&builder, &service_uuid, 1, 1) ||
        !gap_ad_local_name(&builder, local_name, sizeof(local_name) - 1, 1) ||
        !gap_adv_start(ADV_IND, advertising_data, builder.len, NULL, 0, 100)
    ) {
        for (;;) {}
    }

    uint8_t level = 50;
    uint32_t next_update = GET_MILLIS() + 1000;
    uint32_t next_loop_report = GET_MILLIS() + 2000;

    for (;;) {
        uint64_t loop_start_ticks = funSysTick64();
        uint32_t now = GET_MILLIS();

        if (gap_conn_count())
            gap_conn_poll_all();

        gap_hw_scan_poll();
        (void)ble_gap_gatt_poll(gatt_links);

        if ((int32_t)(now - next_update) >= 0) {
            next_update = now + 1000;
            level = level >= 100 ? 0 : (uint8_t)(level + 10);
            (void)gatt_bas_notify_level(&gatt_server, &battery_service, level);
        }

        (void)gap_send_due(NULL, 0, now, NULL, NULL);

        uint32_t loop_us = (uint32_t)((funSysTick64() - loop_start_ticks) /
                                      DELAY_US_TIME);
        basic_gatt_loop_us = loop_us;
        if (loop_us > basic_gatt_loop_max_us)
            basic_gatt_loop_max_us = loop_us;

        uint32_t report_now = GET_MILLIS();

        if ((int32_t)(report_now - next_loop_report) >= 0) {
            next_loop_report = report_now + 2000;
            printf("Loop: %lu us, max: %lu us\n",
                   (unsigned long)basic_gatt_loop_us,
                   (unsigned long)basic_gatt_loop_max_us);
        }
    }
}
