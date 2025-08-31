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
        CH4 PD2
*/


/*
 * initialize TIM1 for PWM
 */
//! Expected funGpioInitAll() before init
void fun_t1pwm_init(uint8_t pin) {
	// Enable TIM1
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
	
	//! TIM2 remap mode
	AFIO->PCFR1 |= AFIO_PCFR1_TIM1_REMAP_NOREMAP;

	funPinMode(pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF);
		
	// Reset TIM1 to init all regs
	RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;
	
	// CTLR1: default is up, events generated, edge align
	// SMCFGR: default clk input is CK_INT
	
	// Prescaler 
	TIM1->PSC = 0x0000;
	
	// Auto Reload - sets period
	TIM1->ATRLR = 255;
	
	// Reload immediately
	TIM1->SWEVGR |= TIM_UG;
	
	// Enable CH1N output, positive pol
	TIM1->CCER |= TIM_CC1NE | TIM_CC1NP;
	
	// Enable CH4 output, positive pol
	TIM1->CCER |= TIM_CC4E | TIM_CC4P;
	
	// CH1 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
	TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1;
	
	// CH2 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
	TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1;
	
	// Set the Capture Compare Register value to 50% initially
	TIM1->CH1CVR = 128;
	TIM1->CH4CVR = 128;
	
	// Enable TIM1 outputs
	TIM1->BDTR |= TIM_MOE;
	
	// Enable TIM1
	TIM1->CTLR1 |= TIM_CEN;
}

/*
 * set timer channel PW
 */
void fun_t1pwm_setpw(uint8_t chl, uint16_t width) {
	switch(chl&3)
	{
		case 0: TIM1->CH1CVR = width; break;
		case 1: TIM1->CH2CVR = width; break;
		case 2: TIM1->CH3CVR = width; break;
		case 3: TIM1->CH4CVR = width; break;
	}
}

/*
 * force output (used for testing / debug)
 */
void fun_t1pwm_force(uint8_t chl, uint8_t val) {
	uint16_t temp;
	
	chl &= 3;
	
	if(chl < 2)
	{
		temp = TIM1->CHCTLR1;
		temp &= ~(TIM_OC1M<<(8*chl));
		temp |= (TIM_OC1M_2 | (val?TIM_OC1M_0:0))<<(8*chl);
		TIM1->CHCTLR1 = temp;
	}
	else
	{
		chl &= 1;
		temp = TIM1->CHCTLR2;
		temp &= ~(TIM_OC1M<<(8*chl));
		temp |= (TIM_OC1M_2 | (val?TIM_OC1M_0:0))<<(8*chl);
		TIM1->CHCTLR2 = temp;
	}
}

static uint32_t tm1pwm_count = 0;

void fund_t1pwm_task() {
	fun_t1pwm_setpw(0, tm1pwm_count); // Chl 1
	// fun_t1pwm_setpw(3, (tm1pwm_count + 128)&255);	// Chl 4
	tm1pwm_count++;
	tm1pwm_count &= 255;
	Delay_Ms(5);
}