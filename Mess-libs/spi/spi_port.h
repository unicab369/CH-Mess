/* SPI interface and MCU implementations for CH32V003 and CH582/CH583. */
#ifndef MESS_SPI_PORT_H
#define MESS_SPI_PORT_H

#include "ch32fun.h"
#include <stdint.h>

#define SPI_PORT_PIN_NONE 0xFFFFFFFFu
#define SPI_FRAME_8BIT 8u
#define SPI_FRAME_16BIT 16u

static uint32_t SPI_DC_PIN = SPI_PORT_PIN_NONE;

/* SPI interface */
static inline void SPI_INIT(void);
static inline void SPI_CONFIGURE_PINS(uint32_t reset_pin, uint32_t dc_pin);
static inline void SPI_RESET(uint32_t reset_pin, uint16_t delay_ms);
static inline void SPI_START(void);
static inline void SPI_STOP(void);
static inline void SPI_SET_PRESCALER(uint8_t prescaler);
static inline void SPI_SET_FRAME_SIZE(uint8_t bits);
static inline void SPI_WAIT_TX_COMPLETE(void);
static inline void SPI_WAIT_RX_AVAILABLE(void);
static inline uint8_t SPI_IS_RX_EMPTY(void);
static inline void SPI_WAIT_NOT_BUSY(void);
static inline uint8_t SPI_READ_8(void);
static inline uint16_t SPI_READ_16(void);
static inline void SPI_WRITE_8(uint8_t data);
static inline void SPI_WRITE_16(uint16_t data);
static inline uint8_t SPI_TRANSFER_8(uint8_t data);
static inline uint16_t SPI_TRANSFER_16(uint16_t data);
static inline void SPI_DC_LOW(void);
static inline void SPI_DC_HIGH(void);
static inline void SPI_CMD_8(uint8_t cmd);
static inline void SPI_CMD_DATA_8(uint8_t data);
static inline void SPI_CMD_DATA_16(uint16_t data);

#if defined(CH32V003)
static inline void SPI_DMA_INIT(DMA_Channel_TypeDef *channel);
static inline void SPI_SEND_DMA(const uint8_t *buffer, uint16_t len);
#endif


#if defined(CH32V003)

static inline void SPI_INIT(void) {
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
	SPI1->CTLR1 = 0;

	/* SPI1 pins: PC5=SCK, PC6=MOSI, PC7=MISO. */
	GPIOC->CFGLR &= ~((0xFu << (4 * 5)) | (0xFu << (4 * 6)) | (0xFu << (4 * 7)));
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 5); /* PC5 SCK */
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 6); /* PC6 MOSI */
	GPIOC->CFGLR |= GPIO_CNF_IN_FLOATING << (4 * 7); /* PC7 MISO */
	SPI1->CTLR1 = SPI_CPHA_1Edge | SPI_CPOL_Low | SPI_Mode_Master |
	              SPI_BaudRatePrescaler_4 | SPI_NSS_Soft |
	              SPI_Direction_2Lines_FullDuplex;
	SPI1->CTLR1 |= SPI_CTLR1_DFF; /* Default to 16-bit frames. */

	SPI_START();
}

