/* SD-card SPI hardware port selected by the active CH32FUN target. */
#ifndef SD_SPI_PORT_H
#define SD_SPI_PORT_H

#include "ch32fun.h"

#if defined(CH582_CH583)

/* CH582 wiring: SPI0 PA13=SCK, PA14=MOSI, PA15=MISO; PA5=card CS. */
#define PIN_MISO PA15
#define PIN_MOSI PA14
#define PIN_SCK  PA13
#define SD_SPI_CS_PIN PA5
#ifndef SPI_CS_HIGH
#define SPI_CS_HIGH() funDigitalWrite(SD_SPI_CS_PIN, FUN_HIGH)
#endif
#ifndef SPI_CS_LOW
#define SPI_CS_LOW() funDigitalWrite(SD_SPI_CS_PIN, FUN_LOW)
#endif

void SPI_SET_PRESCALER(uint8_t prescaler) {
	/* The shared driver uses STM32-style baud-rate values (0 = fastest,
	 * 7 = initialization speed). CH582 takes a direct clock divisor.
	 */
	R8_SPI0_CLOCK_DIV = prescaler >= 7 ? 255 : 4;
}

void SPI_INIT(void) {
	funGpioInitAll(); /* No-op on CH5xx. */
	R16_PIN_ALTERNATE &= (uint16_t)~RB_PIN_SPI0;
	funPinMode(PIN_MOSI, GPIO_CFGLR_OUT_2Mhz_PP);
	funPinMode(PIN_SCK, GPIO_CFGLR_OUT_2Mhz_PP);
	funPinMode(SD_SPI_CS_PIN, GPIO_CFGLR_OUT_2Mhz_PP);
	SPI_CS_HIGH();
	funPinMode(PIN_MISO, GPIO_CFGLR_IN_PUPD);
	R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
	R8_SPI0_CTRL_MOD = RB_SPI_SCK_OE | RB_SPI_MOSI_OE;
	R8_SPI0_CTRL_CFG = RB_SPI_AUTO_IF; /* Mode 0, MSB first; clear byte flag on buffer access. */
	SPI_SET_PRESCALER(0);
}

void SPI_END(void) { R8_SPI0_CTRL_MOD = 0; }
uint8_t SPI_READ_8(void) { return R8_SPI0_FIFO; }
void SPI_WRITE_8(uint8_t data) { R8_SPI0_FIFO = data; }

uint8_t SPI_TRANSFER_8(uint8_t data) {
	/* BUFFER starts a single-byte full-duplex exchange; FIFO transfers require TOTAL_CNT. */
	R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
	R8_SPI0_BUFFER = data;
	while (!(R8_SPI0_INT_FLAG & RB_SPI_FREE)) { }
	return R8_SPI0_BUFFER;
}

void SPI_POWEROFF(void) {
	SPI_END();
	R8_SLP_CLK_OFF1 |= RB_SLP_CLK_SPI0;
}
void SPI_POWERON(void) { R8_SLP_CLK_OFF1 &= (uint8_t)~RB_SLP_CLK_SPI0; }

#elif defined(CH32V003)

/* CH32V003 SPI1 pins: PC5=SCK, PC6=MOSI, PC7=MISO. */
#ifndef SPI_CS_HIGH
#error "Define SPI_CS_HIGH() before including sd_spi_port.h"
#endif
#ifndef SPI_CS_LOW
#error "Define SPI_CS_LOW() before including sd_spi_port.h"
#endif
void SPI_SET_PRESCALER(uint8_t prescaler) {
	SPI1->CTLR1 &= ~SPI_CTLR1_BR;
	SPI1->CTLR1 |= SPI_CTLR1_BR & ((uint32_t)prescaler << 3);
}

void SPI_INIT(void) {
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
	SPI1->CTLR1 = 0;
	GPIOC->CFGLR &= ~((0xFu << (4 * 5)) | (0xFu << (4 * 6)) | (0xFu << (4 * 7)));
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 5);
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 6);
	GPIOC->CFGLR |= GPIO_CNF_IN_FLOATING << (4 * 7);
	SPI1->CTLR1 = SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft |
	              SPI_Mode_Master | SPI_Direction_2Lines_FullDuplex;
	SPI1->CTLR1 &= ~SPI_CTLR1_DFF;
	SPI1->CTLR1 |= SPI_CTLR1_SPE;
	SPI_SET_PRESCALER(0);
}

void SPI_END(void) { SPI1->CTLR1 &= ~SPI_CTLR1_SPE; }
uint8_t SPI_READ_8(void) { return (uint8_t)SPI1->DATAR; }
void SPI_WRITE_8(uint8_t data) { SPI1->DATAR = data; }
uint8_t SPI_TRANSFER_8(uint8_t data) {
	SPI_WRITE_8(data);
	while (!(SPI1->STATR & SPI_STATR_TXE)) { }
	while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
	return SPI_READ_8();
}
void SPI_POWEROFF(void) { SPI_END(); RCC->APB2PCENR &= ~RCC_APB2Periph_SPI1; }
void SPI_POWERON(void) { RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1; }

#else
#error "No SD-card SPI port is defined for this target"
#endif

#endif
