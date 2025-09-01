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

//# Timer 2 pin mappings by AFIO->PCFR1
/*	00 	AFIO_PCFR1_TIM2_REMAP_NOREMAP
		D4		T2CH1ETR
		D3		T2CH2
		C0		T2CH3
		D7		T2CH4		//! RST PIN
	01 	AFIO_PCFR1_TIM2_REMAP_PARTIALREMAP1
		C5		T2CH1ETR_	//! SPI-SCK
		C2		T2CH2_		//! I2C-SDA
		D2		T2CH3_
		C1		T2CH4_		//! I2C-SCL
	10	AFIO_PCFR1_TIM2_REMAP_PARTIALREMAP2
		C1		T2CH1ETR_	//! I2C-SCL
		D3		T2CH2
		C0		T2CH3
		D7		T2CH4
	11	AFIO_PCFR1_TIM2_REMAP_FULLREMAP
		C1		T2CH1ETR_	//! I2C-SCL
		C7		T2CH2_		//! SPI-MISO
		D6		T2CH3_
		D5		T2CH4_
*/


typedef struct {
	uint8_t pin;
	uint8_t channel;
	uint16_t CCER;
	TIM_TypeDef* TIM;
	uint32_t counter;
	uint32_t timeRef;
} TIM_PWM_t;

//! Expected funGpioInitAll() before init
void fun_timPWM_init(TIM_PWM_t* model) {	
	//! TIM2 remap mode
	AFIO->PCFR1 |= AFIO_PCFR1_TIM1_REMAP_NOREMAP;
	
	TIM_TypeDef* timer = model->TIM;
	
	if (timer == TIM1) {
		RCC->APB2PCENR |= RCC_APB2Periph_TIM1;

		// Reset TIM1 to init all regs
		RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
		RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;
		
		timer->BDTR |= TIM_MOE;			// Enable TIM1 outputs
	
	} else if (timer == TIM2) {
		RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

		// Reset TIM2 to init all regs
		RCC->APB1PRSTR |= RCC_APB1Periph_TIM2;
		RCC->APB1PRSTR &= ~RCC_APB1Periph_TIM2;
	}

	// CTLR1: default is up, events generated, edge align
	// SMCFGR: default clk input is CK_INT
	timer->PSC = 0x0000;			// Prescaler 
	timer->ATRLR = 255;				// Auto Reload - sets period

	timer->SWEVGR |= TIM_UG;		// Reload immediately
	timer->CTLR1 |= TIM_CEN;		// Enable timer
}

void fun_timPWM_reload(TIM_PWM_t* model) {
	model->counter = 0;
	model->timeRef = 0;
	funPinMode(model->pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF);
	TIM_TypeDef* timer = model->TIM;

	// default value
	timer->CH1CVR = 255;
	timer->CH2CVR = 255;
	timer->CH3CVR = 255;
	timer->CH4CVR = 255;

	switch (model->CCER) {
		//# TIM1->CHCTLR1 Control Reg1: CH1 & CH2
		case TIM_CC1E:
			timer->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE;		// TIM_OC1PE is used by TIM2 only
			timer->CCER |= TIM_CC1E | TIM_CC1P;
			model->channel = 1;
			break;
		case TIM_CC1NE:
			timer->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1;
			timer->CCER |= TIM_CC1NE | TIM_CC1NP;
			model->channel = 1;
			break;
		case TIM_CC2E:
			timer->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1 | TIM_OC2PE;		// TIM_OC2PE is used by TIM2 only
			timer->CCER |= TIM_CC2E | TIM_CC2P;
			model->channel = 2;
			break;
		case TIM_CC2NE:
			timer->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1;
			timer->CCER |= TIM_CC2NE | TIM_CC2NP;
			model->channel = 2;
			break;
		
		//# TIM1->CHCTLR2 Control Reg2: CH3 & CH4
		case TIM_CC3E:
			timer->CHCTLR2 |= TIM_OC3M_2 | TIM_OC3M_1 | TIM_OC3PE;		// TIM_OC3PE is used by TIM2 only
			timer->CCER |= TIM_CC3E | TIM_CC3P;
			model->channel = 3;
			break;
		// case TIM_CC3NE: TIM1->CCER |= TIM_CC3E | TIM_CC3NP; break;	//! Prevent overwrite SWDIO
		case TIM_CC4E:
			timer->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1 | TIM_OC4PE;		// TIM_OC4PE is used by TIM2 only
			timer->CCER |= TIM_CC4E | TIM_CC4P;
			model->channel = 4;
			break;
	}
}


void fun_timPWM_setpw(TIM_PWM_t* model, uint16_t width) {
	TIM_TypeDef* timer = model->TIM;

	switch(model->channel) {
		case 1: timer->CH1CVR = width; break;
		case 2: timer->CH2CVR = width; break;
		case 3: timer->CH3CVR = width; break;
		case 4: timer->CH4CVR = width; break;
	}
}


void fun_timPWM_task(uint32_t time, TIM_PWM_t* model) {
	if (time - model->timeRef < 5) { return; }
	model->timeRef = time;

	fun_timPWM_setpw(model, model->counter);
	model->counter++;
	model->counter &= 255;
}