static inline void SPI_CONFIGURE_PINS(uint32_t reset_pin, uint32_t dc_pin) {
	SPI_DC_PIN = dc_pin;
	if (reset_pin != SPI_PORT_PIN_NONE)
		funPinMode(reset_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
	if (dc_pin != SPI_PORT_PIN_NONE)
		funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static inline void SPI_START(void) { SPI1->CTLR1 |= SPI_CTLR1_SPE; }
static inline void SPI_STOP(void) { SPI1->CTLR1 &= ~SPI_CTLR1_SPE; }
static inline void SPI_SET_PRESCALER(uint8_t prescaler) {
	SPI_WAIT_NOT_BUSY();
	SPI_STOP();
	SPI1->CTLR1 &= ~SPI_CTLR1_BR;
	SPI1->CTLR1 |= SPI_CTLR1_BR & ((uint32_t)prescaler << 3);
	SPI_START();
}

static inline void SPI_WAIT_TX_COMPLETE(void) { while (!(SPI1->STATR & SPI_STATR_TXE)) { } }
static inline uint8_t SPI_IS_RX_EMPTY(void) { return !(SPI1->STATR & SPI_STATR_RXNE); }
static inline void SPI_WAIT_RX_AVAILABLE(void) { while (SPI_IS_RX_EMPTY()) { } }
static inline void SPI_WAIT_NOT_BUSY(void) { while (SPI1->STATR & SPI_STATR_BSY) { } }
static inline uint8_t SPI_READ_8(void) { return (uint8_t)SPI1->DATAR; }
static inline uint16_t SPI_READ_16(void) { return (uint16_t)SPI1->DATAR; }

static inline void SPI_SET_FRAME_SIZE(uint8_t bits) {
	uint8_t use_16bit = bits == SPI_FRAME_16BIT;
	if (use_16bit == ((SPI1->CTLR1 & SPI_CTLR1_DFF) != 0)) return;
	SPI_WAIT_NOT_BUSY();
	SPI_STOP();
	if (use_16bit) SPI1->CTLR1 |= SPI_CTLR1_DFF;
	else SPI1->CTLR1 &= ~SPI_CTLR1_DFF;
	SPI_START();
}

static inline void SPI_WRITE_8(uint8_t data) {
	uint8_t restore_16bit = (SPI1->CTLR1 & SPI_CTLR1_DFF) != 0;
	if (restore_16bit) SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	SPI1->DATAR = data;
	SPI_WAIT_RX_AVAILABLE();
	(void)SPI_READ_8();
	if (restore_16bit) SPI_SET_FRAME_SIZE(SPI_FRAME_16BIT);
}

static inline uint8_t SPI_TRANSFER_8(uint8_t data) {
	uint8_t restore_16bit = (SPI1->CTLR1 & SPI_CTLR1_DFF) != 0;
	if (restore_16bit) SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	SPI1->DATAR = data;
	SPI_WAIT_RX_AVAILABLE();
	uint8_t received = SPI_READ_8();
	if (restore_16bit) SPI_SET_FRAME_SIZE(SPI_FRAME_16BIT);
	return received;
}

static inline void SPI_WRITE_16(uint16_t data) {
	SPI1->DATAR = data;
	SPI_WAIT_RX_AVAILABLE();
	(void)SPI_READ_16();
	SPI_WAIT_NOT_BUSY();
}

static inline uint16_t SPI_TRANSFER_16(uint16_t data) {
	SPI1->DATAR = data;
	SPI_WAIT_RX_AVAILABLE();
	uint16_t received = SPI_READ_16();
	SPI_WAIT_NOT_BUSY();
	return received;
}

static inline void SPI_DMA_INIT(DMA_Channel_TypeDef *channel) {
	SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
	channel->PADDR = (uint32_t)&SPI1->DATAR;
	channel->CFGR = DMA_M2M_Disable | DMA_Priority_VeryHigh |
	                DMA_MemoryDataSize_Byte | DMA_PeripheralDataSize_Byte |
	                DMA_MemoryInc_Enable | DMA_PeripheralInc_Disable |
	                DMA_Mode_Circular | DMA_DIR_PeripheralDST;
}

static inline void SPI_SEND_DMA(const uint8_t *buffer, uint16_t len) {
	SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	SPI_DC_HIGH();
	DMA1_Channel3->CNTR = len;
	DMA1_Channel3->MADDR = (uint32_t)buffer;
	DMA1_Channel3->CFGR |= DMA_CFGR1_EN;
	DMA1->INTFCR = DMA1_FLAG_TC3;
	while (!(DMA1->INTFR & DMA1_FLAG_TC3)) { }
	DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
	SPI_WAIT_NOT_BUSY();
	SPI_SET_FRAME_SIZE(SPI_FRAME_16BIT);
}

#elif defined(CH582_CH583)

static inline void SPI_INIT(void) {
	funGpioInitAll();
	R16_PIN_ALTERNATE &= (uint16_t)~RB_PIN_SPI0;

	/* SPI0 pins: PA13=SCK, PA14=MOSI, PA15=MISO. */
	funPinMode(PA13, GPIO_CFGLR_OUT_2Mhz_PP); /* PA13 SCK */
	funPinMode(PA14, GPIO_CFGLR_OUT_2Mhz_PP); /* PA14 MOSI */
	funPinMode(PA15, GPIO_CFGLR_IN_FLOAT); /* PA15 MISO */
	/* Master, SPI mode 0: leave RB_SPI_MODE_SLAVE and RB_SPI_MST_SCK_MOD clear. */
	R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR &
	                   (uint8_t)~(RB_SPI_MODE_SLAVE | RB_SPI_MST_SCK_MOD);
	R8_SPI0_CTRL_CFG = RB_SPI_AUTO_IF;
	R8_SPI0_CLOCK_DIV = 4;

	SPI_START();
}

static inline void SPI_CONFIGURE_PINS(uint32_t reset_pin, uint32_t dc_pin) {
	SPI_DC_PIN = dc_pin;
	if (reset_pin != SPI_PORT_PIN_NONE)
		funPinMode(reset_pin, GPIO_CFGLR_OUT_2Mhz_PP);
	if (dc_pin != SPI_PORT_PIN_NONE)
		funPinMode(dc_pin, GPIO_CFGLR_OUT_2Mhz_PP);
}

static inline void SPI_START(void) {
	/* Release the FIFO/count reset before enabling the SPI pins. */
	R8_SPI0_CTRL_MOD = (R8_SPI0_CTRL_MOD & (uint8_t)~RB_SPI_ALL_CLEAR) |
	                   RB_SPI_SCK_OE | RB_SPI_MOSI_OE;
}
static inline void SPI_STOP(void) { R8_SPI0_CTRL_MOD = 0; }
static inline void SPI_SET_PRESCALER(uint8_t prescaler) {
	R8_SPI0_CLOCK_DIV = prescaler >= 7 ? 255 : 4;
}
static inline void SPI_SET_FRAME_SIZE(uint8_t bits) { (void)bits; }
static inline void SPI_WAIT_TX_COMPLETE(void) { while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { } }
static inline uint8_t SPI_IS_RX_EMPTY(void) { return !(R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END); }
static inline void SPI_WAIT_RX_AVAILABLE(void) { while (SPI_IS_RX_EMPTY()) { } }
static inline void SPI_WAIT_NOT_BUSY(void) { while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { } }
static inline uint8_t SPI_READ_8(void) { return R8_SPI0_BUFFER; }
static inline uint16_t SPI_READ_16(void) { return R8_SPI0_BUFFER; }

static inline void SPI_WRITE_8(uint8_t data) {
	R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
	R8_SPI0_BUFFER = data;
	SPI_WAIT_NOT_BUSY();
	(void)SPI_READ_8();
}
static inline uint8_t SPI_TRANSFER_8(uint8_t data) {
	R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
	R8_SPI0_BUFFER = data;
	SPI_WAIT_NOT_BUSY();
	return SPI_READ_8();
}

/* Match WCH's SPI0_MasterRecvByte: select receive direction, then clock 0xFF. */
static inline uint8_t SPI_RECEIVE_8(void) {
	R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
	R8_SPI0_BUFFER = 0xFF;
	SPI_WAIT_NOT_BUSY();
	return R8_SPI0_BUFFER;
}

/* Receive len bytes using the CH582 master FIFO procedure from the datasheet.
 * The caller must assert the peripheral's CS before calling and deassert it
 * after this function returns. The SPI controller generates the receive clocks.
 */
static inline uint8_t SPI_RECEIVE_FIFO(uint8_t *buffer, uint16_t len) {
	/* R16_SPI0_TOTAL_CNT uses only its low 12 bits. */
	if (buffer == 0 || len == 0 || len > 4095) return 0;
	SPI_WAIT_NOT_BUSY();
	while (R8_SPI0_FIFO_COUNT != 0) (void)R8_SPI0_FIFO;

	R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
	R16_SPI0_TOTAL_CNT = len;
	for (uint16_t i = 0; i < len; ++i) {
		uint32_t timeout = 1000000;
		while (R8_SPI0_FIFO_COUNT == 0 && --timeout != 0) { }
		if (timeout == 0) {
			R16_SPI0_TOTAL_CNT = 0;
			R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
			return 0;
		}
		buffer[i] = R8_SPI0_FIFO;
	}
	uint32_t timeout = 1000000;
	while (R16_SPI0_TOTAL_CNT != 0 && --timeout != 0) { }
	if (timeout == 0) {
		R16_SPI0_TOTAL_CNT = 0;
		R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
		return 0;
	}
	timeout = 1000000;
	while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE) && --timeout != 0) { }
	if (timeout == 0) {
		R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
		return 0;
	}
	R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
	return 1;
}

