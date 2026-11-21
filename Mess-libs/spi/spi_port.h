/* SPI interface and MCU implementations for CH32V003 and CH582/CH583. */
#ifndef MESS_SPI_PORT_H
#define MESS_SPI_PORT_H

#include "ch32fun.h"
#include <stdint.h>

#define SPI_PORT_PIN_NONE 0xFFu

static uint8_t SPI_DC_PIN = SPI_PORT_PIN_NONE;
static inline void SPI_DC_HIGH(void);

#if defined(CH32V003)

static inline void SPI_init(uint8_t reset_pin, uint8_t dc_pin) {
	SPI_DC_PIN = dc_pin;
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
	SPI1->CTLR1 = 0;
	GPIOC->CFGLR &= ~((0xFu << (4 * 5)) | (0xFu << (4 * 6)) | (0xFu << (4 * 7)));
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 5); /* SCK */
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 6); /* MOSI */
	GPIOC->CFGLR |= GPIO_CNF_IN_FLOATING << (4 * 7); /* MISO */
	SPI1->CTLR1 = SPI_CPHA_1Edge | SPI_CPOL_Low | SPI_Mode_Master |
	              SPI_BaudRatePrescaler_4 | SPI_NSS_Soft |
	              SPI_Direction_2Lines_FullDuplex;
	SPI1->CTLR1 &= ~SPI_CTLR1_DFF;
	SPI1->CTLR1 |= SPI_CTLR1_SPE;
	if (reset_pin != SPI_PORT_PIN_NONE) {
		funPinMode(reset_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
		funDigitalWrite(reset_pin, FUN_HIGH);
		funDigitalWrite(reset_pin, FUN_LOW);
		Delay_Ms(100);
		funDigitalWrite(reset_pin, FUN_HIGH);
		Delay_Ms(100);
	}
	if (dc_pin != SPI_PORT_PIN_NONE)
		funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static inline void SPI_end(void) { SPI1->CTLR1 &= ~SPI_CTLR1_SPE; }
static inline uint8_t SPI_read_8(void) { return (uint8_t)SPI1->DATAR; }
static inline uint16_t SPI_read_16(void) { return (uint16_t)SPI1->DATAR; }

static inline void SPI_write_8(uint8_t data) {
	SPI1->DATAR = data;
	while (!(SPI1->STATR & SPI_STATR_TXE)) { }
	while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
	(void)SPI1->DATAR;
}

static inline void SPI_write_16(uint16_t data) {
	while (SPI1->STATR & SPI_STATR_BSY) { }
	SPI1->CTLR1 &= ~SPI_CTLR1_SPE;
	SPI1->CTLR1 |= SPI_CTLR1_DFF | SPI_CTLR1_SPE;
	SPI1->DATAR = data;
	while (!(SPI1->STATR & SPI_STATR_TXE)) { }
	while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
	(void)SPI1->DATAR;
	while (SPI1->STATR & SPI_STATR_BSY) { }
	SPI1->CTLR1 &= ~SPI_CTLR1_SPE;
	SPI1->CTLR1 &= ~SPI_CTLR1_DFF;
	SPI1->CTLR1 |= SPI_CTLR1_SPE;
}

static inline void SPI_wait_TX_complete(void) { while (!(SPI1->STATR & SPI_STATR_TXE)) { } }
static inline uint8_t SPI_is_RX_empty(void) { return (SPI1->STATR & SPI_STATR_RXNE) != 0; }
static inline void SPI_wait_RX_available(void) { while (!(SPI1->STATR & SPI_STATR_RXNE)) { } }
static inline void SPI_wait_not_busy(void) { while (SPI1->STATR & SPI_STATR_BSY) { } }
static inline void SPI_wait_transmit_finished(void) {
	SPI_wait_TX_complete();
	SPI_wait_not_busy();
}
static inline uint8_t SPI_transfer_8(uint8_t data) {
	SPI1->DATAR = data;
	while (!(SPI1->STATR & SPI_STATR_TXE)) { }
	while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
	return (uint8_t)SPI1->DATAR;
}
static inline uint16_t SPI_transfer_16(uint16_t data) {
	while (SPI1->STATR & SPI_STATR_BSY) { }
	SPI1->CTLR1 &= ~SPI_CTLR1_SPE;
	SPI1->CTLR1 |= SPI_CTLR1_DFF | SPI_CTLR1_SPE;
	SPI1->DATAR = data;
	while (!(SPI1->STATR & SPI_STATR_TXE)) { }
	while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
	uint16_t result = (uint16_t)SPI1->DATAR;
	while (SPI1->STATR & SPI_STATR_BSY) { }
	SPI1->CTLR1 &= ~SPI_CTLR1_SPE;
	SPI1->CTLR1 &= ~SPI_CTLR1_DFF;
	SPI1->CTLR1 |= SPI_CTLR1_SPE;
	return result;
}

static inline void SPI_DMA_init(DMA_Channel_TypeDef *channel) {
	SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
	channel->PADDR = (uint32_t)&SPI1->DATAR;
	channel->CFGR = DMA_M2M_Disable | DMA_Priority_VeryHigh |
	                DMA_MemoryDataSize_Byte | DMA_PeripheralDataSize_Byte |
	                DMA_MemoryInc_Enable | DMA_PeripheralInc_Disable |
	                DMA_Mode_Circular | DMA_DIR_PeripheralDST;
}

static inline void SPI_send_DMA(const uint8_t *buffer, uint16_t len) {
	SPI_DC_HIGH();
	DMA1_Channel3->CNTR = len;
	DMA1_Channel3->MADDR = (uint32_t)buffer;
	DMA1_Channel3->CFGR |= DMA_CFGR1_EN;
	DMA1->INTFCR = DMA1_FLAG_TC3;
	while (!(DMA1->INTFR & DMA1_FLAG_TC3)) { }
	DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
}

#elif defined(CH582_CH583)

static inline void SPI_init(uint8_t reset_pin, uint8_t dc_pin) {
	SPI_DC_PIN = dc_pin;
	funGpioInitAll();
	R16_PIN_ALTERNATE &= (uint16_t)~RB_PIN_SPI0;
	funPinMode(PA14, GPIO_CFGLR_OUT_2Mhz_PP); /* MOSI */
	funPinMode(PA13, GPIO_CFGLR_OUT_2Mhz_PP); /* SCK */
	R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
	R8_SPI0_CTRL_MOD = RB_SPI_SCK_OE | RB_SPI_MOSI_OE;
	R8_SPI0_CTRL_CFG = RB_SPI_AUTO_IF;
	R8_SPI0_CLOCK_DIV = 4;
	if (reset_pin != SPI_PORT_PIN_NONE) {
		funPinMode(reset_pin, GPIO_CFGLR_OUT_2Mhz_PP);
		funDigitalWrite(reset_pin, FUN_HIGH);
		funDigitalWrite(reset_pin, FUN_LOW);
		Delay_Ms(100);
		funDigitalWrite(reset_pin, FUN_HIGH);
		Delay_Ms(100);
	}
	if (dc_pin != SPI_PORT_PIN_NONE)
		funPinMode(dc_pin, GPIO_CFGLR_OUT_2Mhz_PP);
}

static inline void SPI_end(void) { R8_SPI0_CTRL_MOD = 0; }
static inline uint8_t SPI_read_8(void) { return R8_SPI0_BUFFER; }
static inline uint16_t SPI_read_16(void) { return R8_SPI0_BUFFER; }
static inline void SPI_write_8(uint8_t data) {
	R8_SPI0_BUFFER = data;
	while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { }
	(void)R8_SPI0_BUFFER;
}
static inline void SPI_write_16(uint16_t data) {
	SPI_write_8((uint8_t)(data >> 8));
	SPI_write_8((uint8_t)data);
}
static inline void SPI_wait_TX_complete(void) { while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { } }
static inline uint8_t SPI_is_RX_empty(void) { return (R8_SPI0_INT_FLAG & RB_SPI_FREE) != 0; }
static inline void SPI_wait_RX_available(void) { SPI_wait_TX_complete(); }
static inline void SPI_wait_not_busy(void) { while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { } }
static inline void SPI_wait_transmit_finished(void) { SPI_wait_TX_complete(); }
static inline uint8_t SPI_transfer_8(uint8_t data) {
	R8_SPI0_BUFFER = data;
	while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { }
	return R8_SPI0_BUFFER;
}
static inline uint16_t SPI_transfer_16(uint16_t data) {
	return (uint16_t)SPI_transfer_8((uint8_t)(data >> 8)) << 8 |
	       SPI_transfer_8((uint8_t)data);
}

#else
#error "spi_port.h supports CH32V003 and CH582/CH583 targets"
#endif

static inline void SPI_DC_LOW(void) {
	if (SPI_DC_PIN != SPI_PORT_PIN_NONE) funDigitalWrite(SPI_DC_PIN, FUN_LOW);
}
static inline void SPI_DC_HIGH(void) {
	if (SPI_DC_PIN != SPI_PORT_PIN_NONE) funDigitalWrite(SPI_DC_PIN, FUN_HIGH);
}

static inline void SPI_cmd_8(uint8_t cmd) {
	SPI_DC_LOW();
	SPI_write_8(cmd);
}
static inline void SPI_cmd_data_8(uint8_t data) {
	SPI_DC_HIGH();
	SPI_write_8(data);
}
static inline void SPI_cmd_data_16(uint16_t data) {
	SPI_DC_HIGH();
	SPI_write_16(data);
}

#endif
