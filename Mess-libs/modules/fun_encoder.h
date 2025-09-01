#include "ch32fun.h"
#include <stdio.h>

typedef struct {
	uint16_t initial_count;		// initial count
	uint16_t last_count;		// previous count
	int32_t relative_pos;
	int32_t delta;
} Encoder_t;


//# Timer 2 pin mappings by AFIO->PCFR1
/*	00	AFIO_PCFR1_TIM2_REMAP_NOREMAP
		D4		T2CH1ETR
		D3		T2CH2
		C0		T2CH3
		D7		T2CH4  		//? requires disabling nRST in opt
	01	AFIO_PCFR1_TIM2_REMAP_PARTIALREMAP1
		C5		T2CH1ETR_	//! SPI-SCK
		C2		T2CH2_		//! I2C-SDA
		D2		T2CH3_
		C1		T2CH4_		//! I2C-SCL
	10	AFIO_PCFR1_TIM2_REMAP_PARTIALREMAP2
		C1		T2CH1ETR_	//! I2C-SCL
		D3		T2CH2		
		C0		T2CH3
		D7		T2CH4  		//? requires disabling nRST in opt
	11	AFIO_PCFR1_TIM2_REMAP_FULLREMAP
		C1		T2CH1ETR_	//! I2C-SCL
		C7		T2CH2_		//! SPI-MISO
		D6		T2CH3_		//! UART_TX
		D5		T2CH4_		//! UART_RX
*/

//! Expected funGpioInitAll() before init
void fun_encoder_setup(Encoder_t *model) {
	RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

	//! TIM2 remap mode
	AFIO->PCFR1 |= AFIO_PCFR1_TIM2_REMAP_NOREMAP;

	funPinMode(PD3, GPIO_CFGLR_IN_PUPD);
	funPinMode(PD4, GPIO_CFGLR_IN_PUPD);
	funDigitalWrite(PD3, 1);
	funDigitalWrite(PD4, 1);

	//! Reset TIM2 to init all regs
	RCC->APB1PRSTR |= RCC_APB1Periph_TIM2;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_TIM2;
	
	// set TIM2 clock prescaler If you want to reduce the resolution of the encoder
	// TIM2->PSC = 0x0000;

	// set a automatic reload if you want the counter to wrap earlier than 0xffff
	//TIM2->ATRLR = 0xffff;

	// //# added
	// TIM2->ATRLR = 255;			// set PWM total cycle width

	// //# added
	// #define TIM2_DEFAULT 0xff
	// TIM2->CHCTLR2 |= TIM_OC3M_2 | TIM_OC3M_1 | TIM_OC3PE;	// CH3

	// SMCFGR: set encoder mode SMS=011b
	TIM2->SMCFGR |= TIM_EncoderMode_TI12;

	// set count to about mid-scale to avoid wrap-around
	TIM2->CNT = 0x8fff;

	// //# added
	// TIM2->CTLR1 |= TIM_ARPE;								// enable auto-reload of preload
	// TIM2->CCER |= TIM_CC3E | (TIM_CC3P & TIM2_DEFAULT);		// CH3

	TIM2->SWEVGR |= TIM_UG;			// initialize timer
	TIM2->CTLR1 |= TIM_CEN;			// TIM2 Counter Enable

	model->initial_count = TIM2->CNT;
	model->last_count = TIM2->CNT;
};

static uint32_t encoder_debounceTime = 0;

void fun_encoder_task(uint32_t time, Encoder_t *model, void (*handler)(Encoder_t *model)) {
	// if (time - encoder_debounceTime < 50) return;
	// encoder_debounceTime = time;

	uint16_t count = TIM2->CNT;

	if (count != model->last_count) {
		model->relative_pos = count - model->initial_count;
		model->delta 		= count - model->last_count;
		handler(model);
		model->last_count = count;
	}
}