static inline void SPI_WRITE_16(uint16_t data) {
	SPI_WRITE_8((uint8_t)(data >> 8));
	SPI_WRITE_8((uint8_t)data);
}
static inline uint16_t SPI_TRANSFER_16(uint16_t data) {
	return (uint16_t)SPI_TRANSFER_8((uint8_t)(data >> 8)) << 8 |
	       SPI_TRANSFER_8((uint8_t)data);
}
#else
#error "spi_port.h supports CH32V003 and CH582/CH583 targets"
#endif

static inline void SPI_RESET(uint32_t reset_pin, uint16_t delay_ms) {
	if (reset_pin == SPI_PORT_PIN_NONE) return;
	funDigitalWrite(reset_pin, FUN_HIGH);
	funDigitalWrite(reset_pin, FUN_LOW);
	Delay_Ms(delay_ms);
	funDigitalWrite(reset_pin, FUN_HIGH);
	Delay_Ms(delay_ms);
}

static inline void SPI_DC_LOW(void) {
	if (SPI_DC_PIN != SPI_PORT_PIN_NONE) funDigitalWrite(SPI_DC_PIN, FUN_LOW);
}
static inline void SPI_DC_HIGH(void) {
	if (SPI_DC_PIN != SPI_PORT_PIN_NONE) funDigitalWrite(SPI_DC_PIN, FUN_HIGH);
}

static inline void SPI_CMD_8(uint8_t cmd) {
	SPI_DC_LOW();
	SPI_WRITE_8(cmd);
}
static inline void SPI_CMD_DATA_8(uint8_t data) {
	SPI_DC_HIGH();
	SPI_WRITE_8(data);
}
static inline void SPI_CMD_DATA_16(uint16_t data) {
	SPI_DC_HIGH();
	SPI_WRITE_16(data);
}

#endif
