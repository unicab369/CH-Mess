#include "ch32fun.h"
#include <stdio.h>

#include "modules/modiSLER.h"
#include "modules/systick_irq.h"
#include "modules/modWS2812.h"

#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#endif



void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
	}
}

int main()
{
	SystemInit();
    systick_init();			//! required for millis()

	funGpioInitAll();
	funPinMode(LED, GPIO_CFGLR_OUT_2Mhz_PP);

	RFCoreInit(LL_TX_POWER_0_DBM);
	blink(5);
	printf(".~ ch32fun iSLER ~.\n");

    WS2812BDMAInit();

    uint32_t sec_time = 0;
    WS2812_resetTask(0);

	while(1) {
        uint32_t now = millis();

        if (now - sec_time > 2000) {
            sec_time = now;
            uint8_t data[] = "I like ble 777777";
            modiSLER_adv_data(data, sizeof(data));
            blink(1);
            WS2812_resetTask(now);
        }

        WS2812_task();
	}
}
