/*
 * Example for using Advanced Control Timer (TIM1) for PWM generation
 * 03-28-2023 E. Brombaugh
 */

#include "ch32fun.h"
#include <stdio.h>

//# Timer 1 pin mappings by AFIO->PCFR1
/*  00	AFIO_PCFR1_TIM1_REMAP_NOREMAP
        (ETR/PC5, BKIN/PC2)
        CH1/CH1N PD2/PD0
        CH2/CH2N PA1/PA2
        CH3/CH3N PC3/PD1	//! PD1 SWIO
        CH4 PC4
    01	AFIO_PCFR1_TIM1_REMAP_PARTIALREMAP1
        (ETR/PA12, CH1/PA8, CH2/PA9, CH3/PA10, CH4/PA11, BKIN/PA6, CH1N/PA7, CH2N/PB0, CH3N/PB1)
        CH1/CH1N PC6/PC3	//! PC6 SPI-MOSI
        CH2/CH2N PC7/PC4	//! PC7 SPI-MISO
        CH3/CH3N PC0/PD1	//! PD1 SWIO
        CH4 PD3
    10	AFIO_PCFR1_TIM1_REMAP_PARTIALREMAP2
        (ETR/PD4, CH1/PD2, CH2/PA1, CH3/PC3, CH4/PC4, BKIN/PC2, CH1N/PD0, CN2N/PA2, CH3N/PD1)
        CH1/CH1N PD2/PD0
        CH2/CH2N PA1/PA2
        CH3/CH3N PC3/PD1	//! PD1 SWIO
        CH4 PC4
    11	AFIO_PCFR1_TIM1_REMAP_FULLREMAP
        (ETR/PE7, CH1/PE9, CH2/PE11, CH3/PE13, CH4/PE14, BKIN/PE15, CH1N/PE8, CH2N/PE10, CH3N/PE12)
        CH1/CH1N PC4/PC3	
        CH2/CH2N PC7/PD2	//! PC7 SPI-MISO
        CH3/CH3N PC5/PC6	//! PC5 SPI-SCK, PC6 SPI-MOSI
        CH4 PD4?
*/

typedef struct {
	uint8_t pin;
	uint8_t channel;
	uint16_t CCER;
	uint32_t counter;
	uint32_t timeRef;
} PWM_GPIO_t;

/*
 * initialize TIM1 for PWM
 */
//! Expected funGpioInitAll() before init
void fun_t1pwm_init() {
	// Enable TIM1
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
	
	//! TIM2 remap mode
	AFIO->PCFR1 |= AFIO_PCFR1_TIM1_REMAP_NOREMAP;
		
	// Reset TIM1 to init all regs
	RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;
	
	// CTLR1: default is up, events generated, edge align
	// SMCFGR: default clk input is CK_INT
	TIM1->PSC = 0x0000;			// Prescaler 
	TIM1->ATRLR = 255;			// Auto Reload - sets period
	TIM1->SWEVGR |= TIM_UG;		// Reload immediately
	
	TIM1->BDTR |= TIM_MOE;			// Enable TIM1 outputs
	TIM1->CTLR1 |= TIM_CEN;			// Enable TIM1
}

void fun_t1pwm_reload(PWM_GPIO_t* model) {
	model->counter = 0;
	model->timeRef = 0;
	funPinMode(model->pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF);

	// default value
	TIM1->CH1CVR = 255;
	TIM1->CH2CVR = 255;
	TIM1->CH3CVR = 255;
	TIM1->CH4CVR = 255;

	switch (model->CCER) {
		//# TIM1->CHCTLR1 Control Reg1: CH1 & CH2
		case TIM_CC1E:
			TIM1->CCER |= TIM_CC1E | TIM_CC1P;
			TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1;
			model->channel = 1;
			break;
		case TIM_CC1NE:
			TIM1->CCER |= TIM_CC1NE | TIM_CC1NP;
			TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1;
			model->channel = 1;
			break;
		case TIM_CC2E:
			model->channel = 2;
			TIM1->CCER |= TIM_CC2E | TIM_CC2P;
			TIM1->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1;
			break;
		case TIM_CC2NE:
			model->channel = 2;
			TIM1->CCER |= TIM_CC2NE | TIM_CC2NP;
			TIM1->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1;
			break;
		
		//# TIM1->CHCTLR2 Control Reg2: CH3 & CH4
		case TIM_CC3E:
			model->channel = 3;
			TIM1->CCER |= TIM_CC3E | TIM_CC3P;
			TIM1->CHCTLR2 |= TIM_OC3M_2 | TIM_OC3M_1;
			break;
		// case TIM_CC3NE: TIM1->CCER |= TIM_CC3E | TIM_CC3NP; break;	//! Prevent overwrite SWDIO
		case TIM_CC4E:
			model->channel = 4;
			TIM1->CCER |= TIM_CC4E | TIM_CC4P;
			TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1;
			break;
	}
}

/*
 * set timer channel PW
 */
void fun_t1pwm_setpw(uint8_t channel, uint16_t width) {
	switch(channel) {
		case 1: TIM1->CH1CVR = width; break;
		case 2: TIM1->CH2CVR = width; break;
		case 3: TIM1->CH3CVR = width; break;
		case 4: TIM1->CH4CVR = width; break;
	}
}


void fun_t1pwm_task(uint32_t time, PWM_GPIO_t* model) {
	if (time - model->timeRef < 5) { return; }
	model->timeRef = time;

	fun_t1pwm_setpw(model->channel, model->counter);
	model->counter++;
	model->counter &= 255;
}