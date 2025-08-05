// NOTE: CONNECT WS2812's to PC6

#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

#define WS2812DMA_IMPLEMENTATION
//#define WSRBG //For WS2816C's.
#define WSGRB // For SK6805-EC15
#define NR_LEDS 190

#include "ws2812b_dma_spi_led_driver.h"

#include "color_utilities.h"


uint16_t phases[NR_LEDS];
int frameno = 0;
volatile int tween = -NR_LEDS;

// Callbacks that you must implement.
uint32_t WS2812BLEDCallback( int ledno ) {
	uint8_t index = (phases[ledno])>>8;
	uint8_t rsbase = sintable[index];
	uint8_t rs = rsbase>>3;
	uint32_t fire = ((huetable[(rs+190)&0xff]>>1)<<16) | (huetable[(rs+30)&0xff]) | ((huetable[(rs+0)]>>1)<<8);
	uint32_t ice  = 0x7f0000 | ((rsbase>>1)<<8) | ((rsbase>>1));

	// Because this chip doesn't natively support multiplies, we are going to avoid tweening of 1..254.
	return TweenHexColors( fire, ice, ((tween + ledno)>0)?255:0 ); // Where "tween" is a value from 0 ... 255
}


static int k;
static int tweendir = 0;

void ws2812_setup() {
	WS2812BDMAInit();
	for( k = 0; k < NR_LEDS; k++ ) phases[k] = k<<8;
}

int ws2812_task() {
    // Delay_Ms(12);
    while(WS2812BLEDInUse);
    frameno++;

    if( frameno == 1024 ) {
        tweendir = 1;
    }
    if( frameno == 2048 ) {
        tweendir = -1;
        frameno = 0;
    }

    if( tweendir ) {
        int t = tween + tweendir;
        if( t > 255 ) t = 255;
        if( t < -NR_LEDS ) t = -NR_LEDS;
        tween = t;
    }

    for( k = 0; k < NR_LEDS; k++ ) {
        phases[k] += ((((rands[k&0xff])+0xf)<<2) + (((rands[k&0xff])+0xf)<<1))>>1;
    }

    WS2812BDMAStart(NR_LEDS);
}