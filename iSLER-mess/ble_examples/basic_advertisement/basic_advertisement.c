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

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

int main(void) {
    static const uint8_t local_name[] = "Ch32fun - CH582";
    uint8_t advertising_data[31];
    gap_ad_builder builder;

    SystemInit();
    seed((uint32_t)funSysTick64());
    gap_hw_init();

    if (!gap_ad_builder_init(&builder, advertising_data,
                             sizeof(advertising_data)) ||
        !gap_ad_add_flags(&builder, 0x06) ||
        !gap_ad_add_local_name(&builder, local_name,
                               sizeof(local_name) - 1, 1) ||
        !gap_adv_start(advertising_data, builder.len, 100)) {
        for (;;) {}
    }

    for (;;) {
        gap_send_due(NULL, 0, GET_MILLIS(), NULL, NULL);
    }
}
