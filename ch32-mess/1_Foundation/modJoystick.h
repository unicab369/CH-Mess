
#include "ch32fun.h"
#include <stdio.h>

#define ADC_NUMCHLS 2
volatile uint16_t adc_buffer[ADC_NUMCHLS];

void modJoystick_setup() {
	// ADCCLK = 24 MHz => RCC_ADCPRE = 0: divide by 2
	RCC->CFGR0 &= ~(0x1F<<11);
	
	// Enable GPIOD and ADC
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1;
	
	// GPIOD->CFGLR &= ~(0xf<<(4*4));	// PD4 Analog input Chan7
	// GPIOD->CFGLR &= ~(0xf<<(4*6));	// PD5 Analog input Chan6
	// GPIOD->CFGLR &= ~(0xf<<(4*5));	// PD6 Analog input Chan5
	// GPIOD->CFGLR &= ~(0xf<<(4*3));	// PD3 Analog input Chan4
	// GPIOD->CFGLR &= ~(0xf<<(4*2));	// PD2 Analog input Chan3
	// GPIOC->CFGLR &= ~(0xf<<(4*4));	// PC4 Analog input Chan2
	// GPIOA->CFGLR &= ~(0xf<<(4*0));	// PA1 Analog input Chan1
	// GPIOA->CFGLR &= ~(0xf<<(4*1));	// PA2 Analog input Chan0

    int chanA = 0;
    int chanB = 1;
    GPIOA->CFGLR &= ~(0xf<<(4*0));
    GPIOA->CFGLR &= ~(0xf<<(4*1));
	
	// Reset the ADC to init all regs
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

	// Set up four conversions on chl 7, 4, 3, 2
	ADC1->RSQR1 = (ADC_NUMCHLS-1) << 20;	// four chls in the sequence
	ADC1->RSQR2 = 0;
    ADC1->RSQR3 = (chanA<<(5*0)) | (chanB<<(5*1));
	
	// set sampling time for chl 7, 4, 3, 2
	// 0:7 => 3/9/15/30/43/57/73/241 cycles
    ADC1->SAMPTR2 = (7<<(3*chanA)) | (7<<(3*chanB));
    
    // turn on ADC
	ADC1->CTLR2 |= ADC_ADON;
	
	// Reset and calibrate
	ADC1->CTLR2 |= ADC_RSTCAL;
	while(ADC1->CTLR2 & ADC_RSTCAL);
	ADC1->CTLR2 |= ADC_CAL;
	while(ADC1->CTLR2 & ADC_CAL);
	
	// Turn on DMA
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
	
	//DMA1_Channel1 is for ADC
	DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;
	DMA1_Channel1->MADDR = (uint32_t)adc_buffer;
	DMA1_Channel1->CNTR  = ADC_NUMCHLS;
	DMA1_Channel1->CFGR  =
		DMA_M2M_Disable |		 
		DMA_Priority_VeryHigh |
		DMA_MemoryDataSize_HalfWord |
		DMA_PeripheralDataSize_HalfWord |
		DMA_MemoryInc_Enable |
		DMA_Mode_Circular |
		DMA_DIR_PeripheralSRC;
	
	DMA1_Channel1->CFGR |= DMA_CFGR1_EN;    // Turn on DMA channel 1
	ADC1->CTLR1 |= ADC_SCAN;                // enable scanning
	
	// Enable continuous conversion and DMA
	ADC1->CTLR2 |= ADC_CONT | ADC_DMA | ADC_EXTSEL;
	ADC1->CTLR2 |= ADC_SWSTART;             // start conversion
}

uint16_t last_x = 0;
uint16_t last_y = 0;

int modJoystick_task() {
    printf("%4d %4d\n\r", adc_buffer[0], adc_buffer[1]);
}